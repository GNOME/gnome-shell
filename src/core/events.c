/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002, 2003, 2004 Red Hat, Inc.
 * Copyright (C) 2003, 2004 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "events.h"

#include <X11/Xatom.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/shape.h>

#include <meta/errors.h>
#include "display-private.h"
#include "window-private.h"
#include "bell.h"
#include "workspace-private.h"
#include "backends/meta-backend.h"
#include "backends/native/meta-backend-native.h"
#include "backends/native/meta-idle-monitor-native.h"
#include "backends/x11/meta-backend-x11.h"

#include "x11/window-x11.h"
#include "x11/xprops.h"
#include "wayland/meta-xwayland.h"
#include "wayland/meta-wayland-private.h"
#include "meta-surface-actor-wayland.h"

static MetaWindow *
get_window_for_event (MetaDisplay        *display,
                      const ClutterEvent *event)
{
  ClutterActor *source;

  if (display->grab_op != META_GRAB_OP_NONE)
    return display->grab_window;

  /* Always use the key focused window for key events. */
  switch (event->type)
    {
    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
      return display->focus_window;
    default:
      break;
    }

  source = clutter_event_get_source (event);
  if (META_IS_SURFACE_ACTOR (source))
    return meta_surface_actor_get_window (META_SURFACE_ACTOR (source));

  return NULL;
}

static XIEvent *
get_input_event (MetaDisplay *display,
                 XEvent      *event)
{
  if (event->type == GenericEvent &&
      event->xcookie.extension == display->xinput_opcode)
    {
      XIEvent *input_event;

      /* NB: GDK event filters already have generic events
       * allocated, so no need to do XGetEventData() on our own
       */
      input_event = (XIEvent *) event->xcookie.data;

      switch (input_event->evtype)
        {
        case XI_Motion:
        case XI_ButtonPress:
        case XI_ButtonRelease:
          if (((XIDeviceEvent *) input_event)->deviceid == META_VIRTUAL_CORE_POINTER_ID)
            return input_event;
          break;
        case XI_KeyPress:
        case XI_KeyRelease:
          if (((XIDeviceEvent *) input_event)->deviceid == META_VIRTUAL_CORE_KEYBOARD_ID)
            return input_event;
          break;
        case XI_FocusIn:
        case XI_FocusOut:
          if (((XIEnterEvent *) input_event)->deviceid == META_VIRTUAL_CORE_KEYBOARD_ID)
            return input_event;
          break;
        case XI_Enter:
        case XI_Leave:
          if (((XIEnterEvent *) input_event)->deviceid == META_VIRTUAL_CORE_POINTER_ID)
            return input_event;
          break;
#ifdef HAVE_XI23
        case XI_BarrierHit:
        case XI_BarrierLeave:
          if (((XIBarrierEvent *) input_event)->deviceid == META_VIRTUAL_CORE_POINTER_ID)
            return input_event;
          break;
#endif /* HAVE_XI23 */
        default:
          break;
        }
    }

  return NULL;
}

static Window
xievent_get_modified_window (MetaDisplay *display,
                             XIEvent *input_event)
{
  switch (input_event->evtype)
    {
    case XI_Motion:
    case XI_ButtonPress:
    case XI_ButtonRelease:
    case XI_KeyPress:
    case XI_KeyRelease:
      return ((XIDeviceEvent *) input_event)->event;
    case XI_FocusIn:
    case XI_FocusOut:
    case XI_Enter:
    case XI_Leave:
      return ((XIEnterEvent *) input_event)->event;
#ifdef HAVE_XI23
    case XI_BarrierHit:
    case XI_BarrierLeave:
      return ((XIBarrierEvent *) input_event)->event;
#endif /* HAVE_XI23 */
    }

  return None;
}

/* Return the window this has to do with, if any, rather
 * than the frame or root window that was selecting
 * for substructure
 */
static Window
event_get_modified_window (MetaDisplay *display,
                           XEvent *event)
{
  XIEvent *input_event = get_input_event (display, event);

  if (input_event)
    return xievent_get_modified_window (display, input_event);

  switch (event->type)
    {
    case KeymapNotify:
    case Expose:
    case GraphicsExpose:
    case NoExpose:
    case VisibilityNotify:
    case ResizeRequest:
    case PropertyNotify:
    case SelectionClear:
    case SelectionRequest:
    case SelectionNotify:
    case ColormapNotify:
    case ClientMessage:
      return event->xany.window;

    case CreateNotify:
      return event->xcreatewindow.window;

    case DestroyNotify:
      return event->xdestroywindow.window;

    case UnmapNotify:
      return event->xunmap.window;

    case MapNotify:
      return event->xmap.window;

    case MapRequest:
      return event->xmaprequest.window;

    case ReparentNotify:
     return event->xreparent.window;

    case ConfigureNotify:
      return event->xconfigure.window;

    case ConfigureRequest:
      return event->xconfigurerequest.window;

    case GravityNotify:
      return event->xgravity.window;

    case CirculateNotify:
      return event->xcirculate.window;

    case CirculateRequest:
      return event->xcirculaterequest.window;

    case MappingNotify:
      return None;

    default:
      if (META_DISPLAY_HAS_SHAPE (display) &&
          event->type == (display->shape_event_base + ShapeNotify))
        {
          XShapeEvent *sev = (XShapeEvent*) event;
          return sev->window;
        }

      return None;
    }
}

static guint32
event_get_time (MetaDisplay *display,
                XEvent      *event)
{
  XIEvent *input_event = get_input_event (display, event);

  if (input_event)
    return input_event->time;

  switch (event->type)
    {
    case PropertyNotify:
      return event->xproperty.time;

    case SelectionClear:
    case SelectionRequest:
    case SelectionNotify:
      return event->xselection.time;

    case KeymapNotify:
    case Expose:
    case GraphicsExpose:
    case NoExpose:
    case MapNotify:
    case UnmapNotify:
    case VisibilityNotify:
    case ResizeRequest:
    case ColormapNotify:
    case ClientMessage:
    case CreateNotify:
    case DestroyNotify:
    case MapRequest:
    case ReparentNotify:
    case ConfigureNotify:
    case ConfigureRequest:
    case GravityNotify:
    case CirculateNotify:
    case CirculateRequest:
    case MappingNotify:
    default:
      return CurrentTime;
    }
}

G_GNUC_UNUSED const char*
meta_event_detail_to_string (int d)
{
  const char *detail = "???";
  switch (d)
    {
      /* We are an ancestor in the A<->B focus change relationship */
    case XINotifyAncestor:
      detail = "NotifyAncestor";
      break;
    case XINotifyDetailNone:
      detail = "NotifyDetailNone";
      break;
      /* We are a descendant in the A<->B focus change relationship */
    case XINotifyInferior:
      detail = "NotifyInferior";
      break;
    case XINotifyNonlinear:
      detail = "NotifyNonlinear";
      break;
    case XINotifyNonlinearVirtual:
      detail = "NotifyNonlinearVirtual";
      break;
    case XINotifyPointer:
      detail = "NotifyPointer";
      break;
    case XINotifyPointerRoot:
      detail = "NotifyPointerRoot";
      break;
    case XINotifyVirtual:
      detail = "NotifyVirtual";
      break;
    }

  return detail;
}

G_GNUC_UNUSED const char*
meta_event_mode_to_string (int m)
{
  const char *mode = "???";
  switch (m)
    {
    case XINotifyNormal:
      mode = "NotifyNormal";
      break;
    case XINotifyGrab:
      mode = "NotifyGrab";
      break;
    case XINotifyUngrab:
      mode = "NotifyUngrab";
      break;
    case XINotifyWhileGrabbed:
      mode = "NotifyWhileGrabbed";
      break;
    }

  return mode;
}

G_GNUC_UNUSED static const char*
stack_mode_to_string (int mode)
{
  switch (mode)
    {
    case Above:
      return "Above";
    case Below:
      return "Below";
    case TopIf:
      return "TopIf";
    case BottomIf:
      return "BottomIf";
    case Opposite:
      return "Opposite";
    }

  return "Unknown";
}

G_GNUC_UNUSED static gint64
sync_value_to_64 (const XSyncValue *value)
{
  gint64 v;

  v = XSyncValueLow32 (*value);
  v |= (((gint64)XSyncValueHigh32 (*value)) << 32);

  return v;
}

G_GNUC_UNUSED static const char*
alarm_state_to_string (XSyncAlarmState state)
{
  switch (state)
    {
    case XSyncAlarmActive:
      return "Active";
    case XSyncAlarmInactive:
      return "Inactive";
    case XSyncAlarmDestroyed:
      return "Destroyed";
    default:
      return "(unknown)";
    }
}

G_GNUC_UNUSED static void
meta_spew_xi2_event (MetaDisplay *display,
                     XIEvent     *input_event,
                     const char **name_p,
                     char       **extra_p)
{
  const char *name = NULL;
  char *extra = NULL;

  XIEnterEvent *enter_event = (XIEnterEvent *) input_event;

  switch (input_event->evtype)
    {
    case XI_FocusIn:
      name = "XI_FocusIn";
      break;
    case XI_FocusOut:
      name = "XI_FocusOut";
      break;
    case XI_Enter:
      name = "XI_Enter";
      break;
    case XI_Leave:
      name = "XI_Leave";
      break;
#ifdef HAVE_XI23
    case XI_BarrierHit:
      name = "XI_BarrierHit";
      break;
    case XI_BarrierLeave:
      name = "XI_BarrierLeave";
      break;
#endif /* HAVE_XI23 */
    }

  switch (input_event->evtype)
    {
    case XI_FocusIn:
    case XI_FocusOut:
      extra = g_strdup_printf ("detail: %s mode: %s\n",
                               meta_event_detail_to_string (enter_event->detail),
                               meta_event_mode_to_string (enter_event->mode));
      break;
    case XI_Enter:
    case XI_Leave:
      extra = g_strdup_printf ("win: 0x%lx root: 0x%lx mode: %s detail: %s focus: %d x: %g y: %g",
                               enter_event->event,
                               enter_event->root,
                               meta_event_mode_to_string (enter_event->mode),
                               meta_event_detail_to_string (enter_event->detail),
                               enter_event->focus,
                               enter_event->root_x,
                               enter_event->root_y);
      break;
    }

  *name_p = name;
  *extra_p = extra;
}

G_GNUC_UNUSED static void
meta_spew_core_event (MetaDisplay *display,
                      XEvent      *event,
                      const char **name_p,
                      char       **extra_p)
{
  const char *name = NULL;
  char *extra = NULL;

  switch (event->type)
    {
    case KeymapNotify:
      name = "KeymapNotify";
      break;
    case Expose:
      name = "Expose";
      break;
    case GraphicsExpose:
      name = "GraphicsExpose";
      break;
    case NoExpose:
      name = "NoExpose";
      break;
    case VisibilityNotify:
      name = "VisibilityNotify";
      break;
    case CreateNotify:
      name = "CreateNotify";
      extra = g_strdup_printf ("parent: 0x%lx window: 0x%lx",
                               event->xcreatewindow.parent,
                               event->xcreatewindow.window);
      break;
    case DestroyNotify:
      name = "DestroyNotify";
      extra = g_strdup_printf ("event: 0x%lx window: 0x%lx",
                               event->xdestroywindow.event,
                               event->xdestroywindow.window);
      break;
    case UnmapNotify:
      name = "UnmapNotify";
      extra = g_strdup_printf ("event: 0x%lx window: 0x%lx from_configure: %d",
                               event->xunmap.event,
                               event->xunmap.window,
                               event->xunmap.from_configure);
      break;
    case MapNotify:
      name = "MapNotify";
      extra = g_strdup_printf ("event: 0x%lx window: 0x%lx override_redirect: %d",
                               event->xmap.event,
                               event->xmap.window,
                               event->xmap.override_redirect);
      break;
    case MapRequest:
      name = "MapRequest";
      extra = g_strdup_printf ("window: 0x%lx parent: 0x%lx\n",
                               event->xmaprequest.window,
                               event->xmaprequest.parent);
      break;
    case ReparentNotify:
      name = "ReparentNotify";
      extra = g_strdup_printf ("window: 0x%lx parent: 0x%lx event: 0x%lx\n",
                               event->xreparent.window,
                               event->xreparent.parent,
                               event->xreparent.event);
      break;
    case ConfigureNotify:
      name = "ConfigureNotify";
      extra = g_strdup_printf ("x: %d y: %d w: %d h: %d above: 0x%lx override_redirect: %d",
                               event->xconfigure.x,
                               event->xconfigure.y,
                               event->xconfigure.width,
                               event->xconfigure.height,
                               event->xconfigure.above,
                               event->xconfigure.override_redirect);
      break;
    case ConfigureRequest:
      name = "ConfigureRequest";
      extra = g_strdup_printf ("parent: 0x%lx window: 0x%lx x: %d %sy: %d %sw: %d %sh: %d %sborder: %d %sabove: %lx %sstackmode: %s %s",
                               event->xconfigurerequest.parent,
                               event->xconfigurerequest.window,
                               event->xconfigurerequest.x,
                               event->xconfigurerequest.value_mask &
                               CWX ? "" : "(unset) ",
                               event->xconfigurerequest.y,
                               event->xconfigurerequest.value_mask &
                               CWY ? "" : "(unset) ",
                               event->xconfigurerequest.width,
                               event->xconfigurerequest.value_mask &
                               CWWidth ? "" : "(unset) ",
                               event->xconfigurerequest.height,
                               event->xconfigurerequest.value_mask &
                               CWHeight ? "" : "(unset) ",
                               event->xconfigurerequest.border_width,
                               event->xconfigurerequest.value_mask &
                               CWBorderWidth ? "" : "(unset)",
                               event->xconfigurerequest.above,
                               event->xconfigurerequest.value_mask &
                               CWSibling ? "" : "(unset)",
                               stack_mode_to_string (event->xconfigurerequest.detail),
                               event->xconfigurerequest.value_mask &
                               CWStackMode ? "" : "(unset)");
      break;
    case GravityNotify:
      name = "GravityNotify";
      break;
    case ResizeRequest:
      name = "ResizeRequest";
      extra = g_strdup_printf ("width = %d height = %d",
                               event->xresizerequest.width,
                               event->xresizerequest.height);
      break;
    case CirculateNotify:
      name = "CirculateNotify";
      break;
    case CirculateRequest:
      name = "CirculateRequest";
      break;
    case PropertyNotify:
      {
        char *str;
        const char *state;

        name = "PropertyNotify";

        meta_error_trap_push (display);
        str = XGetAtomName (display->xdisplay,
                            event->xproperty.atom);
        meta_error_trap_pop (display);

        if (event->xproperty.state == PropertyNewValue)
          state = "PropertyNewValue";
        else if (event->xproperty.state == PropertyDelete)
          state = "PropertyDelete";
        else
          state = "???";

        extra = g_strdup_printf ("atom: %s state: %s",
                                 str ? str : "(unknown atom)",
                                 state);
        meta_XFree (str);
      }
      break;
    case SelectionClear:
      name = "SelectionClear";
      break;
    case SelectionRequest:
      name = "SelectionRequest";
      break;
    case SelectionNotify:
      name = "SelectionNotify";
      break;
    case ColormapNotify:
      name = "ColormapNotify";
      break;
    case ClientMessage:
      {
        char *str;
        name = "ClientMessage";
        meta_error_trap_push (display);
        str = XGetAtomName (display->xdisplay,
                            event->xclient.message_type);
        meta_error_trap_pop (display);
        extra = g_strdup_printf ("type: %s format: %d\n",
                                 str ? str : "(unknown atom)",
                                 event->xclient.format);
        meta_XFree (str);
      }
      break;
    case MappingNotify:
      name = "MappingNotify";
      break;
    default:
      if (META_DISPLAY_HAS_XSYNC (display) &&
          event->type == (display->xsync_event_base + XSyncAlarmNotify))
        {
          XSyncAlarmNotifyEvent *aevent = (XSyncAlarmNotifyEvent*) event;

          name = "XSyncAlarmNotify";
          extra =
            g_strdup_printf ("alarm: 0x%lx"
                             " counter_value: %" G_GINT64_FORMAT
                             " alarm_value: %" G_GINT64_FORMAT
                             " time: %u alarm state: %s",
                             aevent->alarm,
                             (gint64) sync_value_to_64 (&aevent->counter_value),
                             (gint64) sync_value_to_64 (&aevent->alarm_value),
                             (unsigned int)aevent->time,
                             alarm_state_to_string (aevent->state));
        }
      else
        if (META_DISPLAY_HAS_SHAPE (display) &&
            event->type == (display->shape_event_base + ShapeNotify))
          {
            XShapeEvent *sev = (XShapeEvent*) event;

            name = "ShapeNotify";

            extra =
              g_strdup_printf ("kind: %s "
                               "x: %d y: %d w: %u h: %u "
                               "shaped: %d",
                               sev->kind == ShapeBounding ?
                               "ShapeBounding" :
                               (sev->kind == ShapeClip ?
                                "ShapeClip" : "(unknown)"),
                               sev->x, sev->y, sev->width, sev->height,
                               sev->shaped);
          }
        else
          {
            name = "(Unknown event)";
            extra = g_strdup_printf ("type: %d", event->xany.type);
          }
      break;
    }

  *name_p = name;
  *extra_p = extra;
}

G_GNUC_UNUSED static void
meta_spew_event (MetaDisplay *display,
                 XEvent      *event)
{
  MetaScreen *screen = display->screen;
  const char *name = NULL;
  char *extra = NULL;
  char *winname;
  XIEvent *input_event;

  /* filter overnumerous events */
  if (event->type == Expose || event->type == MotionNotify ||
      event->type == NoExpose)
    return;

  if (event->type == (display->damage_event_base + XDamageNotify))
    return;

  if (event->type == (display->xsync_event_base + XSyncAlarmNotify))
    return;

  if (event->type == PropertyNotify && event->xproperty.atom == display->atom__NET_WM_USER_TIME)
    return;

  input_event = get_input_event (display, event);

  if (input_event)
    meta_spew_xi2_event (display, input_event, &name, &extra);
  else
    meta_spew_core_event (display, event, &name, &extra);

  if (event->xany.window == screen->xroot)
    winname = g_strdup_printf ("root %d", screen->number);
  else
    winname = g_strdup_printf ("0x%lx", event->xany.window);

  g_print ("%s on %s%s %s %sserial %lu\n", name, winname,
           extra ? ":" : "", extra ? extra : "",
           event->xany.send_event ? "SEND " : "",
           event->xany.serial);

  g_free (winname);

  if (extra)
    g_free (extra);
}

static void
handle_window_focus_event (MetaDisplay  *display,
                           MetaWindow   *window,
                           XIEnterEvent *event,
                           unsigned long serial)
{
  MetaWindow *focus_window;
#ifdef WITH_VERBOSE_MODE
  const char *window_type;

  /* Note the event can be on either the window or the frame,
   * we focus the frame for shaded windows
   */
  if (window)
    {
      if (event->event == window->xwindow)
        window_type = "client window";
      else if (window->frame && event->event == window->frame->xwindow)
        window_type = "frame window";
      else
        window_type = "unknown client window";
    }
  else if (meta_display_xwindow_is_a_no_focus_window (display, event->event))
    window_type = "no_focus_window";
  else if (event->event == display->screen->xroot)
    window_type = "root window";
  else
    window_type = "unknown window";

  meta_topic (META_DEBUG_FOCUS,
              "Focus %s event received on %s 0x%lx (%s) "
              "mode %s detail %s serial %lu\n",
              event->evtype == XI_FocusIn ? "in" :
              event->evtype == XI_FocusOut ? "out" :
              "???",
              window ? window->desc : "",
              event->event, window_type,
              meta_event_mode_to_string (event->mode),
              meta_event_detail_to_string (event->mode),
              event->serial);
#endif

  /* FIXME our pointer tracking is broken; see how
   * gtk+/gdk/x11/gdkevents-x11.c or XFree86/xc/programs/xterm/misc.c
   * for how to handle it the correct way.  In brief you need to track
   * pointer focus and regular focus, and handle EnterNotify in
   * PointerRoot mode with no window manager.  However as noted above,
   * accurate focus tracking will break things because we want to keep
   * windows "focused" when using keybindings on them, and also we
   * sometimes "focus" a window by focusing its frame or
   * no_focus_window; so this all needs rethinking massively.
   *
   * My suggestion is to change it so that we clearly separate
   * actual keyboard focus tracking using the xterm algorithm,
   * and mutter's "pretend" focus window, and go through all
   * the code and decide which one should be used in each place;
   * a hard bit is deciding on a policy for that.
   *
   * http://bugzilla.gnome.org/show_bug.cgi?id=90382
   */

  /* We ignore grabs, though this is questionable. It may be better to
   * increase the intelligence of the focus window tracking.
   *
   * The problem is that keybindings for windows are done with
   * XGrabKey, which means focus_window disappears and the front of
   * the MRU list gets confused from what the user expects once a
   * keybinding is used.
   */

  if (event->mode == XINotifyGrab ||
      event->mode == XINotifyUngrab ||
      /* From WindowMaker, ignore all funky pointer root events */
      event->detail > XINotifyNonlinearVirtual)
    {
      meta_topic (META_DEBUG_FOCUS,
                  "Ignoring focus event generated by a grab or other weirdness\n");
      return;
    }

  if (event->evtype == XI_FocusIn)
    {
      display->server_focus_window = event->event;
      display->server_focus_serial = serial;
      focus_window = window;
    }
  else if (event->evtype == XI_FocusOut)
    {
      if (event->detail == XINotifyInferior)
        {
          /* This event means the client moved focus to a subwindow */
          meta_topic (META_DEBUG_FOCUS,
                      "Ignoring focus out with NotifyInferior\n");
          return;
        }

      display->server_focus_window = None;
      display->server_focus_serial = serial;
      focus_window = NULL;
    }
  else
    g_return_if_reached ();

  /* If display->focused_by_us, then the focus_serial will be used only
   * for a focus change we made and have already accounted for.
   * (See request_xserver_input_focus_change().) Otherwise, we can get
   * multiple focus events with the same serial.
   */
  if (display->server_focus_serial > display->focus_serial ||
      (!display->focused_by_us &&
       display->server_focus_serial == display->focus_serial))
    {
      meta_display_update_focus_window (display,
                                        focus_window,
                                        focus_window ? focus_window->xwindow : None,
                                        display->server_focus_serial,
                                        FALSE);
    }
}

static gboolean
crossing_serial_is_ignored (MetaDisplay  *display,
                            unsigned long serial)
{
  int i;

  i = 0;
  while (i < N_IGNORED_CROSSING_SERIALS)
    {
      if (display->ignored_crossing_serials[i] == serial)
        return TRUE;
      ++i;
    }
  return FALSE;
}

static gboolean
handle_input_xevent (MetaDisplay *display,
                     XIEvent     *input_event,
                     gulong       serial)
{
  XIEnterEvent *enter_event = (XIEnterEvent *) input_event;
  Window modified;
  MetaWindow *window;
  MetaScreen *screen = display->screen;

  if (input_event == NULL)
    return FALSE;

  switch (input_event->evtype)
    {
    case XI_Enter:
    case XI_Leave:
    case XI_FocusIn:
    case XI_FocusOut:
      break;
    default:
      return FALSE;
    }

  modified = xievent_get_modified_window (display, input_event);
  window = modified != None ? meta_display_lookup_x_window (display, modified) : NULL;

  /* If this is an event for a GTK+ widget, let GTK+ handle it. */
  if (meta_ui_window_is_widget (display->screen->ui, modified))
    return FALSE;

  switch (input_event->evtype)
    {
    case XI_Enter:
      if (display->grab_op == META_GRAB_OP_COMPOSITOR)
        break;

      /* Check if we've entered a window; do this even if window->has_focus to
       * avoid races.
       */
      if (window && !crossing_serial_is_ignored (display, serial) &&
          enter_event->mode != XINotifyGrab &&
          enter_event->mode != XINotifyUngrab &&
          enter_event->detail != XINotifyInferior &&
          meta_display_focus_sentinel_clear (display))
        {
          meta_window_handle_enter (window,
                                    enter_event->time,
                                    enter_event->root_x,
                                    enter_event->root_y);
        }
      break;
    case XI_Leave:
      if (display->grab_op == META_GRAB_OP_COMPOSITOR)
        break;

      if (window != NULL &&
          enter_event->mode != XINotifyGrab &&
          enter_event->mode != XINotifyUngrab)
        {
          meta_window_handle_leave (window);
        }
      break;
    case XI_FocusIn:
    case XI_FocusOut:
      handle_window_focus_event (display, window, enter_event, serial);
      if (!window)
        {
          /* Check if the window is a root window. */
          if (enter_event->root != enter_event->event)
            break;

          if (enter_event->evtype == XI_FocusIn &&
              enter_event->mode == XINotifyDetailNone)
            {
              meta_topic (META_DEBUG_FOCUS,
                          "Focus got set to None, probably due to "
                          "brain-damage in the X protocol (see bug "
                          "125492).  Setting the default focus window.\n");
              meta_workspace_focus_default_window (screen->active_workspace,
                                                   NULL,
                                                   meta_display_get_current_time_roundtrip (display));
            }
          else if (enter_event->evtype == XI_FocusIn &&
                   enter_event->mode == XINotifyNormal &&
                   enter_event->detail == XINotifyInferior)
            {
              meta_topic (META_DEBUG_FOCUS,
                          "Focus got set to root window, probably due to "
                          "gnome-session logout dialog usage (see bug "
                          "153220).  Setting the default focus window.\n");
              meta_workspace_focus_default_window (screen->active_workspace,
                                                   NULL,
                                                   meta_display_get_current_time_roundtrip (display));
            }

        }
      break;
    }

  /* Don't eat events for GTK frames (we need to update the :hover state on buttons) */
  if (window && window->frame && modified == window->frame->xwindow)
    return FALSE;

  /* Don't pass these events through to Clutter / GTK+ */
  return TRUE;
}

static void
reload_xkb_rules (MetaScreen  *screen)
{
  MetaWaylandCompositor *compositor;
  char **names;
  int n_names;
  gboolean ok;
  const char *rules, *model, *layout, *variant, *options;

  compositor = meta_wayland_compositor_get_default ();

  ok = meta_prop_get_latin1_list (screen->display, screen->xroot,
                                  screen->display->atom__XKB_RULES_NAMES,
                                  &names, &n_names);
  if (!ok)
    return;

  if (n_names != 5)
    goto out;

  rules = names[0];
  model = names[1];
  layout = names[2];
  variant = names[3];
  options = names[4];

  meta_wayland_keyboard_set_keymap_names (&compositor->seat->keyboard,
                                          rules, model, layout, variant, options,
                                          META_WAYLAND_KEYBOARD_SKIP_XCLIENTS);

 out:
  g_strfreev (names);
}

static void
process_request_frame_extents (MetaDisplay    *display,
                               XEvent         *event)
{
  /* The X window whose frame extents will be set. */
  Window xwindow = event->xclient.window;
  unsigned long data[4] = { 0, 0, 0, 0 };

  MotifWmHints *hints = NULL;
  gboolean hints_set = FALSE;

  meta_verbose ("Setting frame extents for 0x%lx\n", xwindow);

  /* See if the window is decorated. */
  hints_set = meta_prop_get_motif_hints (display,
                                         xwindow,
                                         display->atom__MOTIF_WM_HINTS,
                                         &hints);
  if ((hints_set && hints->decorations) || !hints_set)
    {
      MetaFrameBorders borders;

      /* Return estimated frame extents for a normal window. */
      meta_ui_theme_get_frame_borders (display->screen->ui,
                                       META_FRAME_TYPE_NORMAL,
                                       0,
                                       &borders);
      data[0] = borders.visible.left;
      data[1] = borders.visible.right;
      data[2] = borders.visible.top;
      data[3] = borders.visible.bottom;
    }

  meta_topic (META_DEBUG_GEOMETRY,
              "Setting _NET_FRAME_EXTENTS on unmanaged window 0x%lx "
              "to top = %lu, left = %lu, bottom = %lu, right = %lu\n",
              xwindow, data[0], data[1], data[2], data[3]);

  meta_error_trap_push (display);
  XChangeProperty (display->xdisplay, xwindow,
                   display->atom__NET_FRAME_EXTENTS,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 4);
  meta_error_trap_pop (display);

  meta_XFree (hints);
}

/* from fvwm2, Copyright Matthias Clasen, Dominik Vogt */
static gboolean
convert_property (MetaDisplay *display,
                  MetaScreen  *screen,
                  Window       w,
                  Atom         target,
                  Atom         property)
{
#define N_TARGETS 4
  Atom conversion_targets[N_TARGETS];
  long icccm_version[] = { 2, 0 };

  conversion_targets[0] = display->atom_TARGETS;
  conversion_targets[1] = display->atom_MULTIPLE;
  conversion_targets[2] = display->atom_TIMESTAMP;
  conversion_targets[3] = display->atom_VERSION;

  meta_error_trap_push (display);
  if (target == display->atom_TARGETS)
    XChangeProperty (display->xdisplay, w, property,
		     XA_ATOM, 32, PropModeReplace,
		     (unsigned char *)conversion_targets, N_TARGETS);
  else if (target == display->atom_TIMESTAMP)
    XChangeProperty (display->xdisplay, w, property,
		     XA_INTEGER, 32, PropModeReplace,
		     (unsigned char *)&screen->wm_sn_timestamp, 1);
  else if (target == display->atom_VERSION)
    XChangeProperty (display->xdisplay, w, property,
		     XA_INTEGER, 32, PropModeReplace,
		     (unsigned char *)icccm_version, 2);
  else
    {
      meta_error_trap_pop_with_return (display);
      return FALSE;
    }

  if (meta_error_trap_pop_with_return (display) != Success)
    return FALSE;

  /* Be sure the PropertyNotify has arrived so we
   * can send SelectionNotify
   */
  /* FIXME the error trap pop synced anyway, right? */
  meta_topic (META_DEBUG_SYNC, "Syncing on %s\n", G_STRFUNC);
  XSync (display->xdisplay, False);

  return TRUE;
}

/* from fvwm2, Copyright Matthias Clasen, Dominik Vogt */
static void
process_selection_request (MetaDisplay   *display,
                           XEvent        *event)
{
  MetaScreen *screen = display->screen;
  XSelectionEvent reply;

  if (screen->wm_sn_selection_window != event->xselectionrequest.owner ||
      screen->wm_sn_atom != event->xselectionrequest.selection)
    {
      char *str;

      meta_error_trap_push (display);
      str = XGetAtomName (display->xdisplay,
                          event->xselectionrequest.selection);
      meta_error_trap_pop (display);

      meta_verbose ("Selection request with selection %s window 0x%lx not a WM_Sn selection we recognize\n",
                    str ? str : "(bad atom)", event->xselectionrequest.owner);

      meta_XFree (str);

      return;
    }

  reply.type = SelectionNotify;
  reply.display = display->xdisplay;
  reply.requestor = event->xselectionrequest.requestor;
  reply.selection = event->xselectionrequest.selection;
  reply.target = event->xselectionrequest.target;
  reply.property = None;
  reply.time = event->xselectionrequest.time;

  if (event->xselectionrequest.target == display->atom_MULTIPLE)
    {
      if (event->xselectionrequest.property != None)
        {
          Atom type, *adata;
          int i, format;
          unsigned long num, rest;
          unsigned char *data;

          meta_error_trap_push (display);
          if (XGetWindowProperty (display->xdisplay,
                                  event->xselectionrequest.requestor,
                                  event->xselectionrequest.property, 0, 256, False,
                                  display->atom_ATOM_PAIR,
                                  &type, &format, &num, &rest, &data) != Success)
            {
              meta_error_trap_pop_with_return (display);
              return;
            }

          if (meta_error_trap_pop_with_return (display) == Success)
            {
              /* FIXME: to be 100% correct, should deal with rest > 0,
               * but since we have 4 possible targets, we will hardly ever
               * meet multiple requests with a length > 8
               */
              adata = (Atom*)data;
              i = 0;
              while (i < (int) num)
                {
                  if (!convert_property (display, screen,
                                         event->xselectionrequest.requestor,
                                         adata[i], adata[i+1]))
                    adata[i+1] = None;
                  i += 2;
                }

              meta_error_trap_push (display);
              XChangeProperty (display->xdisplay,
                               event->xselectionrequest.requestor,
                               event->xselectionrequest.property,
                               display->atom_ATOM_PAIR,
                               32, PropModeReplace, data, num);
              meta_error_trap_pop (display);
              meta_XFree (data);
            }
        }
    }
  else
    {
      if (event->xselectionrequest.property == None)
        event->xselectionrequest.property = event->xselectionrequest.target;

      if (convert_property (display, screen,
                            event->xselectionrequest.requestor,
                            event->xselectionrequest.target,
                            event->xselectionrequest.property))
        reply.property = event->xselectionrequest.property;
    }

  XSendEvent (display->xdisplay,
              event->xselectionrequest.requestor,
              False, 0L, (XEvent*)&reply);

  meta_verbose ("Handled selection request\n");
}

static void
process_selection_clear (MetaDisplay   *display,
                         XEvent        *event)
{
  MetaScreen *screen = display->screen;

  if (screen->wm_sn_selection_window != event->xselectionclear.window ||
      screen->wm_sn_atom != event->xselectionclear.selection)
    {
      char *str;

      meta_error_trap_push (display);
      str = XGetAtomName (display->xdisplay,
                          event->xselectionclear.selection);
      meta_error_trap_pop (display);

      meta_verbose ("Selection clear with selection %s window 0x%lx not a WM_Sn selection we recognize\n",
                    str ? str : "(bad atom)", event->xselectionclear.window);

      meta_XFree (str);

      return;
    }

  meta_verbose ("Got selection clear for screen %d on display %s\n",
                screen->number, display->name);

  meta_display_unmanage_screen (display, display->screen,
                                event->xselectionclear.time);
}

static gboolean
handle_other_xevent (MetaDisplay *display,
                     XEvent      *event)
{
  Window modified;
  MetaWindow *window;
  MetaWindow *property_for_window;
  gboolean frame_was_receiver;
  gboolean bypass_gtk = FALSE;

  modified = event_get_modified_window (display, event);
  window = modified != None ? meta_display_lookup_x_window (display, modified) : NULL;
  frame_was_receiver = (window && window->frame && modified == window->frame->xwindow);

  /* We only want to respond to _NET_WM_USER_TIME property notify
   * events on _NET_WM_USER_TIME_WINDOW windows; in particular,
   * responding to UnmapNotify events is kind of bad.
   */
  property_for_window = NULL;
  if (window && modified == window->user_time_window)
    {
      property_for_window = window;
      window = NULL;
    }

  if (META_DISPLAY_HAS_XSYNC (display) &&
      event->type == (display->xsync_event_base + XSyncAlarmNotify))
    {
      MetaWindow *alarm_window = meta_display_lookup_sync_alarm (display,
                                                                 ((XSyncAlarmNotifyEvent*)event)->alarm);

      if (alarm_window != NULL)
        {
          XSyncValue value = ((XSyncAlarmNotifyEvent*)event)->counter_value;
          gint64 new_counter_value;
          new_counter_value = XSyncValueLow32 (value) + ((gint64)XSyncValueHigh32 (value) << 32);
          meta_window_x11_update_sync_request_counter (alarm_window, new_counter_value);
          bypass_gtk = TRUE; /* GTK doesn't want to see this really */
        }

      goto out;
    }

  if (META_DISPLAY_HAS_SHAPE (display) &&
      event->type == (display->shape_event_base + ShapeNotify))
    {
      bypass_gtk = TRUE; /* GTK doesn't want to see this really */

      if (window && !frame_was_receiver)
        {
          XShapeEvent *sev = (XShapeEvent*) event;

          if (sev->kind == ShapeBounding)
            meta_window_x11_update_shape_region (window);
          else if (sev->kind == ShapeInput)
            meta_window_x11_update_input_region (window);
        }
      else
        {
          meta_topic (META_DEBUG_SHAPES,
                      "ShapeNotify not on a client window (window %s frame_was_receiver = %d)\n",
                      window ? window->desc : "(none)",
                      frame_was_receiver);
        }

      goto out;
    }

  switch (event->type)
    {
    case KeymapNotify:
      break;
    case Expose:
      break;
    case GraphicsExpose:
      break;
    case NoExpose:
      break;
    case VisibilityNotify:
      break;
    case CreateNotify:
      {
        if (event->xcreatewindow.parent == display->screen->xroot)
          meta_stack_tracker_create_event (display->screen->stack_tracker,
                                           &event->xcreatewindow);
      }
      break;

    case DestroyNotify:
      {
        if (event->xdestroywindow.event == display->screen->xroot)
          meta_stack_tracker_destroy_event (display->screen->stack_tracker,
                                            &event->xdestroywindow);
      }
      if (window)
        {
          /* FIXME: It sucks that DestroyNotify events don't come with
           * a timestamp; could we do something better here?  Maybe X
           * will change one day?
           */
          guint32 timestamp;
          timestamp = meta_display_get_current_time_roundtrip (display);

          if (display->grab_op != META_GRAB_OP_NONE &&
              display->grab_window == window)
            meta_display_end_grab_op (display, timestamp);

          if (frame_was_receiver)
            {
              meta_warning ("Unexpected destruction of frame 0x%lx, not sure if this should silently fail or be considered a bug\n",
                            window->frame->xwindow);
              meta_error_trap_push (display);
              meta_window_destroy_frame (window->frame->window);
              meta_error_trap_pop (display);
            }
          else
            {
              /* Unmanage destroyed window */
              meta_window_unmanage (window, timestamp);
              window = NULL;
            }
        }
      break;
    case UnmapNotify:
      if (window)
        {
          /* FIXME: It sucks that UnmapNotify events don't come with
           * a timestamp; could we do something better here?  Maybe X
           * will change one day?
           */
          guint32 timestamp;
          timestamp = meta_display_get_current_time_roundtrip (display);

          if (display->grab_op != META_GRAB_OP_NONE &&
              display->grab_window == window &&
              window->frame == NULL)
            meta_display_end_grab_op (display, timestamp);

          if (!frame_was_receiver)
            {
              if (window->unmaps_pending == 0)
                {
                  meta_topic (META_DEBUG_WINDOW_STATE,
                              "Window %s withdrawn\n",
                              window->desc);

                  /* Unmanage withdrawn window */
                  window->withdrawn = TRUE;
                  meta_window_unmanage (window, timestamp);
                  window = NULL;
                }
              else
                {
                  window->unmaps_pending -= 1;
                  meta_topic (META_DEBUG_WINDOW_STATE,
                              "Received pending unmap, %d now pending\n",
                              window->unmaps_pending);
                }
            }
        }
      break;
    case MapNotify:
      /* NB: override redirect windows wont cause a map request so we
       * watch out for map notifies against any root windows too if a
       * compositor is enabled: */
      if (window == NULL && event->xmap.event == display->screen->xroot)
        {
          window = meta_window_x11_new (display, event->xmap.window,
                                        FALSE, META_COMP_EFFECT_CREATE);
        }
      break;
    case MapRequest:
      if (window == NULL)
        {
          window = meta_window_x11_new (display, event->xmaprequest.window,
                                        FALSE, META_COMP_EFFECT_CREATE);
        }
      /* if frame was receiver it's some malicious send event or something */
      else if (!frame_was_receiver && window)
        {
          meta_verbose ("MapRequest on %s mapped = %d minimized = %d\n",
                        window->desc, window->mapped, window->minimized);
          if (window->minimized)
            {
              meta_window_unminimize (window);
              if (window->workspace != window->screen->active_workspace)
                {
                  meta_verbose ("Changing workspace due to MapRequest mapped = %d minimized = %d\n",
                                window->mapped, window->minimized);
                  meta_window_change_workspace (window,
                                                window->screen->active_workspace);
                }
            }
        }
      break;
    case ReparentNotify:
      {
        if (event->xreparent.event == display->screen->xroot)
          meta_stack_tracker_reparent_event (display->screen->stack_tracker,
                                             &event->xreparent);
      }
      break;
    case ConfigureNotify:
      if (event->xconfigure.event != event->xconfigure.window)
        {
          if (event->xconfigure.event == display->screen->xroot)
            meta_stack_tracker_configure_event (display->screen->stack_tracker,
                                                &event->xconfigure);
        }

      if (window && window->override_redirect)
        meta_window_x11_configure_notify (window, &event->xconfigure);

      break;
    case ConfigureRequest:
      /* This comment and code is found in both twm and fvwm */
      /*
       * According to the July 27, 1988 ICCCM draft, we should ignore size and
       * position fields in the WM_NORMAL_HINTS property when we map a window.
       * Instead, we'll read the current geometry.  Therefore, we should respond
       * to configuration requests for windows which have never been mapped.
       */
      if (window == NULL)
        {
          unsigned int xwcm;
          XWindowChanges xwc;

          xwcm = event->xconfigurerequest.value_mask &
            (CWX | CWY | CWWidth | CWHeight | CWBorderWidth);

          xwc.x = event->xconfigurerequest.x;
          xwc.y = event->xconfigurerequest.y;
          xwc.width = event->xconfigurerequest.width;
          xwc.height = event->xconfigurerequest.height;
          xwc.border_width = event->xconfigurerequest.border_width;

          meta_verbose ("Configuring withdrawn window to %d,%d %dx%d border %d (some values may not be in mask)\n",
                        xwc.x, xwc.y, xwc.width, xwc.height, xwc.border_width);
          meta_error_trap_push (display);
          XConfigureWindow (display->xdisplay, event->xconfigurerequest.window,
                            xwcm, &xwc);
          meta_error_trap_pop (display);
        }
      else
        {
          if (!frame_was_receiver)
            meta_window_x11_configure_request (window, event);
        }
      break;
    case GravityNotify:
      break;
    case ResizeRequest:
      break;
    case CirculateNotify:
      break;
    case CirculateRequest:
      break;
    case PropertyNotify:
      {
        MetaGroup *group;

        if (window && !frame_was_receiver)
          meta_window_x11_property_notify (window, event);
        else if (property_for_window && !frame_was_receiver)
          meta_window_x11_property_notify (property_for_window, event);

        group = meta_display_lookup_group (display,
                                           event->xproperty.window);
        if (group != NULL)
          meta_group_property_notify (group, event);

        if (event->xproperty.window == display->screen->xroot)
          {
            if (event->xproperty.atom ==
                display->atom__NET_DESKTOP_LAYOUT)
              meta_screen_update_workspace_layout (display->screen);
            else if (event->xproperty.atom ==
                     display->atom__NET_DESKTOP_NAMES)
              meta_screen_update_workspace_names (display->screen);
            else if (meta_is_wayland_compositor () &&
                     event->xproperty.atom ==
                     display->atom__XKB_RULES_NAMES)
              reload_xkb_rules (display->screen);

            /* we just use this property as a sentinel to avoid
             * certain race conditions.  See the comment for the
             * sentinel_counter variable declaration in display.h
             */
            if (event->xproperty.atom ==
                display->atom__MUTTER_SENTINEL)
              {
                meta_display_decrement_focus_sentinel (display);
              }
          }
      }
      break;
    case SelectionRequest:
      process_selection_request (display, event);
      break;
    case SelectionNotify:
      break;
    case ColormapNotify:
      break;
    case ClientMessage:
      if (window)
        {
          if (event->xclient.message_type == display->atom_WL_SURFACE_ID)
            {
              guint32 surface_id = event->xclient.data.l[0];
              meta_xwayland_handle_wl_surface_id (window, surface_id);
            }
          else if (!frame_was_receiver)
            meta_window_x11_client_message (window, event);
        }
      else
        {
          if (event->xclient.window == display->screen->xroot)
            {
              if (event->xclient.message_type ==
                  display->atom__NET_CURRENT_DESKTOP)
                {
                  int space;
                  MetaWorkspace *workspace;
                  guint32 time;

                  space = event->xclient.data.l[0];
                  time = event->xclient.data.l[1];

                  meta_verbose ("Request to change current workspace to %d with "
                                "specified timestamp of %u\n",
                                space, time);

                  workspace = meta_screen_get_workspace_by_index (display->screen, space);

                  /* Handle clients using the older version of the spec... */
                  if (time == 0 && workspace)
                    {
                      meta_warning ("Received a NET_CURRENT_DESKTOP message "
                                    "from a broken (outdated) client who sent "
                                    "a 0 timestamp\n");
                      time = meta_display_get_current_time_roundtrip (display);
                    }

                  if (workspace)
                    meta_workspace_activate (workspace, time);
                  else
                    meta_verbose ("Don't know about workspace %d\n", space);
                }
              else if (event->xclient.message_type ==
                       display->atom__NET_NUMBER_OF_DESKTOPS)
                {
                  int num_spaces;

                  num_spaces = event->xclient.data.l[0];

                  meta_verbose ("Request to set number of workspaces to %d\n",
                                num_spaces);

                  meta_prefs_set_num_workspaces (num_spaces);
                }
              else if (event->xclient.message_type ==
                       display->atom__NET_SHOWING_DESKTOP)
                {
                  gboolean showing_desktop;
                  guint32  timestamp;

                  showing_desktop = event->xclient.data.l[0] != 0;
                  /* FIXME: Braindead protocol doesn't have a timestamp */
                  timestamp = meta_display_get_current_time_roundtrip (display);
                  meta_verbose ("Request to %s desktop\n",
                                showing_desktop ? "show" : "hide");

                  if (showing_desktop)
                    meta_screen_show_desktop (display->screen, timestamp);
                  else
                    {
                      meta_screen_unshow_desktop (display->screen);
                      meta_workspace_focus_default_window (display->screen->active_workspace, NULL, timestamp);
                    }
                }
              else if (event->xclient.message_type ==
                       display->atom_WM_PROTOCOLS)
                {
                  meta_verbose ("Received WM_PROTOCOLS message\n");

                  if ((Atom)event->xclient.data.l[0] == display->atom__NET_WM_PING)
                    {
                      guint32 timestamp = event->xclient.data.l[1];

                      meta_display_pong_for_serial (display, timestamp);

                      /* We don't want ping reply events going into
                       * the GTK+ event loop because gtk+ will treat
                       * them as ping requests and send more replies.
                       */
                      bypass_gtk = TRUE;
                    }
                }
            }

          if (event->xclient.message_type ==
              display->atom__NET_REQUEST_FRAME_EXTENTS)
            {
              meta_verbose ("Received _NET_REQUEST_FRAME_EXTENTS message\n");
              process_request_frame_extents (display, event);
            }
        }
      break;
    case MappingNotify:
      {
        gboolean ignore_current;

        ignore_current = FALSE;

        /* Check whether the next event is an identical MappingNotify
         * event.  If it is, ignore the current event, we'll update
         * when we get the next one.
         */
        if (XPending (display->xdisplay))
          {
            XEvent next_event;

            XPeekEvent (display->xdisplay, &next_event);

            if (next_event.type == MappingNotify &&
                next_event.xmapping.request == event->xmapping.request)
              ignore_current = TRUE;
          }

        if (!ignore_current)
          {
            /* Let XLib know that there is a new keyboard mapping.
             */
            XRefreshKeyboardMapping (&event->xmapping);
            meta_display_process_mapping_event (display, event);
          }
      }
      break;
    default:
#ifdef HAVE_XKB
      if (event->type == display->xkb_base_event_type)
        {
          XkbAnyEvent *xkb_ev = (XkbAnyEvent *) event;

          switch (xkb_ev->xkb_type)
            {
            case XkbBellNotify:
              if (XSERVER_TIME_IS_BEFORE(display->last_bell_time,
                                         xkb_ev->time - 100))
                {
                  display->last_bell_time = xkb_ev->time;
                  meta_bell_notify (display, xkb_ev);
                }
              break;
            case XkbNewKeyboardNotify:
            case XkbMapNotify:
              if (xkb_ev->device == META_VIRTUAL_CORE_KEYBOARD_ID)
                meta_display_process_mapping_event (display, event);
              break;
            }
        }
#endif
      break;
    }

 out:
  return bypass_gtk;
}

static gboolean
window_has_xwindow (MetaWindow *window,
                    Window      xwindow)
{
  if (window->xwindow == xwindow)
    return TRUE;

  if (window->frame && window->frame->xwindow == xwindow)
    return TRUE;

  return FALSE;
}

/**
 * meta_display_handle_xevent:
 * @display: The MetaDisplay that events are coming from
 * @event: The event that just happened
 *
 * This is the most important function in the whole program. It is the heart,
 * it is the nexus, it is the Grand Central Station of Mutter's world.
 * When we create a #MetaDisplay, we ask GDK to pass *all* events for *all*
 * windows to this function. So every time anything happens that we might
 * want to know about, this function gets called. You see why it gets a bit
 * busy around here. Most of this function is a ginormous switch statement
 * dealing with all the kinds of events that might turn up.
 */
static gboolean
meta_display_handle_xevent (MetaDisplay *display,
                            XEvent      *event)
{
  Window modified;
  gboolean bypass_compositor = FALSE, bypass_gtk = FALSE;
  XIEvent *input_event;

#if 0
  meta_spew_event (display, event);
#endif

#ifdef HAVE_STARTUP_NOTIFICATION
  if (sn_display_process_event (display->sn_display, event))
    {
      bypass_gtk = bypass_compositor = TRUE;
      goto out;
    }
#endif

  display->current_time = event_get_time (display, event);
  display->monitor_cache_invalidated = TRUE;

  if (display->focused_by_us &&
      event->xany.serial > display->focus_serial &&
      display->focus_window &&
      !window_has_xwindow (display->focus_window, display->server_focus_window))
    {
      meta_topic (META_DEBUG_FOCUS, "Earlier attempt to focus %s failed\n",
                  display->focus_window->desc);
      meta_display_update_focus_window (display,
                                        meta_display_lookup_x_window (display, display->server_focus_window),
                                        display->server_focus_window,
                                        display->server_focus_serial,
                                        FALSE);
    }

  if (event->xany.window == display->screen->xroot)
    {
      if (meta_screen_handle_xevent (display->screen, event))
        {
          bypass_gtk = bypass_compositor = TRUE;
          goto out;
        }
    }

  modified = event_get_modified_window (display, event);

  input_event = get_input_event (display, event);

  if (event->type == UnmapNotify)
    {
      if (meta_ui_window_should_not_cause_focus (display->xdisplay,
                                                 modified))
        {
          meta_display_add_ignored_crossing_serial (display, event->xany.serial);
          meta_topic (META_DEBUG_FOCUS,
                      "Adding EnterNotify serial %lu to ignored focus serials\n",
                      event->xany.serial);
        }
    }
  else if (input_event &&
           input_event->evtype == XI_Leave &&
           ((XILeaveEvent *)input_event)->mode == XINotifyUngrab &&
           modified == display->ungrab_should_not_cause_focus_window)
    {
      meta_display_add_ignored_crossing_serial (display, event->xany.serial);
      meta_topic (META_DEBUG_FOCUS,
                  "Adding LeaveNotify serial %lu to ignored focus serials\n",
                  event->xany.serial);
    }

#ifdef HAVE_XI23
  if (meta_display_process_barrier_event (display, input_event))
    {
      bypass_gtk = bypass_compositor = TRUE;
      goto out;
    }
#endif /* HAVE_XI23 */

  /* libXi does not properly copy the serial to XI2 events, so pull it
   * from the parent XAnyEvent and pass it to handle_input_xevent.
   * See: https://bugs.freedesktop.org/show_bug.cgi?id=64687
   */
  if (handle_input_xevent (display, input_event, event->xany.serial))
    {
      bypass_gtk = bypass_compositor = TRUE;
      goto out;
    }

  if (handle_other_xevent (display, event))
    {
      bypass_gtk = TRUE;
      goto out;
    }

  if (event->type == SelectionClear)
    {
      /* Do this here so we can return without any further
       * processing. */
      process_selection_clear (display, event);
      /* Note that processing that may have resulted in
       * closing the display... */
      bypass_gtk = bypass_compositor = TRUE;
      goto out;
    }

 out:
  if (!bypass_compositor)
    {
      MetaWindow *window = modified != None ? meta_display_lookup_x_window (display, modified) : NULL;

      if (meta_compositor_process_event (display->compositor, event, window))
        bypass_gtk = TRUE;
    }

  display->current_time = CurrentTime;
  return bypass_gtk;
}

static void
handle_idletime_for_event (const ClutterEvent *event)
{
  /* This is handled by XSync under X11. */
  MetaBackend *backend = meta_get_backend ();

  if (META_IS_BACKEND_NATIVE (backend))
    {
      ClutterInputDevice *device, *source_device;
      MetaIdleMonitor *core_monitor, *device_monitor;
      int device_id;

      device = clutter_event_get_device (event);
      if (device == NULL)
        return;

      device_id = clutter_input_device_get_device_id (device);

      core_monitor = meta_idle_monitor_get_core ();
      device_monitor = meta_idle_monitor_get_for_device (device_id);

      meta_idle_monitor_native_reset_idletime (core_monitor);
      meta_idle_monitor_native_reset_idletime (device_monitor);

      source_device = clutter_event_get_source_device (event);
      if (source_device != device)
        {
          device_id = clutter_input_device_get_device_id (device);
          device_monitor = meta_idle_monitor_get_for_device (device_id);
          meta_idle_monitor_native_reset_idletime (device_monitor);
        }
    }
}

static gboolean
meta_display_handle_event (MetaDisplay        *display,
                           const ClutterEvent *event)
{
  MetaWindow *window;
  gboolean bypass_clutter = FALSE, bypass_wayland = FALSE;
  MetaWaylandCompositor *compositor = NULL;

  if (meta_is_wayland_compositor ())
    {
      compositor = meta_wayland_compositor_get_default ();
      meta_wayland_compositor_update (compositor, event);
    }

  handle_idletime_for_event (event);

  window = get_window_for_event (display, event);

  display->current_time = event->any.time;

  if (window && !window->override_redirect &&
      (event->type == CLUTTER_KEY_PRESS || event->type == CLUTTER_BUTTON_PRESS))
    {
      if (CurrentTime == display->current_time)
        {
          /* We can't use missing (i.e. invalid) timestamps to set user time,
           * nor do we want to use them to sanity check other timestamps.
           * See bug 313490 for more details.
           */
          meta_warning ("Event has no timestamp! You may be using a broken "
                        "program such as xse.  Please ask the authors of that "
                        "program to fix it.\n");
        }
      else
        {
          meta_window_set_user_time (window, display->current_time);
          meta_display_sanity_check_timestamps (display, display->current_time);
        }
    }

  if (display->grab_window == window &&
      meta_grab_op_is_moving_or_resizing (display->grab_op))
    {
      if (meta_window_handle_mouse_grab_op_event (window, event))
        {
          bypass_clutter = TRUE;
          bypass_wayland = TRUE;
          goto out;
        }
    }

  /* For key events, it's important to enforce single-handling, or
   * we can get into a confused state. So if a keybinding is
   * handled (because it's one of our hot-keys, or because we are
   * in a keyboard-grabbed mode like moving a window, we don't
   * want to pass the key event to the compositor or Wayland at all.
   */
  if (meta_keybindings_process_event (display, window, event))
    {
      bypass_clutter = TRUE;
      bypass_wayland = TRUE;
      goto out;
    }

  if (window)
    {
      /* Swallow all events on windows that come our way. */
      bypass_clutter = TRUE;

      /* Under X11, we have a Sync grab and in order to send it back to
       * clients, we have to explicitly replay it.
       *
       * Under Wayland, we retrieve all events and we have to make sure
       * to filter them out from Wayland clients.
       */
      if (meta_window_handle_ungrabbed_event (window, event))
        {
          bypass_wayland = TRUE;
        }
      else
        {
          MetaBackend *backend = meta_get_backend ();
          if (META_IS_BACKEND_X11 (backend))
            {
              Display *xdisplay = meta_backend_x11_get_xdisplay (META_BACKEND_X11 (backend));
              meta_verbose ("Allowing events time %u\n",
                            (unsigned int)event->button.time);
              XIAllowEvents (xdisplay, clutter_event_get_device_id (event),
                             XIReplayDevice, event->button.time);
            }
        }

      goto out;
    }

 out:
  /* If the compositor has a grab, don't pass that through to Wayland */
  if (display->grab_op == META_GRAB_OP_COMPOSITOR)
    bypass_wayland = TRUE;

  /* If a Wayland client has a grab, don't pass that through to Clutter */
  if (display->grab_op == META_GRAB_OP_WAYLAND_POPUP)
    bypass_clutter = TRUE;

  if (compositor && !bypass_wayland)
    {
      if (meta_wayland_compositor_handle_event (compositor, event))
        bypass_clutter = TRUE;
    }

  display->current_time = CurrentTime;
  return bypass_clutter;
}

static GdkFilterReturn
xevent_filter (GdkXEvent *xevent,
               GdkEvent  *event,
               gpointer   data)
{
  MetaDisplay *display = data;

  if (meta_display_handle_xevent (display, xevent))
    return GDK_FILTER_REMOVE;
  else
    return GDK_FILTER_CONTINUE;
}

static gboolean
event_callback (const ClutterEvent *event,
                gpointer            data)
{
  MetaDisplay *display = data;

  return meta_display_handle_event (display, event);
}

void
meta_display_init_events (MetaDisplay *display)
{
  gdk_window_add_filter (NULL, xevent_filter, display);
  display->clutter_event_filter = clutter_event_add_filter (NULL,
                                                            event_callback,
                                                            NULL,
                                                            display);
}

void
meta_display_free_events (MetaDisplay *display)
{
  gdk_window_remove_filter (NULL, xevent_filter, display);
  clutter_event_remove_filter (display->clutter_event_filter);
  display->clutter_event_filter = 0;
}
