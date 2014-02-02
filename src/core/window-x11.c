/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Havoc Pennington, Anders Carlsson
 * Copyright (C) 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2003 Rob Adams
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

#include "window-x11.h"

#include <string.h>
#include <X11/Xatom.h>
#include <X11/Xlibint.h> /* For display->resource_mask */

#ifdef HAVE_SHAPE
#include <X11/extensions/shape.h>
#endif

#include <X11/extensions/Xcomposite.h>
#include "core.h"

#include <meta/common.h>
#include <meta/errors.h>
#include <meta/prefs.h>

#include "window-private.h"
#include "window-props.h"
#include "xprops.h"

void
meta_window_x11_set_net_wm_state (MetaWindow *window)
{
  int i;
  unsigned long data[13];

  i = 0;
  if (window->shaded)
    {
      data[i] = window->display->atom__NET_WM_STATE_SHADED;
      ++i;
    }
  if (window->wm_state_modal)
    {
      data[i] = window->display->atom__NET_WM_STATE_MODAL;
      ++i;
    }
  if (window->skip_pager)
    {
      data[i] = window->display->atom__NET_WM_STATE_SKIP_PAGER;
      ++i;
    }
  if (window->skip_taskbar)
    {
      data[i] = window->display->atom__NET_WM_STATE_SKIP_TASKBAR;
      ++i;
    }
  if (window->maximized_horizontally)
    {
      data[i] = window->display->atom__NET_WM_STATE_MAXIMIZED_HORZ;
      ++i;
    }
  if (window->maximized_vertically)
    {
      data[i] = window->display->atom__NET_WM_STATE_MAXIMIZED_VERT;
      ++i;
    }
  if (window->fullscreen)
    {
      data[i] = window->display->atom__NET_WM_STATE_FULLSCREEN;
      ++i;
    }
  if (!meta_window_showing_on_its_workspace (window) || window->shaded)
    {
      data[i] = window->display->atom__NET_WM_STATE_HIDDEN;
      ++i;
    }
  if (window->wm_state_above)
    {
      data[i] = window->display->atom__NET_WM_STATE_ABOVE;
      ++i;
    }
  if (window->wm_state_below)
    {
      data[i] = window->display->atom__NET_WM_STATE_BELOW;
      ++i;
    }
  if (window->wm_state_demands_attention)
    {
      data[i] = window->display->atom__NET_WM_STATE_DEMANDS_ATTENTION;
      ++i;
    }
  if (window->on_all_workspaces_requested)
    {
      data[i] = window->display->atom__NET_WM_STATE_STICKY;
      ++i;
    }
  if (meta_window_appears_focused (window))
    {
      data[i] = window->display->atom__NET_WM_STATE_FOCUSED;
      ++i;
    }

  meta_verbose ("Setting _NET_WM_STATE with %d atoms\n", i);

  meta_error_trap_push (window->display);
  XChangeProperty (window->display->xdisplay, window->xwindow,
                   window->display->atom__NET_WM_STATE,
                   XA_ATOM,
                   32, PropModeReplace, (guchar*) data, i);
  meta_error_trap_pop (window->display);

  if (window->fullscreen)
    {
      data[0] = meta_screen_monitor_index_to_xinerama_index (window->screen,
                                                             window->fullscreen_monitors[0]);
      data[1] = meta_screen_monitor_index_to_xinerama_index (window->screen,
                                                             window->fullscreen_monitors[1]);
      data[2] = meta_screen_monitor_index_to_xinerama_index (window->screen,
                                                             window->fullscreen_monitors[2]);
      data[3] = meta_screen_monitor_index_to_xinerama_index (window->screen,
                                                             window->fullscreen_monitors[3]);

      meta_verbose ("Setting _NET_WM_FULLSCREEN_MONITORS\n");
      meta_error_trap_push (window->display);
      XChangeProperty (window->display->xdisplay,
                       window->xwindow,
                       window->display->atom__NET_WM_FULLSCREEN_MONITORS,
                       XA_CARDINAL, 32, PropModeReplace,
                       (guchar*) data, 4);
      meta_error_trap_pop (window->display);
    }
}

void
meta_window_x11_update_net_wm_type (MetaWindow *window)
{
  int n_atoms;
  Atom *atoms;
  int i;

  window->type_atom = None;
  n_atoms = 0;
  atoms = NULL;

  meta_prop_get_atom_list (window->display, window->xwindow,
                           window->display->atom__NET_WM_WINDOW_TYPE,
                           &atoms, &n_atoms);

  i = 0;
  while (i < n_atoms)
    {
      /* We break as soon as we find one we recognize,
       * supposed to prefer those near the front of the list
       */
      if (atoms[i] == window->display->atom__NET_WM_WINDOW_TYPE_DESKTOP ||
          atoms[i] == window->display->atom__NET_WM_WINDOW_TYPE_DOCK ||
          atoms[i] == window->display->atom__NET_WM_WINDOW_TYPE_TOOLBAR ||
          atoms[i] == window->display->atom__NET_WM_WINDOW_TYPE_MENU ||
          atoms[i] == window->display->atom__NET_WM_WINDOW_TYPE_UTILITY ||
          atoms[i] == window->display->atom__NET_WM_WINDOW_TYPE_SPLASH ||
          atoms[i] == window->display->atom__NET_WM_WINDOW_TYPE_DIALOG ||
          atoms[i] ==
	    window->display->atom__NET_WM_WINDOW_TYPE_DROPDOWN_MENU ||
          atoms[i] == window->display->atom__NET_WM_WINDOW_TYPE_POPUP_MENU ||
          atoms[i] == window->display->atom__NET_WM_WINDOW_TYPE_TOOLTIP ||
          atoms[i] ==
	    window->display->atom__NET_WM_WINDOW_TYPE_NOTIFICATION ||
          atoms[i] == window->display->atom__NET_WM_WINDOW_TYPE_COMBO ||
          atoms[i] == window->display->atom__NET_WM_WINDOW_TYPE_DND ||
          atoms[i] == window->display->atom__NET_WM_WINDOW_TYPE_NORMAL)
        {
          window->type_atom = atoms[i];
          break;
        }

      ++i;
    }

  meta_XFree (atoms);

  if (meta_is_verbose ())
    {
      char *str;

      str = NULL;
      if (window->type_atom != None)
        {
          meta_error_trap_push (window->display);
          str = XGetAtomName (window->display->xdisplay, window->type_atom);
          meta_error_trap_pop (window->display);
        }

      meta_verbose ("Window %s type atom %s\n", window->desc,
                    str ? str : "(none)");

      if (str)
        meta_XFree (str);
    }

  meta_window_recalc_window_type (window);
}

void
meta_window_x11_update_role (MetaWindow *window)
{
  char *str;

  g_return_if_fail (!window->override_redirect);

  if (window->role)
    g_free (window->role);
  window->role = NULL;

  if (meta_prop_get_latin1_string (window->display, window->xwindow,
                                   window->display->atom_WM_WINDOW_ROLE,
                                   &str))
    {
      window->role = g_strdup (str);
      meta_XFree (str);
    }

  meta_verbose ("Updated role of %s to '%s'\n",
                window->desc, window->role ? window->role : "null");
}

static void
meta_window_set_opaque_region (MetaWindow     *window,
                               cairo_region_t *region)
{
  g_clear_pointer (&window->opaque_region, cairo_region_destroy);

  if (region != NULL)
    window->opaque_region = cairo_region_reference (region);

  if (window->display->compositor)
    meta_compositor_window_shape_changed (window->display->compositor, window);
}

void
meta_window_x11_update_opaque_region (MetaWindow *window)
{
  cairo_region_t *opaque_region = NULL;
  gulong *region = NULL;
  int nitems;

  if (meta_prop_get_cardinal_list (window->display,
                                   window->xwindow,
                                   window->display->atom__NET_WM_OPAQUE_REGION,
                                   &region, &nitems))
    {
      cairo_rectangle_int_t *rects;
      int i, rect_index, nrects;

      if (nitems % 4 != 0)
        {
          meta_verbose ("_NET_WM_OPAQUE_REGION does not have a list of 4-tuples.");
          goto out;
        }

      /* empty region */
      if (nitems == 0)
        goto out;

      nrects = nitems / 4;

      rects = g_new (cairo_rectangle_int_t, nrects);

      rect_index = 0;
      i = 0;
      while (i < nitems)
        {
          cairo_rectangle_int_t *rect = &rects[rect_index];

          rect->x = region[i++];
          rect->y = region[i++];
          rect->width = region[i++];
          rect->height = region[i++];

          rect_index++;
        }

      opaque_region = cairo_region_create_rectangles (rects, nrects);

      g_free (rects);
    }

 out:
  meta_XFree (region);

  meta_window_set_opaque_region (window, opaque_region);
  cairo_region_destroy (opaque_region);
}

static cairo_region_t *
region_create_from_x_rectangles (const XRectangle *rects,
                                 int n_rects)
{
  int i;
  cairo_rectangle_int_t *cairo_rects = g_newa (cairo_rectangle_int_t, n_rects);

  for (i = 0; i < n_rects; i ++)
    {
      cairo_rects[i].x = rects[i].x;
      cairo_rects[i].y = rects[i].y;
      cairo_rects[i].width = rects[i].width;
      cairo_rects[i].height = rects[i].height;
    }

  return cairo_region_create_rectangles (cairo_rects, n_rects);
}

static void
meta_window_set_input_region (MetaWindow     *window,
                              cairo_region_t *region)
{
  g_clear_pointer (&window->input_region, cairo_region_destroy);

  if (region != NULL)
    window->input_region = cairo_region_reference (region);

  if (window->display->compositor)
    meta_compositor_window_shape_changed (window->display->compositor, window);
}

void
meta_window_x11_update_input_region (MetaWindow *window)
{
  cairo_region_t *region = NULL;

#ifdef HAVE_SHAPE
  if (META_DISPLAY_HAS_SHAPE (window->display))
    {
      /* Translate the set of XShape rectangles that we
       * get from the X server to a cairo_region. */
      XRectangle *rects = NULL;
      int n_rects, ordering;

      int x_bounding, y_bounding, x_clip, y_clip;
      unsigned w_bounding, h_bounding, w_clip, h_clip;
      int bounding_shaped, clip_shaped;

      meta_error_trap_push (window->display);
      XShapeQueryExtents (window->display->xdisplay, window->xwindow,
                          &bounding_shaped, &x_bounding, &y_bounding,
                          &w_bounding, &h_bounding,
                          &clip_shaped, &x_clip, &y_clip,
                          &w_clip, &h_clip);

      rects = XShapeGetRectangles (window->display->xdisplay,
                                   window->xwindow,
                                   ShapeInput,
                                   &n_rects,
                                   &ordering);
      meta_error_trap_pop (window->display);

      /* XXX: The x shape extension doesn't provide a way to only test if an
       * input shape has been specified, so we have to query and throw away the
       * rectangles. */
      if (rects)
        {
          if (n_rects > 1 ||
              (n_rects == 1 &&
               (rects[0].x != x_bounding ||
                rects[1].y != y_bounding ||
                rects[2].width != w_bounding ||
                rects[3].height != h_bounding)))
            region = region_create_from_x_rectangles (rects, n_rects);

          XFree (rects);
        }
    }
#endif /* HAVE_SHAPE */

  if (region != NULL)
    {
      cairo_rectangle_int_t client_area;

      client_area.x = 0;
      client_area.y = 0;
      client_area.width = window->rect.width;
      client_area.height = window->rect.height;

      /* The shape we get back from the client may have coordinates
       * outside of the frame. The X SHAPE Extension requires that
       * the overall shape the client provides never exceeds the
       * "bounding rectangle" of the window -- the shape that the
       * window would have gotten if it was unshaped. In our case,
       * this is simply the client area.
       */
      cairo_region_intersect_rectangle (region, &client_area);
    }

  meta_window_set_input_region (window, region);
  cairo_region_destroy (region);
}

static void
meta_window_set_shape_region (MetaWindow     *window,
                              cairo_region_t *region)
{
  g_clear_pointer (&window->shape_region, cairo_region_destroy);

  if (region != NULL)
    window->shape_region = cairo_region_reference (region);

  if (window->display->compositor)
    meta_compositor_window_shape_changed (window->display->compositor, window);
}

void
meta_window_x11_update_shape_region (MetaWindow *window)
{
  cairo_region_t *region = NULL;

#ifdef HAVE_SHAPE
  if (META_DISPLAY_HAS_SHAPE (window->display))
    {
      /* Translate the set of XShape rectangles that we
       * get from the X server to a cairo_region. */
      XRectangle *rects = NULL;
      int n_rects, ordering;

      int x_bounding, y_bounding, x_clip, y_clip;
      unsigned w_bounding, h_bounding, w_clip, h_clip;
      int bounding_shaped, clip_shaped;

      meta_error_trap_push (window->display);
      XShapeQueryExtents (window->display->xdisplay, window->xwindow,
                          &bounding_shaped, &x_bounding, &y_bounding,
                          &w_bounding, &h_bounding,
                          &clip_shaped, &x_clip, &y_clip,
                          &w_clip, &h_clip);

      if (bounding_shaped)
        {
          rects = XShapeGetRectangles (window->display->xdisplay,
                                       window->xwindow,
                                       ShapeBounding,
                                       &n_rects,
                                       &ordering);
        }
      meta_error_trap_pop (window->display);

      if (rects)
        {
          region = region_create_from_x_rectangles (rects, n_rects);
          XFree (rects);
        }
    }
#endif /* HAVE_SHAPE */

  if (region != NULL)
    {
      cairo_rectangle_int_t client_area;

      client_area.x = 0;
      client_area.y = 0;
      client_area.width = window->rect.width;
      client_area.height = window->rect.height;

      /* The shape we get back from the client may have coordinates
       * outside of the frame. The X SHAPE Extension requires that
       * the overall shape the client provides never exceeds the
       * "bounding rectangle" of the window -- the shape that the
       * window would have gotten if it was unshaped. In our case,
       * this is simply the client area.
       */
      cairo_region_intersect_rectangle (region, &client_area);
    }

  meta_window_set_shape_region (window, region);
  cairo_region_destroy (region);
}

/* Generally meta_window_same_application() is a better idea
 * of "sameness", since it handles the case where multiple apps
 * want to look like the same app or the same app wants to look
 * like multiple apps, but in the case of workarounds for legacy
 * applications (which likely aren't setting the group properly
 * anyways), it may be desirable to check this as well.
 */
static gboolean
meta_window_same_client (MetaWindow *window,
                         MetaWindow *other_window)
{
  int resource_mask = window->display->xdisplay->resource_mask;

  return ((window->xwindow & ~resource_mask) ==
          (other_window->xwindow & ~resource_mask));
}

gboolean
meta_window_x11_configure_request (MetaWindow *window,
                                   XEvent     *event)
{
  /* Note that x, y is the corner of the window border,
   * and width, height is the size of the window inside
   * its border, but that we always deny border requests
   * and give windows a border of 0. But we save the
   * requested border here.
   */
  if (event->xconfigurerequest.value_mask & CWBorderWidth)
    window->border_width = event->xconfigurerequest.border_width;

  meta_window_move_resize_request(window,
                                  event->xconfigurerequest.value_mask,
                                  window->size_hints.win_gravity,
                                  event->xconfigurerequest.x,
                                  event->xconfigurerequest.y,
                                  event->xconfigurerequest.width,
                                  event->xconfigurerequest.height);

  /* Handle stacking. We only handle raises/lowers, mostly because
   * stack.c really can't deal with anything else.  I guess we'll fix
   * that if a client turns up that really requires it. Only a very
   * few clients even require the raise/lower (and in fact all client
   * attempts to deal with stacking order are essentially broken,
   * since they have no idea what other clients are involved or how
   * the stack looks).
   *
   * I'm pretty sure no interesting client uses TopIf, BottomIf, or
   * Opposite anyway, so the only possible missing thing is
   * Above/Below with a sibling set. For now we just pretend there's
   * never a sibling set and always do the full raise/lower instead of
   * the raise-just-above/below-sibling.
   */
  if (event->xconfigurerequest.value_mask & CWStackMode)
    {
      MetaWindow *active_window;
      active_window = window->display->focus_window;
      if (meta_prefs_get_disable_workarounds ())
        {
          meta_topic (META_DEBUG_STACK,
                      "%s sent an xconfigure stacking request; this is "
                      "broken behavior and the request is being ignored.\n",
                      window->desc);
        }
      else if (active_window &&
               !meta_window_same_application (window, active_window) &&
               !meta_window_same_client (window, active_window) &&
               XSERVER_TIME_IS_BEFORE (window->net_wm_user_time,
                                       active_window->net_wm_user_time))
        {
          meta_topic (META_DEBUG_STACK,
                      "Ignoring xconfigure stacking request from %s (with "
                      "user_time %u); currently active application is %s (with "
                      "user_time %u).\n",
                      window->desc,
                      window->net_wm_user_time,
                      active_window->desc,
                      active_window->net_wm_user_time);
          if (event->xconfigurerequest.detail == Above)
            meta_window_set_demands_attention(window);
        }
      else
        {
          switch (event->xconfigurerequest.detail)
            {
            case Above:
              meta_window_raise (window);
              break;
            case Below:
              meta_window_lower (window);
              break;
            case TopIf:
            case BottomIf:
            case Opposite:
              break;
            }
        }
    }

  return TRUE;
}

static gboolean
process_property_notify (MetaWindow     *window,
                         XPropertyEvent *event)
{
  Window xid = window->xwindow;

  if (meta_is_verbose ()) /* avoid looking up the name if we don't have to */
    {
      char *property_name = XGetAtomName (window->display->xdisplay,
                                          event->atom);

      meta_verbose ("Property notify on %s for %s\n",
                    window->desc, property_name);
      XFree (property_name);
    }

  if (event->atom == window->display->atom__NET_WM_USER_TIME &&
      window->user_time_window)
    {
        xid = window->user_time_window;
    }

  meta_window_reload_property_from_xwindow (window, xid, event->atom, FALSE);

  return TRUE;
}

gboolean
meta_window_x11_property_notify (MetaWindow *window,
                                 XEvent     *event)
{
  return process_property_notify (window, &event->xproperty);
}

#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10
#define _NET_WM_MOVERESIZE_CANCEL           11

static int
query_pressed_buttons (MetaWindow *window)
{
  double x, y, query_root_x, query_root_y;
  Window root, child;
  XIButtonState buttons;
  XIModifierState mods;
  XIGroupState group;
  int button = 0;

  meta_error_trap_push (window->display);
  XIQueryPointer (window->display->xdisplay,
                  META_VIRTUAL_CORE_POINTER_ID,
                  window->xwindow,
                  &root, &child,
                  &query_root_x, &query_root_y,
                  &x, &y,
                  &buttons, &mods, &group);

  if (meta_error_trap_pop_with_return (window->display) != Success)
    goto out;

  if (XIMaskIsSet (buttons.mask, Button1))
    button |= 1 << 1;
  if (XIMaskIsSet (buttons.mask, Button2))
    button |= 1 << 2;
  if (XIMaskIsSet (buttons.mask, Button3))
    button |= 1 << 3;

  free (buttons.mask);

 out:
  return button;
}

gboolean
meta_window_x11_client_message (MetaWindow *window,
                                XEvent     *event)
{
  MetaDisplay *display;

  display = window->display;

  if (window->override_redirect)
    {
      /* Don't warn here: we could warn on any of the messages below,
       * but we might also receive other client messages that are
       * part of protocols we don't know anything about. So, silently
       * ignoring is simplest.
       */
      return FALSE;
    }

  if (event->xclient.message_type ==
      display->atom__NET_CLOSE_WINDOW)
    {
      guint32 timestamp;

      if (event->xclient.data.l[0] != 0)
	timestamp = event->xclient.data.l[0];
      else
        {
          meta_warning ("Receiving a NET_CLOSE_WINDOW message for %s without "
                        "a timestamp!  This means some buggy (outdated) "
                        "application is on the loose!\n",
                        window->desc);
          timestamp = meta_display_get_current_time (window->display);
        }

      meta_window_delete (window, timestamp);

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom__NET_WM_DESKTOP)
    {
      int space;
      MetaWorkspace *workspace;

      space = event->xclient.data.l[0];

      meta_verbose ("Request to move %s to workspace %d\n",
                    window->desc, space);

      workspace =
        meta_screen_get_workspace_by_index (window->screen,
                                            space);

      if (workspace)
        {
          if (window->on_all_workspaces_requested)
            meta_window_unstick (window);
          meta_window_change_workspace (window, workspace);
        }
      else if (space == (int) 0xFFFFFFFF)
        {
          meta_window_stick (window);
        }
      else
        {
          meta_verbose ("No such workspace %d for screen\n", space);
        }

      meta_verbose ("Window %s now on_all_workspaces = %d\n",
                    window->desc, window->on_all_workspaces);

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom__NET_WM_STATE)
    {
      gulong action;
      Atom first;
      Atom second;

      action = event->xclient.data.l[0];
      first = event->xclient.data.l[1];
      second = event->xclient.data.l[2];

      if (meta_is_verbose ())
        {
          char *str1;
          char *str2;

          meta_error_trap_push_with_return (display);
          str1 = XGetAtomName (display->xdisplay, first);
          if (meta_error_trap_pop_with_return (display) != Success)
            str1 = NULL;

          meta_error_trap_push_with_return (display);
          str2 = XGetAtomName (display->xdisplay, second);
          if (meta_error_trap_pop_with_return (display) != Success)
            str2 = NULL;

          meta_verbose ("Request to change _NET_WM_STATE action %lu atom1: %s atom2: %s\n",
                        action,
                        str1 ? str1 : "(unknown)",
                        str2 ? str2 : "(unknown)");

          meta_XFree (str1);
          meta_XFree (str2);
        }

      if (first == display->atom__NET_WM_STATE_SHADED ||
          second == display->atom__NET_WM_STATE_SHADED)
        {
          gboolean shade;
          guint32 timestamp;

          /* Stupid protocol has no timestamp; of course, shading
           * sucks anyway so who really cares that we're forced to do
           * a roundtrip here?
           */
          timestamp = meta_display_get_current_time_roundtrip (window->display);

          shade = (action == _NET_WM_STATE_ADD ||
                   (action == _NET_WM_STATE_TOGGLE && !window->shaded));
          if (shade && window->has_shade_func)
            meta_window_shade (window, timestamp);
          else
            meta_window_unshade (window, timestamp);
        }

      if (first == display->atom__NET_WM_STATE_FULLSCREEN ||
          second == display->atom__NET_WM_STATE_FULLSCREEN)
        {
          gboolean make_fullscreen;

          make_fullscreen = (action == _NET_WM_STATE_ADD ||
                             (action == _NET_WM_STATE_TOGGLE && !window->fullscreen));
          if (make_fullscreen && window->has_fullscreen_func)
            meta_window_make_fullscreen (window);
          else
            meta_window_unmake_fullscreen (window);
        }

      if (first == display->atom__NET_WM_STATE_MAXIMIZED_HORZ ||
          second == display->atom__NET_WM_STATE_MAXIMIZED_HORZ ||
          first == display->atom__NET_WM_STATE_MAXIMIZED_VERT ||
          second == display->atom__NET_WM_STATE_MAXIMIZED_VERT)
        {
          gboolean max;
          MetaMaximizeFlags directions = 0;

          max = (action == _NET_WM_STATE_ADD ||
                 (action == _NET_WM_STATE_TOGGLE &&
                  !window->maximized_horizontally));

          if (first == display->atom__NET_WM_STATE_MAXIMIZED_HORZ ||
              second == display->atom__NET_WM_STATE_MAXIMIZED_HORZ)
            directions |= META_MAXIMIZE_HORIZONTAL;

          if (first == display->atom__NET_WM_STATE_MAXIMIZED_VERT ||
              second == display->atom__NET_WM_STATE_MAXIMIZED_VERT)
            directions |= META_MAXIMIZE_VERTICAL;

          if (max && window->has_maximize_func)
            {
              if (meta_prefs_get_raise_on_click ())
                meta_window_raise (window);
              meta_window_maximize (window, directions);
            }
          else
            {
              if (meta_prefs_get_raise_on_click ())
                meta_window_raise (window);
              meta_window_unmaximize (window, directions);
            }
        }

      if (first == display->atom__NET_WM_STATE_MODAL ||
          second == display->atom__NET_WM_STATE_MODAL)
        {
          window->wm_state_modal =
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->wm_state_modal);

          meta_window_recalc_window_type (window);
          meta_window_queue(window, META_QUEUE_MOVE_RESIZE);
        }

      if (first == display->atom__NET_WM_STATE_SKIP_PAGER ||
          second == display->atom__NET_WM_STATE_SKIP_PAGER)
        {
          window->wm_state_skip_pager =
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->skip_pager);

          meta_window_recalc_features (window);
          meta_window_x11_set_net_wm_state (window);
        }

      if (first == display->atom__NET_WM_STATE_SKIP_TASKBAR ||
          second == display->atom__NET_WM_STATE_SKIP_TASKBAR)
        {
          window->wm_state_skip_taskbar =
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->skip_taskbar);

          meta_window_recalc_features (window);
          meta_window_x11_set_net_wm_state (window);
        }

      if (first == display->atom__NET_WM_STATE_ABOVE ||
          second == display->atom__NET_WM_STATE_ABOVE)
        {
          if ((action == _NET_WM_STATE_ADD) ||
              (action == _NET_WM_STATE_TOGGLE && !window->wm_state_demands_attention))
            meta_window_make_above (window);
          else
            meta_window_unmake_above (window);
        }

      if (first == display->atom__NET_WM_STATE_BELOW ||
          second == display->atom__NET_WM_STATE_BELOW)
        {
          window->wm_state_below =
            (action == _NET_WM_STATE_ADD) ||
            (action == _NET_WM_STATE_TOGGLE && !window->wm_state_below);

          meta_window_update_layer (window);
          meta_window_x11_set_net_wm_state (window);
        }

      if (first == display->atom__NET_WM_STATE_DEMANDS_ATTENTION ||
          second == display->atom__NET_WM_STATE_DEMANDS_ATTENTION)
        {
          if ((action == _NET_WM_STATE_ADD) ||
              (action == _NET_WM_STATE_TOGGLE && !window->wm_state_demands_attention))
            meta_window_set_demands_attention (window);
          else
            meta_window_unset_demands_attention (window);
        }

       if (first == display->atom__NET_WM_STATE_STICKY ||
          second == display->atom__NET_WM_STATE_STICKY)
        {
          if ((action == _NET_WM_STATE_ADD) ||
              (action == _NET_WM_STATE_TOGGLE && !window->on_all_workspaces_requested))
            meta_window_stick (window);
          else
            meta_window_unstick (window);
        }

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom_WM_CHANGE_STATE)
    {
      meta_verbose ("WM_CHANGE_STATE client message, state: %ld\n",
                    event->xclient.data.l[0]);
      if (event->xclient.data.l[0] == IconicState &&
          window->has_minimize_func)
        meta_window_minimize (window);

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom__NET_WM_MOVERESIZE)
    {
      int x_root;
      int y_root;
      int action;
      MetaGrabOp op;
      int button;
      guint32 timestamp;

      /* _NET_WM_MOVERESIZE messages are almost certainly going to come from
       * clients when users click on the fake "frame" that the client has,
       * thus we should also treat such messages as though it were a
       * "frame action".
       */
      gboolean const frame_action = TRUE;

      x_root = event->xclient.data.l[0];
      y_root = event->xclient.data.l[1];
      action = event->xclient.data.l[2];
      button = event->xclient.data.l[3];

      /* FIXME: What a braindead protocol; no timestamp?!? */
      timestamp = meta_display_get_current_time_roundtrip (display);
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Received _NET_WM_MOVERESIZE message on %s, %d,%d action = %d, button %d\n",
                  window->desc,
                  x_root, y_root, action, button);

      op = META_GRAB_OP_NONE;
      switch (action)
        {
        case _NET_WM_MOVERESIZE_SIZE_TOPLEFT:
          op = META_GRAB_OP_RESIZING_NW;
          break;
        case _NET_WM_MOVERESIZE_SIZE_TOP:
          op = META_GRAB_OP_RESIZING_N;
          break;
        case _NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
          op = META_GRAB_OP_RESIZING_NE;
          break;
        case _NET_WM_MOVERESIZE_SIZE_RIGHT:
          op = META_GRAB_OP_RESIZING_E;
          break;
        case _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
          op = META_GRAB_OP_RESIZING_SE;
          break;
        case _NET_WM_MOVERESIZE_SIZE_BOTTOM:
          op = META_GRAB_OP_RESIZING_S;
          break;
        case _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
          op = META_GRAB_OP_RESIZING_SW;
          break;
        case _NET_WM_MOVERESIZE_SIZE_LEFT:
          op = META_GRAB_OP_RESIZING_W;
          break;
        case _NET_WM_MOVERESIZE_MOVE:
          op = META_GRAB_OP_MOVING;
          break;
        case _NET_WM_MOVERESIZE_SIZE_KEYBOARD:
          op = META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN;
          break;
        case _NET_WM_MOVERESIZE_MOVE_KEYBOARD:
          op = META_GRAB_OP_KEYBOARD_MOVING;
          break;
        case _NET_WM_MOVERESIZE_CANCEL:
          /* handled below */
          break;
        default:
          break;
        }

      if (action == _NET_WM_MOVERESIZE_CANCEL)
        {
          meta_display_end_grab_op (window->display, timestamp);
        }
      else if (op != META_GRAB_OP_NONE &&
          ((window->has_move_func && op == META_GRAB_OP_KEYBOARD_MOVING) ||
           (window->has_resize_func && op == META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN)))
        {
          meta_window_begin_grab_op (window, op, frame_action, timestamp);
        }
      else if (op != META_GRAB_OP_NONE &&
               ((window->has_move_func && op == META_GRAB_OP_MOVING) ||
               (window->has_resize_func &&
                (op != META_GRAB_OP_MOVING &&
                 op != META_GRAB_OP_KEYBOARD_MOVING))))
        {
          int button_mask;

          meta_topic (META_DEBUG_WINDOW_OPS,
                      "Beginning move/resize with button = %d\n", button);
          meta_display_begin_grab_op (window->display,
                                      window->screen,
                                      window,
                                      op,
                                      FALSE,
                                      frame_action,
                                      button, 0,
                                      timestamp,
                                      x_root,
                                      y_root);

          button_mask = query_pressed_buttons (window);

          if (button == 0)
            {
              /*
               * the button SHOULD already be included in the message
               */
              if ((button_mask & (1 << 1)) != 0)
                button = 1;
              else if ((button_mask & (1 << 2)) != 0)
                button = 2;
              else if ((button_mask & (1 << 3)) != 0)
                button = 3;

              if (button != 0)
                window->display->grab_button = button;
              else
                meta_display_end_grab_op (window->display,
                                          timestamp);
            }
          else
            {
              /* There is a potential race here. If the user presses and
               * releases their mouse button very fast, it's possible for
               * both the ButtonPress and ButtonRelease to be sent to the
               * client before it can get a chance to send _NET_WM_MOVERESIZE
               * to us. When that happens, we'll become stuck in a grab
               * state, as we haven't received a ButtonRelease to cancel the
               * grab.
               *
               * We can solve this by querying after we take the explicit
               * pointer grab -- if the button isn't pressed, we cancel the
               * drag immediately.
               */

              if ((button_mask & (1 << button)) == 0)
                meta_display_end_grab_op (window->display, timestamp);
            }
        }

      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom__NET_MOVERESIZE_WINDOW)
    {
      int gravity;
      guint value_mask;

      gravity = (event->xclient.data.l[0] & 0xff);
      value_mask = (event->xclient.data.l[0] & 0xf00) >> 8;
      /* source = (event->xclient.data.l[0] & 0xf000) >> 12; */

      if (gravity == 0)
        gravity = window->size_hints.win_gravity;

      meta_window_move_resize_request(window,
                                      value_mask,
                                      gravity,
                                      event->xclient.data.l[1],  /* x */
                                      event->xclient.data.l[2],  /* y */
                                      event->xclient.data.l[3],  /* width */
                                      event->xclient.data.l[4]); /* height */
    }
  else if (event->xclient.message_type ==
           display->atom__NET_ACTIVE_WINDOW)
    {
      MetaClientType source_indication;
      guint32        timestamp;

      meta_verbose ("_NET_ACTIVE_WINDOW request for window '%s', activating\n",
                    window->desc);

      source_indication = event->xclient.data.l[0];
      timestamp = event->xclient.data.l[1];

      if (source_indication > META_CLIENT_TYPE_MAX_RECOGNIZED)
        source_indication = META_CLIENT_TYPE_UNKNOWN;

      if (timestamp == 0)
        {
          /* Client using older EWMH _NET_ACTIVE_WINDOW without a timestamp */
          meta_warning ("Buggy client sent a _NET_ACTIVE_WINDOW message with a "
                        "timestamp of 0 for %s\n",
                        window->desc);
          timestamp = meta_display_get_current_time (display);
        }

      meta_window_activate_full (window, timestamp, source_indication, NULL);
      return TRUE;
    }
  else if (event->xclient.message_type ==
           display->atom__NET_WM_FULLSCREEN_MONITORS)
    {
      gulong top, bottom, left, right;

      meta_verbose ("_NET_WM_FULLSCREEN_MONITORS request for window '%s'\n",
                    window->desc);

      top = meta_screen_xinerama_index_to_monitor_index (window->screen,
                                                         event->xclient.data.l[0]);
      bottom = meta_screen_xinerama_index_to_monitor_index (window->screen,
                                                            event->xclient.data.l[1]);
      left = meta_screen_xinerama_index_to_monitor_index (window->screen,
                                                          event->xclient.data.l[2]);
      right = meta_screen_xinerama_index_to_monitor_index (window->screen,
                                                           event->xclient.data.l[3]);
      /* source_indication = event->xclient.data.l[4]; */

      meta_window_update_fullscreen_monitors (window, top, bottom, left, right);
    }

  return FALSE;
}

static void
set_wm_state_on_xwindow (MetaDisplay *display,
                         Window       xwindow,
                         int          state)
{
  unsigned long data[2];

  /* Mutter doesn't use icon windows, so data[1] should be None
   * according to the ICCCM 2.0 Section 4.1.3.1.
   */
  data[0] = state;
  data[1] = None;

  meta_error_trap_push (display);
  XChangeProperty (display->xdisplay, xwindow,
                   display->atom_WM_STATE,
                   display->atom_WM_STATE,
                   32, PropModeReplace, (guchar*) data, 2);
  meta_error_trap_pop (display);
}

void
meta_window_x11_set_wm_state (MetaWindow *window)
{
  int state;

  if (window->withdrawn)
    state = WithdrawnState;
  else if (window->iconic)
    state = IconicState;
  else
    state = NormalState;

  set_wm_state_on_xwindow (window->display, window->xwindow, state);
}

/* The MUTTER_WM_CLASS_FILTER environment variable is designed for
 * performance and regression testing environments where we want to do
 * tests with only a limited set of windows and ignore all other windows
 *
 * When it is set to a comma separated list of WM_CLASS class names, all
 * windows not matching the list will be ignored.
 *
 * Returns TRUE if window has been filtered out and should be ignored.
 */
static gboolean
maybe_filter_xwindow (MetaDisplay       *display,
                      Window             xwindow,
                      gboolean           must_be_viewable,
                      XWindowAttributes *attrs)
{
  static char **filter_wm_classes = NULL;
  static gboolean initialized = FALSE;
  XClassHint class_hint;
  gboolean filtered;
  Status success;
  int i;

  if (!initialized)
    {
      const char *filter_string = g_getenv ("MUTTER_WM_CLASS_FILTER");
      if (filter_string)
        filter_wm_classes = g_strsplit (filter_string, ",", -1);
      initialized = TRUE;
    }

  if (!filter_wm_classes || !filter_wm_classes[0])
    return FALSE;

  filtered = TRUE;

  meta_error_trap_push (display);
  success = XGetClassHint (display->xdisplay, xwindow, &class_hint);

  if (success)
    {
      for (i = 0; filter_wm_classes[i]; i++)
        {
          if (strcmp (class_hint.res_class, filter_wm_classes[i]) == 0)
            {
              filtered = FALSE;
              break;
            }
        }

      XFree (class_hint.res_name);
      XFree (class_hint.res_class);
    }

  if (filtered)
    {
      /* We want to try and get the window managed by the next WM that come along,
       * so we need to make sure that windows that are requested to be mapped while
       * Mutter is running (!must_be_viewable), or windows already viewable at startup
       * get a non-withdrawn WM_STATE property. Previously unmapped windows are left
       * with whatever WM_STATE property they had.
       */
      if (!must_be_viewable || attrs->map_state == IsViewable)
        {
          gulong old_state;

          if (!meta_prop_get_cardinal_with_atom_type (display, xwindow,
                                                      display->atom_WM_STATE,
                                                      display->atom_WM_STATE,
                                                      &old_state))
            old_state = WithdrawnState;

          if (old_state == WithdrawnState)
            set_wm_state_on_xwindow (display, xwindow, NormalState);
        }

      /* Make sure filtered windows are hidden from view */
      XUnmapWindow (display->xdisplay, xwindow);
    }

  meta_error_trap_pop (display);

  return filtered;
}

static gboolean
is_our_xwindow (MetaDisplay       *display,
                MetaScreen        *screen,
                Window             xwindow,
                XWindowAttributes *attrs)
{
  if (xwindow == screen->no_focus_window)
    return TRUE;

  if (xwindow == screen->flash_window)
    return TRUE;

  if (xwindow == screen->wm_sn_selection_window)
    return TRUE;

  if (xwindow == screen->wm_cm_selection_window)
    return TRUE;

  if (xwindow == screen->guard_window)
    return TRUE;

  if (display->compositor && xwindow == XCompositeGetOverlayWindow (display->xdisplay, screen->xroot))
    return TRUE;

  /* Any windows created via meta_create_offscreen_window */
  if (attrs->override_redirect && attrs->x == -100 && attrs->height == -100 && attrs->width == 1 && attrs->height == 1)
    return TRUE;

  return FALSE;
}

#ifdef WITH_VERBOSE_MODE
static const char*
wm_state_to_string (int state)
{
  switch (state)
    {
    case NormalState:
      return "NormalState";
    case IconicState:
      return "IconicState";
    case WithdrawnState:
      return "WithdrawnState";
    }

  return "Unknown";
}
#endif

MetaWindow *
meta_window_x11_new (MetaDisplay       *display,
                     Window             xwindow,
                     gboolean           must_be_viewable,
                     MetaCompEffect     effect)
{
  XWindowAttributes attrs;
  MetaScreen *screen = NULL;
  GSList *tmp;
  gulong existing_wm_state;
  MetaWindow *window = NULL;
  gulong event_mask;

  meta_verbose ("Attempting to manage 0x%lx\n", xwindow);

  if (meta_display_xwindow_is_a_no_focus_window (display, xwindow))
    {
      meta_verbose ("Not managing no_focus_window 0x%lx\n",
                    xwindow);
      return NULL;
    }

  meta_error_trap_push (display); /* Push a trap over all of window
                                   * creation, to reduce XSync() calls
                                   */
  /*
   * This function executes without any server grabs held. This means that
   * the window could have already gone away, or could go away at any point,
   * so we must be careful with X error handling.
   */

  if (!XGetWindowAttributes (display->xdisplay, xwindow, &attrs))
    {
      meta_verbose ("Failed to get attributes for window 0x%lx\n",
                    xwindow);
      goto error;
    }

  for (tmp = display->screens; tmp != NULL; tmp = tmp->next)
    {
      MetaScreen *scr = tmp->data;

      if (scr->xroot == attrs.root)
        {
          screen = tmp->data;
          break;
        }
    }

  g_assert (screen);

  if (is_our_xwindow (display, screen, xwindow, &attrs))
    {
      meta_verbose ("Not managing our own windows\n");
      goto error;
    }

  if (maybe_filter_xwindow (display, xwindow, must_be_viewable, &attrs))
    {
      meta_verbose ("Not managing filtered window\n");
      goto error;
    }

  existing_wm_state = WithdrawnState;
  if (must_be_viewable && attrs.map_state != IsViewable)
    {
      /* Only manage if WM_STATE is IconicState or NormalState */
      gulong state;

      /* WM_STATE isn't a cardinal, it's type WM_STATE, but is an int */
      if (!(meta_prop_get_cardinal_with_atom_type (display, xwindow,
                                                   display->atom_WM_STATE,
                                                   display->atom_WM_STATE,
                                                   &state) &&
            (state == IconicState || state == NormalState)))
        {
          meta_verbose ("Deciding not to manage unmapped or unviewable window 0x%lx\n", xwindow);
          goto error;
        }

      existing_wm_state = state;
      meta_verbose ("WM_STATE of %lx = %s\n", xwindow,
                    wm_state_to_string (existing_wm_state));
    }

  meta_error_trap_push_with_return (display);

  /*
   * XAddToSaveSet can only be called on windows created by a different
   * client.  with Mutter we want to be able to create manageable windows
   * from within the process (such as a dummy desktop window). As we do not
   * want this call failing to prevent the window from being managed, we
   * call this before creating the return-checked error trap.
   */
  XAddToSaveSet (display->xdisplay, xwindow);

  meta_error_trap_push_with_return (display);

  event_mask = PropertyChangeMask | ColormapChangeMask;
  if (attrs.override_redirect)
    event_mask |= StructureNotifyMask;

  /* If the window is from this client (a menu, say) we need to augment
   * the event mask, not replace it. For windows from other clients,
   * attrs.your_event_mask will be empty at this point.
   */
  XSelectInput (display->xdisplay, xwindow, attrs.your_event_mask | event_mask);

  {
    unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
    XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };

    meta_core_add_old_event_mask (display->xdisplay, xwindow, &mask);

    XISetMask (mask.mask, XI_Enter);
    XISetMask (mask.mask, XI_Leave);
    XISetMask (mask.mask, XI_FocusIn);
    XISetMask (mask.mask, XI_FocusOut);

    XISelectEvents (display->xdisplay, xwindow, &mask, 1);
  }

  /* Get rid of any borders */
  if (attrs.border_width != 0)
    XSetWindowBorderWidth (display->xdisplay, xwindow, 0);

  /* Get rid of weird gravities */
  if (attrs.win_gravity != NorthWestGravity)
    {
      XSetWindowAttributes set_attrs;

      set_attrs.win_gravity = NorthWestGravity;

      XChangeWindowAttributes (display->xdisplay,
                               xwindow,
                               CWWinGravity,
                               &set_attrs);
    }

  if (meta_error_trap_pop_with_return (display) != Success)
    {
      meta_verbose ("Window 0x%lx disappeared just as we tried to manage it\n",
                    xwindow);
      goto error;
    }

  window = _meta_window_shared_new (display,
                                    screen,
                                    META_WINDOW_CLIENT_TYPE_X11,
                                    NULL,
                                    xwindow,
                                    existing_wm_state,
                                    effect,
                                    &attrs);

  /* When running as an X compositor, we can simply show the window now.
   *
   * When running as a Wayland compositor, we need to wait until we see
   * the Wayland surface appear. We will later call meta_window_set_surface_mapped()
   * to show the window in our in our set_surface_id implementation */
  if (!meta_is_wayland_compositor ())
    meta_window_set_surface_mapped (window, TRUE);

  meta_error_trap_pop (display); /* pop the XSync()-reducing trap */
  return window;

error:
  meta_error_trap_pop (display);
  return NULL;
}
