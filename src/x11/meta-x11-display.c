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

/**
 * SECTION:display
 * @title: MetaX11Display
 * @short_description: Mutter X display handler
 *
 * The X11 display is represented as a #MetaX11Display struct.
 */

#include "config.h"

#include "core/display-private.h"
#include "x11/meta-x11-display-private.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xatom.h>
#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif
#include <X11/extensions/shape.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xinerama.h>

#include "backends/meta-backend-private.h"
#include "backends/meta-logical-monitor.h"
#include "backends/x11/meta-backend-x11.h"
#include "core/frame.h"
#include "core/util-private.h"
#include "meta/errors.h"
#include "meta/main.h"

#include "x11/events.h"
#include "x11/group-props.h"
#include "x11/window-props.h"
#include "x11/xprops.h"

#ifdef HAVE_WAYLAND
#include "wayland/meta-xwayland-private.h"
#endif

G_DEFINE_TYPE (MetaX11Display, meta_x11_display, G_TYPE_OBJECT)

static GQuark quark_x11_display_logical_monitor_data = 0;

typedef struct _MetaX11DisplayLogicalMonitorData
{
  int xinerama_index;
} MetaX11DisplayLogicalMonitorData;

static const char *gnome_wm_keybindings = "Mutter";
static const char *net_wm_name = "Mutter";

static char *get_screen_name (Display *xdisplay,
                              int      number);

static void on_monitors_changed (MetaDisplay    *display,
                                 MetaX11Display *x11_display);

static void update_cursor_theme (MetaX11Display *x11_display);
static void unset_wm_check_hint (MetaX11Display *x11_display);

static void
meta_x11_display_dispose (GObject *object)
{
  MetaX11Display *x11_display = META_X11_DISPLAY (object);

  meta_x11_display_ungrab_keys (x11_display);

  if (x11_display->ui)
    {
      meta_ui_free (x11_display->ui);
      x11_display->ui = NULL;
    }

  if (x11_display->no_focus_window != None)
    {
      XUnmapWindow (x11_display->xdisplay, x11_display->no_focus_window);
      XDestroyWindow (x11_display->xdisplay, x11_display->no_focus_window);

      x11_display->no_focus_window = None;
    }

  if (x11_display->composite_overlay_window != None)
    {
      XCompositeReleaseOverlayWindow (x11_display->xdisplay,
                                      x11_display->composite_overlay_window);

      x11_display->composite_overlay_window = None;
    }

  if (x11_display->wm_sn_selection_window != None)
    {
      XDestroyWindow (x11_display->xdisplay, x11_display->wm_sn_selection_window);
      x11_display->wm_sn_selection_window = None;
    }

  if (x11_display->timestamp_pinging_window != None)
    {
      XDestroyWindow (x11_display->xdisplay, x11_display->timestamp_pinging_window);
      x11_display->timestamp_pinging_window = None;
    }

  if (x11_display->leader_window != None)
    {
      XDestroyWindow (x11_display->xdisplay, x11_display->leader_window);
      x11_display->leader_window = None;
    }

  if (x11_display->guard_window != None)
    {
      MetaStackTracker *stack_tracker = x11_display->display->stack_tracker;

      if (stack_tracker)
        {
          unsigned long serial;

          serial = XNextRequest (x11_display->xdisplay);
          meta_stack_tracker_record_remove (stack_tracker,
                                            x11_display->guard_window,
                                            serial);
        }

      XUnmapWindow (x11_display->xdisplay, x11_display->guard_window);
      XDestroyWindow (x11_display->xdisplay, x11_display->guard_window);

      x11_display->guard_window = None;
    }

  if (x11_display->prop_hooks)
    {
      meta_x11_display_free_window_prop_hooks (x11_display);
      x11_display->prop_hooks = NULL;
    }

  if (x11_display->group_prop_hooks)
    {
      meta_x11_display_free_group_prop_hooks (x11_display);
      x11_display->group_prop_hooks = NULL;
    }

  if (x11_display->xids)
    {
      /* Must be after all calls to meta_window_unmanage() since they
       * unregister windows
       */
      g_hash_table_destroy (x11_display->xids);
      x11_display->xids = NULL;
    }

  if (x11_display->xroot != None)
    {
      unset_wm_check_hint (x11_display);

      meta_error_trap_push (x11_display);
      XSelectInput (x11_display->xdisplay, x11_display->xroot, 0);
      if (meta_error_trap_pop_with_return (x11_display) != Success)
        meta_warning ("Could not release screen %d on display \"%s\"\n",
                      meta_ui_get_screen_number (), x11_display->name);

      x11_display->xroot = None;
    }


  if (x11_display->xdisplay)
    {
      meta_x11_display_free_events (x11_display);

      x11_display->xdisplay = NULL;
    }

  g_free (x11_display->name);
  x11_display->name = NULL;

  g_free (x11_display->screen_name);
  x11_display->screen_name = NULL;

  G_OBJECT_CLASS (meta_x11_display_parent_class)->dispose (object);
}

static void
meta_x11_display_class_init (MetaX11DisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = meta_x11_display_dispose;
}

static void
meta_x11_display_init (MetaX11Display *x11_display)
{
  quark_x11_display_logical_monitor_data =
    g_quark_from_static_string ("-meta-x11-display-logical-monitor-data");
}

static void
query_xsync_extension (MetaX11Display *x11_display)
{
  int major, minor;

  x11_display->have_xsync = FALSE;

  x11_display->xsync_error_base = 0;
  x11_display->xsync_event_base = 0;

  /* I don't think we really have to fill these in */
  major = SYNC_MAJOR_VERSION;
  minor = SYNC_MINOR_VERSION;

  if (!XSyncQueryExtension (x11_display->xdisplay,
                            &x11_display->xsync_event_base,
                            &x11_display->xsync_error_base) ||
      !XSyncInitialize (x11_display->xdisplay,
                        &major, &minor))
    {
      x11_display->xsync_error_base = 0;
      x11_display->xsync_event_base = 0;
    }
  else
    {
      x11_display->have_xsync = TRUE;
      XSyncSetPriority (x11_display->xdisplay, None, 10);
    }

  meta_verbose ("Attempted to init Xsync, found version %d.%d error base %d event base %d\n",
                major, minor,
                x11_display->xsync_error_base,
                x11_display->xsync_event_base);
}

static void
query_xshape_extension (MetaX11Display *x11_display)
{
  x11_display->have_shape = FALSE;

  x11_display->shape_error_base = 0;
  x11_display->shape_event_base = 0;

  if (!XShapeQueryExtension (x11_display->xdisplay,
                             &x11_display->shape_event_base,
                             &x11_display->shape_error_base))
    {
      x11_display->shape_error_base = 0;
      x11_display->shape_event_base = 0;
    }
  else
    x11_display->have_shape = TRUE;

  meta_verbose ("Attempted to init Shape, found error base %d event base %d\n",
                x11_display->shape_error_base,
                x11_display->shape_event_base);
}

static void
query_xcomposite_extension (MetaX11Display *x11_display)
{
  x11_display->have_composite = FALSE;

  x11_display->composite_error_base = 0;
  x11_display->composite_event_base = 0;

  if (!XCompositeQueryExtension (x11_display->xdisplay,
                                 &x11_display->composite_event_base,
                                 &x11_display->composite_error_base))
    {
      x11_display->composite_error_base = 0;
      x11_display->composite_event_base = 0;
    }
  else
    {
      x11_display->composite_major_version = 0;
      x11_display->composite_minor_version = 0;
      if (XCompositeQueryVersion (x11_display->xdisplay,
                                  &x11_display->composite_major_version,
                                  &x11_display->composite_minor_version))
        {
          x11_display->have_composite = TRUE;
        }
      else
        {
          x11_display->composite_major_version = 0;
          x11_display->composite_minor_version = 0;
        }
    }

  meta_verbose ("Attempted to init Composite, found error base %d event base %d "
                "extn ver %d %d\n",
                x11_display->composite_error_base,
                x11_display->composite_event_base,
                x11_display->composite_major_version,
                x11_display->composite_minor_version);
}

static void
query_xdamage_extension (MetaX11Display *x11_display)
{
  x11_display->have_damage = FALSE;

  x11_display->damage_error_base = 0;
  x11_display->damage_event_base = 0;

  if (!XDamageQueryExtension (x11_display->xdisplay,
                              &x11_display->damage_event_base,
                              &x11_display->damage_error_base))
    {
      x11_display->damage_error_base = 0;
      x11_display->damage_event_base = 0;
    }
  else
    x11_display->have_damage = TRUE;

  meta_verbose ("Attempted to init Damage, found error base %d event base %d\n",
                x11_display->damage_error_base,
                x11_display->damage_event_base);
}

static void
query_xfixes_extension (MetaX11Display *x11_display)
{
  x11_display->xfixes_error_base = 0;
  x11_display->xfixes_event_base = 0;

  if (XFixesQueryExtension (x11_display->xdisplay,
                            &x11_display->xfixes_event_base,
                            &x11_display->xfixes_error_base))
    {
      int xfixes_major, xfixes_minor;

      XFixesQueryVersion (x11_display->xdisplay, &xfixes_major, &xfixes_minor);

      if (xfixes_major * 100 + xfixes_minor < 500)
        meta_fatal ("Mutter requires XFixes 5.0");
    }
  else
    {
      meta_fatal ("Mutter requires XFixes 5.0");
    }

  meta_verbose ("Attempted to init XFixes, found error base %d event base %d\n",
                x11_display->xfixes_error_base,
                x11_display->xfixes_event_base);
}

static void
query_xi_extension (MetaX11Display *x11_display)
{
  int major = 2, minor = 3;
  gboolean has_xi = FALSE;

  if (XQueryExtension (x11_display->xdisplay,
                       "XInputExtension",
                       &x11_display->xinput_opcode,
                       &x11_display->xinput_error_base,
                       &x11_display->xinput_event_base))
    {
        if (XIQueryVersion (x11_display->xdisplay, &major, &minor) == Success)
        {
          int version = (major * 10) + minor;
          if (version >= 22)
            has_xi = TRUE;

#ifdef HAVE_XI23
          if (version >= 23)
            x11_display->have_xinput_23 = TRUE;
#endif /* HAVE_XI23 */
        }
    }

  if (!has_xi)
    meta_fatal ("X server doesn't have the XInput extension, version 2.2 or newer\n");
}

static void
set_desktop_geometry_hint (MetaX11Display *x11_display)
{
  unsigned long data[2];

  if (x11_display->display->closing > 0)
    return;

  data[0] = x11_display->display->rect.width;
  data[1] = x11_display->display->rect.height;

  meta_verbose ("Setting _NET_DESKTOP_GEOMETRY to %lu, %lu\n", data[0], data[1]);

  meta_error_trap_push (x11_display);
  XChangeProperty (x11_display->xdisplay,
                   x11_display->xroot,
                   x11_display->atom__NET_DESKTOP_GEOMETRY,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 2);
  meta_error_trap_pop (x11_display);
}

static void
set_desktop_viewport_hint (MetaX11Display *x11_display)
{
  unsigned long data[2];

  if (x11_display->display->closing > 0)
    return;

  /*
   * Mutter does not implement viewports, so this is a fixed 0,0
   */
  data[0] = 0;
  data[1] = 0;

  meta_verbose ("Setting _NET_DESKTOP_VIEWPORT to 0, 0\n");

  meta_error_trap_push (x11_display);
  XChangeProperty (x11_display->xdisplay,
                   x11_display->xroot,
                   x11_display->atom__NET_DESKTOP_VIEWPORT,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 2);
  meta_error_trap_pop (x11_display);
}

static int
set_wm_check_hint (MetaX11Display *x11_display)
{
  unsigned long data[1];

  g_return_val_if_fail (x11_display->leader_window != None, 0);

  data[0] = x11_display->leader_window;

  XChangeProperty (x11_display->xdisplay,
                   x11_display->xroot,
                   x11_display->atom__NET_SUPPORTING_WM_CHECK,
                   XA_WINDOW,
                   32, PropModeReplace, (guchar*) data, 1);

  return Success;
}

static void
unset_wm_check_hint (MetaX11Display *x11_display)
{
  XDeleteProperty (x11_display->xdisplay,
                   x11_display->xroot,
                   x11_display->atom__NET_SUPPORTING_WM_CHECK);
}

static int
set_supported_hint (MetaX11Display *x11_display)
{
  Atom atoms[] = {
#define EWMH_ATOMS_ONLY
#define item(x)  x11_display->atom_##x,
#include "x11/atomnames.h"
#undef item
#undef EWMH_ATOMS_ONLY

    x11_display->atom__GTK_FRAME_EXTENTS,
    x11_display->atom__GTK_SHOW_WINDOW_MENU,
  };

  XChangeProperty (x11_display->xdisplay,
                   x11_display->xroot,
                   x11_display->atom__NET_SUPPORTED,
                   XA_ATOM,
                   32, PropModeReplace,
                   (guchar*) atoms, G_N_ELEMENTS(atoms));

  return Success;
}

static int
set_wm_icon_size_hint (MetaX11Display *x11_display)
{
#define N_VALS 6
  gulong vals[N_VALS];

  /* We've bumped the real icon size up to 96x96, but
   * we really should not add these sorts of constraints
   * on clients still using the legacy WM_HINTS interface.
   */
#define LEGACY_ICON_SIZE 32

  /* min width, min height, max w, max h, width inc, height inc */
  vals[0] = LEGACY_ICON_SIZE;
  vals[1] = LEGACY_ICON_SIZE;
  vals[2] = LEGACY_ICON_SIZE;
  vals[3] = LEGACY_ICON_SIZE;
  vals[4] = 0;
  vals[5] = 0;
#undef LEGACY_ICON_SIZE

  XChangeProperty (x11_display->xdisplay,
                   x11_display->xroot,
                   x11_display->atom_WM_ICON_SIZE,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) vals, N_VALS);

  return Success;
#undef N_VALS
}

static Window
take_manager_selection (MetaX11Display *x11_display,
                        Window          xroot,
                        Atom            manager_atom,
                        int             timestamp,
                        gboolean        should_replace)
{
  Window current_owner, new_owner;

  current_owner = XGetSelectionOwner (x11_display->xdisplay, manager_atom);
  if (current_owner != None)
    {
      XSetWindowAttributes attrs;

      if (should_replace)
        {
          /* We want to find out when the current selection owner dies */
          meta_error_trap_push (x11_display);
          attrs.event_mask = StructureNotifyMask;
          XChangeWindowAttributes (x11_display->xdisplay, current_owner, CWEventMask, &attrs);
          if (meta_error_trap_pop_with_return (x11_display) != Success)
            current_owner = None; /* don't wait for it to die later on */
        }
      else
        {
          meta_warning (_("Display “%s” already has a window manager; try using the --replace option to replace the current window manager."),
                        x11_display->name);
          return None;
        }
    }

  /* We need SelectionClear and SelectionRequest events on the new owner,
   * but those cannot be masked, so we only need NoEventMask.
   */
  new_owner = meta_x11_display_create_offscreen_window (x11_display, xroot, NoEventMask);

  XSetSelectionOwner (x11_display->xdisplay, manager_atom, new_owner, timestamp);

  if (XGetSelectionOwner (x11_display->xdisplay, manager_atom) != new_owner)
    {
      meta_warning ("Could not acquire selection: %s", XGetAtomName (x11_display->xdisplay, manager_atom));
      return None;
    }

  {
    /* Send client message indicating that we are now the selection owner */
    XClientMessageEvent ev;

    ev.type = ClientMessage;
    ev.window = xroot;
    ev.message_type = x11_display->atom_MANAGER;
    ev.format = 32;
    ev.data.l[0] = timestamp;
    ev.data.l[1] = manager_atom;

    XSendEvent (x11_display->xdisplay, xroot, False, StructureNotifyMask, (XEvent *) &ev);
  }

  /* Wait for old window manager to go away */
  if (current_owner != None)
    {
      XEvent event;

      /* We sort of block infinitely here which is probably lame. */

      meta_verbose ("Waiting for old window manager to exit\n");
      do
        XWindowEvent (x11_display->xdisplay, current_owner, StructureNotifyMask, &event);
      while (event.type != DestroyNotify);
    }

  return new_owner;
}

/* Create the leader window here. Set its properties and
 * use the timestamp from one of the PropertyNotify events
 * that will follow.
 */
static void
init_leader_window (MetaX11Display *x11_display,
                    guint32        *timestamp)
{
  gulong data[1];
  XEvent event;

  /* We only care about the PropertyChangeMask in the next 30 or so lines of
   * code.  Note that gdk will at some point unset the PropertyChangeMask for
   * this window, so we can't rely on it still being set later.  See bug
   * 354213 for details.
   */
  x11_display->leader_window =
    meta_x11_display_create_offscreen_window (x11_display,
                                              x11_display->xroot,
                                              PropertyChangeMask);

  meta_prop_set_utf8_string_hint (x11_display,
                                  x11_display->leader_window,
                                  x11_display->atom__NET_WM_NAME,
                                  net_wm_name);

  meta_prop_set_utf8_string_hint (x11_display,
                                  x11_display->leader_window,
                                  x11_display->atom__GNOME_WM_KEYBINDINGS,
                                  gnome_wm_keybindings);

  meta_prop_set_utf8_string_hint (x11_display,
                                  x11_display->leader_window,
                                  x11_display->atom__MUTTER_VERSION,
                                  VERSION);

  data[0] = x11_display->leader_window;
  XChangeProperty (x11_display->xdisplay,
                   x11_display->leader_window,
                   x11_display->atom__NET_SUPPORTING_WM_CHECK,
                   XA_WINDOW,
                   32, PropModeReplace, (guchar*) data, 1);

  XWindowEvent (x11_display->xdisplay,
                x11_display->leader_window,
                PropertyChangeMask,
                &event);

  if (timestamp)
   *timestamp = event.xproperty.time;

  /* Make it painfully clear that we can't rely on PropertyNotify events on
   * this window, as per bug 354213.
   */
  XSelectInput (x11_display->xdisplay,
                x11_display->leader_window,
                NoEventMask);
}

static void
init_event_masks (MetaX11Display *x11_display)
{
  long event_mask;
  unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
  XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };

  XISetMask (mask.mask, XI_Enter);
  XISetMask (mask.mask, XI_Leave);
  XISetMask (mask.mask, XI_FocusIn);
  XISetMask (mask.mask, XI_FocusOut);
#ifdef HAVE_XI23
  if (META_X11_DISPLAY_HAS_XINPUT_23 (x11_display))
    {
      XISetMask (mask.mask, XI_BarrierHit);
      XISetMask (mask.mask, XI_BarrierLeave);
    }
#endif /* HAVE_XI23 */
  XISelectEvents (x11_display->xdisplay, x11_display->xroot, &mask, 1);

  event_mask = (SubstructureRedirectMask | SubstructureNotifyMask |
                StructureNotifyMask | ColormapChangeMask | PropertyChangeMask);
  XSelectInput (x11_display->xdisplay, x11_display->xroot, event_mask);
}

/**
 * meta_set_wm_name: (skip)
 * @wm_name: value for _NET_WM_NAME
 *
 * Set the value to use for the _NET_WM_NAME property. To take effect,
 * it is necessary to call this function before meta_init().
 */
void
meta_set_wm_name (const char *wm_name)
{
  g_return_if_fail (meta_get_display () == NULL);

  net_wm_name = wm_name;
}

/**
 * meta_set_gnome_wm_keybindings: (skip)
 * @wm_keybindings: value for _GNOME_WM_KEYBINDINGS
 *
 * Set the value to use for the _GNOME_WM_KEYBINDINGS property. To take
 * effect, it is necessary to call this function before meta_init().
 */
void
meta_set_gnome_wm_keybindings (const char *wm_keybindings)
{
  g_return_if_fail (meta_get_display () == NULL);

  gnome_wm_keybindings = wm_keybindings;
}

/**
 * meta_x11_display_new:
 *
 * Opens a new X11 display, sets it up, initialises all the X extensions
 * we will need.
 *
 * Returns: #MetaX11Display if the display was opened successfully,
 * and %NULL otherwise-- that is, if the display doesn't exist or
 * it already has a window manager, and sets the error appropriately.
 */
MetaX11Display *
meta_x11_display_new (MetaDisplay *display, GError **error)
{
  MetaX11Display *x11_display;
  Display *xdisplay;
  Screen *xscreen;
  Window xroot;
  int i, number;
  Window new_wm_sn_owner;
  gboolean replace_current_wm;
  Atom wm_sn_atom;
  char buf[128];
  guint32 timestamp;

  /* A list of all atom names, so that we can intern them in one go. */
  const char *atom_names[] = {
#define item(x) #x,
#include "x11/atomnames.h"
#undef item
  };
  Atom atoms[G_N_ELEMENTS(atom_names)];

  meta_verbose ("Opening display '%s'\n", XDisplayName (NULL));

  xdisplay = meta_ui_get_display ();

  if (xdisplay == NULL)
    {
      meta_warning (_("Failed to open X Window System display “%s”\n"),
                    XDisplayName (NULL));

      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to open X11 display");

      return NULL;
    }

#ifdef HAVE_WAYLAND
  if (meta_is_wayland_compositor ())
    meta_xwayland_complete_init ();
#endif

  if (meta_is_syncing ())
    XSynchronize (xdisplay, True);

  replace_current_wm = meta_get_replace_current_wm ();

  number = meta_ui_get_screen_number ();

  xroot = RootWindow (xdisplay, number);

  /* FVWM checks for None here, I don't know if this
   * ever actually happens
   */
  if (xroot == None)
    {
      meta_warning (_("Screen %d on display “%s” is invalid\n"),
                    number, XDisplayName (NULL));

      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to open default X11 screen");

      XFlush (xdisplay);
      XCloseDisplay (xdisplay);

      return NULL;
    }

  xscreen = ScreenOfDisplay (xdisplay, number);

  x11_display = g_object_new (META_TYPE_X11_DISPLAY, NULL);
  x11_display->display = display;

  /* here we use XDisplayName which is what the user
   * probably put in, vs. DisplayString(display) which is
   * canonicalized by XOpenDisplay()
   */
  x11_display->xdisplay = xdisplay;
  x11_display->xroot = xroot;

  x11_display->name = g_strdup (XDisplayName (NULL));
  x11_display->screen_name = get_screen_name (xdisplay, number);
  x11_display->default_xvisual = DefaultVisualOfScreen (xscreen);
  x11_display->default_depth = DefaultDepthOfScreen (xscreen);

  meta_verbose ("Creating %d atoms\n", (int) G_N_ELEMENTS (atom_names));
  XInternAtoms (xdisplay, (char **)atom_names, G_N_ELEMENTS (atom_names),
                False, atoms);

  i = 0;
#define item(x) x11_display->atom_##x = atoms[i++];
#include "x11/atomnames.h"
#undef item

  query_xsync_extension (x11_display);
  query_xshape_extension (x11_display);
  query_xcomposite_extension (x11_display);
  query_xdamage_extension (x11_display);
  query_xfixes_extension (x11_display);
  query_xi_extension (x11_display);

  g_signal_connect_object (display,
                           "cursor-updated",
                           G_CALLBACK (update_cursor_theme),
                           x11_display,
                           G_CONNECT_SWAPPED);

  update_cursor_theme (x11_display);

  x11_display->xids = g_hash_table_new (meta_unsigned_long_hash,
                                        meta_unsigned_long_equal);

  x11_display->groups_by_leader = NULL;
  x11_display->ui = NULL;
  x11_display->composite_overlay_window = None;
  x11_display->guard_window = None;
  x11_display->leader_window = None;
  x11_display->timestamp_pinging_window = None;
  x11_display->wm_sn_selection_window = None;

  x11_display->focus_serial = 0;
  x11_display->server_focus_window = None;
  x11_display->server_focus_serial = 0;

  x11_display->prop_hooks = NULL;
  meta_x11_display_init_window_prop_hooks (x11_display);
  x11_display->group_prop_hooks = NULL;
  meta_x11_display_init_group_prop_hooks (x11_display);

  g_signal_connect_object (display,
                           "monitors-changed",
                           G_CALLBACK (on_monitors_changed),
                           x11_display,
                           0);

  init_leader_window (x11_display, &timestamp);
  x11_display->timestamp = timestamp;

  /* Make a little window used only for pinging the server for timestamps; note
   * that meta_create_offscreen_window already selects for PropertyChangeMask.
   */
  x11_display->timestamp_pinging_window =
    meta_x11_display_create_offscreen_window (x11_display,
                                              xroot,
                                              PropertyChangeMask);

  sprintf (buf, "WM_S%d", number);

  wm_sn_atom = XInternAtom (xdisplay, buf, False);
  new_wm_sn_owner = take_manager_selection (x11_display, xroot, wm_sn_atom, timestamp, replace_current_wm);
  if (new_wm_sn_owner == None)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to acquire window manager ownership");

      g_object_run_dispose (G_OBJECT (x11_display));
      g_clear_object (&x11_display);

      return NULL;
    }

  x11_display->wm_sn_selection_window = new_wm_sn_owner;
  x11_display->wm_sn_atom = wm_sn_atom;
  x11_display->wm_sn_timestamp = timestamp;

  init_event_masks (x11_display);

  /* Select for cursor changes so the cursor tracker is up to date. */
  XFixesSelectCursorInput (xdisplay, xroot, XFixesDisplayCursorNotifyMask);

  /* If we're a Wayland compositor, then we don't grab the COW, since it
   * will map it. */
  if (!meta_is_wayland_compositor ())
    x11_display->composite_overlay_window = XCompositeGetOverlayWindow (xdisplay, xroot);

  /* Now that we've gotten taken a reference count on the COW, we
   * can close the helper that is holding on to it */
  meta_restart_finish ();

  /* Handle creating a no_focus_window for this screen */
  x11_display->no_focus_window =
    meta_x11_display_create_offscreen_window (x11_display,
                                              xroot,
                                              FocusChangeMask|KeyPressMask|KeyReleaseMask);
  XMapWindow (xdisplay, x11_display->no_focus_window);
  /* Done with no_focus_window stuff */

  meta_x11_display_init_events (x11_display);

  set_wm_icon_size_hint (x11_display);

  set_supported_hint (x11_display);

  set_wm_check_hint (x11_display);

  set_desktop_viewport_hint (x11_display);

  set_desktop_geometry_hint (x11_display);

  x11_display->ui = meta_ui_new (xdisplay);

  x11_display->keys_grabbed = FALSE;
  meta_x11_display_grab_keys (x11_display);

  return x11_display;
}

int
meta_x11_display_get_screen_number (MetaX11Display *x11_display)
{
  return meta_ui_get_screen_number ();
}

/**
 * meta_x11_display_get_xdisplay: (skip)
 * @x11_display: a #MetaX11Display
 *
 */
Display *
meta_x11_display_get_xdisplay (MetaX11Display *x11_display)
{
  return x11_display->xdisplay;
}

/**
 * meta_x11_display_get_xroot: (skip)
 * @x11_display: A #MetaX11Display
 *
 */
Window
meta_x11_display_get_xroot (MetaX11Display *x11_display)
{
  return x11_display->xroot;
}

/**
 * meta_x11_display_get_xinput_opcode: (skip)
 * @x11_display: a #MetaX11Display
 *
 */
int
meta_x11_display_get_xinput_opcode (MetaX11Display *x11_display)
{
  return x11_display->xinput_opcode;
}

int
meta_x11_display_get_damage_event_base (MetaX11Display *x11_display)
{
  return x11_display->damage_event_base;
}

int
meta_x11_display_get_shape_event_base (MetaX11Display *x11_display)
{
  return x11_display->shape_event_base;
}

gboolean
meta_x11_display_has_shape (MetaX11Display *x11_display)
{
  return META_X11_DISPLAY_HAS_SHAPE (x11_display);
}

Window
meta_x11_display_create_offscreen_window (MetaX11Display *x11_display,
                                          Window          parent,
                                          long            valuemask)
{
  XSetWindowAttributes attrs;

  /* we want to be override redirect because sometimes we
   * create a window on a screen we aren't managing.
   * (but on a display we are managing at least one screen for)
   */
  attrs.override_redirect = True;
  attrs.event_mask = valuemask;

  return XCreateWindow (x11_display->xdisplay,
                        parent,
                        -100, -100, 1, 1,
                        0,
                        CopyFromParent,
                        CopyFromParent,
                        (Visual *)CopyFromParent,
                        CWOverrideRedirect | CWEventMask,
                        &attrs);
}

Cursor
meta_x11_display_create_x_cursor (MetaX11Display *x11_display,
                                  MetaCursor      cursor)
{
  return meta_create_x_cursor (x11_display->xdisplay, cursor);
}

static char *
get_screen_name (Display *xdisplay,
                 int      number)
{
  char *p;
  char *dname;
  char *scr;

  /* DisplayString gives us a sort of canonical display,
   * vs. the user-entered name from XDisplayName()
   */
  dname = g_strdup (DisplayString (xdisplay));

  /* Change display name to specify this screen.
   */
  p = strrchr (dname, ':');
  if (p)
    {
      p = strchr (p, '.');
      if (p)
        *p = '\0';
    }

  scr = g_strdup_printf ("%s.%d", dname, number);

  g_free (dname);

  return scr;
}

void
meta_x11_display_reload_cursor (MetaX11Display *x11_display)
{
  Cursor xcursor;
  MetaCursor cursor = x11_display->display->current_cursor;

  /* Set a cursor for X11 applications that don't specify their own */
  xcursor = meta_x11_display_create_x_cursor (x11_display, cursor);

  XDefineCursor (x11_display->xdisplay, x11_display->xroot, xcursor);
  XFlush (x11_display->xdisplay);
  XFreeCursor (x11_display->xdisplay, xcursor);
}

static void
set_cursor_theme (Display *xdisplay)
{
  XcursorSetTheme (xdisplay, meta_prefs_get_cursor_theme ());
  XcursorSetDefaultSize (xdisplay, meta_prefs_get_cursor_size ());
}

static void
update_cursor_theme (MetaX11Display *x11_display)
{
  MetaBackend *backend = meta_get_backend ();

  set_cursor_theme (x11_display->xdisplay);
  meta_x11_display_reload_cursor (x11_display);

  if (META_IS_BACKEND_X11 (backend))
    {
      MetaBackendX11 *backend_x11 = META_BACKEND_X11 (backend);
      Display *xdisplay = meta_backend_x11_get_xdisplay (backend_x11);

      set_cursor_theme (xdisplay);
    }
}

MetaWindow *
meta_x11_display_lookup_x_window (MetaX11Display *x11_display,
                                  Window          xwindow)
{
  return g_hash_table_lookup (x11_display->xids, &xwindow);
}

void
meta_x11_display_register_x_window (MetaX11Display *x11_display,
                                    Window         *xwindowp,
                                    MetaWindow     *window)
{
  g_return_if_fail (g_hash_table_lookup (x11_display->xids, xwindowp) == NULL);

  g_hash_table_insert (x11_display->xids, xwindowp, window);
}

void
meta_x11_display_unregister_x_window (MetaX11Display *x11_display,
                                      Window          xwindow)
{
  g_return_if_fail (g_hash_table_lookup (x11_display->xids, &xwindow) != NULL);

  g_hash_table_remove (x11_display->xids, &xwindow);
}


/* We store sync alarms in the window ID hash table, because they are
 * just more types of XIDs in the same global space, but we have
 * typesafe functions to register/unregister for readability.
 */

MetaWindow *
meta_x11_display_lookup_sync_alarm (MetaX11Display *x11_display,
                                    XSyncAlarm      alarm)
{
  return g_hash_table_lookup (x11_display->xids, &alarm);
}

void
meta_x11_display_register_sync_alarm (MetaX11Display *x11_display,
                                      XSyncAlarm     *alarmp,
                                      MetaWindow     *window)
{
  g_return_if_fail (g_hash_table_lookup (x11_display->xids, alarmp) == NULL);

  g_hash_table_insert (x11_display->xids, alarmp, window);
}

void
meta_x11_display_unregister_sync_alarm (MetaX11Display *x11_display,
                                        XSyncAlarm      alarm)
{
  g_return_if_fail (g_hash_table_lookup (x11_display->xids, &alarm) != NULL);

  g_hash_table_remove (x11_display->xids, &alarm);
}

void
meta_x11_display_set_alarm_filter (MetaX11Display *x11_display,
                                   MetaAlarmFilter filter,
                                   gpointer        data)
{
  g_return_if_fail (filter == NULL || x11_display->alarm_filter == NULL);

  x11_display->alarm_filter = filter;
  x11_display->alarm_filter_data = data;
}

/* The guard window allows us to leave minimized windows mapped so
 * that compositor code may provide live previews of them.
 * Instead of being unmapped/withdrawn, they get pushed underneath
 * the guard window. We also select events on the guard window, which
 * should effectively be forwarded to events on the background actor,
 * providing that the scene graph is set up correctly.
 */
static Window
create_guard_window (MetaX11Display *x11_display)
{
  XSetWindowAttributes attributes;
  Window guard_window;
  gulong create_serial;

  attributes.event_mask = NoEventMask;
  attributes.override_redirect = True;

  /* We have to call record_add() after we have the new window ID,
   * so save the serial for the CreateWindow request until then */
  create_serial = XNextRequest (x11_display->xdisplay);
  guard_window =
    XCreateWindow (x11_display->xdisplay,
                   x11_display->xroot,
                   0, /* x */
                   0, /* y */
                   x11_display->display->rect.width,
                   x11_display->display->rect.height,
                   0, /* border width */
                   0, /* depth */
                   InputOnly, /* class */
                   CopyFromParent, /* visual */
                   CWEventMask | CWOverrideRedirect,
                   &attributes);

  /* https://bugzilla.gnome.org/show_bug.cgi?id=710346 */
  XStoreName (x11_display->xdisplay, guard_window, "mutter guard window");

  {
    if (!meta_is_wayland_compositor ())
      {
        MetaBackendX11 *backend = META_BACKEND_X11 (meta_get_backend ());
        Display *backend_xdisplay = meta_backend_x11_get_xdisplay (backend);
        unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
        XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };

        XISetMask (mask.mask, XI_ButtonPress);
        XISetMask (mask.mask, XI_ButtonRelease);
        XISetMask (mask.mask, XI_Motion);

        /* Sync on the connection we created the window on to
         * make sure it's created before we select on it on the
         * backend connection. */
        XSync (x11_display->xdisplay, False);

        XISelectEvents (backend_xdisplay, guard_window, &mask, 1);
      }
  }

  meta_stack_tracker_record_add (x11_display->display->stack_tracker,
                                 guard_window,
                                 create_serial);

  meta_stack_tracker_lower (x11_display->display->stack_tracker,
                            guard_window);

  XMapWindow (x11_display->xdisplay, guard_window);
  return guard_window;
}

void
meta_x11_display_create_guard_window (MetaX11Display *x11_display)
{
  if (x11_display->guard_window == None)
    x11_display->guard_window = create_guard_window (x11_display);
}

static void
on_monitors_changed (MetaDisplay    *display,
                     MetaX11Display *x11_display)
{
  set_desktop_geometry_hint (x11_display);

  /* Resize the guard window to fill the screen again. */
  if (x11_display->guard_window != None)
    {
      XWindowChanges changes;

      changes.x = 0;
      changes.y = 0;
      changes.width = display->rect.width;
      changes.height = display->rect.height;

      XConfigureWindow (x11_display->xdisplay,
                        x11_display->guard_window,
                        CWX | CWY | CWWidth | CWHeight,
                        &changes);
    }

  x11_display->has_xinerama_indices = FALSE;
}

void
meta_x11_display_set_cm_selection (MetaX11Display *x11_display)
{
  char selection[32];
  Atom a;
  guint32 timestamp;

  timestamp = meta_x11_display_get_current_time_roundtrip (x11_display);
  g_snprintf (selection, sizeof (selection), "_NET_WM_CM_S%d",
              meta_ui_get_screen_number ());
  a = XInternAtom (x11_display->xdisplay, selection, False);

  x11_display->wm_cm_selection_window = take_manager_selection (x11_display, x11_display->xroot, a, timestamp, TRUE);
}

static Bool
find_timestamp_predicate (Display  *xdisplay,
                          XEvent   *ev,
                          XPointer  arg)
{
  MetaX11Display *x11_display = (MetaX11Display *) arg;

  return (ev->type == PropertyNotify &&
          ev->xproperty.atom == x11_display->atom__MUTTER_TIMESTAMP_PING);
}

/* Get a timestamp, even if it means a roundtrip */
guint32
meta_x11_display_get_current_time_roundtrip (MetaX11Display *x11_display)
{
  guint32 timestamp;

  timestamp = meta_display_get_current_time (x11_display->display);
  if (timestamp == CurrentTime)
    {
      XEvent property_event;

      XChangeProperty (x11_display->xdisplay,
                       x11_display->timestamp_pinging_window,
                       x11_display->atom__MUTTER_TIMESTAMP_PING,
                       XA_STRING, 8, PropModeAppend, NULL, 0);
      XIfEvent (x11_display->xdisplay,
                &property_event,
                find_timestamp_predicate,
                (XPointer) x11_display);
      timestamp = property_event.xproperty.time;
    }

  meta_display_sanity_check_timestamps (x11_display->display, timestamp);

  return timestamp;
}

/**
 * meta_x11_display_xwindow_is_a_no_focus_window:
 * @x11_display: A #MetaX11Display
 * @xwindow: An X11 window
 *
 * Returns: %TRUE iff window is one of mutter's internal "no focus" windows
 * which will have the focus when there is no actual client window focused.
 */
gboolean
meta_x11_display_xwindow_is_a_no_focus_window (MetaX11Display *x11_display,
                                               Window xwindow)
{
  return xwindow == x11_display->no_focus_window;
}

void
meta_x11_display_increment_event_serial (MetaX11Display *x11_display)

{
  /* We just make some random X request */
  XDeleteProperty (x11_display->xdisplay,
                   x11_display->leader_window,
                   x11_display->atom__MOTIF_WM_HINTS);
}

void
meta_x11_display_update_active_window_hint (MetaX11Display *x11_display)
{
  MetaWindow *focus_window = x11_display->display->focus_window;
  gulong data[1];

  if (x11_display->display->closing)
    return; /* Leave old value for a replacement */

  if (focus_window)
    data[0] = focus_window->xwindow;
  else
    data[0] = None;

  meta_error_trap_push (x11_display);
  XChangeProperty (x11_display->xdisplay,
                   x11_display->xroot,
                   x11_display->atom__NET_ACTIVE_WINDOW,
                   XA_WINDOW,
                   32, PropModeReplace, (guchar*) data, 1);
  meta_error_trap_pop (x11_display);
}

static void
request_xserver_input_focus_change (MetaX11Display *x11_display,
                                    MetaWindow     *meta_window,
                                    Window          xwindow,
                                    guint32         timestamp)
{
  gulong serial;

  if (meta_display_timestamp_too_old (x11_display->display, &timestamp))
    return;

  meta_error_trap_push (x11_display);

  /* In order for mutter to know that the focus request succeeded, we track
   * the serial of the "focus request" we made, but if we take the serial
   * of the XSetInputFocus request, then there's no way to determine the
   * difference between focus events as a result of the SetInputFocus and
   * focus events that other clients send around the same time. Ensure that
   * we know which is which by making two requests that the server will
   * process at the same time.
   */
  XGrabServer (x11_display->xdisplay);

  serial = XNextRequest (x11_display->xdisplay);

  XSetInputFocus (x11_display->xdisplay,
                  xwindow,
                  RevertToPointerRoot,
                  timestamp);

  XChangeProperty (x11_display->xdisplay,
                   x11_display->timestamp_pinging_window,
                   x11_display->atom__MUTTER_FOCUS_SET,
                   XA_STRING, 8, PropModeAppend, NULL, 0);

  XUngrabServer (x11_display->xdisplay);
  XFlush (x11_display->xdisplay);

  meta_display_update_focus_window (x11_display->display,
                                    meta_window,
                                    xwindow,
                                    serial,
                                    TRUE);

  meta_error_trap_pop (x11_display);

  x11_display->display->last_focus_time = timestamp;

  if (meta_window == NULL || meta_window != x11_display->display->autoraise_window)
    meta_display_remove_autoraise_callback (x11_display->display);
}

void
meta_x11_display_set_input_focus_window (MetaX11Display *x11_display,
                                         MetaWindow     *window,
                                         gboolean        focus_frame,
                                         guint32         timestamp)
{
  request_xserver_input_focus_change (x11_display,
                                      window,
                                      focus_frame ? window->frame->xwindow : window->xwindow,
                                      timestamp);
}

void
meta_x11_display_set_input_focus_xwindow (MetaX11Display *x11_display,
                                          Window          window,
                                          guint32         timestamp)
{
  request_xserver_input_focus_change (x11_display,
                                      NULL,
                                      window,
                                      timestamp);
}

void
meta_x11_display_focus_the_no_focus_window (MetaX11Display *x11_display,
                                            guint32         timestamp)
{
  request_xserver_input_focus_change (x11_display,
                                      NULL,
                                      x11_display->no_focus_window,
                                      timestamp);
}

static MetaX11DisplayLogicalMonitorData *
get_x11_display_logical_monitor_data (MetaLogicalMonitor *logical_monitor)
{
  return g_object_get_qdata (G_OBJECT (logical_monitor),
                             quark_x11_display_logical_monitor_data);
}

static MetaX11DisplayLogicalMonitorData *
ensure_x11_display_logical_monitor_data (MetaLogicalMonitor *logical_monitor)
{
  MetaX11DisplayLogicalMonitorData *data;

  data = get_x11_display_logical_monitor_data (logical_monitor);
  if (data)
    return data;

  data = g_new0 (MetaX11DisplayLogicalMonitorData, 1);
  g_object_set_qdata_full (G_OBJECT (logical_monitor),
                           quark_x11_display_logical_monitor_data,
                           data,
                           g_free);

  return data;
}

static void
meta_x11_display_ensure_xinerama_indices (MetaX11Display *x11_display)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  GList *logical_monitors, *l;
  XineramaScreenInfo *infos;
  int n_infos, j;

  if (x11_display->has_xinerama_indices)
    return;

  x11_display->has_xinerama_indices = TRUE;

  if (!XineramaIsActive (x11_display->xdisplay))
    return;

  infos = XineramaQueryScreens (x11_display->xdisplay,
                                &n_infos);
  if (n_infos <= 0 || infos == NULL)
    {
      meta_XFree (infos);
      return;
    }

  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);

  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;

      for (j = 0; j < n_infos; ++j)
        {
          if (logical_monitor->rect.x == infos[j].x_org &&
              logical_monitor->rect.y == infos[j].y_org &&
              logical_monitor->rect.width == infos[j].width &&
              logical_monitor->rect.height == infos[j].height)
            {
              MetaX11DisplayLogicalMonitorData *logical_monitor_data;

              logical_monitor_data =
                ensure_x11_display_logical_monitor_data (logical_monitor);
              logical_monitor_data->xinerama_index = j;
            }
        }
    }

  meta_XFree (infos);
}

int
meta_x11_display_logical_monitor_to_xinerama_index (MetaX11Display     *x11_display,
                                                    MetaLogicalMonitor *logical_monitor)
{
  MetaX11DisplayLogicalMonitorData *logical_monitor_data;

  g_return_val_if_fail (logical_monitor, -1);

  meta_x11_display_ensure_xinerama_indices (x11_display);

  logical_monitor_data = get_x11_display_logical_monitor_data (logical_monitor);

  return logical_monitor_data->xinerama_index;
}

MetaLogicalMonitor *
meta_x11_display_xinerama_index_to_logical_monitor (MetaX11Display *x11_display,
                                                    int             xinerama_index)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  GList *logical_monitors, *l;

  meta_x11_display_ensure_xinerama_indices (x11_display);

  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);

  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;
      MetaX11DisplayLogicalMonitorData *logical_monitor_data;

      logical_monitor_data =
        ensure_x11_display_logical_monitor_data (logical_monitor);

      if (logical_monitor_data->xinerama_index == xinerama_index)
        return logical_monitor;
    }

  return NULL;
}
