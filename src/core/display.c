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
 * @title: MetaDisplay
 * @short_description: Mutter X display handler
 *
 * The display is represented as a #MetaDisplay struct.
 */

#define _XOPEN_SOURCE 600 /* for gethostname() */

#include <config.h>
#include "display-private.h"
#include "events.h"
#include "util-private.h"
#include <meta/main.h>
#include "screen-private.h"
#include "window-private.h"
#include "frame.h"
#include <meta/errors.h>
#include "keybindings-private.h"
#include <meta/prefs.h>
#include "resizepopup.h"
#include "workspace-private.h"
#include "bell.h"
#include <meta/compositor.h>
#include <meta/compositor-mutter.h>
#include <X11/Xatom.h>
#include "mutter-enum-types.h"
#include "meta-idle-monitor-dbus.h"
#include "meta-cursor-tracker-private.h"

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif
#ifdef HAVE_SHAPE
#include <X11/extensions/shape.h>
#endif
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "x11/window-x11.h"
#include "x11/window-props.h"
#include "x11/group-props.h"
#include "x11/xprops.h"

#include "wayland/meta-xwayland-private.h"
#include "meta-surface-actor-wayland.h"

/*
 * SECTION:pings
 *
 * Sometimes we want to see whether a window is responding,
 * so we send it a "ping" message and see whether it sends us back a "pong"
 * message within a reasonable time. Here we have a system which lets us
 * nominate one function to be called if we get the pong in time and another
 * function if we don't. The system is rather more complicated than it needs
 * to be, since we only ever use it to destroy windows which are asked to
 * close themselves and don't do so within a reasonable amount of time, and
 * therefore we always use the same callbacks. It's possible that we might
 * use it for other things in future, or on the other hand we might decide
 * that we're never going to do so and simplify it a bit.
 */

/**
 * MetaPingData:
 *
 * Describes a ping on a window. When we send a ping to a window, we build
 * one of these structs, and it eventually gets passed to the timeout function
 * or to the function which handles the response from the window. If the window
 * does or doesn't respond to the ping, we use this information to deal with
 * these facts; we have a handler function for each.
 */
typedef struct
{
  MetaWindow  *window;
  guint32      timestamp;
  MetaWindowPingFunc ping_reply_func;
  MetaWindowPingFunc ping_timeout_func;
  void        *user_data;
  guint        ping_timeout_id;
} MetaPingData;

G_DEFINE_TYPE(MetaDisplay, meta_display, G_TYPE_OBJECT);

/* Signals */
enum
{
  OVERLAY_KEY,
  ACCELERATOR_ACTIVATED,
  MODIFIERS_ACCELERATOR_ACTIVATED,
  FOCUS_WINDOW,
  WINDOW_CREATED,
  WINDOW_DEMANDS_ATTENTION,
  WINDOW_MARKED_URGENT,
  GRAB_OP_BEGIN,
  GRAB_OP_END,
  LAST_SIGNAL
};

enum {
  PROP_0,

  PROP_FOCUS_WINDOW
};

static guint display_signals [LAST_SIGNAL] = { 0 };

/*
 * The display we're managing.  This is a singleton object.  (Historically,
 * this was a list of displays, but there was never any way to add more
 * than one element to it.)  The goofy name is because we don't want it
 * to shadow the parameter in its object methods.
 */
static MetaDisplay *the_display = NULL;


static const char *gnome_wm_keybindings = "Mutter";
static const char *net_wm_name = "Mutter";

static void    update_window_grab_modifiers (MetaDisplay *display);

static void    prefs_changed_callback    (MetaPreference pref,
                                          void          *data);
static void
meta_display_get_property(GObject         *object,
                          guint            prop_id,
                          GValue          *value,
                          GParamSpec      *pspec)
{
  MetaDisplay *display = META_DISPLAY (object);

  switch (prop_id)
    {
    case PROP_FOCUS_WINDOW:
      g_value_set_object (value, display->focus_window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_display_set_property(GObject         *object,
                          guint            prop_id,
                          const GValue    *value,
                          GParamSpec      *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_display_class_init (MetaDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = meta_display_get_property;
  object_class->set_property = meta_display_set_property;

  display_signals[OVERLAY_KEY] =
    g_signal_new ("overlay-key",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  display_signals[ACCELERATOR_ACTIVATED] =
    g_signal_new ("accelerator-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);

  /**
   * MetaDisplay::modifiers-accelerator-activated:
   * @display: the #MetaDisplay instance
   *
   * The ::modifiers-accelerator-activated signal will be emitted when
   * a special modifiers-only keybinding is activated.
   *
   * Returns: %TRUE means that the keyboard device should remain
   *    frozen and %FALSE for the default behavior of unfreezing the
   *    keyboard.
   */
  display_signals[MODIFIERS_ACCELERATOR_ACTIVATED] =
    g_signal_new ("modifiers-accelerator-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_first_wins, NULL, NULL,
                  G_TYPE_BOOLEAN, 0);

  display_signals[WINDOW_CREATED] =
    g_signal_new ("window-created",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, META_TYPE_WINDOW);

  display_signals[WINDOW_DEMANDS_ATTENTION] =
    g_signal_new ("window-demands-attention",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, META_TYPE_WINDOW);

  display_signals[WINDOW_MARKED_URGENT] =
    g_signal_new ("window-marked-urgent",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  META_TYPE_WINDOW);

  display_signals[GRAB_OP_BEGIN] =
    g_signal_new ("grab-op-begin",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 3,
                  META_TYPE_SCREEN,
                  META_TYPE_WINDOW,
                  META_TYPE_GRAB_OP);

  display_signals[GRAB_OP_END] =
    g_signal_new ("grab-op-end",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 3,
                  META_TYPE_SCREEN,
                  META_TYPE_WINDOW,
                  META_TYPE_GRAB_OP);

  g_object_class_install_property (object_class,
                                   PROP_FOCUS_WINDOW,
                                   g_param_spec_object ("focus-window",
                                                        "Focus window",
                                                        "Currently focused window",
                                                        META_TYPE_WINDOW,
                                                        G_PARAM_READABLE));
}


/**
 * ping_data_free:
 *
 * Destructor for #MetaPingData structs. Will destroy the
 * event source for the struct as well.
 */
static void
ping_data_free (MetaPingData *ping_data)
{
  /* Remove the timeout */
  if (ping_data->ping_timeout_id != 0)
    g_source_remove (ping_data->ping_timeout_id);

  g_free (ping_data);
}

/**
 * remove_pending_pings_for_window:
 * @display: The display the window appears on
 * @xwindow: The X ID of the window whose pings we should remove
 *
 * Frees every pending ping structure for the given X window on the
 * given display. This means that we also destroy the timeouts.
 */
static void
remove_pending_pings_for_window (MetaDisplay *display, Window xwindow)
{
  GSList *tmp;
  GSList *dead;

  /* could obviously be more efficient, don't care */

  /* build list to be removed */
  dead = NULL;
  for (tmp = display->pending_pings; tmp; tmp = tmp->next)
    {
      MetaPingData *ping_data = tmp->data;

      if (ping_data->window->xwindow == xwindow)
        dead = g_slist_prepend (dead, ping_data);
    }

  /* remove what we found */
  for (tmp = dead; tmp; tmp = tmp->next)
    {
      MetaPingData *ping_data = tmp->data;

      display->pending_pings = g_slist_remove (display->pending_pings, ping_data);
      ping_data_free (ping_data);
    }

  g_slist_free (dead);
}


#ifdef HAVE_STARTUP_NOTIFICATION
static void
sn_error_trap_push (SnDisplay *sn_display,
                    Display   *xdisplay)
{
  MetaDisplay *display;
  display = meta_display_for_x_display (xdisplay);
  if (display != NULL)
    meta_error_trap_push (display);
}

static void
sn_error_trap_pop (SnDisplay *sn_display,
                   Display   *xdisplay)
{
  MetaDisplay *display;
  display = meta_display_for_x_display (xdisplay);
  if (display != NULL)
    meta_error_trap_pop (display);
}
#endif

static void
enable_compositor (MetaDisplay *display)
{
  if (!META_DISPLAY_HAS_COMPOSITE (display) ||
      !META_DISPLAY_HAS_DAMAGE (display) ||
      !META_DISPLAY_HAS_RENDER (display))
    {
      meta_warning ("Missing %s extension required for compositing",
                    !META_DISPLAY_HAS_COMPOSITE (display) ? "composite" :
                    !META_DISPLAY_HAS_DAMAGE (display) ? "damage" : "render");
      return;
    }

  if (!display->compositor)
      display->compositor = meta_compositor_new (display);

  if (!display->compositor)
    return;

  meta_compositor_manage (display->compositor);
}

static void
meta_display_init (MetaDisplay *disp)
{
  /* Some stuff could go in here that's currently in _open,
   * but it doesn't really matter. */
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
  g_return_if_fail (the_display == NULL);

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
  g_return_if_fail (the_display == NULL);

  gnome_wm_keybindings = wm_keybindings;
}

/**
 * meta_display_open:
 *
 * Opens a new display, sets it up, initialises all the X extensions
 * we will need, and adds it to the list of displays.
 *
 * Returns: %TRUE if the display was opened successfully, and %FALSE
 * otherwise-- that is, if the display doesn't exist or it already
 * has a window manager.
 */
gboolean
meta_display_open (void)
{
  Display *xdisplay;
  MetaScreen *screen;
  int i;
  guint32 timestamp;

  /* A list of all atom names, so that we can intern them in one go. */
  char *atom_names[] = {
#define item(x) #x,
#include <meta/atomnames.h>
#undef item
  };
  Atom atoms[G_N_ELEMENTS(atom_names)];
  
  meta_verbose ("Opening display '%s'\n", XDisplayName (NULL));

  xdisplay = meta_ui_get_display ();
  
  if (xdisplay == NULL)
    {
      meta_warning (_("Failed to open X Window System display '%s'\n"),
		    XDisplayName (NULL));
      return FALSE;
    }

  if (meta_is_wayland_compositor ())
    meta_xwayland_complete_init ();

  if (meta_is_syncing ())
    XSynchronize (xdisplay, True);
  
  g_assert (the_display == NULL);
  the_display = g_object_new (META_TYPE_DISPLAY, NULL);

  the_display->closing = 0;
  
  /* here we use XDisplayName which is what the user
   * probably put in, vs. DisplayString(display) which is
   * canonicalized by XOpenDisplay()
   */
  the_display->name = g_strdup (XDisplayName (NULL));
  the_display->xdisplay = xdisplay;
  the_display->server_grab_count = 0;
  the_display->display_opening = TRUE;

  the_display->pending_pings = NULL;
  the_display->autoraise_timeout_id = 0;
  the_display->autoraise_window = NULL;
  the_display->focus_window = NULL;
  the_display->focus_serial = 0;
  the_display->server_focus_window = None;
  the_display->server_focus_serial = 0;
  the_display->grab_old_window_stacking = NULL;

  the_display->mouse_mode = TRUE; /* Only relevant for mouse or sloppy focus */
  the_display->allow_terminal_deactivation = TRUE; /* Only relevant for when a
                                                  terminal has the focus */
  
  meta_bell_init (the_display);

  meta_display_init_keys (the_display);

  update_window_grab_modifiers (the_display);

  meta_prefs_add_listener (prefs_changed_callback, the_display);

  meta_verbose ("Creating %d atoms\n", (int) G_N_ELEMENTS (atom_names));
  XInternAtoms (the_display->xdisplay, atom_names, G_N_ELEMENTS (atom_names),
                False, atoms);
  {
    int i = 0;    
#define item(x) the_display->atom_##x = atoms[i++];
#include <meta/atomnames.h>
#undef item
  }

  the_display->prop_hooks = NULL;
  meta_display_init_window_prop_hooks (the_display);
  the_display->group_prop_hooks = NULL;
  meta_display_init_group_prop_hooks (the_display);
  
  /* Offscreen unmapped window used for _NET_SUPPORTING_WM_CHECK,
   * created in screen_new
   */
  the_display->leader_window = None;
  the_display->timestamp_pinging_window = None;

  the_display->monitor_cache_invalidated = TRUE;

  the_display->groups_by_leader = NULL;

  the_display->window_with_menu = NULL;
  the_display->window_menu = NULL;
  
  the_display->screen = NULL;
  
#ifdef HAVE_STARTUP_NOTIFICATION
  the_display->sn_display = sn_display_new (the_display->xdisplay,
                                        sn_error_trap_push,
                                        sn_error_trap_pop);
#endif

  /* Get events */
  meta_display_init_events (the_display);

  the_display->xids = g_hash_table_new (meta_unsigned_long_hash,
                                        meta_unsigned_long_equal);
  the_display->wayland_windows = g_hash_table_new (NULL, NULL);

  i = 0;
  while (i < N_IGNORED_CROSSING_SERIALS)
    {
      the_display->ignored_crossing_serials[i] = 0;
      ++i;
    }
  the_display->ungrab_should_not_cause_focus_window = None;
  
  the_display->current_time = CurrentTime;
  the_display->sentinel_counter = 0;

  the_display->grab_resize_timeout_id = 0;
  the_display->grab_have_keyboard = FALSE;
  
#ifdef HAVE_XKB  
  the_display->last_bell_time = 0;
#endif

  the_display->grab_op = META_GRAB_OP_NONE;
  the_display->grab_window = NULL;
  the_display->grab_resize_popup = NULL;
  the_display->grab_tile_mode = META_TILE_NONE;
  the_display->grab_tile_monitor_number = -1;

  the_display->grab_edge_resistance_data = NULL;

#ifdef HAVE_XSYNC
  {
    int major, minor;

    the_display->have_xsync = FALSE;
    
    the_display->xsync_error_base = 0;
    the_display->xsync_event_base = 0;

    /* I don't think we really have to fill these in */
    major = SYNC_MAJOR_VERSION;
    minor = SYNC_MINOR_VERSION;
    
    if (!XSyncQueryExtension (the_display->xdisplay,
                              &the_display->xsync_event_base,
                              &the_display->xsync_error_base) ||
        !XSyncInitialize (the_display->xdisplay,
                          &major, &minor))
      {
        the_display->xsync_error_base = 0;
        the_display->xsync_event_base = 0;
      }
    else
      {
        the_display->have_xsync = TRUE;
        XSyncSetPriority (the_display->xdisplay, None, 10);
      }

    meta_verbose ("Attempted to init Xsync, found version %d.%d error base %d event base %d\n",
                  major, minor,
                  the_display->xsync_error_base,
                  the_display->xsync_event_base);
  }
#else  /* HAVE_XSYNC */
  meta_verbose ("Not compiled with Xsync support\n");
#endif /* !HAVE_XSYNC */


#ifdef HAVE_SHAPE
  {
    the_display->have_shape = FALSE;
    
    the_display->shape_error_base = 0;
    the_display->shape_event_base = 0;
    
    if (!XShapeQueryExtension (the_display->xdisplay,
                               &the_display->shape_event_base,
                               &the_display->shape_error_base))
      {
        the_display->shape_error_base = 0;
        the_display->shape_event_base = 0;
      }
    else
      the_display->have_shape = TRUE;
    
    meta_verbose ("Attempted to init Shape, found error base %d event base %d\n",
                  the_display->shape_error_base,
                  the_display->shape_event_base);
  }
#else  /* HAVE_SHAPE */
  meta_verbose ("Not compiled with Shape support\n");
#endif /* !HAVE_SHAPE */

  {
    the_display->have_render = FALSE;
    
    the_display->render_error_base = 0;
    the_display->render_event_base = 0;
    
    if (!XRenderQueryExtension (the_display->xdisplay,
                                &the_display->render_event_base,
                                &the_display->render_error_base))
      {
        the_display->render_error_base = 0;
        the_display->render_event_base = 0;
      }
    else
      the_display->have_render = TRUE;
    
    meta_verbose ("Attempted to init Render, found error base %d event base %d\n",
                  the_display->render_error_base,
                  the_display->render_event_base);
  }

  {
    the_display->have_composite = FALSE;

    the_display->composite_error_base = 0;
    the_display->composite_event_base = 0;

    if (!XCompositeQueryExtension (the_display->xdisplay,
                                   &the_display->composite_event_base,
                                   &the_display->composite_error_base))
      {
        the_display->composite_error_base = 0;
        the_display->composite_event_base = 0;
      } 
    else
      {
        the_display->composite_major_version = 0;
        the_display->composite_minor_version = 0;
        if (XCompositeQueryVersion (the_display->xdisplay,
                                    &the_display->composite_major_version,
                                    &the_display->composite_minor_version))
          {
            the_display->have_composite = TRUE;
          }
        else
          {
            the_display->composite_major_version = 0;
            the_display->composite_minor_version = 0;
          }
      }

    meta_verbose ("Attempted to init Composite, found error base %d event base %d "
                  "extn ver %d %d\n",
                  the_display->composite_error_base, 
                  the_display->composite_event_base,
                  the_display->composite_major_version,
                  the_display->composite_minor_version);

    the_display->have_damage = FALSE;

    the_display->damage_error_base = 0;
    the_display->damage_event_base = 0;

    if (!XDamageQueryExtension (the_display->xdisplay,
                                &the_display->damage_event_base,
                                &the_display->damage_error_base))
      {
        the_display->damage_error_base = 0;
        the_display->damage_event_base = 0;
      } 
    else
      the_display->have_damage = TRUE;

    meta_verbose ("Attempted to init Damage, found error base %d event base %d\n",
                  the_display->damage_error_base, 
                  the_display->damage_event_base);

    the_display->xfixes_error_base = 0;
    the_display->xfixes_event_base = 0;

    if (XFixesQueryExtension (the_display->xdisplay,
                              &the_display->xfixes_event_base,
                              &the_display->xfixes_error_base))
      {
        int xfixes_major, xfixes_minor;

        XFixesQueryVersion (the_display->xdisplay, &xfixes_major, &xfixes_minor);

        if (xfixes_major * 100 + xfixes_minor < 500)
          meta_fatal ("Mutter requires XFixes 5.0");
      }
    else
      {
        meta_fatal ("Mutter requires XFixes 5.0");
      }

    meta_verbose ("Attempted to init XFixes, found error base %d event base %d\n",
                  the_display->xfixes_error_base, 
                  the_display->xfixes_event_base);
  }

  {
    int major = 2, minor = 3;
    gboolean has_xi = FALSE;

    if (XQueryExtension (the_display->xdisplay,
                         "XInputExtension",
                         &the_display->xinput_opcode,
                         &the_display->xinput_error_base,
                         &the_display->xinput_event_base))
      {
        if (XIQueryVersion (the_display->xdisplay, &major, &minor) == Success)
          {
            int version = (major * 10) + minor;
            if (version >= 22)
              has_xi = TRUE;

#ifdef HAVE_XI23
            if (version >= 23)
              the_display->have_xinput_23 = TRUE;
#endif /* HAVE_XI23 */
          }
      }

    if (!has_xi)
      meta_fatal ("X server doesn't have the XInput extension, version 2.2 or newer\n");
  }

  {
    XcursorSetTheme (the_display->xdisplay, meta_prefs_get_cursor_theme ());
    XcursorSetDefaultSize (the_display->xdisplay, meta_prefs_get_cursor_size ());
  }

  /* Create the leader window here. Set its properties and
   * use the timestamp from one of the PropertyNotify events
   * that will follow.
   */
  {
    gulong data[1];
    XEvent event;

    /* We only care about the PropertyChangeMask in the next 30 or so lines of
     * code.  Note that gdk will at some point unset the PropertyChangeMask for
     * this window, so we can't rely on it still being set later.  See bug
     * 354213 for details.
     */
    the_display->leader_window =
      meta_create_offscreen_window (the_display->xdisplay,
                                    DefaultRootWindow (the_display->xdisplay),
                                    PropertyChangeMask);

    meta_prop_set_utf8_string_hint (the_display,
                                    the_display->leader_window,
                                    the_display->atom__NET_WM_NAME,
                                    net_wm_name);

    meta_prop_set_utf8_string_hint (the_display,
                                    the_display->leader_window,
                                    the_display->atom__GNOME_WM_KEYBINDINGS,
                                    gnome_wm_keybindings);
    
    meta_prop_set_utf8_string_hint (the_display,
                                    the_display->leader_window,
                                    the_display->atom__MUTTER_VERSION,
                                    VERSION);

    data[0] = the_display->leader_window;
    XChangeProperty (the_display->xdisplay,
                     the_display->leader_window,
                     the_display->atom__NET_SUPPORTING_WM_CHECK,
                     XA_WINDOW,
                     32, PropModeReplace, (guchar*) data, 1);

    XWindowEvent (the_display->xdisplay,
                  the_display->leader_window,
                  PropertyChangeMask,
                  &event);

    timestamp = event.xproperty.time;

    /* Make it painfully clear that we can't rely on PropertyNotify events on
     * this window, as per bug 354213.
     */
    XSelectInput(the_display->xdisplay,
                 the_display->leader_window,
                 NoEventMask);
  }

  /* Make a little window used only for pinging the server for timestamps; note
   * that meta_create_offscreen_window already selects for PropertyChangeMask.
   */
  the_display->timestamp_pinging_window =
    meta_create_offscreen_window (the_display->xdisplay,
                                  DefaultRootWindow (the_display->xdisplay),
                                  PropertyChangeMask);

  the_display->last_focus_time = timestamp;
  the_display->last_user_time = timestamp;
  the_display->compositor = NULL;

  /* Mutter used to manage all X screens of the display in a single process, but
   * now it always manages exactly one screen as specified by the DISPLAY
   * environment variable.
   */
  i = meta_ui_get_screen_number ();
  screen = meta_screen_new (the_display, i, timestamp);

  if (!screen)
    {
      /* This would typically happen because all the screens already
       * have window managers.
       */
      meta_display_close (the_display, timestamp);
      return FALSE;
    }

  the_display->screen = screen;

  enable_compositor (the_display);

  meta_screen_create_guard_window (screen);

  /* We know that if mutter is running as a Wayland compositor,
   * we start out with no windows.
   */
  if (!meta_is_wayland_compositor ())
    meta_screen_manage_all_windows (screen);

  {
    Window focus;
    int ret_to;

    /* kinda bogus because GetInputFocus has no possible errors */
    meta_error_trap_push (the_display);

    /* FIXME: This is totally broken; see comment 9 of bug 88194 about this */
    focus = None;
    ret_to = RevertToPointerRoot;
    XGetInputFocus (the_display->xdisplay, &focus, &ret_to);

    /* Force a new FocusIn (does this work?) */

    /* Use the same timestamp that was passed to meta_screen_new(),
     * as it is the most recent timestamp.
     */
    if (focus == None || focus == PointerRoot)
      /* Just focus the no_focus_window on the first screen */
      meta_display_focus_the_no_focus_window (the_display,
                                              the_display->screen,
                                              timestamp);
    else
      {
        MetaWindow * window;
        window  = meta_display_lookup_x_window (the_display, focus);
        if (window)
          meta_display_set_input_focus_window (the_display, window, FALSE, timestamp);
        else
          /* Just focus the no_focus_window on the first screen */
          meta_display_focus_the_no_focus_window (the_display,
                                                  the_display->screen,
                                                  timestamp);
      }

    meta_error_trap_pop (the_display);
  }

  meta_idle_monitor_init_dbus ();

  /* Done opening new display */
  the_display->display_opening = FALSE;

  return TRUE;
}

static gint
ptrcmp (gconstpointer a, gconstpointer b)
{
  if (a < b)
    return -1;
  else if (a > b)
    return 1;
  else
    return 0;
}

/**
 * meta_display_list_windows:
 * @display: a #MetaDisplay
 * @flags: options for listing
 *
 * Lists windows for the display, the @flags parameter for
 * now determines whether override-redirect windows will be
 * included.
 *
 * Return value: (transfer container): the list of windows.
 */
GSList*
meta_display_list_windows (MetaDisplay          *display,
                           MetaListWindowsFlags  flags)
{
  GSList *winlist;
  GSList *prev;
  GSList *tmp;
  GHashTableIter iter;
  gpointer key, value;

  winlist = NULL;

  g_hash_table_iter_init (&iter, display->xids);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      MetaWindow *window = value;

      if (!META_IS_WINDOW (window))
        continue;

      if (!window->override_redirect ||
          (flags & META_LIST_INCLUDE_OVERRIDE_REDIRECT) != 0)
        winlist = g_slist_prepend (winlist, window);
    }

  g_hash_table_iter_init (&iter, display->wayland_windows);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      MetaWindow *window = value;

      if (!META_IS_WINDOW (window))
        continue;

      if (!window->override_redirect ||
          (flags & META_LIST_INCLUDE_OVERRIDE_REDIRECT) != 0)
        winlist = g_slist_prepend (winlist, window);
    }

  /* Uniquify the list, since both frame windows and plain
   * windows are in the hash
   */
  winlist = g_slist_sort (winlist, ptrcmp);

  prev = NULL;
  tmp = winlist;
  while (tmp != NULL)
    {
      GSList *next;

      next = tmp->next;
      
      if (next &&
          next->data == tmp->data)
        {
          /* Delete tmp from list */

          if (prev)
            prev->next = next;

          if (tmp == winlist)
            winlist = next;
          
          g_slist_free_1 (tmp);

          /* leave prev unchanged */
        }
      else
        {
          prev = tmp;
        }
      
      tmp = next;
    }

  return winlist;
}

void
meta_display_close (MetaDisplay *display,
                    guint32      timestamp)
{
  g_assert (display != NULL);

  if (display->closing != 0)
    {
      /* The display's already been closed. */
      return;
    }

  display->closing += 1;

  meta_prefs_remove_listener (prefs_changed_callback, display);
  
  meta_display_remove_autoraise_callback (display);

  if (display->focus_timeout_id)
    g_source_remove (display->focus_timeout_id);
  display->focus_timeout_id = 0;

  if (display->grab_old_window_stacking)
    g_list_free (display->grab_old_window_stacking);

  /* Stop caring about events */
  meta_display_free_events (display);
  
  meta_screen_free (display->screen, timestamp);

#ifdef HAVE_STARTUP_NOTIFICATION
  if (display->sn_display)
    {
      sn_display_unref (display->sn_display);
      display->sn_display = NULL;
    }
#endif
  
  /* Must be after all calls to meta_window_unmanage() since they
   * unregister windows
   */
  g_hash_table_destroy (display->xids);

  if (display->leader_window != None)
    XDestroyWindow (display->xdisplay, display->leader_window);

  XFlush (display->xdisplay);

  meta_display_free_window_prop_hooks (display);
  meta_display_free_group_prop_hooks (display);
  
  g_free (display->name);

  meta_display_shutdown_keys (display);

  if (display->compositor)
    meta_compositor_destroy (display->compositor);
  
  g_object_unref (display);
  the_display = NULL;

  meta_quit (META_EXIT_SUCCESS);
}

/* Grab/ungrab routines taken from fvwm.
 * Calling this function will cause X to ignore all other clients until
 * you ungrab. This may not be quite as bad as it sounds, yet there is
 * agreement that avoiding server grabs except when they are clearly needed
 * is a good thing.
 *
 * If you do use such grabs, please clearly explain the necessity for their
 * usage in a comment. Try to keep their scope extremely limited. In
 * particular, try to avoid emitting any signals or notifications while
 * a grab is active (if the signal receiver tries to block on an X request
 * from another client at this point, you will have a deadlock).
 */
void
meta_display_grab (MetaDisplay *display)
{
  if (display->server_grab_count == 0)
    {
      XGrabServer (display->xdisplay);
    }
  display->server_grab_count += 1;
  meta_verbose ("Grabbing display, grab count now %d\n",
                display->server_grab_count);
}

void
meta_display_ungrab (MetaDisplay *display)
{
  if (display->server_grab_count == 0)
    meta_bug ("Ungrabbed non-grabbed server\n");
  
  display->server_grab_count -= 1;
  if (display->server_grab_count == 0)
    {
      /* FIXME we want to purge all pending "queued" stuff
       * at this point, such as window hide/show
       */
      XUngrabServer (display->xdisplay);
      XFlush (display->xdisplay);
    }

  meta_verbose ("Ungrabbing display, grab count now %d\n",
                display->server_grab_count);
}

/**
 * meta_display_for_x_display:
 * @xdisplay: An X display
 *
 * Returns the singleton MetaDisplay if @xdisplay matches the X display it's
 * managing; otherwise gives a warning and returns %NULL.  When we were claiming
 * to be able to manage multiple displays, this was supposed to find the
 * display out of the list which matched that display.  Now it's merely an
 * extra sanity check.
 *
 * Returns: The singleton X display, or %NULL if @xdisplay isn't the one
 *          we're managing.
 */
MetaDisplay*
meta_display_for_x_display (Display *xdisplay)
{
  if (the_display->xdisplay == xdisplay)
    return the_display;

  meta_warning ("Could not find display for X display %p, probably going to crash\n",
                xdisplay);
  
  return NULL;
}

/**
 * meta_get_display:
 *
 * Accessor for the singleton MetaDisplay.
 *
 * Returns: The only #MetaDisplay there is.  This can be %NULL, but only
 *          during startup.
 */
MetaDisplay*
meta_get_display (void)
{
  return the_display;
}

static gboolean
grab_op_is_mouse_only (MetaGrabOp op)
{
  switch (op)
    {
    case META_GRAB_OP_MOVING:
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_S:      
    case META_GRAB_OP_RESIZING_SW:      
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_RESIZING_W:
    case META_GRAB_OP_RESIZING_E:
      return TRUE;

    default:
      return FALSE;
    }
}

gboolean
meta_grab_op_is_mouse (MetaGrabOp op)
{
  switch (op)
    {
    case META_GRAB_OP_MOVING:
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_S:      
    case META_GRAB_OP_RESIZING_SW:      
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_RESIZING_W:
    case META_GRAB_OP_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
    case META_GRAB_OP_KEYBOARD_MOVING:
    case META_GRAB_OP_COMPOSITOR:
      return TRUE;

    default:
      return FALSE;
    }
}

gboolean
meta_grab_op_is_keyboard (MetaGrabOp op)
{
  switch (op)
    {
    case META_GRAB_OP_KEYBOARD_MOVING:
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
    case META_GRAB_OP_COMPOSITOR:
      return TRUE;

    default:
      return FALSE;
    }
}

gboolean
meta_grab_op_is_resizing (MetaGrabOp op)
{
  switch (op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_RESIZING_S:      
    case META_GRAB_OP_RESIZING_SW:      
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_RESIZING_W:
    case META_GRAB_OP_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
      return TRUE;

    default:
      return FALSE;
    }
}

gboolean
meta_grab_op_is_moving (MetaGrabOp op)
{
  switch (op)
    {
    case META_GRAB_OP_MOVING:
    case META_GRAB_OP_KEYBOARD_MOVING:
      return TRUE;
      
    default:
      return FALSE;
    }
}

gboolean
meta_grab_op_is_clicking (MetaGrabOp grab_op)
{
  switch (grab_op)
    {
    case META_GRAB_OP_CLICKING_MINIMIZE:
    case META_GRAB_OP_CLICKING_MAXIMIZE:
    case META_GRAB_OP_CLICKING_UNMAXIMIZE:
    case META_GRAB_OP_CLICKING_DELETE:
    case META_GRAB_OP_CLICKING_MENU:
    case META_GRAB_OP_CLICKING_SHADE:
    case META_GRAB_OP_CLICKING_UNSHADE:
    case META_GRAB_OP_CLICKING_ABOVE:
    case META_GRAB_OP_CLICKING_UNABOVE:
    case META_GRAB_OP_CLICKING_STICK:
    case META_GRAB_OP_CLICKING_UNSTICK:
      return TRUE;

    default:
      return FALSE;
    }
}

/**
 * meta_grab_op_should_block_wayland:
 * @op: A #MetaGrabOp
 *
 * Starting a grab with one of these grab operations means
 * that we will remove key / pointer focus from the current
 * Wayland focus.
 */
gboolean
meta_grab_op_should_block_wayland (MetaGrabOp op)
{
  return (op != META_GRAB_OP_NONE && !meta_grab_op_is_clicking (op));
}

/**
 * meta_display_xserver_time_is_before:
 * @display: a #MetaDisplay
 * @time1: An event timestamp
 * @time2: An event timestamp
 *
 * Xserver time can wraparound, thus comparing two timestamps needs to take
 * this into account. If no wraparound has occurred, this is equivalent to
 *   time1 < time2
 * Otherwise, we need to account for the fact that wraparound can occur
 * and the fact that a timestamp of 0 must be special-cased since it
 * means "older than anything else".
 *
 * Note that this is NOT an equivalent for time1 <= time2; if that's what
 * you need then you'll need to swap the order of the arguments and negate
 * the result.
 */
gboolean
meta_display_xserver_time_is_before (MetaDisplay   *display,
                                     guint32        time1,
                                     guint32        time2)
{
  return XSERVER_TIME_IS_BEFORE(time1, time2);
}

/**
 * meta_display_get_last_user_time:
 * @display: a #MetaDisplay
 *
 * Returns: Timestamp of the last user interaction event with a window
 */
guint32
meta_display_get_last_user_time (MetaDisplay *display)
{
  return display->last_user_time;
}

/* Get time of current event, or CurrentTime if none. */
guint32
meta_display_get_current_time (MetaDisplay *display)
{
  return display->current_time;
}

static Bool
find_timestamp_predicate (Display  *xdisplay,
                          XEvent   *ev,
                          XPointer  arg)
{
  MetaDisplay *display = (MetaDisplay *) arg;

  return (ev->type == PropertyNotify &&
          ev->xproperty.atom == display->atom__MUTTER_TIMESTAMP_PING);
}

/* Get a timestamp, even if it means a roundtrip */
guint32
meta_display_get_current_time_roundtrip (MetaDisplay *display)
{
  guint32 timestamp;
  
  timestamp = meta_display_get_current_time (display);
  if (timestamp == CurrentTime)
    {
      XEvent property_event;

      XChangeProperty (display->xdisplay, display->timestamp_pinging_window,
                       display->atom__MUTTER_TIMESTAMP_PING,
                       XA_STRING, 8, PropModeAppend, NULL, 0);
      XIfEvent (display->xdisplay,
                &property_event,
                find_timestamp_predicate,
                (XPointer) display);
      timestamp = property_event.xproperty.time;
    }

  meta_display_sanity_check_timestamps (display, timestamp);

  return timestamp;
}

/**
 * meta_display_add_ignored_crossing_serial:
 * @display: a #MetaDisplay
 * @serial: the serial to ignore
 *
 * Save the specified serial and ignore crossing events with that
 * serial for the purpose of focus-follows-mouse. This can be used
 * for certain changes to the window hierarchy that we don't want
 * to change the focus window, even if they cause the pointer to
 * end up in a new window.
 */
void
meta_display_add_ignored_crossing_serial (MetaDisplay  *display,
                                          unsigned long serial)
{
  int i;

  /* don't add the same serial more than once */
  if (display->ignored_crossing_serials[N_IGNORED_CROSSING_SERIALS-1] == serial)
    return;
  
  /* shift serials to the left */
  i = 0;
  while (i < (N_IGNORED_CROSSING_SERIALS - 1))
    {
      display->ignored_crossing_serials[i] = display->ignored_crossing_serials[i+1];
      ++i;
    }
  /* put new one on the end */
  display->ignored_crossing_serials[i] = serial;
}

static gboolean 
window_raise_with_delay_callback (void *data)
{
  MetaWindow *window = data;

  window->display->autoraise_timeout_id = 0;
  window->display->autoraise_window = NULL;

  /* If we aren't already on top, check whether the pointer is inside
   * the window and raise the window if so.
   */      
  if (meta_stack_get_top (window->screen->stack) != window) 
    {
      int x, y, root_x, root_y;
      Window root, child;
      MetaRectangle frame_rect;
      unsigned int mask;
      gboolean same_screen;
      gboolean point_in_window;

      meta_error_trap_push (window->display);
      same_screen = XQueryPointer (window->display->xdisplay,
				   window->xwindow,
				   &root, &child,
				   &root_x, &root_y, &x, &y, &mask);
      meta_error_trap_pop (window->display);

      meta_window_get_frame_rect (window, &frame_rect);
      point_in_window = POINT_IN_RECT (root_x, root_y, frame_rect);
      if (same_screen && point_in_window)
	meta_window_raise (window);
      else
	meta_topic (META_DEBUG_FOCUS, 
		    "Pointer not inside window, not raising %s\n", 
		    window->desc);
    }

  return FALSE;
}

void
meta_display_queue_autoraise_callback (MetaDisplay *display,
                                       MetaWindow  *window)
{
  meta_topic (META_DEBUG_FOCUS, 
              "Queuing an autoraise timeout for %s with delay %d\n", 
              window->desc, 
              meta_prefs_get_auto_raise_delay ());
  
  if (display->autoraise_timeout_id != 0)
    g_source_remove (display->autoraise_timeout_id);

  display->autoraise_timeout_id = 
    g_timeout_add_full (G_PRIORITY_DEFAULT,
                        meta_prefs_get_auto_raise_delay (),
                        window_raise_with_delay_callback,
                        window, NULL);
  g_source_set_name_by_id (display->autoraise_timeout_id, "[mutter] window_raise_with_delay_callback");
  display->autoraise_window = window;
}

#if 0
static void
handle_net_restack_window (MetaDisplay* display,
                           XEvent *event)
{
  MetaWindow *window;

  window = meta_display_lookup_x_window (display,
                                         event->xclient.window);

  if (window)
    {
      /* FIXME: The EWMH includes a sibling for the restack request, but we
       * (stupidly) don't currently support these types of raises.
       *
       * Also, unconditionally following these is REALLY stupid--we should
       * combine this code with the stuff in
       * meta_window_x11_configure_request() which is smart about whether to
       * follow the request or do something else (though not smart enough
       * and is also too stupid to handle the sibling stuff).
       */
      switch (event->xclient.data.l[2])
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
#endif

void
meta_display_sync_wayland_input_focus (MetaDisplay *display)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaWindow *focus_window = NULL;

  if (meta_grab_op_should_block_wayland (display->grab_op))
    focus_window = NULL;
  else if (meta_display_xwindow_is_a_no_focus_window (display, display->focus_xwindow))
    focus_window = NULL;
  else if (display->focus_window && display->focus_window->surface)
    focus_window = display->focus_window;
  else
    meta_topic (META_DEBUG_FOCUS, "Focus change has no effect, because there is no matching wayland surface");

  meta_wayland_compositor_set_input_focus (compositor, focus_window);

  meta_wayland_seat_repick (compositor->seat, NULL);
}

void
meta_display_update_focus_window (MetaDisplay *display,
                                  MetaWindow  *window,
                                  Window       xwindow,
                                  gulong       serial,
                                  gboolean     focused_by_us)
{
  display->focus_serial = serial;
  display->focused_by_us = focused_by_us;

  if (display->focus_xwindow == xwindow &&
      display->focus_window == window)
    return;

  if (display->focus_window)
    {
      MetaWindow *previous;

      meta_topic (META_DEBUG_FOCUS,
                  "%s is now the previous focus window due to being focused out or unmapped\n",
                  display->focus_window->desc);

      /* Make sure that signals handlers invoked by
       * meta_window_set_focused_internal() don't see
       * display->focus_window->has_focus == FALSE
       */
      previous = display->focus_window;
      display->focus_window = NULL;
      display->focus_xwindow = None;

      meta_window_set_focused_internal (previous, FALSE);
    }

  display->focus_window = window;
  display->focus_xwindow = xwindow;

  if (display->focus_window)
    {
      meta_topic (META_DEBUG_FOCUS, "* Focus --> %s with serial %lu\n",
                  display->focus_window->desc, serial);
      meta_window_set_focused_internal (display->focus_window, TRUE);
    }
  else
    meta_topic (META_DEBUG_FOCUS, "* Focus --> NULL with serial %lu\n", serial);

  if (meta_is_wayland_compositor ())
    meta_display_sync_wayland_input_focus (display);

  g_object_notify (G_OBJECT (display), "focus-window");
  meta_display_update_active_window_hint (display);
}

gboolean
meta_display_timestamp_too_old (MetaDisplay *display,
                                guint32     *timestamp)
{
  /* FIXME: If Soeren's suggestion in bug 151984 is implemented, it will allow
   * us to sanity check the timestamp here and ensure it doesn't correspond to
   * a future time (though we would want to rename to
   * timestamp_too_old_or_in_future).
   */

  if (*timestamp == CurrentTime)
    {
      *timestamp = meta_display_get_current_time_roundtrip (display);
      return FALSE;
    }
  else if (XSERVER_TIME_IS_BEFORE (*timestamp, display->last_focus_time))
    {
      if (XSERVER_TIME_IS_BEFORE (*timestamp, display->last_user_time))
        return TRUE;
      else
        {
          *timestamp = display->last_focus_time;
          return FALSE;
        }
    }

  return FALSE;
}

static void
request_xserver_input_focus_change (MetaDisplay *display,
                                    MetaScreen  *screen,
                                    MetaWindow  *meta_window,
                                    Window       xwindow,
                                    guint32      timestamp)
{
  gulong serial;

  if (meta_display_timestamp_too_old (display, &timestamp))
    return;

  meta_error_trap_push (display);

  /* In order for mutter to know that the focus request succeeded, we track
   * the serial of the "focus request" we made, but if we take the serial
   * of the XSetInputFocus request, then there's no way to determine the
   * difference between focus events as a result of the SetInputFocus and
   * focus events that other clients send around the same time. Ensure that
   * we know which is which by making two requests that the server will
   * process at the same time.
   */
  meta_display_grab (display);

  serial = XNextRequest (display->xdisplay);

  XSetInputFocus (display->xdisplay,
                  xwindow,
                  RevertToPointerRoot,
                  timestamp);

  XChangeProperty (display->xdisplay, display->timestamp_pinging_window,
                   display->atom__MUTTER_FOCUS_SET,
                   XA_STRING, 8, PropModeAppend, NULL, 0);

  meta_display_ungrab (display);

  meta_display_update_focus_window (display,
                                    meta_window,
                                    xwindow,
                                    serial,
                                    TRUE);

  meta_error_trap_pop (display);

  display->last_focus_time = timestamp;

  if (meta_window == NULL || meta_window != display->autoraise_window)
    meta_display_remove_autoraise_callback (display);
}

MetaWindow*
meta_display_lookup_x_window (MetaDisplay *display,
                              Window       xwindow)
{
  return g_hash_table_lookup (display->xids, &xwindow);
}

void
meta_display_register_x_window (MetaDisplay *display,
                                Window      *xwindowp,
                                MetaWindow  *window)
{
  g_return_if_fail (g_hash_table_lookup (display->xids, xwindowp) == NULL);
  
  g_hash_table_insert (display->xids, xwindowp, window);
}

void
meta_display_unregister_x_window (MetaDisplay *display,
                                  Window       xwindow)
{
  g_return_if_fail (g_hash_table_lookup (display->xids, &xwindow) != NULL);

  g_hash_table_remove (display->xids, &xwindow);

  /* Remove any pending pings */
  remove_pending_pings_for_window (display, xwindow);
}

void
meta_display_register_wayland_window (MetaDisplay *display,
                                      MetaWindow  *window)
{
  g_hash_table_add (display->wayland_windows, window);
}

void
meta_display_unregister_wayland_window (MetaDisplay *display,
                                        MetaWindow  *window)
{
  g_hash_table_remove (display->wayland_windows, window);
}

#ifdef HAVE_XSYNC
/* We store sync alarms in the window ID hash table, because they are
 * just more types of XIDs in the same global space, but we have
 * typesafe functions to register/unregister for readability.
 */

MetaWindow*
meta_display_lookup_sync_alarm (MetaDisplay *display,
                                XSyncAlarm   alarm)
{
  return g_hash_table_lookup (display->xids, &alarm);
}

void
meta_display_register_sync_alarm (MetaDisplay *display,
                                  XSyncAlarm  *alarmp,
                                  MetaWindow  *window)
{
  g_return_if_fail (g_hash_table_lookup (display->xids, alarmp) == NULL);

  g_hash_table_insert (display->xids, alarmp, window);
}

void
meta_display_unregister_sync_alarm (MetaDisplay *display,
                                    XSyncAlarm   alarm)
{
  g_return_if_fail (g_hash_table_lookup (display->xids, &alarm) != NULL);

  g_hash_table_remove (display->xids, &alarm);
}
#endif /* HAVE_XSYNC */

void
meta_display_notify_window_created (MetaDisplay  *display,
                                    MetaWindow   *window)
{
  g_signal_emit (display, display_signals[WINDOW_CREATED], 0, window);
}

/**
 * meta_display_xwindow_is_a_no_focus_window:
 * @display: A #MetaDisplay
 * @xwindow: An X11 window
 *
 * Returns: %TRUE iff window is one of mutter's internal "no focus" windows
 * (there is one per screen) which will have the focus when there is no
 * actual client window focused.
 */
gboolean
meta_display_xwindow_is_a_no_focus_window (MetaDisplay *display,
                                           Window xwindow)
{
  return xwindow == display->screen->no_focus_window;
}

static MetaCursor
meta_cursor_for_grab_op (MetaGrabOp op)
{
  switch (op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
      return META_CURSOR_SE_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
      return META_CURSOR_SOUTH_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
      return META_CURSOR_SW_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
      return META_CURSOR_NORTH_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
      return META_CURSOR_NE_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
      return META_CURSOR_NW_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
      return META_CURSOR_WEST_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
      return META_CURSOR_EAST_RESIZE;
      break;
    case META_GRAB_OP_MOVING:
    case META_GRAB_OP_KEYBOARD_MOVING:
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
      return META_CURSOR_MOVE_OR_RESIZE_WINDOW;
      break;
    default:
      break;
    }

  return META_CURSOR_DEFAULT;
}

void
meta_display_set_grab_op_cursor (MetaDisplay *display,
                                 MetaScreen  *screen,
                                 MetaGrabOp   op,
                                 Window       grab_xwindow,
                                 guint32      timestamp)
{
  unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
  XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };
  MetaCursor cursor = meta_cursor_for_grab_op (op);
  MetaCursorReference *cursor_ref;

  XISetMask (mask.mask, XI_ButtonPress);
  XISetMask (mask.mask, XI_ButtonRelease);
  XISetMask (mask.mask, XI_Enter);
  XISetMask (mask.mask, XI_Leave);
  XISetMask (mask.mask, XI_Motion);

  g_assert (screen != NULL);

  meta_error_trap_push (display);
  if (XIGrabDevice (display->xdisplay,
                    META_VIRTUAL_CORE_POINTER_ID,
                    grab_xwindow,
                    timestamp,
                    meta_display_create_x_cursor (display, cursor),
                    XIGrabModeAsync, XIGrabModeAsync,
                    False, /* owner_events */
                    &mask) == Success)
    {
      display->grab_have_pointer = TRUE;
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "XIGrabDevice() returned GrabSuccess time %u\n",
                  timestamp);
    }
  else
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "XIGrabDevice() failed time %u\n",
                  timestamp);
    }

  meta_error_trap_pop (display);

  cursor_ref = meta_cursor_reference_from_theme (screen->cursor_tracker, cursor);
  meta_cursor_tracker_set_grab_cursor (screen->cursor_tracker, cursor_ref);
  meta_cursor_reference_unref (cursor_ref);
}

gboolean
meta_display_begin_grab_op (MetaDisplay *display,
			    MetaScreen  *screen,
                            MetaWindow  *window,
                            MetaGrabOp   op,
                            gboolean     pointer_already_grabbed,
                            gboolean     frame_action,
                            int          button,
                            gulong       modmask, /* XXX - ignored */
                            guint32      timestamp,
                            int          root_x,
                            int          root_y)
{
  MetaWindow *grab_window = NULL;
  Window grab_xwindow;
  
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Doing grab op %u on window %s button %d pointer already grabbed: %d pointer pos %d,%d\n",
              op, window ? window->desc : "none", button, pointer_already_grabbed,
              root_x, root_y);
  
  if (display->grab_op != META_GRAB_OP_NONE)
    {
      if (window)
        meta_warning ("Attempt to perform window operation %u on window %s when operation %u on %s already in effect\n",
                      op, window->desc, display->grab_op,
                      display->grab_window ? display->grab_window->desc : "none");
      return FALSE;
    }

  if (window &&
      (meta_grab_op_is_moving (op) || meta_grab_op_is_resizing (op)))
    {
      if (meta_prefs_get_raise_on_click ())
        meta_window_raise (window);
      else
        {
          display->grab_initial_x = root_x;
          display->grab_initial_y = root_y;
          display->grab_threshold_movement_reached = FALSE;
        }
    }

  /* If window is a modal dialog attached to its parent,
   * grab the parent instead for moving.
   */
  if (window && meta_window_is_attached_dialog (window) &&
      meta_grab_op_is_moving (op))
    grab_window = meta_window_get_transient_for (window);

  if (grab_window == NULL)
    grab_window = window;

  /* FIXME:
   *   If we have no MetaWindow we do our best
   *   and try to do the grab on the RootWindow.
   *   This will fail if anyone else has any
   *   key grab on the RootWindow.
   */
  if (grab_window)
    grab_xwindow = meta_window_get_toplevel_xwindow (grab_window);
  else
    grab_xwindow = screen->xroot;

  display->grab_have_pointer = FALSE;
  
  if (pointer_already_grabbed)
    display->grab_have_pointer = TRUE;
  
  meta_display_set_grab_op_cursor (display, screen, op, grab_xwindow, timestamp);

  if (!display->grab_have_pointer && !meta_grab_op_is_keyboard (op))
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "XIGrabDevice() failed\n");
      return FALSE;
    }

  /* Grab keys for keyboard ops and mouse move/resizes; see #126497 */
  if (meta_grab_op_is_keyboard (op) || grab_op_is_mouse_only (op))
    {
      if (grab_window)
        display->grab_have_keyboard =
                     meta_window_grab_all_keys (grab_window, timestamp);

      else
        display->grab_have_keyboard =
                     meta_screen_grab_all_keys (screen, timestamp);
      
      if (!display->grab_have_keyboard)
        {
          meta_topic (META_DEBUG_WINDOW_OPS,
                      "grabbing all keys failed, ungrabbing pointer\n");
          XIUngrabDevice (display->xdisplay, META_VIRTUAL_CORE_POINTER_ID, timestamp);
          display->grab_have_pointer = FALSE;
          return FALSE;
        }
    }
  
  display->grab_op = op;
  display->grab_window = grab_window;
  display->grab_xwindow = grab_xwindow;
  display->grab_button = button;
  if (window)
    {
      display->grab_tile_mode = window->tile_mode;
      display->grab_tile_monitor_number = window->tile_monitor_number;
    }
  else
    {
      display->grab_tile_mode = META_TILE_NONE;
      display->grab_tile_monitor_number = -1;
    }
  display->grab_anchor_root_x = root_x;
  display->grab_anchor_root_y = root_y;
  display->grab_latest_motion_x = root_x;
  display->grab_latest_motion_y = root_y;
  display->grab_last_moveresize_time.tv_sec = 0;
  display->grab_last_moveresize_time.tv_usec = 0;
  display->grab_old_window_stacking = NULL;
#ifdef HAVE_XSYNC
  display->grab_last_user_action_was_snap = FALSE;
#endif
  display->grab_frame_action = frame_action;
  display->grab_resize_unmaximize = 0;
  display->grab_timestamp = timestamp;

  if (display->grab_resize_timeout_id)
    {
      g_source_remove (display->grab_resize_timeout_id);
      display->grab_resize_timeout_id = 0;
    }
	
  if (display->grab_window)
    {
      meta_window_get_client_root_coords (display->grab_window,
                                          &display->grab_initial_window_pos);
      display->grab_anchor_window_pos = display->grab_initial_window_pos;

#ifdef HAVE_XSYNC
      if ( meta_grab_op_is_resizing (display->grab_op) &&
           display->grab_window->sync_request_counter != None)
        {
          meta_window_create_sync_request_alarm (display->grab_window);
        }
#endif
    }
  
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Grab op %u on window %s successful\n",
              display->grab_op, window ? window->desc : "(null)");

  g_assert (display->grab_window != NULL);
  g_assert (display->grab_op != META_GRAB_OP_NONE);

  if (display->grab_window)
    {
      meta_window_refresh_resize_popup (display->grab_window);
    }

  if (meta_is_wayland_compositor ())
    meta_display_sync_wayland_input_focus (display);

  g_signal_emit (display, display_signals[GRAB_OP_BEGIN], 0,
                 screen, display->grab_window, display->grab_op);
  
  return TRUE;
}

void
meta_display_end_grab_op (MetaDisplay *display,
                          guint32      timestamp)
{
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Ending grab op %u at time %u\n", display->grab_op, timestamp);
  
  if (display->grab_op == META_GRAB_OP_NONE)
    return;

  g_signal_emit (display, display_signals[GRAB_OP_END], 0,
                 display->screen, display->grab_window, display->grab_op);

  if (display->grab_window != NULL)
    display->grab_window->shaken_loose = FALSE;
  
  if (display->grab_window != NULL &&
      !meta_prefs_get_raise_on_click () &&
      (meta_grab_op_is_moving (display->grab_op) ||
       meta_grab_op_is_resizing (display->grab_op)))
    {
      /* Only raise the window in orthogonal raise
       * ('do-not-raise-on-click') mode if the user didn't try to move
       * or resize the given window by at least a threshold amount.
       * For raise on click mode, the window was raised at the
       * beginning of the grab_op.
       */
      if (!display->grab_threshold_movement_reached)
        meta_window_raise (display->grab_window);
    }
  
  /* If this was a move or resize clear out the edge cache */
  if (meta_grab_op_is_resizing (display->grab_op) || 
      meta_grab_op_is_moving (display->grab_op))
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Clearing out the edges for resistance/snapping");
      meta_display_cleanup_edges (display);
    }

  if (display->grab_old_window_stacking != NULL)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Clearing out the old stack position, which was %p.\n",
                  display->grab_old_window_stacking);
      g_list_free (display->grab_old_window_stacking);
      display->grab_old_window_stacking = NULL;
    }

  if (display->grab_have_pointer)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Ungrabbing pointer with timestamp %u\n", timestamp);
      XIUngrabDevice (display->xdisplay, META_VIRTUAL_CORE_POINTER_ID, timestamp);
    }

  if (display->grab_have_keyboard)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Ungrabbing all keys timestamp %u\n", timestamp);
      if (display->grab_window)
        meta_window_ungrab_all_keys (display->grab_window, timestamp);
      else
        meta_screen_ungrab_all_keys (display->screen, timestamp);
    }

  meta_cursor_tracker_set_grab_cursor (display->screen->cursor_tracker, NULL);

  display->grab_timestamp = 0;
  display->grab_window = NULL;
  display->grab_xwindow = None;
  display->grab_tile_mode = META_TILE_NONE;
  display->grab_tile_monitor_number = -1;
  display->grab_op = META_GRAB_OP_NONE;

  if (display->grab_resize_popup)
    {
      meta_ui_resize_popup_free (display->grab_resize_popup);
      display->grab_resize_popup = NULL;
    }

  if (display->grab_resize_timeout_id)
    {
      g_source_remove (display->grab_resize_timeout_id);
      display->grab_resize_timeout_id = 0;
    }

  if (meta_is_wayland_compositor ())
    meta_display_sync_wayland_input_focus (display);
}

/**
 * meta_display_get_grab_op:
 * @display: The #MetaDisplay that the window is on

 * Gets the current grab operation, if any.
 *
 * Return value: the current grab operation, or %META_GRAB_OP_NONE if
 * Mutter doesn't currently have a grab. %META_GRAB_OP_COMPOSITOR will
 * be returned if a compositor-plugin modal operation is in effect
 * (See mutter_begin_modal_for_plugin())
 */
MetaGrabOp
meta_display_get_grab_op (MetaDisplay *display)
{
  return display->grab_op;
}

void
meta_display_check_threshold_reached (MetaDisplay *display,
                                      int          x,
                                      int          y)
{
  /* Don't bother doing the check again if we've already reached the threshold */
  if (meta_prefs_get_raise_on_click () ||
      display->grab_threshold_movement_reached)
    return;

  if (ABS (display->grab_initial_x - x) >= 8 ||
      ABS (display->grab_initial_y - y) >= 8)
    display->grab_threshold_movement_reached = TRUE;
}

static void
meta_change_button_grab (MetaDisplay *display,
                         Window       xwindow,
                         gboolean     grab,
                         gboolean     sync,
                         int          button,
                         int          modmask)
{
  unsigned int ignored_mask;

  unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
  XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };

  XISetMask (mask.mask, XI_ButtonPress);
  XISetMask (mask.mask, XI_ButtonRelease);
  XISetMask (mask.mask, XI_Motion);

  meta_verbose ("%s 0x%lx sync = %d button = %d modmask 0x%x\n",
                grab ? "Grabbing" : "Ungrabbing",
                xwindow,
                sync, button, modmask);
  
  meta_error_trap_push (display);
  
  ignored_mask = 0;
  while (ignored_mask <= display->ignored_modifier_mask)
    {
      XIGrabModifiers mods;

      if (ignored_mask & ~(display->ignored_modifier_mask))
        {
          /* Not a combination of ignored modifiers
           * (it contains some non-ignored modifiers)
           */
          ++ignored_mask;
          continue;
        }

      mods = (XIGrabModifiers) { modmask | ignored_mask, 0 };

      if (meta_is_debugging ())
        meta_error_trap_push (display);

      /* GrabModeSync means freeze until XAllowEvents */
      
      if (grab)
        XIGrabButton (display->xdisplay,
                      META_VIRTUAL_CORE_POINTER_ID,
                      button, xwindow, None,
                      sync ? XIGrabModeSync : XIGrabModeAsync,
                      XIGrabModeAsync, False,
                      &mask, 1, &mods);
      else
        XIUngrabButton (display->xdisplay,
                        META_VIRTUAL_CORE_POINTER_ID,
                        button, xwindow, 1, &mods);

      if (meta_is_debugging ())
        {
          int result;
          
          result = meta_error_trap_pop_with_return (display);
          
          if (result != Success)
            meta_verbose ("Failed to %s button %d with mask 0x%x for window 0x%lx error code %d\n",
                          grab ? "grab" : "ungrab",
                          button, modmask | ignored_mask, xwindow, result);
        }
      
      ++ignored_mask;
    }

  meta_error_trap_pop (display);
}

void
meta_display_grab_window_buttons (MetaDisplay *display,
                                  Window       xwindow)
{  
  /* Grab Alt + button1 for moving window.
   * Grab Alt + button2 for resizing window.
   * Grab Alt + button3 for popping up window menu.
   * Grab Alt + Shift + button1 for snap-moving window.
   */
  meta_verbose ("Grabbing window buttons for 0x%lx\n", xwindow);
  
  /* FIXME If we ignored errors here instead of spewing, we could
   * put one big error trap around the loop and avoid a bunch of
   * XSync()
   */

  if (display->window_grab_modifiers != 0)
    {
      gboolean debug = g_getenv ("MUTTER_DEBUG_BUTTON_GRABS") != NULL;
      int i;
      for (i = 1; i < 4; i++)
        {
          meta_change_button_grab (display, xwindow,
                                   TRUE,
                                   FALSE,
                                   i, display->window_grab_modifiers);  
          
          /* This is for debugging, since I end up moving the Xnest
           * otherwise ;-)
           */
          if (debug)
            meta_change_button_grab (display, xwindow,
                                     TRUE,
                                     FALSE,
                                     i, ControlMask);
        }

      /* In addition to grabbing Alt+Button1 for moving the window,
       * grab Alt+Shift+Button1 for snap-moving the window.  See bug
       * 112478.  Unfortunately, this doesn't work with
       * Shift+Alt+Button1 for some reason; so at least part of the
       * order still matters, which sucks (please FIXME).
       */
      meta_change_button_grab (display, xwindow,
                               TRUE,
                               FALSE,
                               1, display->window_grab_modifiers | ShiftMask);
    }
}

void
meta_display_ungrab_window_buttons  (MetaDisplay *display,
                                     Window       xwindow)
{
  gboolean debug;
  int i;

  if (display->window_grab_modifiers == 0)
    return;
  
  debug = g_getenv ("MUTTER_DEBUG_BUTTON_GRABS") != NULL;
  i = 1;
  while (i < 4)
    {
      meta_change_button_grab (display, xwindow,
                               FALSE, FALSE, i,
                               display->window_grab_modifiers);
      
      if (debug)
        meta_change_button_grab (display, xwindow,
                                 FALSE, FALSE, i, ControlMask);
      
      ++i;
    }
}

/* Grab buttons we only grab while unfocused in click-to-focus mode */
#define MAX_FOCUS_BUTTON 4
void
meta_display_grab_focus_window_button (MetaDisplay *display,
                                       MetaWindow  *window)
{
  /* Grab button 1 for activating unfocused windows */
  meta_verbose ("Grabbing unfocused window buttons for %s\n", window->desc);

#if 0
  /* FIXME:115072 */
  /* Don't grab at all unless in click to focus mode. In click to
   * focus, we may sometimes be clever about intercepting and eating
   * the focus click. But in mouse focus, we never do that since the
   * focus window may not be raised, and who wants to think about
   * mouse focus anyway.
   */
  if (meta_prefs_get_focus_mode () != G_DESKTOP_FOCUS_MODE_CLICK)
    {
      meta_verbose (" (well, not grabbing since not in click to focus mode)\n");
      return;
    }
#endif
  
  if (window->have_focus_click_grab)
    {
      meta_verbose (" (well, not grabbing since we already have the grab)\n");
      return;
    }
  
  /* FIXME If we ignored errors here instead of spewing, we could
   * put one big error trap around the loop and avoid a bunch of
   * XSync()
   */
  
  {
    int i = 1;
    while (i < MAX_FOCUS_BUTTON)
      {
        meta_change_button_grab (display,
                                 window->xwindow,
                                 TRUE, TRUE,
                                 i, 0);
        
        ++i;
      }

    window->have_focus_click_grab = TRUE;
  }
}

void
meta_display_ungrab_focus_window_button (MetaDisplay *display,
                                         MetaWindow  *window)
{
  meta_verbose ("Ungrabbing unfocused window buttons for %s\n", window->desc);

  if (!window->have_focus_click_grab)
    return;
  
  {
    int i = 1;
    while (i < MAX_FOCUS_BUTTON)
      {
        meta_change_button_grab (display, window->xwindow,
                                 FALSE, FALSE, i, 0);
        
        ++i;
      }

    window->have_focus_click_grab = FALSE;
  }
}

void
meta_display_increment_event_serial (MetaDisplay *display)
{
  /* We just make some random X request */
  XDeleteProperty (display->xdisplay, display->leader_window,
                   display->atom__MOTIF_WM_HINTS);
}

void
meta_display_update_active_window_hint (MetaDisplay *display)
{
  gulong data[1];

  if (display->focus_window)
    data[0] = display->focus_window->xwindow;
  else
    data[0] = None;

  meta_error_trap_push (display);
  XChangeProperty (display->xdisplay, display->screen->xroot,
                   display->atom__NET_ACTIVE_WINDOW,
                   XA_WINDOW,
                   32, PropModeReplace, (guchar*) data, 1);
  meta_error_trap_pop (display);
}

void
meta_display_queue_retheme_all_windows (MetaDisplay *display)
{
  GSList* windows;
  GSList *tmp;

  windows = meta_display_list_windows (display, META_LIST_DEFAULT);
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *window = tmp->data;
      
      meta_window_queue (window, META_QUEUE_MOVE_RESIZE);
      meta_window_frame_size_changed (window);
      if (window->frame)
        {
          meta_frame_queue_draw (window->frame);
        }
      
      tmp = tmp->next;
    }

  g_slist_free (windows);
}

void
meta_display_retheme_all (void)
{
  meta_display_queue_retheme_all_windows (meta_get_display ());
}

void 
meta_display_set_cursor_theme (const char *theme, 
			       int         size)
{
  MetaDisplay *display = meta_get_display ();

  XcursorSetTheme (display->xdisplay, theme);
  XcursorSetDefaultSize (display->xdisplay, size);

  meta_screen_update_cursor (display->screen);
}

/*
 * Stores whether syncing is currently enabled.
 */
static gboolean is_syncing = FALSE;

/**
 * meta_is_syncing:
 *
 * Returns whether X synchronisation is currently enabled.
 *
 * FIXME: This is *only* called by meta_display_open(), but by that time
 * we have already turned syncing on or off on startup, and we don't
 * have any way to do so while Mutter is running, so it's rather
 * pointless.
 *
 * Returns: %TRUE if we must wait for events whenever we send X requests;
 * %FALSE otherwise.
 */
gboolean
meta_is_syncing (void)
{
  return is_syncing;
}

/**
 * meta_set_syncing:
 * @setting: whether to turn syncing on or off
 *
 * A handy way to turn on synchronisation on or off for every display.
 */
void
meta_set_syncing (gboolean setting)
{
  if (setting != is_syncing)
    {
      is_syncing = setting;
      if (meta_get_display ())
        XSynchronize (meta_get_display ()->xdisplay, is_syncing);
    }
}

/*
 * How long, in milliseconds, we should wait after pinging a window
 * before deciding it's not going to get back to us.
 */
#define PING_TIMEOUT_DELAY 5000

/**
 * meta_display_ping_timeout:
 * @data: All the information about this ping. It is a #MetaPingData
 *        cast to a #gpointer in order to be passable to a timeout function.
 *        This function will also free this parameter.
 *
 * Does whatever it is we decided to do when a window didn't respond
 * to a ping. We also remove the ping from the display's list of
 * pending pings. This function is called by the event loop when the timeout
 * times out which we created at the start of the ping.
 *
 * Returns: Always returns %FALSE, because this function is called as a
 *          timeout and we don't want to run the timer again.
 */
static gboolean
meta_display_ping_timeout (gpointer data)
{
  MetaPingData *ping_data = data;
  MetaDisplay *display = ping_data->window->display;

  ping_data->ping_timeout_id = 0;

  meta_topic (META_DEBUG_PING,
              "Ping %u on window %s timed out\n",
              ping_data->timestamp, ping_data->window->desc);

  (* ping_data->ping_timeout_func) (ping_data->window, ping_data->timestamp, ping_data->user_data);

  display->pending_pings = g_slist_remove (display->pending_pings, ping_data);
  ping_data_free (ping_data);

  return FALSE;
}

/**
 * meta_display_ping_window:
 * @display: The #MetaDisplay that the window is on
 * @window: The #MetaWindow to send the ping to
 * @timestamp: The timestamp of the ping. Used for uniqueness.
 *             Cannot be CurrentTime; use a real timestamp!
 * @ping_reply_func: The callback to call if we get a response.
 * @ping_timeout_func: The callback to call if we don't get a response.
 * @user_data: Arbitrary data that will be passed to the callback
 *             function. (In practice it's often a pointer to
 *             the window.)
 *
 * Sends a ping request to a window. The window must respond to
 * the request within a certain amount of time. If it does, we
 * will call one callback; if the time passes and we haven't had
 * a response, we call a different callback. The window must have
 * the hint showing that it can respond to a ping; if it doesn't,
 * we call the "got a response" callback immediately and return.
 * This function returns straight away after setting things up;
 * the callbacks will be called from the event loop.
 */
void
meta_display_ping_window (MetaWindow        *window,
			  guint32            timestamp,
			  MetaWindowPingFunc ping_reply_func,
			  MetaWindowPingFunc ping_timeout_func,
			  gpointer           user_data)
{
  MetaDisplay *display = window->display;
  MetaPingData *ping_data;

  if (timestamp == CurrentTime)
    {
      meta_warning ("Tried to ping a window with CurrentTime! Not allowed.\n");
      return;
    }

  if (!window->can_ping)
    {
      if (ping_reply_func)
        (* ping_reply_func) (window, timestamp, user_data);

      return;
    }

  ping_data = g_new (MetaPingData, 1);
  ping_data->window = window;
  ping_data->timestamp = timestamp;
  ping_data->ping_reply_func = ping_reply_func;
  ping_data->ping_timeout_func = ping_timeout_func;
  ping_data->user_data = user_data;
  ping_data->ping_timeout_id = g_timeout_add (PING_TIMEOUT_DELAY,
					      meta_display_ping_timeout,
					      ping_data);
  g_source_set_name_by_id (ping_data->ping_timeout_id, "[mutter] meta_display_ping_timeout");

  display->pending_pings = g_slist_prepend (display->pending_pings, ping_data);

  meta_topic (META_DEBUG_PING,
              "Sending ping with timestamp %u to window %s\n",
              timestamp, window->desc);

  META_WINDOW_GET_CLASS (window)->ping (window, timestamp);
}

/**
 * meta_display_pong_for_serial:
 * @display: the display we got the pong from
 * @serial: the serial in the pong repsonse
 *
 * Process the pong (the response message) from the ping we sent
 * to the window. This involves removing the timeout, calling the
 * reply handler function, and freeing memory.
 */
void
meta_display_pong_for_serial (MetaDisplay    *display,
                              guint32         serial)
{
  GSList *tmp;

  meta_topic (META_DEBUG_PING, "Received a pong with serial %u\n", serial);

  for (tmp = display->pending_pings; tmp; tmp = tmp->next)
    {
      MetaPingData *ping_data = tmp->data;

      if (serial == ping_data->timestamp)
        {
          meta_topic (META_DEBUG_PING,
                      "Matching ping found for pong %u\n",
                      ping_data->timestamp);

          /* Remove the ping data from the list */
          display->pending_pings = g_slist_remove (display->pending_pings,
                                                   ping_data);

          /* Remove the timeout */
          if (ping_data->ping_timeout_id != 0)
            {
              g_source_remove (ping_data->ping_timeout_id);
              ping_data->ping_timeout_id = 0;
            }

          /* Call callback */
          (* ping_data->ping_reply_func) (ping_data->window,
                                          ping_data->timestamp,
                                          ping_data->user_data);

          ping_data_free (ping_data);

          break;
        }
    }
}

static MetaGroup *
get_focused_group (MetaDisplay *display)
{
  if (display->focus_window)
    return display->focus_window->group;
  else
    return NULL;
}

#define IN_TAB_CHAIN(w,t) (((t) == META_TAB_LIST_NORMAL && META_WINDOW_IN_NORMAL_TAB_CHAIN (w)) \
    || ((t) == META_TAB_LIST_DOCKS && META_WINDOW_IN_DOCK_TAB_CHAIN (w)) \
    || ((t) == META_TAB_LIST_GROUP && META_WINDOW_IN_GROUP_TAB_CHAIN (w, get_focused_group (w->display))) \
    || ((t) == META_TAB_LIST_NORMAL_ALL && META_WINDOW_IN_NORMAL_TAB_CHAIN_TYPE (w)))

static MetaWindow*
find_tab_forward (MetaDisplay   *display,
                  MetaTabList    type,
                  MetaWorkspace *workspace,
                  GList         *start,
                  gboolean       skip_first)
{
  GList *tmp;

  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (workspace != NULL, NULL);

  tmp = start;
  if (skip_first)
    tmp = tmp->next;

  while (tmp != NULL)
    {
      MetaWindow *window = tmp->data;

      if (IN_TAB_CHAIN (window, type))
        return window;

      tmp = tmp->next;
    }

  tmp = workspace->mru_list;
  while (tmp != start)
    {
      MetaWindow *window = tmp->data;

      if (IN_TAB_CHAIN (window, type))
        return window;

      tmp = tmp->next;
    }  

  return NULL;
}

static MetaWindow*
find_tab_backward (MetaDisplay   *display,
                   MetaTabList    type,
                   MetaWorkspace *workspace,
                   GList         *start,
                   gboolean       skip_last)
{
  GList *tmp;

  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (workspace != NULL, NULL);

  tmp = start;
  if (skip_last)  
    tmp = tmp->prev;
  while (tmp != NULL)
    {
      MetaWindow *window = tmp->data;

      if (IN_TAB_CHAIN (window, type))
        return window;

      tmp = tmp->prev;
    }

  tmp = g_list_last (workspace->mru_list);
  while (tmp != start)
    {
      MetaWindow *window = tmp->data;

      if (IN_TAB_CHAIN (window, type))
        return window;

      tmp = tmp->prev;
    }

  return NULL;
}

static int
mru_cmp (gconstpointer a,
         gconstpointer b)
{
  guint32 time_a, time_b;

  time_a = meta_window_get_user_time ((MetaWindow *)a);
  time_b = meta_window_get_user_time ((MetaWindow *)b);

  if (time_a > time_b)
    return -1;
  else if (time_a < time_b)
    return 1;
  else
    return 0;
}

/**
 * meta_display_get_tab_list:
 * @display: a #MetaDisplay
 * @type: type of tab list
 * @screen: a #MetaScreen
 * @workspace: (allow-none): origin workspace
 *
 * Determine the list of windows that should be displayed for Alt-TAB
 * functionality.  The windows are returned in most recently used order.
 * If @workspace is not %NULL, the list only conains windows that are on
 * @workspace or have the demands-attention hint set; otherwise it contains
 * all windows on @screen.
 *
 * Returns: (transfer container) (element-type Meta.Window): List of windows
 */
GList*
meta_display_get_tab_list (MetaDisplay   *display,
                           MetaTabList    type,
                           MetaScreen    *screen,
                           MetaWorkspace *workspace)
{
  GList *tab_list = NULL;
  GList *global_mru_list = NULL;
  GList *mru_list, *tmp;
  GSList *windows = meta_display_list_windows (display, META_LIST_DEFAULT);
  GSList *w;

  if (workspace == NULL)
    {
      /* Yay for mixing GList and GSList in the API */
      for (w = windows; w; w = w->next)
        global_mru_list = g_list_prepend (global_mru_list, w->data);
      global_mru_list = g_list_sort (global_mru_list, mru_cmp);
    }

  mru_list = workspace ? workspace->mru_list : global_mru_list;

  /* Windows sellout mode - MRU order.
   */
  for (tmp = mru_list; tmp; tmp = tmp->next)
    {
      MetaWindow *window = tmp->data;

      if (IN_TAB_CHAIN (window, type))
        tab_list = g_list_prepend (tab_list, window);
    }

  tab_list = g_list_reverse (tab_list);

  /* If filtering by workspace, include windows from
   * other workspaces that demand attention
   */
  if (workspace)
    for (w = windows; w; w = w->next)
      {
        MetaWindow *l_window = w->data;

        if (l_window->wm_state_demands_attention &&
            l_window->workspace != workspace &&
            IN_TAB_CHAIN (l_window, type))
          tab_list = g_list_prepend (tab_list, l_window);
      }

  g_list_free (global_mru_list);
  g_slist_free (windows);
  
  return tab_list;
}

/**
 * meta_display_get_tab_next:
 * @display: a #MetaDisplay
 * @type: type of tab list
 * @workspace: origin workspace
 * @window: (allow-none): starting window 
 * @backward: If %TRUE, look for the previous window.  
 *
 * Determine the next window that should be displayed for Alt-TAB
 * functionality.
 *
 * Returns: (transfer none): Next window
 *
 */
MetaWindow*
meta_display_get_tab_next (MetaDisplay   *display,
                           MetaTabList    type,
                           MetaWorkspace *workspace,
                           MetaWindow    *window,
                           gboolean       backward)
{
  gboolean skip;
  GList *tab_list;
  MetaWindow *ret;
  tab_list = meta_display_get_tab_list (display, type, NULL, workspace);

  if (tab_list == NULL)
    return NULL;
  
  if (window != NULL)
    {
      g_assert (window->display == display);
      
      if (backward)
        ret = find_tab_backward (display, type, workspace, g_list_find (tab_list, window), TRUE);
      else
        ret = find_tab_forward (display, type, workspace, g_list_find (tab_list, window), TRUE);
    }
  else
    {
      skip = display->focus_window != NULL && 
             tab_list->data == display->focus_window;
      if (backward)
        ret = find_tab_backward (display, type, workspace, tab_list, skip);
      else
        ret = find_tab_forward (display, type, workspace, tab_list, skip);
    }

  g_list_free (tab_list);
  return ret;
}

/**
 * meta_display_get_tab_current:
 * @display: a #MetaDisplay
 * @type: type of tab list
 * @workspace: origin workspace
 *
 * Determine the active window that should be displayed for Alt-TAB.
 *
 * Returns: (transfer none): Current window
 *
 */
MetaWindow*
meta_display_get_tab_current (MetaDisplay   *display,
                              MetaTabList    type,
                              MetaWorkspace *workspace)
{
  MetaWindow *window;

  window = display->focus_window;
  
  if (window != NULL &&
      IN_TAB_CHAIN (window, type) &&
      (workspace == NULL ||
       meta_window_located_on_workspace (window, workspace)))
    return window;
  else
    return NULL;
}

int
meta_resize_gravity_from_grab_op (MetaGrabOp op)
{
  int gravity;
  
  gravity = -1;
  switch (op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
      gravity = NorthWestGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
    case META_GRAB_OP_RESIZING_S:
      gravity = NorthGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
    case META_GRAB_OP_RESIZING_SW:
      gravity = NorthEastGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
    case META_GRAB_OP_RESIZING_N:
      gravity = SouthGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
    case META_GRAB_OP_RESIZING_NE:
      gravity = SouthWestGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
    case META_GRAB_OP_RESIZING_NW:
      gravity = SouthEastGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
    case META_GRAB_OP_RESIZING_E:
      gravity = WestGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
    case META_GRAB_OP_RESIZING_W:
      gravity = EastGravity;
      break;
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
      gravity = CenterGravity;
      break;
    default:
      break;
    }

  return gravity;
}

void
meta_display_unmanage_screen (MetaDisplay *display,
                              MetaScreen  *screen,
                              guint32      timestamp)
{
  meta_verbose ("Unmanaging screen %d on display %s\n",
                screen->number, display->name);
  meta_display_close (display, timestamp);
}

void
meta_display_unmanage_windows_for_screen (MetaDisplay *display,
                                          MetaScreen  *screen,
                                          guint32      timestamp)
{
  GSList *tmp;
  GSList *winlist;

  winlist = meta_display_list_windows (display,
                                       META_LIST_INCLUDE_OVERRIDE_REDIRECT);
  winlist = g_slist_sort (winlist, meta_display_stack_cmp);
  g_slist_foreach (winlist, (GFunc)g_object_ref, NULL);

  /* Unmanage all windows */
  tmp = winlist;
  while (tmp != NULL)
    {
      MetaWindow *window = tmp->data;

      /* Check if already unmanaged for safety - in particular, catch
       * the case where unmanaging a parent window can cause attached
       * dialogs to be (temporarily) unmanaged.
       */
      if (!window->unmanaging)
        meta_window_unmanage (window, timestamp);
      g_object_unref (window);
      
      tmp = tmp->next;
    }
  g_slist_free (winlist);
}

int
meta_display_stack_cmp (const void *a,
                        const void *b)
{
  MetaWindow *aw = (void*) a;
  MetaWindow *bw = (void*) b;

  return meta_stack_windows_cmp (aw->screen->stack, aw, bw);
}

/**
 * meta_display_sort_windows_by_stacking:
 * @display: a #MetaDisplay
 * @windows: (element-type MetaWindow): Set of windows
 *
 * Sorts a set of windows according to their current stacking order. If windows
 * from multiple screens are present in the set of input windows, then all the
 * windows on screen 0 are sorted below all the windows on screen 1, and so forth.
 * Since the stacking order of override-redirect windows isn't controlled by
 * Metacity, if override-redirect windows are in the input, the result may not
 * correspond to the actual stacking order in the X server.
 *
 * An example of using this would be to sort the list of transient dialogs for a
 * window into their current stacking order.
 *
 * Returns: (transfer container) (element-type MetaWindow): Input windows sorted by stacking order, from lowest to highest
 */
GSList *
meta_display_sort_windows_by_stacking (MetaDisplay *display,
                                       GSList      *windows)
{
  GSList *copy = g_slist_copy (windows);

  copy = g_slist_sort (copy, meta_display_stack_cmp);

  return copy;
}

void
meta_display_devirtualize_modifiers (MetaDisplay        *display,
                                     MetaVirtualModifier modifiers,
                                     unsigned int       *mask)
{
  *mask = 0;
  
  if (modifiers & META_VIRTUAL_SHIFT_MASK)
    *mask |= ShiftMask;
  if (modifiers & META_VIRTUAL_CONTROL_MASK)
    *mask |= ControlMask;
  if (modifiers & META_VIRTUAL_ALT_MASK)
    *mask |= Mod1Mask;
  if (modifiers & META_VIRTUAL_META_MASK)
    *mask |= display->meta_mask;
  if (modifiers & META_VIRTUAL_HYPER_MASK)
    *mask |= display->hyper_mask;
  if (modifiers & META_VIRTUAL_SUPER_MASK)
    *mask |= display->super_mask;
  if (modifiers & META_VIRTUAL_MOD2_MASK)
    *mask |= Mod2Mask;
  if (modifiers & META_VIRTUAL_MOD3_MASK)
    *mask |= Mod3Mask;
  if (modifiers & META_VIRTUAL_MOD4_MASK)
    *mask |= Mod4Mask;
  if (modifiers & META_VIRTUAL_MOD5_MASK)
    *mask |= Mod5Mask;  
}

static void
update_window_grab_modifiers (MetaDisplay *display)
{
  MetaVirtualModifier virtual_mods;
  unsigned int mods;
    
  virtual_mods = meta_prefs_get_mouse_button_mods ();
  meta_display_devirtualize_modifiers (display, virtual_mods,
                                       &mods);
    
  display->window_grab_modifiers = mods;
}

static void
prefs_changed_callback (MetaPreference pref,
                        void          *data)
{
  MetaDisplay *display = data;
  
  /* It may not be obvious why we regrab on focus mode
   * change; it's because we handle focus clicks a
   * bit differently for the different focus modes.
   */
  if (pref == META_PREF_MOUSE_BUTTON_MODS ||
      pref == META_PREF_FOCUS_MODE)
    {
      MetaDisplay *display = data;
      GSList *windows;
      GSList *tmp;
      
      windows = meta_display_list_windows (display, META_LIST_DEFAULT);
      
      /* Ungrab all */
      tmp = windows;
      while (tmp != NULL)
        {
          MetaWindow *w = tmp->data;
          meta_display_ungrab_window_buttons (display, w->xwindow);
          meta_display_ungrab_focus_window_button (display, w);
          tmp = tmp->next;
        }

      /* change our modifier */
      if (pref == META_PREF_MOUSE_BUTTON_MODS)
        update_window_grab_modifiers (display);

      /* Grab all */
      tmp = windows;
      while (tmp != NULL)
        {
          MetaWindow *w = tmp->data;
          if (w->type != META_WINDOW_DOCK)
            {
              meta_display_grab_focus_window_button (display, w);
              meta_display_grab_window_buttons (display, w->xwindow);
            }
          tmp = tmp->next;
        }

      g_slist_free (windows);
    }
  else if (pref == META_PREF_AUDIBLE_BELL)
    {
      meta_bell_set_audible (display, meta_prefs_bell_is_audible ());
    }
}

void
meta_display_increment_focus_sentinel (MetaDisplay *display)
{
  unsigned long data[1];

  data[0] = meta_display_get_current_time (display);
  
  XChangeProperty (display->xdisplay,
                   display->screen->xroot,
                   display->atom__MUTTER_SENTINEL,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);
  
  display->sentinel_counter += 1;
}

void
meta_display_decrement_focus_sentinel (MetaDisplay *display)
{
  display->sentinel_counter -= 1;

  if (display->sentinel_counter < 0)
    display->sentinel_counter = 0;
}

gboolean
meta_display_focus_sentinel_clear (MetaDisplay *display)
{
  return (display->sentinel_counter == 0);
}

void
meta_display_sanity_check_timestamps (MetaDisplay *display,
                                      guint32      timestamp)
{
  if (XSERVER_TIME_IS_BEFORE (timestamp, display->last_focus_time))
    {
      meta_warning ("last_focus_time (%u) is greater than comparison "
                    "timestamp (%u).  This most likely represents a buggy "
                    "client sending inaccurate timestamps in messages such as "
                    "_NET_ACTIVE_WINDOW.  Trying to work around...\n",
                    display->last_focus_time, timestamp);
      display->last_focus_time = timestamp;
    }
  if (XSERVER_TIME_IS_BEFORE (timestamp, display->last_user_time))
    {
      GSList *windows;
      GSList *tmp;

      meta_warning ("last_user_time (%u) is greater than comparison "
                    "timestamp (%u).  This most likely represents a buggy "
                    "client sending inaccurate timestamps in messages such as "
                    "_NET_ACTIVE_WINDOW.  Trying to work around...\n",
                    display->last_user_time, timestamp);
      display->last_user_time = timestamp;

      windows = meta_display_list_windows (display, META_LIST_DEFAULT);
      tmp = windows;
      while (tmp != NULL)
        {
          MetaWindow *window = tmp->data;
          
          if (XSERVER_TIME_IS_BEFORE (timestamp, window->net_wm_user_time))
            {
              meta_warning ("%s appears to be one of the offending windows "
                            "with a timestamp of %u.  Working around...\n",
                            window->desc, window->net_wm_user_time);
              meta_window_set_user_time (window, timestamp);
            }

          tmp = tmp->next;
        }

      g_slist_free (windows);
    }
}

void
meta_display_set_input_focus_window (MetaDisplay *display, 
                                     MetaWindow  *window,
                                     gboolean     focus_frame,
                                     guint32      timestamp)
{
  request_xserver_input_focus_change (display,
                                      window->screen,
                                      window,
                                      focus_frame ? window->frame->xwindow : window->xwindow,
                                      timestamp);
}

void
meta_display_set_input_focus_xwindow (MetaDisplay *display,
                                      MetaScreen  *screen,
                                      Window       window,
                                      guint32      timestamp)
{
  request_xserver_input_focus_change (display,
                                      screen,
                                      NULL,
                                      window,
                                      timestamp);
}

void
meta_display_focus_the_no_focus_window (MetaDisplay *display,
                                        MetaScreen  *screen,
                                        guint32      timestamp)
{
  request_xserver_input_focus_change (display,
                                      screen,
                                      NULL,
                                      screen->no_focus_window,
                                      timestamp);
}

void
meta_display_remove_autoraise_callback (MetaDisplay *display)
{
  if (display->autoraise_timeout_id != 0)
    {
      g_source_remove (display->autoraise_timeout_id);
      display->autoraise_timeout_id = 0;
      display->autoraise_window = NULL;
    }
}

void
meta_display_overlay_key_activate (MetaDisplay *display)
{
  g_signal_emit (display, display_signals[OVERLAY_KEY], 0);
}

void
meta_display_accelerator_activate (MetaDisplay     *display,
                                   guint            action,
                                   ClutterKeyEvent *event)
{
  g_signal_emit (display, display_signals[ACCELERATOR_ACTIVATED],
                 0, action,
                 clutter_input_device_get_device_id (event->device),
                 event->time);
}

gboolean
meta_display_modifiers_accelerator_activate (MetaDisplay *display)
{
  gboolean freeze;

  g_signal_emit (display, display_signals[MODIFIERS_ACCELERATOR_ACTIVATED], 0, &freeze);

  return freeze;
}

void
meta_display_get_compositor_version (MetaDisplay *display,
                                     int         *major,
                                     int         *minor)
{
  *major = display->composite_major_version;
  *minor = display->composite_minor_version;
}

/**
 * meta_display_get_xinput_opcode: (skip)
 * @display: a #MetaDisplay
 *
 */
int
meta_display_get_xinput_opcode (MetaDisplay *display)
{
  return display->xinput_opcode;
}

/**
 * meta_display_supports_extended_barriers:
 * @display: a #MetaDisplay
 *
 * Returns: whether the X server supports extended barrier
 * features as defined in version 2.3 of the XInput 2
 * specification.
 *
 * Clients should use this method to determine whether their
 * interfaces should depend on new barrier features.
 */
gboolean
meta_display_supports_extended_barriers (MetaDisplay *display)
{
  return META_DISPLAY_HAS_XINPUT_23 (display) && !meta_is_wayland_compositor ();
}

/**
 * meta_display_get_xdisplay: (skip)
 * @display: a #MetaDisplay
 *
 */
Display *
meta_display_get_xdisplay (MetaDisplay *display)
{
  return display->xdisplay;
}

/**
 * meta_display_get_compositor: (skip)
 * @display: a #MetaDisplay
 *
 */
MetaCompositor *
meta_display_get_compositor (MetaDisplay *display)
{
  return display->compositor;
}

gboolean
meta_display_has_shape (MetaDisplay *display)
{
  return META_DISPLAY_HAS_SHAPE (display);
}

/**
 * meta_display_get_focus_window:
 * @display: a #MetaDisplay
 *
 * Get our best guess as to the "currently" focused window (that is,
 * the window that we expect will be focused at the point when the X
 * server processes our next request).
 *
 * Return Value: (transfer none): The current focus window
 */
MetaWindow *
meta_display_get_focus_window (MetaDisplay *display)
{
  return display->focus_window;
}

int 
meta_display_get_damage_event_base (MetaDisplay *display)
{
  return display->damage_event_base;
}

#ifdef HAVE_SHAPE
int
meta_display_get_shape_event_base (MetaDisplay *display)
{
  return display->shape_event_base;
}
#endif

/**
 * meta_display_clear_mouse_mode:
 * @display: a #MetaDisplay
 *
 * Sets the mouse-mode flag to %FALSE, which means that motion events are
 * no longer ignored in mouse or sloppy focus.
 * This is an internal function. It should be used only for reimplementing
 * keybindings, and only in a manner compatible with core code.
 */
void
meta_display_clear_mouse_mode (MetaDisplay *display)
{
  display->mouse_mode = FALSE;
}
