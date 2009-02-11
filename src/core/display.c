/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter X display handler */

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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/**
 * \file display.c Handles operations on an X display.
 *
 * The display is represented as a MetaDisplay struct.
 */

#include <config.h>
#include "display-private.h"
#include "util.h"
#include "main.h"
#include "screen-private.h"
#include "window-private.h"
#include "window-props.h"
#include "group-props.h"
#include "frame-private.h"
#include "errors.h"
#include "keybindings-private.h"
#include "prefs.h"
#include "resizepopup.h"
#include "xprops.h"
#include "workspace-private.h"
#include "bell.h"
#include "compositor.h"
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#ifdef HAVE_SOLARIS_XINERAMA
#include <X11/extensions/xinerama.h>
#endif
#ifdef HAVE_XFREE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif
#ifdef HAVE_SHAPE
#include <X11/extensions/shape.h>
#endif
#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif
#ifdef HAVE_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <string.h>

#define GRAB_OP_IS_WINDOW_SWITCH(g)                     \
        (g == META_GRAB_OP_KEYBOARD_TABBING_NORMAL  ||  \
         g == META_GRAB_OP_KEYBOARD_TABBING_DOCK    ||  \
         g == META_GRAB_OP_KEYBOARD_TABBING_GROUP   ||  \
         g == META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL ||  \
         g == META_GRAB_OP_KEYBOARD_ESCAPING_DOCK   ||  \
         g == META_GRAB_OP_KEYBOARD_ESCAPING_GROUP)

/**
 * \defgroup pings Pings
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
 * Describes a ping on a window. When we send a ping to a window, we build
 * one of these structs, and it eventually gets passed to the timeout function
 * or to the function which handles the response from the window. If the window
 * does or doesn't respond to the ping, we use this information to deal with
 * these facts; we have a handler function for each.
 *
 * \ingroup pings
 */
typedef struct 
{
  MetaDisplay *display;
  Window       xwindow;
  guint32      timestamp;
  MetaWindowPingFunc ping_reply_func;
  MetaWindowPingFunc ping_timeout_func;
  void        *user_data;
  guint        ping_timeout_id;
} MetaPingData;

typedef struct 
{
  MetaDisplay *display;
  Window xwindow;
} MetaAutoRaiseData;

G_DEFINE_TYPE(MetaDisplay, meta_display, G_TYPE_OBJECT);

/* Signals */
enum
{
  OVERLAY_KEY,
  FOCUS_WINDOW,
  WINDOW_CREATED,
  WINDOW_DEMANDS_ATTENTION,
  WINDOW_MARKED_URGENT,
  LAST_SIGNAL
};

enum {
  PROP_0,

  PROP_FOCUS_WINDOW
};

static guint display_signals [LAST_SIGNAL] = { 0 };

/**
 * The display we're managing.  This is a singleton object.  (Historically,
 * this was a list of displays, but there was never any way to add more
 * than one element to it.)  The goofy name is because we don't want it
 * to shadow the parameter in its object methods.
 */
static MetaDisplay *the_display = NULL;

#ifdef WITH_VERBOSE_MODE
static void   meta_spew_event           (MetaDisplay    *display,
                                         XEvent         *event);
#endif

static gboolean event_callback          (XEvent         *event,
                                         gpointer        data);
static Window event_get_modified_window (MetaDisplay    *display,
                                         XEvent         *event);
static guint32 event_get_time           (MetaDisplay    *display,
                                         XEvent         *event);
static void    process_request_frame_extents (MetaDisplay    *display,
                                              XEvent         *event);
static void    process_pong_message     (MetaDisplay    *display,
                                         XEvent         *event);
static void    process_selection_request (MetaDisplay   *display,
                                          XEvent        *event);
static void    process_selection_clear   (MetaDisplay   *display,
                                          XEvent        *event);

static void    update_window_grab_modifiers (MetaDisplay *display);

static void    prefs_changed_callback    (MetaPreference pref,
                                          void          *data);

static void    sanity_check_timestamps   (MetaDisplay *display,
                                          guint32      known_good_timestamp);

MetaGroup*     get_focussed_group (MetaDisplay *display);

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
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  display_signals[WINDOW_CREATED] =
    g_signal_new ("window-created",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, META_TYPE_WINDOW);

  display_signals[WINDOW_DEMANDS_ATTENTION] =
    g_signal_new ("window-demands-attention",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, META_TYPE_WINDOW);

  display_signals[WINDOW_MARKED_URGENT] =
    g_signal_new ("window-marked-urgent",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  META_TYPE_WINDOW);

  g_object_class_install_property (object_class,
                                   PROP_FOCUS_WINDOW,
                                   g_param_spec_object ("focus-window",
                                                        "Focus window",
                                                        "Currently focused window",
                                                        META_TYPE_WINDOW,
                                                        G_PARAM_READABLE));
}


/**
 * Destructor for MetaPingData structs. Will destroy the
 * event source for the struct as well.
 *
 * \ingroup pings
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
 * Frees every pending ping structure for the given X window on the
 * given display. This means that we also destroy the timeouts.
 *
 * \param display The display the window appears on
 * \param xwindow The X ID of the window whose pings we should remove
 *
 * \ingroup pings
 *
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

      if (ping_data->xwindow == xwindow)
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
    meta_error_trap_pop (display, FALSE);
}
#endif

static void
enable_compositor (MetaDisplay *display,
                   gboolean     composite_windows)
{
  GSList *list;

  if (!META_DISPLAY_HAS_COMPOSITE (display) ||
      !META_DISPLAY_HAS_DAMAGE (display) ||
      !META_DISPLAY_HAS_XFIXES (display) ||
      !META_DISPLAY_HAS_RENDER (display))
    {
      meta_warning (_("Missing %s extension required for compositing"),
                    !META_DISPLAY_HAS_COMPOSITE (display) ? "composite" :
                    !META_DISPLAY_HAS_DAMAGE (display) ? "damage" :
                    !META_DISPLAY_HAS_XFIXES (display) ? "xfixes" : "render");
      return;
    }

  if (!display->compositor)
      display->compositor = meta_compositor_new (display);

  if (!display->compositor)
    return;
  
  for (list = display->screens; list != NULL; list = list->next)
    {
      MetaScreen *screen = list->data;
      
      meta_compositor_manage_screen (screen->display->compositor,
				     screen);

      if (composite_windows)
        meta_screen_composite_all_windows (screen);
    }
}

static void
disable_compositor (MetaDisplay *display)
{
  GSList *list;
  
  if (!display->compositor)
    return;
  
  for (list = display->screens; list != NULL; list = list->next)
    {
      MetaScreen *screen = list->data;
      
      meta_compositor_unmanage_screen (screen->display->compositor,
				       screen);
    }
  
  meta_compositor_destroy (display->compositor);
  display->compositor = NULL;
}

static void
meta_display_init (MetaDisplay *disp)
{
  /* Some stuff could go in here that's currently in _open,
   * but it doesn't really matter. */
}

/**
 * Opens a new display, sets it up, initialises all the X extensions
 * we will need, and adds it to the list of displays.
 *
 * \return True if the display was opened successfully, and False
 * otherwise-- that is, if the display doesn't exist or it already
 * has a window manager.
 *
 * \ingroup main
 */
gboolean
meta_display_open (void)
{
  Display *xdisplay;
  GSList *screens;
  GSList *tmp;
  int i;
  guint32 timestamp;

  /* A list of all atom names, so that we can intern them in one go. */
  char *atom_names[] = {
#define item(x) #x,
#include "atomnames.h"
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
  the_display->error_trap_synced_at_last_pop = TRUE;
  the_display->error_traps = 0;
  the_display->error_trap_handler = NULL;
  the_display->server_grab_count = 0;
  the_display->display_opening = TRUE;

  the_display->pending_pings = NULL;
  the_display->autoraise_timeout_id = 0;
  the_display->autoraise_window = NULL;
  the_display->focus_window = NULL;
  the_display->expected_focus_window = NULL;
  the_display->grab_old_window_stacking = NULL;

  the_display->mouse_mode = TRUE; /* Only relevant for mouse or sloppy focus */
  the_display->allow_terminal_deactivation = TRUE; /* Only relevant for when a
                                                  terminal has the focus */

#ifdef HAVE_XSYNC
  the_display->grab_sync_request_alarm = None;
#endif
  
  /* FIXME copy the checks from GDK probably */
  the_display->static_gravity_works = g_getenv ("MUTTER_USE_STATIC_GRAVITY") != NULL;
  
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
#include "atomnames.h"
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
  
  the_display->screens = NULL;
  the_display->active_screen = NULL;
  
#ifdef HAVE_STARTUP_NOTIFICATION
  the_display->sn_display = sn_display_new (the_display->xdisplay,
                                        sn_error_trap_push,
                                        sn_error_trap_pop);
#endif
  
  the_display->events = NULL;

  /* Get events */
  meta_ui_add_event_func (the_display->xdisplay,
                          event_callback,
                          the_display);
  
  the_display->window_ids = g_hash_table_new (meta_unsigned_long_hash,
                                          meta_unsigned_long_equal);
  
  i = 0;
  while (i < N_IGNORED_SERIALS)
    {
      the_display->ignored_serials[i] = 0;
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
  the_display->grab_screen = NULL;
  the_display->grab_resize_popup = NULL;

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
      the_display->have_xsync = TRUE;
    
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

    the_display->have_xfixes = FALSE;

    the_display->xfixes_error_base = 0;
    the_display->xfixes_event_base = 0;

    if (!XFixesQueryExtension (the_display->xdisplay,
                               &the_display->xfixes_event_base,
                               &the_display->xfixes_error_base))
      {
        the_display->xfixes_error_base = 0;
        the_display->xfixes_event_base = 0;
      } 
    else
      the_display->have_xfixes = TRUE;

    meta_verbose ("Attempted to init XFixes, found error base %d event base %d\n",
                  the_display->xfixes_error_base, 
                  the_display->xfixes_event_base);
  }
      
#ifdef HAVE_XCURSOR
  {
    XcursorSetTheme (the_display->xdisplay, meta_prefs_get_cursor_theme ());
    XcursorSetDefaultSize (the_display->xdisplay, meta_prefs_get_cursor_size ());
  }
#else /* HAVE_XCURSOR */
  meta_verbose ("Not compiled with Xcursor support\n");
#endif /* !HAVE_XCURSOR */

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
                                    "Mutter");

    /* The GNOME keybindings capplet should include both the Mutter and Metacity
     * keybindings */
    meta_prop_set_utf8_string_hint (the_display,
                                    the_display->leader_window,
                                    the_display->atom__GNOME_WM_KEYBINDINGS,
                                    "Mutter,Metacity");
    
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
  
  screens = NULL;
  
  i = 0;
  while (i < ScreenCount (xdisplay))
    {
      MetaScreen *screen;

      screen = meta_screen_new (the_display, i, timestamp);

      if (screen)
        screens = g_slist_prepend (screens, screen);
      ++i;
    }
  
  the_display->screens = screens;
  
  if (screens == NULL)
    {
      /* This would typically happen because all the screens already
       * have window managers.
       */
      meta_display_close (the_display, timestamp);
      return FALSE;
    }

  /* We don't composite the windows here because they will be composited 
     faster with the call to meta_screen_manage_all_windows further down 
     the code */
  if (1) /* meta_prefs_get_compositing_manager ()) FIXME */
    enable_compositor (the_display, FALSE);
   
  meta_display_grab (the_display);
  
  /* Now manage all existing windows */
  tmp = the_display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;
	
      meta_screen_manage_all_windows (screen);

      tmp = tmp->next;
    }

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
                                              the_display->screens->data,
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
                                                  the_display->screens->data,
                                                  timestamp);
      }

    meta_error_trap_pop (the_display, FALSE);
  }
  
  meta_display_ungrab (the_display);  

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
  GSList *tmp;
  GSList *prev;
  GHashTableIter iter;
  gpointer key, value;

  winlist = NULL;

  g_hash_table_iter_init (&iter, display->window_ids);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      MetaWindow *window = value;

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
  GSList *tmp;

  g_assert (display != NULL);

  if (display->closing != 0)
    {
      /* The display's already been closed. */
      return;
    }

  if (display->error_traps > 0)
    meta_bug ("Display closed with error traps pending\n");

  display->closing += 1;

  meta_prefs_remove_listener (prefs_changed_callback, display);
  
  meta_display_remove_autoraise_callback (display);

  if (display->grab_old_window_stacking)
    g_list_free (display->grab_old_window_stacking);
  
  /* Stop caring about events */
  meta_ui_remove_event_func (display->xdisplay,
                             event_callback,
                             display);
  
  /* Free all screens */
  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;
      meta_screen_free (screen, timestamp);
      tmp = tmp->next;
    }

  g_slist_free (display->screens);
  display->screens = NULL;

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
  g_hash_table_destroy (display->window_ids);

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

/**
 * meta_display_screen_for_root:
 * @display: a #MetaDisplay
 *
 * Return the #MetaScreen corresponding to a specified X root window ID.
 *
 * Return Value: (transfer none): the screen for the specified root window ID, or %NULL
 */
MetaScreen*
meta_display_screen_for_root (MetaDisplay *display,
                              Window       xroot)
{
  GSList *tmp;

  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;

      if (xroot == screen->xroot)
        return screen;

      tmp = tmp->next;
    }

  return NULL;
}

MetaScreen*
meta_display_screen_for_xwindow (MetaDisplay *display,
                                 Window       xwindow)
{
  XWindowAttributes attr;
  int result;
  
  meta_error_trap_push (display);
  attr.screen = NULL;
  result = XGetWindowAttributes (display->xdisplay, xwindow, &attr);
  meta_error_trap_pop (display, TRUE);

  /* Note, XGetWindowAttributes is on all kinds of crack
   * and returns 1 on success 0 on failure, rather than Success
   * on success.
   */
  if (result == 0 || attr.screen == NULL)
    return NULL;
  
  return meta_display_screen_for_x_screen (display, attr.screen);
}

MetaScreen*
meta_display_screen_for_x_screen (MetaDisplay *display,
                                  Screen      *xscreen)
{
  GSList *tmp;

  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;

      if (xscreen == screen->xscreen)
        return screen;

      tmp = tmp->next;
    }

  return NULL;
}

/* Grab/ungrab routines taken from fvwm */
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
 * Returns the singleton MetaDisplay if "xdisplay" matches the X display it's
 * managing; otherwise gives a warning and returns NULL.  When we were claiming
 * to be able to manage multiple displays, this was supposed to find the
 * display out of the list which matched that display.  Now it's merely an
 * extra sanity check.
 *
 * \param xdisplay  An X display
 * \return  The singleton X display, or NULL if "xdisplay" isn't the one
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
 * Accessor for the singleton MetaDisplay.
 *
 * \return  The only MetaDisplay there is.  This can be NULL, but only
 *          during startup.
 */
MetaDisplay*
meta_get_display (void)
{
  return the_display;
}

#ifdef WITH_VERBOSE_MODE
static gboolean dump_events = TRUE;
#endif

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

static gboolean
grab_op_is_mouse (MetaGrabOp op)
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

static gboolean
grab_op_is_keyboard (MetaGrabOp op)
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
    case META_GRAB_OP_KEYBOARD_TABBING_NORMAL:
    case META_GRAB_OP_KEYBOARD_TABBING_DOCK:
    case META_GRAB_OP_KEYBOARD_TABBING_GROUP:
    case META_GRAB_OP_KEYBOARD_ESCAPING_NORMAL:
    case META_GRAB_OP_KEYBOARD_ESCAPING_DOCK:
    case META_GRAB_OP_KEYBOARD_ESCAPING_GROUP:
    case META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING:
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

/* Get time of current event, or CurrentTime if none. */
guint32
meta_display_get_current_time (MetaDisplay *display)
{
  return display->current_time;
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

      /* Using the property XA_PRIMARY because it's safe; nothing
       * would use it as a property. The type doesn't matter.
       */
      XChangeProperty (display->xdisplay,
                       display->timestamp_pinging_window,
                       XA_PRIMARY, XA_STRING, 8,
                       PropModeAppend, NULL, 0);
      XWindowEvent (display->xdisplay,
                    display->timestamp_pinging_window,
                    PropertyChangeMask,
                    &property_event);
      timestamp = property_event.xproperty.time;
    }

  sanity_check_timestamps (display, timestamp);

  return timestamp;
}

static void
add_ignored_serial (MetaDisplay  *display,
                    unsigned long serial)
{
  int i;

  /* don't add the same serial more than once */
  if (display->ignored_serials[N_IGNORED_SERIALS-1] == serial)
    return;
  
  /* shift serials to the left */
  i = 0;
  while (i < (N_IGNORED_SERIALS - 1))
    {
      display->ignored_serials[i] = display->ignored_serials[i+1];
      ++i;
    }
  /* put new one on the end */
  display->ignored_serials[i] = serial;
}

static gboolean
serial_is_ignored (MetaDisplay  *display,
                   unsigned long serial)
{
  int i;

  i = 0;
  while (i < N_IGNORED_SERIALS)
    {
      if (display->ignored_serials[i] == serial)
        return TRUE;
      ++i;
    }
  return FALSE;
}

static void
reset_ignores (MetaDisplay *display)
{
  int i;

  i = 0;
  while (i < N_IGNORED_SERIALS)
    {
      display->ignored_serials[i] = 0;
      ++i;
    }

  display->ungrab_should_not_cause_focus_window = None;
}

static gboolean 
window_raise_with_delay_callback (void *data)
{
  MetaWindow *window;
  MetaAutoRaiseData *auto_raise;

  auto_raise = data;

  meta_topic (META_DEBUG_FOCUS, 
	      "In autoraise callback for window 0x%lx\n", 
	      auto_raise->xwindow);

  auto_raise->display->autoraise_timeout_id = 0;
  auto_raise->display->autoraise_window = NULL;

  window  = meta_display_lookup_x_window (auto_raise->display, 
					  auto_raise->xwindow);
  
  if (window == NULL) 
    return FALSE;

  /* If we aren't already on top, check whether the pointer is inside
   * the window and raise the window if so.
   */      
  if (meta_stack_get_top (window->screen->stack) != window) 
    {
      int x, y, root_x, root_y;
      Window root, child;
      unsigned int mask;
      gboolean same_screen;
      gboolean point_in_window;

      meta_error_trap_push (window->display);
      same_screen = XQueryPointer (window->display->xdisplay,
				   window->xwindow,
				   &root, &child,
				   &root_x, &root_y, &x, &y, &mask);
      meta_error_trap_pop (window->display, TRUE);

      point_in_window = 
        (window->frame && POINT_IN_RECT (root_x, root_y, window->frame->rect)) ||
        (window->frame == NULL && POINT_IN_RECT (root_x, root_y, window->rect));
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
  MetaAutoRaiseData *auto_raise_data;

  meta_topic (META_DEBUG_FOCUS, 
              "Queuing an autoraise timeout for %s with delay %d\n", 
              window->desc, 
              meta_prefs_get_auto_raise_delay ());
  
  auto_raise_data = g_new (MetaAutoRaiseData, 1);
  auto_raise_data->display = window->display;
  auto_raise_data->xwindow = window->xwindow;
  
  if (display->autoraise_timeout_id != 0)
    g_source_remove (display->autoraise_timeout_id);

  display->autoraise_timeout_id = 
    g_timeout_add_full (G_PRIORITY_DEFAULT,
                        meta_prefs_get_auto_raise_delay (),
                        window_raise_with_delay_callback,
                        auto_raise_data,
                        g_free);
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
       * meta_window_configure_request() which is smart about whether to
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

/**
 * This is the most important function in the whole program. It is the heart,
 * it is the nexus, it is the Grand Central Station of Mutter's world.
 * When we create a MetaDisplay, we ask GDK to pass *all* events for *all*
 * windows to this function. So every time anything happens that we might
 * want to know about, this function gets called. You see why it gets a bit
 * busy around here. Most of this function is a ginormous switch statement
 * dealing with all the kinds of events that might turn up.
 *
 * \param event The event that just happened
 * \param data  The MetaDisplay that events are coming from, cast to a gpointer
 *              so that it can be sent to a callback
 *
 * \ingroup main
 */
static gboolean
event_callback (XEvent   *event,
                gpointer  data)
{
  MetaWindow *window;
  MetaWindow *property_for_window;
  MetaDisplay *display;
  Window modified;
  gboolean frame_was_receiver;
  gboolean bypass_compositor;
  gboolean filter_out_event;

  display = data;
  
#ifdef WITH_VERBOSE_MODE
  if (dump_events)
    meta_spew_event (display, event);
#endif

#ifdef HAVE_STARTUP_NOTIFICATION
  sn_display_process_event (display->sn_display, event);
#endif
  
  bypass_compositor = FALSE;
  filter_out_event = FALSE;
  display->current_time = event_get_time (display, event);
  display->monitor_cache_invalidated = TRUE;
  
  modified = event_get_modified_window (display, event);
  
  if (event->type == UnmapNotify)
    {
      if (meta_ui_window_should_not_cause_focus (display->xdisplay,
                                                 modified))
        {
          add_ignored_serial (display, event->xany.serial);
          meta_topic (META_DEBUG_FOCUS,
                      "Adding EnterNotify serial %lu to ignored focus serials\n",
                      event->xany.serial);
        }
    }
  else if (event->type == LeaveNotify &&
           event->xcrossing.mode == NotifyUngrab &&
           modified == display->ungrab_should_not_cause_focus_window)
    {
      add_ignored_serial (display, event->xany.serial);
      meta_topic (META_DEBUG_FOCUS,
                  "Adding LeaveNotify serial %lu to ignored focus serials\n",
                  event->xany.serial);
    }

  if (modified != None)
    window = meta_display_lookup_x_window (display, modified);
  else
    window = NULL;

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
    

  frame_was_receiver = FALSE;
  if (window &&
      window->frame &&
      modified == window->frame->xwindow)
    {
      /* Note that if the frame and the client both have an
       * XGrabButton (as is normal with our setup), the event
       * goes to the frame.
       */
      frame_was_receiver = TRUE;
      meta_topic (META_DEBUG_EVENTS, "Frame was receiver of event for %s\n",
                  window->desc);
    }

#ifdef HAVE_XSYNC
  if (META_DISPLAY_HAS_XSYNC (display) && 
      event->type == (display->xsync_event_base + XSyncAlarmNotify) &&
      ((XSyncAlarmNotifyEvent*)event)->alarm == display->grab_sync_request_alarm)
    {
      filter_out_event = TRUE; /* GTK doesn't want to see this really */
      
      if (display->grab_op != META_GRAB_OP_NONE &&
          display->grab_window != NULL &&
          grab_op_is_mouse (display->grab_op))
	meta_window_handle_mouse_grab_op_event (display->grab_window, event);
    }
#endif /* HAVE_XSYNC */

#ifdef HAVE_SHAPE
  if (META_DISPLAY_HAS_SHAPE (display) && 
      event->type == (display->shape_event_base + ShapeNotify))
    {
      filter_out_event = TRUE; /* GTK doesn't want to see this really */
      
      if (window && !frame_was_receiver)
        {
          XShapeEvent *sev = (XShapeEvent*) event;

          if (sev->kind == ShapeBounding)
            {
              if (sev->shaped && !window->has_shape)
                {
                  window->has_shape = TRUE;                  
                  meta_topic (META_DEBUG_SHAPES,
                              "Window %s now has a shape\n",
                              window->desc);
                }
              else if (!sev->shaped && window->has_shape)
                {
                  window->has_shape = FALSE;
                  meta_topic (META_DEBUG_SHAPES,
                              "Window %s no longer has a shape\n",
                              window->desc);
                }
              else
                {
                  meta_topic (META_DEBUG_SHAPES,
                              "Window %s shape changed\n",
                              window->desc);
                }

              if (window->frame)
                {
                  window->frame->need_reapply_frame_shape = TRUE;
		  meta_warning("from event callback\n");		  
                  meta_window_queue (window, META_QUEUE_MOVE_RESIZE);
                }
            }
        }
      else
        {
          meta_topic (META_DEBUG_SHAPES,
                      "ShapeNotify not on a client window (window %s frame_was_receiver = %d)\n",
                      window ? window->desc : "(none)",
                      frame_was_receiver);
        }
    }
#endif /* HAVE_SHAPE */

  if (window && ((event->type == KeyPress) || (event->type == ButtonPress)))
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
          sanity_check_timestamps (display, display->current_time);
        }
    }
  
  switch (event->type)
    {
    case KeyPress:
    case KeyRelease:
      if (display->grab_op == META_GRAB_OP_COMPOSITOR)
        break;

      /* For key events, it's important to enforce single-handling, or
       * we can get into a confused state. So if a keybinding is
       * handled (because it's one of our hot-keys, or because we are
       * in a keyboard-grabbed mode like moving a window, we don't
       * want to pass the key event to the compositor at all.
       */
      bypass_compositor = meta_display_process_key_event (display, window, event);
      break;
    case ButtonPress:
      if (display->grab_op == META_GRAB_OP_COMPOSITOR)
        break;

      if (event->xbutton.button == 4 || event->xbutton.button == 5)
        /* Scrollwheel event, do nothing and deliver event to compositor below */
        break;

      if ((window &&
           grab_op_is_mouse (display->grab_op) &&
           display->grab_button != (int) event->xbutton.button &&
           display->grab_window == window) ||
          grab_op_is_keyboard (display->grab_op))
        {
          meta_topic (META_DEBUG_WINDOW_OPS,
                      "Ending grab op %u on window %s due to button press\n",
                      display->grab_op,
                      (display->grab_window ?
                       display->grab_window->desc : 
                       "none"));
          if (GRAB_OP_IS_WINDOW_SWITCH (display->grab_op))
            {
              MetaScreen *screen;
              meta_topic (META_DEBUG_WINDOW_OPS, 
                          "Syncing to old stack positions.\n");
              screen = 
                meta_display_screen_for_root (display, event->xany.window);

              if (screen!=NULL)
                meta_stack_set_positions (screen->stack,
                                          display->grab_old_window_stacking);
            }
          meta_display_end_grab_op (display,
                                    event->xbutton.time);
        }
      else if (window && display->grab_op == META_GRAB_OP_NONE)
        {
          gboolean begin_move = FALSE;
          unsigned int grab_mask;
          gboolean unmodified;

          grab_mask = display->window_grab_modifiers;
          if (g_getenv ("MUTTER_DEBUG_BUTTON_GRABS"))
            grab_mask |= ControlMask;

          /* Two possible sources of an unmodified event; one is a
           * client that's letting button presses pass through to the
           * frame, the other is our focus_window_grab on unmodified
           * button 1.  So for all such events we focus the window.
           */
          unmodified = (event->xbutton.state & grab_mask) == 0;
          
          if (unmodified ||
              event->xbutton.button == 1)
            {
              /* don't focus if frame received, will be lowered in
               * frames.c or special-cased if the click was on a
               * minimize/close button.
               */
              if (!frame_was_receiver)
                {
                  if (meta_prefs_get_raise_on_click ()) 
                    meta_window_raise (window);
                  else
                    meta_topic (META_DEBUG_FOCUS,
                                "Not raising window on click due to don't-raise-on-click option\n");

                  /* Don't focus panels--they must explicitly request focus.
                   * See bug 160470
                   */
		  if (window->type != META_WINDOW_DOCK)
                    {
                      meta_topic (META_DEBUG_FOCUS,
                                  "Focusing %s due to unmodified button %u press (display.c)\n",
                                  window->desc, event->xbutton.button);
                      meta_window_focus (window, event->xbutton.time);
                    }
                  else
                    /* However, do allow terminals to lose focus due to new
                     * window mappings after the user clicks on a panel.
                     */
                    display->allow_terminal_deactivation = TRUE;
                }
              
              /* you can move on alt-click but not on
               * the click-to-focus
               */
              if (!unmodified)
                begin_move = TRUE;
            }
          else if (!unmodified && event->xbutton.button == meta_prefs_get_mouse_button_resize())
            {
              if (window->has_resize_func)
                {
                  gboolean north, south;
                  gboolean west, east;
                  int root_x, root_y;
                  MetaGrabOp op;

                  meta_window_get_position (window, &root_x, &root_y);

                  west = event->xbutton.x_root <  (root_x + 1 * window->rect.width  / 3);
                  east = event->xbutton.x_root >  (root_x + 2 * window->rect.width  / 3);
                  north = event->xbutton.y_root < (root_y + 1 * window->rect.height / 3);
                  south = event->xbutton.y_root > (root_y + 2 * window->rect.height / 3);

                  if (north && west)
                    op = META_GRAB_OP_RESIZING_NW;
                  else if (north && east)
                    op = META_GRAB_OP_RESIZING_NE;
                  else if (south && west)
                    op = META_GRAB_OP_RESIZING_SW;
                  else if (south && east)
                    op = META_GRAB_OP_RESIZING_SE;
                  else if (north)
                    op = META_GRAB_OP_RESIZING_N;
                  else if (west)
                    op = META_GRAB_OP_RESIZING_W;
                  else if (east)
                    op = META_GRAB_OP_RESIZING_E;
                  else if (south)
                    op = META_GRAB_OP_RESIZING_S;
                  else /* Middle region is no-op to avoid user triggering wrong action */
                    op = META_GRAB_OP_NONE;
                  
                  if (op != META_GRAB_OP_NONE)
                    meta_display_begin_grab_op (display,
                                                window->screen,
                                                window,
                                                op,
                                                TRUE,
                                                FALSE,
                                                event->xbutton.button,
                                                0,
                                                event->xbutton.time,
                                                event->xbutton.x_root,
                                                event->xbutton.y_root);
                }
            }
          else if (event->xbutton.button == meta_prefs_get_mouse_button_menu())
            {
              if (meta_prefs_get_raise_on_click ())
                meta_window_raise (window);
              meta_window_show_menu (window,
                                     event->xbutton.x_root,
                                     event->xbutton.y_root,
                                     event->xbutton.button,
                                     event->xbutton.time);
            }

          if (!frame_was_receiver && unmodified)
            {
              /* This is from our synchronous grab since
               * it has no modifiers and was on the client window
               */
              int mode;
              
              /* When clicking a different app in click-to-focus
               * in application-based mode, and the different
               * app is not a dock or desktop, eat the focus click.
               */
              if (meta_prefs_get_focus_mode () == META_FOCUS_MODE_CLICK &&
                  meta_prefs_get_application_based () &&
                  !window->has_focus &&
                  window->type != META_WINDOW_DOCK &&
                  window->type != META_WINDOW_DESKTOP &&
                  (display->focus_window == NULL ||
                   !meta_window_same_application (window,
                                                  display->focus_window)))
                mode = AsyncPointer; /* eat focus click */
              else
                mode = ReplayPointer; /* give event back */

              meta_verbose ("Allowing events mode %s time %u\n",
                            mode == AsyncPointer ? "AsyncPointer" : "ReplayPointer",
                            (unsigned int)event->xbutton.time);
              
              XAllowEvents (display->xdisplay,
                            mode, event->xbutton.time);
            }

          if (begin_move && window->has_move_func)
            {
              meta_display_begin_grab_op (display,
                                          window->screen,
                                          window,
                                          META_GRAB_OP_MOVING,
                                          TRUE,
                                          FALSE,
                                          event->xbutton.button,
                                          0,
                                          event->xbutton.time,
                                          event->xbutton.x_root,
                                          event->xbutton.y_root);
            }
        }
      break;
    case ButtonRelease:
      if (display->grab_op == META_GRAB_OP_COMPOSITOR)
        break;

      if (display->grab_window == window &&
          grab_op_is_mouse (display->grab_op))
        meta_window_handle_mouse_grab_op_event (window, event);
      break;
    case MotionNotify:
      if (display->grab_op == META_GRAB_OP_COMPOSITOR)
        break;

      if (display->grab_window == window &&
          grab_op_is_mouse (display->grab_op))
        meta_window_handle_mouse_grab_op_event (window, event);
      break;
    case EnterNotify:
      if (display->grab_op == META_GRAB_OP_COMPOSITOR)
        break;

      if (display->grab_window == window &&
          grab_op_is_mouse (display->grab_op))
        {
          meta_window_handle_mouse_grab_op_event (window, event);
          break;
        }

      /* If the mouse switches screens, active the default window on the new
       * screen; this will make keybindings and workspace-launched items
       * actually appear on the right screen.
       */
      {
        MetaScreen *new_screen = 
          meta_display_screen_for_root (display, event->xcrossing.root);

        if (new_screen != NULL && display->active_screen != new_screen)
          meta_workspace_focus_default_window (new_screen->active_workspace, 
                                               NULL,
                                               event->xcrossing.time);
      }

      /* Check if we've entered a window; do this even if window->has_focus to
       * avoid races.
       */
      if (window && !serial_is_ignored (display, event->xany.serial) &&
               event->xcrossing.mode != NotifyGrab && 
               event->xcrossing.mode != NotifyUngrab &&
               event->xcrossing.detail != NotifyInferior &&
               meta_display_focus_sentinel_clear (display))
        {
          switch (meta_prefs_get_focus_mode ())
            {
            case META_FOCUS_MODE_SLOPPY:
            case META_FOCUS_MODE_MOUSE:
              display->mouse_mode = TRUE;
              if (window->type != META_WINDOW_DOCK &&
                  window->type != META_WINDOW_DESKTOP)
                {
                  meta_topic (META_DEBUG_FOCUS,
                              "Focusing %s due to enter notify with serial %lu "
                              "at time %lu, and setting display->mouse_mode to "
                              "TRUE.\n",
                              window->desc, 
                              event->xany.serial,
                              event->xcrossing.time);

                  meta_window_focus (window, event->xcrossing.time);

                  /* stop ignoring stuff */
                  reset_ignores (display);
                  
                  if (meta_prefs_get_auto_raise ()) 
                    {
                      meta_display_queue_autoraise_callback (display, window);
                    }
                  else
                    {
                      meta_topic (META_DEBUG_FOCUS,
                                  "Auto raise is disabled\n");		      
                    }
                }
              /* In mouse focus mode, we defocus when the mouse *enters*
               * the DESKTOP window, instead of defocusing on LeaveNotify.
               * This is because having the mouse enter override-redirect
               * child windows unfortunately causes LeaveNotify events that
               * we can't distinguish from the mouse actually leaving the
               * toplevel window as we expect.  But, since we filter out
               * EnterNotify events on override-redirect windows, this
               * alternative mechanism works great.
               */
              if (window->type == META_WINDOW_DESKTOP &&
                  meta_prefs_get_focus_mode() == META_FOCUS_MODE_MOUSE &&
                  display->expected_focus_window != NULL)
                {
                  meta_topic (META_DEBUG_FOCUS,
                              "Unsetting focus from %s due to mouse entering "
                              "the DESKTOP window\n",
                              display->expected_focus_window->desc);
                  meta_display_focus_the_no_focus_window (display, 
                                                          window->screen,
                                                          event->xcrossing.time);
                }
              break;
            case META_FOCUS_MODE_CLICK:
              break;
            }
          
          if (window->type == META_WINDOW_DOCK)
            meta_window_raise (window);
        }
      break;
    case LeaveNotify:
      if (display->grab_op == META_GRAB_OP_COMPOSITOR)
        break;

      if (display->grab_window == window &&
          grab_op_is_mouse (display->grab_op))
        meta_window_handle_mouse_grab_op_event (window, event);
      else if (window != NULL)
        {
          if (window->type == META_WINDOW_DOCK &&
              event->xcrossing.mode != NotifyGrab &&
              event->xcrossing.mode != NotifyUngrab &&
              !window->has_focus)
            meta_window_lower (window);
        }
      break;
    case FocusIn:
    case FocusOut:
      if (window)
        {
          meta_window_notify_focus (window, event);
        }
      else if (meta_display_xwindow_is_a_no_focus_window (display,
                                                          event->xany.window))
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Focus %s event received on no_focus_window 0x%lx "
                      "mode %s detail %s\n",
                      event->type == FocusIn ? "in" :
                      event->type == FocusOut ? "out" :
                      "???",
                      event->xany.window,
                      meta_event_mode_to_string (event->xfocus.mode),
                      meta_event_detail_to_string (event->xfocus.detail));
        }
      else
        {
          MetaScreen *screen =
                meta_display_screen_for_root(display,
                                             event->xany.window);
          if (screen == NULL)
            break;

          meta_topic (META_DEBUG_FOCUS,
                      "Focus %s event received on root window 0x%lx "
                      "mode %s detail %s\n",
                      event->type == FocusIn ? "in" :
                      event->type == FocusOut ? "out" :
                      "???",
                      event->xany.window,
                      meta_event_mode_to_string (event->xfocus.mode),
                      meta_event_detail_to_string (event->xfocus.detail));
          
          if (event->type == FocusIn &&
              event->xfocus.detail == NotifyDetailNone)
            {
              meta_topic (META_DEBUG_FOCUS, 
                          "Focus got set to None, probably due to "
                          "brain-damage in the X protocol (see bug "
                          "125492).  Setting the default focus window.\n");
              meta_workspace_focus_default_window (screen->active_workspace,
                                                   NULL,
                                                   meta_display_get_current_time_roundtrip (display));
            }
          else if (event->type == FocusIn &&
              event->xfocus.mode == NotifyNormal &&
              event->xfocus.detail == NotifyInferior)
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
        MetaScreen *screen;

        screen = meta_display_screen_for_root (display,
                                               event->xcreatewindow.parent);
        if (screen)
          meta_stack_tracker_create_event (screen->stack_tracker,
                                           &event->xcreatewindow);
      }
      break;
      
    case DestroyNotify:
      {
        MetaScreen *screen;

        screen = meta_display_screen_for_root (display,
                                               event->xdestroywindow.event);
        if (screen)
          meta_stack_tracker_destroy_event (screen->stack_tracker,
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
              meta_error_trap_pop (display, FALSE);
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
              ((window->frame == NULL) || !window->frame->mapped))
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

          /* Unfocus on UnmapNotify, do this after the possible
           * window_free above so that window_free can see if window->has_focus
           * and move focus to another window
           */
          if (window)
            meta_window_notify_focus (window, event);
        }
      break;
    case MapNotify:
      /* NB: override redirect windows wont cause a map request so we
       * watch out for map notifies against any root windows too if a
       * compositor is enabled: */
      if (display->compositor && window == NULL
	  && meta_display_screen_for_root (display, event->xmap.event))
        {
          window = meta_window_new (display, event->xmap.window,
                                    FALSE);
        }
      break;
    case MapRequest:
      if (window == NULL)
        {
          window = meta_window_new (display, event->xmaprequest.window,
                                    FALSE);
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
        MetaScreen *screen;

        screen = meta_display_screen_for_root (display,
                                               event->xconfigure.event);
        if (screen)
          {
            if (screen)
              meta_stack_tracker_reparent_event (screen->stack_tracker,
                                                 &event->xreparent);
          }
      }
      break;
    case ConfigureNotify:
      if (event->xconfigure.event != event->xconfigure.window)
        {
          MetaScreen *screen;

          screen = meta_display_screen_for_root (display,
                                                 event->xconfigure.event);
          if (screen)
            meta_stack_tracker_configure_event (screen->stack_tracker,
                                                &event->xconfigure);
        }
      if (window && window->override_redirect)
	meta_window_configure_notify (window, &event->xconfigure);
      else
	/* Handle screen resize */
	{
	  MetaScreen *screen;

	  screen = meta_display_screen_for_root (display,
						 event->xconfigure.window);

	  if (screen != NULL)
	    {
#ifdef HAVE_RANDR
	      /* do the resize the official way */
	      XRRUpdateConfiguration (event);
#else
	      /* poke around in Xlib */
	      screen->xscreen->width   = event->xconfigure.width;
	      screen->xscreen->height  = event->xconfigure.height;
#endif
	      
	      meta_screen_resize (screen, 
				  event->xconfigure.width,
				  event->xconfigure.height);
	    }
	}
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
          meta_error_trap_pop (display, FALSE);
        }
      else
        {
          if (!frame_was_receiver)
            meta_window_configure_request (window, event);
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
        MetaScreen *screen;
        
        if (window && !frame_was_receiver)
          meta_window_property_notify (window, event);
        else if (property_for_window && !frame_was_receiver)
          meta_window_property_notify (property_for_window, event);

        group = meta_display_lookup_group (display,
                                           event->xproperty.window);
        if (group != NULL)
          meta_group_property_notify (group, event);
        
        screen = NULL;
        if (window == NULL &&
            group == NULL) /* window/group != NULL means it wasn't a root window */
          screen = meta_display_screen_for_root (display,
                                                 event->xproperty.window);
            
        if (screen != NULL)
          {
            if (event->xproperty.atom ==
                display->atom__NET_DESKTOP_LAYOUT)
              meta_screen_update_workspace_layout (screen);
            else if (event->xproperty.atom ==
                     display->atom__NET_DESKTOP_NAMES)
              meta_screen_update_workspace_names (screen);
#if 0
            else if (event->xproperty.atom ==
                     display->atom__NET_RESTACK_WINDOW)
              handle_net_restack_window (display, event);
#endif

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
    case SelectionClear:
      /* do this here instead of at end of function
       * so we can return
       */

      /* FIXME: Clearing display->current_time here makes no sense to
       * me; who put this here and why?
       */
      display->current_time = CurrentTime;

      process_selection_clear (display, event);
      /* Note that processing that may have resulted in
       * closing the display... so return right away.
       */
      return FALSE;
    case SelectionRequest:
      process_selection_request (display, event);
      break;
    case SelectionNotify:
      break;
    case ColormapNotify:
      if (window && !frame_was_receiver)
        window->colormap = event->xcolormap.colormap;
      break;
    case ClientMessage:
      if (window)
        {
          if (!frame_was_receiver)
            meta_window_client_message (window, event);
        }
      else
        {
          MetaScreen *screen;

          screen = meta_display_screen_for_root (display,
                                                 event->xclient.window);
          
          if (screen)
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

                  workspace =
                    meta_screen_get_workspace_by_index (screen,
                                                        space);

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
                    meta_screen_show_desktop (screen, timestamp);
                  else
                    {
                      meta_screen_unshow_desktop (screen);
                      meta_workspace_focus_default_window (screen->active_workspace, NULL, timestamp);
                    }
                }
              else if (event->xclient.message_type ==
                       display->atom__MUTTER_RESTART_MESSAGE)
                {
                  meta_verbose ("Received restart request\n");
                  meta_restart ();
                }
              else if (event->xclient.message_type ==
                       display->atom__MUTTER_RELOAD_THEME_MESSAGE)
                {
                  meta_verbose ("Received reload theme request\n");
                  meta_ui_set_current_theme (meta_prefs_get_theme (),
                                             TRUE);
                  meta_display_retheme_all ();
                }
              else if (event->xclient.message_type ==
                       display->atom__MUTTER_SET_KEYBINDINGS_MESSAGE)
                {
                  meta_verbose ("Received set keybindings request = %d\n",
                                (int) event->xclient.data.l[0]);
                  meta_set_keybindings_disabled (!event->xclient.data.l[0]);
                }
              else if (event->xclient.message_type ==
                       display->atom__MUTTER_TOGGLE_VERBOSE)
                {
                  meta_verbose ("Received toggle verbose message\n");
                  meta_set_verbose (!meta_is_verbose ());
                }
	      else if (event->xclient.message_type ==
		       display->atom_WM_PROTOCOLS) 
		{
                  meta_verbose ("Received WM_PROTOCOLS message\n");
                  
		  if ((Atom)event->xclient.data.l[0] == display->atom__NET_WM_PING)
                    {
                      process_pong_message (display, event);

                      /* We don't want ping reply events going into
                       * the GTK+ event loop because gtk+ will treat
                       * them as ping requests and send more replies.
                       */
                      filter_out_event = TRUE;
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
                                         xkb_ev->time - 1000))
                {
                  display->last_bell_time = xkb_ev->time;
                  meta_bell_notify (display, xkb_ev);
                }
	      break;
	    }
	}
#endif
      break;
    }

  if (display->compositor && !bypass_compositor)
    {
      if (meta_compositor_process_event (display->compositor,
                                         event,
                                         window))
        filter_out_event = TRUE;
    }
  
  display->current_time = CurrentTime;
  return filter_out_event;
}

/* Return the window this has to do with, if any, rather
 * than the frame or root window that was selecting
 * for substructure
 */
static Window
event_get_modified_window (MetaDisplay *display,
                           XEvent *event)
{
  switch (event->type)
    {
    case KeyPress:
    case KeyRelease:
    case ButtonPress:
    case ButtonRelease:
    case MotionNotify:
    case FocusIn:
    case FocusOut:
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
    case EnterNotify:
    case LeaveNotify:
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
#ifdef HAVE_SHAPE
      if (META_DISPLAY_HAS_SHAPE (display) && 
          event->type == (display->shape_event_base + ShapeNotify))
        {
          XShapeEvent *sev = (XShapeEvent*) event;
          return sev->window;
        }
#endif

      return None;
    }
}

static guint32
event_get_time (MetaDisplay *display,
                XEvent      *event)
{
  switch (event->type)
    {
    case KeyPress:
    case KeyRelease:
      return event->xkey.time;
      
    case ButtonPress:
    case ButtonRelease:
      return event->xbutton.time;
      
    case MotionNotify:
      return event->xmotion.time;

    case PropertyNotify:
      return event->xproperty.time;

    case SelectionClear:
    case SelectionRequest:
    case SelectionNotify:
      return event->xselection.time;

    case EnterNotify:
    case LeaveNotify:
      return event->xcrossing.time;

    case FocusIn:
    case FocusOut:
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

#ifdef WITH_VERBOSE_MODE
const char*
meta_event_detail_to_string (int d)
{
  const char *detail = "???";
  switch (d)
    {
      /* We are an ancestor in the A<->B focus change relationship */
    case NotifyAncestor:
      detail = "NotifyAncestor";
      break;
    case NotifyDetailNone:
      detail = "NotifyDetailNone";
      break;
      /* We are a descendant in the A<->B focus change relationship */
    case NotifyInferior:
      detail = "NotifyInferior";
      break;
    case NotifyNonlinear:
      detail = "NotifyNonlinear";
      break;
    case NotifyNonlinearVirtual:
      detail = "NotifyNonlinearVirtual";
      break;
    case NotifyPointer:
      detail = "NotifyPointer";
      break;
    case NotifyPointerRoot:
      detail = "NotifyPointerRoot";
      break;
    case NotifyVirtual:
      detail = "NotifyVirtual";
      break;
    }

  return detail;
}
#endif /* WITH_VERBOSE_MODE */

#ifdef WITH_VERBOSE_MODE
const char*
meta_event_mode_to_string (int m)
{
  const char *mode = "???";
  switch (m)
    {
    case NotifyNormal:
      mode = "NotifyNormal";
      break;
    case NotifyGrab:
      mode = "NotifyGrab";
      break;
    case NotifyUngrab:
      mode = "NotifyUngrab";
      break;
      /* not sure any X implementations are missing this, but
       * it seems to be absent from some docs.
       */
#ifdef NotifyWhileGrabbed
    case NotifyWhileGrabbed:
      mode = "NotifyWhileGrabbed";
      break;
#endif
    }

  return mode;
}
#endif /* WITH_VERBOSE_MODE */

#ifdef WITH_VERBOSE_MODE
static const char*
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
#endif /* WITH_VERBOSE_MODE */

#ifdef WITH_VERBOSE_MODE
static char*
key_event_description (Display *xdisplay,
                       XEvent  *event)
{
  KeySym keysym;
  const char *str;
  
  keysym = XKeycodeToKeysym (xdisplay, event->xkey.keycode, 0);  

  str = XKeysymToString (keysym);
  
  return g_strdup_printf ("Key '%s' state 0x%x", 
                          str ? str : "none", event->xkey.state);
}
#endif /* WITH_VERBOSE_MODE */

#ifdef HAVE_XSYNC
#ifdef WITH_VERBOSE_MODE
static gint64
sync_value_to_64 (const XSyncValue *value)
{
  gint64 v;

  v = XSyncValueLow32 (*value);
  v |= (((gint64)XSyncValueHigh32 (*value)) << 32);
  
  return v;
}
#endif /* WITH_VERBOSE_MODE */

#ifdef WITH_VERBOSE_MODE
static const char*
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
#endif /* WITH_VERBOSE_MODE */

#endif /* HAVE_XSYNC */

#ifdef WITH_VERBOSE_MODE
static void
meta_spew_event (MetaDisplay *display,
                 XEvent      *event)
{
  const char *name = NULL;
  char *extra = NULL;
  char *winname;
  MetaScreen *screen;

  if (!meta_is_verbose())
    return;
  
  /* filter overnumerous events */
  if (event->type == Expose || event->type == MotionNotify ||
      event->type == NoExpose)
    return;
      
  switch (event->type)
    {
    case KeyPress:
      name = "KeyPress";
      extra = key_event_description (display->xdisplay, event);
      break;
    case KeyRelease:
      name = "KeyRelease";
      extra = key_event_description (display->xdisplay, event);
      break;
    case ButtonPress:
      name = "ButtonPress";
      extra = g_strdup_printf ("button %u state 0x%x x %d y %d root 0x%lx same_screen %d",
                               event->xbutton.button,
                               event->xbutton.state,
                               event->xbutton.x,
                               event->xbutton.y,
                               event->xbutton.root,
                               event->xbutton.same_screen);
      break;
    case ButtonRelease:
      name = "ButtonRelease";
      extra = g_strdup_printf ("button %u state 0x%x x %d y %d root 0x%lx same_screen %d",
                               event->xbutton.button,
                               event->xbutton.state,
                               event->xbutton.x,
                               event->xbutton.y,
                               event->xbutton.root,
                               event->xbutton.same_screen);
      break;
    case MotionNotify:
      name = "MotionNotify";
      extra = g_strdup_printf ("win: 0x%lx x: %d y: %d",
                               event->xmotion.window,
                               event->xmotion.x,
                               event->xmotion.y);
      break;
    case EnterNotify:
      name = "EnterNotify";
      extra = g_strdup_printf ("win: 0x%lx root: 0x%lx subwindow: 0x%lx mode: %s detail: %s focus: %d x: %d y: %d",
                               event->xcrossing.window,
                               event->xcrossing.root,
                               event->xcrossing.subwindow,
                               meta_event_mode_to_string (event->xcrossing.mode),
                               meta_event_detail_to_string (event->xcrossing.detail),
                               event->xcrossing.focus,
                               event->xcrossing.x,
                               event->xcrossing.y);
      break;
    case LeaveNotify:
      name = "LeaveNotify";
      extra = g_strdup_printf ("win: 0x%lx root: 0x%lx subwindow: 0x%lx mode: %s detail: %s focus: %d x: %d y: %d",
                               event->xcrossing.window,
                               event->xcrossing.root,
                               event->xcrossing.subwindow,
                               meta_event_mode_to_string (event->xcrossing.mode),
                               meta_event_detail_to_string (event->xcrossing.detail),
                               event->xcrossing.focus,
                               event->xcrossing.x,
                               event->xcrossing.y);
      break;
    case FocusIn:
      name = "FocusIn";
      extra = g_strdup_printf ("detail: %s mode: %s\n",
                               meta_event_detail_to_string (event->xfocus.detail),
                               meta_event_mode_to_string (event->xfocus.mode));
      break;
    case FocusOut:
      name = "FocusOut";
      extra = g_strdup_printf ("detail: %s mode: %s\n",
                               meta_event_detail_to_string (event->xfocus.detail),
                               meta_event_mode_to_string (event->xfocus.mode));
      break;
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
        meta_error_trap_pop (display, TRUE);

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
        meta_error_trap_pop (display, TRUE);
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
#ifdef HAVE_XSYNC
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
#endif /* HAVE_XSYNC */
#ifdef HAVE_SHAPE
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
#endif /* HAVE_SHAPE */      
        {
          name = "(Unknown event)";
          extra = g_strdup_printf ("type: %d", event->xany.type);
        }
      break;
    }

  screen = meta_display_screen_for_root (display, event->xany.window);
      
  if (screen)
    winname = g_strdup_printf ("root %d", screen->number);
  else
    winname = g_strdup_printf ("0x%lx", event->xany.window);
      
  meta_topic (META_DEBUG_EVENTS,
              "%s on %s%s %s %sserial %lu\n", name, winname,
              extra ? ":" : "", extra ? extra : "",
              event->xany.send_event ? "SEND " : "",
              event->xany.serial);

  g_free (winname);

  if (extra)
    g_free (extra);
}
#endif /* WITH_VERBOSE_MODE */

MetaWindow*
meta_display_lookup_x_window (MetaDisplay *display,
                              Window       xwindow)
{
  return g_hash_table_lookup (display->window_ids, &xwindow);
}

void
meta_display_register_x_window (MetaDisplay *display,
                                Window      *xwindowp,
                                MetaWindow  *window)
{
  g_return_if_fail (g_hash_table_lookup (display->window_ids, xwindowp) == NULL);
  
  g_hash_table_insert (display->window_ids, xwindowp, window);
}

void
meta_display_unregister_x_window (MetaDisplay *display,
                                  Window       xwindow)
{
  g_return_if_fail (g_hash_table_lookup (display->window_ids, &xwindow) != NULL);

  g_hash_table_remove (display->window_ids, &xwindow);

  /* Remove any pending pings */
  remove_pending_pings_for_window (display, xwindow);
}

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
 * Returns %TRUE iff window is one of mutter's internal "no focus" windows
 * (there is one per screen) which will have the focus when there is no
 * actual client window focused.
 */
gboolean
meta_display_xwindow_is_a_no_focus_window (MetaDisplay *display,
                                           Window xwindow)
{
  gboolean is_a_no_focus_window = FALSE;
  GSList *temp = display->screens;
  while (temp != NULL) {
    MetaScreen *screen = temp->data;
    if (screen->no_focus_window == xwindow) {
      is_a_no_focus_window = TRUE;
      break;
    }
    temp = temp->next;
  }

  return is_a_no_focus_window;
}

Cursor
meta_display_create_x_cursor (MetaDisplay *display,
                              MetaCursor cursor)
{
  Cursor xcursor;
  guint glyph;

  switch (cursor)
    {
    case META_CURSOR_DEFAULT:
      glyph = XC_left_ptr;
      break;
    case META_CURSOR_NORTH_RESIZE:
      glyph = XC_top_side;
      break;
    case META_CURSOR_SOUTH_RESIZE:
      glyph = XC_bottom_side;
      break;
    case META_CURSOR_WEST_RESIZE:
      glyph = XC_left_side;
      break;
    case META_CURSOR_EAST_RESIZE:
      glyph = XC_right_side;
      break;
    case META_CURSOR_SE_RESIZE:
      glyph = XC_bottom_right_corner;
      break;
    case META_CURSOR_SW_RESIZE:
      glyph = XC_bottom_left_corner;
      break;
    case META_CURSOR_NE_RESIZE:
      glyph = XC_top_right_corner;
      break;
    case META_CURSOR_NW_RESIZE:
      glyph = XC_top_left_corner;
      break;
    case META_CURSOR_MOVE_OR_RESIZE_WINDOW:
      glyph = XC_fleur;
      break;
    case META_CURSOR_BUSY:
      glyph = XC_watch;
      break;
      
    default:
      g_assert_not_reached ();
      glyph = 0; /* silence compiler */
      break;
    }
  
  xcursor = XCreateFontCursor (display->xdisplay, glyph);

  return xcursor;
}

static Cursor
xcursor_for_op (MetaDisplay *display,
                MetaGrabOp   op)
{
  MetaCursor cursor = META_CURSOR_DEFAULT;
  
  switch (op)
    {
    case META_GRAB_OP_RESIZING_SE:
    case META_GRAB_OP_KEYBOARD_RESIZING_SE:
      cursor = META_CURSOR_SE_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_S:
    case META_GRAB_OP_KEYBOARD_RESIZING_S:
      cursor = META_CURSOR_SOUTH_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_SW:
    case META_GRAB_OP_KEYBOARD_RESIZING_SW:
      cursor = META_CURSOR_SW_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_N:
    case META_GRAB_OP_KEYBOARD_RESIZING_N:
      cursor = META_CURSOR_NORTH_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_NE:
    case META_GRAB_OP_KEYBOARD_RESIZING_NE:
      cursor = META_CURSOR_NE_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_NW:
    case META_GRAB_OP_KEYBOARD_RESIZING_NW:
      cursor = META_CURSOR_NW_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_W:
    case META_GRAB_OP_KEYBOARD_RESIZING_W:
      cursor = META_CURSOR_WEST_RESIZE;
      break;
    case META_GRAB_OP_RESIZING_E:
    case META_GRAB_OP_KEYBOARD_RESIZING_E:
      cursor = META_CURSOR_EAST_RESIZE;
      break;
    case META_GRAB_OP_MOVING:
    case META_GRAB_OP_KEYBOARD_MOVING:
    case META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN:
      cursor = META_CURSOR_MOVE_OR_RESIZE_WINDOW;
      break;
      
    default:
      break;
    }

  if (cursor == META_CURSOR_DEFAULT)
    return None;
  return meta_display_create_x_cursor (display, cursor);
}

void
meta_display_set_grab_op_cursor (MetaDisplay *display,
                                 MetaScreen  *screen,
                                 MetaGrabOp   op,
                                 gboolean     change_pointer,
                                 Window       grab_xwindow,
                                 guint32      timestamp)
{
  Cursor cursor;

  cursor = xcursor_for_op (display, op);

#define GRAB_MASK (PointerMotionMask |                          \
                   ButtonPressMask | ButtonReleaseMask |        \
		   EnterWindowMask | LeaveWindowMask)

  if (change_pointer)
    {
      meta_error_trap_push_with_return (display);
      XChangeActivePointerGrab (display->xdisplay,
                                GRAB_MASK,
                                cursor,
                                timestamp);

      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Changed pointer with XChangeActivePointerGrab()\n");

      if (meta_error_trap_pop_with_return (display, FALSE) != Success)
        {
          meta_topic (META_DEBUG_WINDOW_OPS,
                      "Error trapped from XChangeActivePointerGrab()\n");
          if (display->grab_have_pointer)
            display->grab_have_pointer = FALSE;
        }
    }
  else
    {
      g_assert (screen != NULL);

      meta_error_trap_push (display);
      if (XGrabPointer (display->xdisplay,
                        grab_xwindow,
                        False,
                        GRAB_MASK,
                        GrabModeAsync, GrabModeAsync,
                        screen->xroot,
                        cursor,
                        timestamp) == GrabSuccess)
        {
          display->grab_have_pointer = TRUE;
          meta_topic (META_DEBUG_WINDOW_OPS,
                      "XGrabPointer() returned GrabSuccess time %u\n",
                      timestamp);
        }
      else
        {
          meta_topic (META_DEBUG_WINDOW_OPS,
                      "XGrabPointer() failed time %u\n",
                      timestamp);
        }
      meta_error_trap_pop (display, TRUE);
    }

#undef GRAB_MASK
  
  if (cursor != None)
    XFreeCursor (display->xdisplay, cursor);
}

gboolean
meta_display_begin_grab_op (MetaDisplay *display,
			    MetaScreen  *screen,
                            MetaWindow  *window,
                            MetaGrabOp   op,
                            gboolean     pointer_already_grabbed,
                            gboolean     frame_action,
                            int          button,
                            gulong       modmask,
                            guint32      timestamp,
                            int          root_x,
                            int          root_y)
{
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

  /* FIXME:
   *   If we have no MetaWindow we do our best
   *   and try to do the grab on the RootWindow.
   *   This will fail if anyone else has any
   *   key grab on the RootWindow.
   */
  if (window)
    grab_xwindow = window->frame ? window->frame->xwindow : window->xwindow;
  else
    grab_xwindow = screen->xroot;

  display->grab_have_pointer = FALSE;
  
  if (pointer_already_grabbed)
    display->grab_have_pointer = TRUE;
  
  meta_display_set_grab_op_cursor (display, screen, op, FALSE, grab_xwindow,
                                   timestamp);

  if (!display->grab_have_pointer)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "XGrabPointer() failed\n");
      return FALSE;
    }

  /* Grab keys for keyboard ops and mouse move/resizes; see #126497 */
  if (grab_op_is_keyboard (op) || grab_op_is_mouse_only (op))
    {
      if (window)
        display->grab_have_keyboard =
                     meta_window_grab_all_keys (window, timestamp);

      else
        display->grab_have_keyboard =
                     meta_screen_grab_all_keys (screen, timestamp);
      
      if (!display->grab_have_keyboard)
        {
          meta_topic (META_DEBUG_WINDOW_OPS,
                      "grabbing all keys failed, ungrabbing pointer\n");
          XUngrabPointer (display->xdisplay, timestamp);
          display->grab_have_pointer = FALSE;
          return FALSE;
        }
    }
  
  display->grab_op = op;
  display->grab_window = window;
  display->grab_screen = screen;
  display->grab_xwindow = grab_xwindow;
  display->grab_button = button;
  display->grab_mask = modmask;
  display->grab_anchor_root_x = root_x;
  display->grab_anchor_root_y = root_y;
  display->grab_latest_motion_x = root_x;
  display->grab_latest_motion_y = root_y;
  display->grab_last_moveresize_time.tv_sec = 0;
  display->grab_last_moveresize_time.tv_usec = 0;
  display->grab_motion_notify_time = 0;
  display->grab_old_window_stacking = NULL;
#ifdef HAVE_XSYNC
  display->grab_sync_request_alarm = None;
  display->grab_last_user_action_was_snap = FALSE;
#endif
  display->grab_frame_action = frame_action;

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
          XSyncAlarmAttributes values;
	  XSyncValue init;

          meta_error_trap_push_with_return (display);

	  /* Set the counter to 0, so we know that the application's
	   * responses to the client messages will always trigger
	   * a PositiveTransition
	   */
	  
	  XSyncIntToValue (&init, 0);
	  XSyncSetCounter (display->xdisplay,
			   display->grab_window->sync_request_counter, init);
	  
	  display->grab_window->sync_request_serial = 0;
	  display->grab_window->sync_request_time.tv_sec = 0;
	  display->grab_window->sync_request_time.tv_usec = 0;
	  
          values.trigger.counter = display->grab_window->sync_request_counter;
          values.trigger.value_type = XSyncAbsolute;
          values.trigger.test_type = XSyncPositiveTransition;
          XSyncIntToValue (&values.trigger.wait_value,
			   display->grab_window->sync_request_serial + 1);
	  
          /* After triggering, increment test_value by this.
           * (NOT wait_value above)
           */
          XSyncIntToValue (&values.delta, 1);
	  
          /* we want events (on by default anyway) */
          values.events = True;
          
          display->grab_sync_request_alarm = XSyncCreateAlarm (display->xdisplay,
                                                         XSyncCACounter |
                                                         XSyncCAValueType |
                                                         XSyncCAValue |
                                                         XSyncCATestType |
                                                         XSyncCADelta |
                                                         XSyncCAEvents,
                                                         &values);

          if (meta_error_trap_pop_with_return (display, FALSE) != Success)
	    display->grab_sync_request_alarm = None;

          meta_topic (META_DEBUG_RESIZING,
                      "Created update alarm 0x%lx\n",
                      display->grab_sync_request_alarm);
        }
#endif
    }
  
  meta_topic (META_DEBUG_WINDOW_OPS,
              "Grab op %u on window %s successful\n",
              display->grab_op, window ? window->desc : "(null)");

  g_assert (display->grab_window != NULL || display->grab_screen != NULL);
  g_assert (display->grab_op != META_GRAB_OP_NONE);

  /* Save the old stacking */
  if (GRAB_OP_IS_WINDOW_SWITCH (display->grab_op))
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Saving old stack positions; old pointer was %p.\n",
                  display->grab_old_window_stacking);
      display->grab_old_window_stacking = 
        meta_stack_get_positions (screen->stack);
    }

  if (display->grab_window)
    {
      meta_window_refresh_resize_popup (display->grab_window);
    }
  
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

  if (GRAB_OP_IS_WINDOW_SWITCH (display->grab_op) ||
      display->grab_op == META_GRAB_OP_KEYBOARD_WORKSPACE_SWITCHING)
    {
      if (GRAB_OP_IS_WINDOW_SWITCH (display->grab_op))
        meta_screen_tab_popup_destroy (display->grab_screen);
      else
        meta_screen_workspace_popup_destroy (display->grab_screen);

      /* If the ungrab here causes an EnterNotify, ignore it for
       * sloppy focus
       */
      display->ungrab_should_not_cause_focus_window = display->grab_xwindow;
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
      XUngrabPointer (display->xdisplay, timestamp);
    }

  if (display->grab_have_keyboard)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Ungrabbing all keys timestamp %u\n", timestamp);
      if (display->grab_window)
        meta_window_ungrab_all_keys (display->grab_window, timestamp);
      else
        meta_screen_ungrab_all_keys (display->grab_screen, timestamp);
    }

#ifdef HAVE_XSYNC
  if (display->grab_sync_request_alarm != None)
    {
      XSyncDestroyAlarm (display->xdisplay,
                         display->grab_sync_request_alarm);
      display->grab_sync_request_alarm = None;
    }
#endif /* HAVE_XSYNC */
  
  display->grab_window = NULL;
  display->grab_screen = NULL;
  display->grab_xwindow = None;
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
}

/**
 * meta_display_get_grab_op:
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

  meta_verbose ("%s 0x%lx sync = %d button = %d modmask 0x%x\n",
                grab ? "Grabbing" : "Ungrabbing",
                xwindow,
                sync, button, modmask);
  
  meta_error_trap_push (display);
  
  ignored_mask = 0;
  while (ignored_mask <= display->ignored_modifier_mask)
    {
      if (ignored_mask & ~(display->ignored_modifier_mask))
        {
          /* Not a combination of ignored modifiers
           * (it contains some non-ignored modifiers)
           */
          ++ignored_mask;
          continue;
        }

      if (meta_is_debugging ())
        meta_error_trap_push_with_return (display);

      /* GrabModeSync means freeze until XAllowEvents */
      
      if (grab)
        XGrabButton (display->xdisplay, button, modmask | ignored_mask,
                     xwindow, False,
                     ButtonPressMask | ButtonReleaseMask |    
                     PointerMotionMask | PointerMotionHintMask,
                     sync ? GrabModeSync : GrabModeAsync,
                     GrabModeAsync,
                     False, None);
      else
        XUngrabButton (display->xdisplay, button, modmask | ignored_mask,
                       xwindow);

      if (meta_is_debugging ())
        {
          int result;
          
          result = meta_error_trap_pop_with_return (display, FALSE);
          
          if (result != Success)
            meta_verbose ("Failed to %s button %d with mask 0x%x for window 0x%lx error code %d\n",
                          grab ? "grab" : "ungrab",
                          button, modmask | ignored_mask, xwindow, result);
        }
      
      ++ignored_mask;
    }

  meta_error_trap_pop (display, FALSE);
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
  if (meta_prefs_get_focus_mode () != META_FOCUS_MODE_CLICK)
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
  GSList *tmp;
  
  gulong data[1];

  if (display->focus_window)
    data[0] = display->focus_window->xwindow;
  else
    data[0] = None;
  
  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;
      
      meta_error_trap_push (display);
      XChangeProperty (display->xdisplay, screen->xroot,
                       display->atom__NET_ACTIVE_WINDOW,
                       XA_WINDOW,
                       32, PropModeReplace, (guchar*) data, 1);

      meta_error_trap_pop (display, FALSE);

      tmp = tmp->next;
    }
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
      if (window->frame)
        {
          window->frame->need_reapply_frame_shape = TRUE;
          
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
#ifdef HAVE_XCURSOR     
  GSList *tmp;

  MetaDisplay *display = meta_get_display ();

  XcursorSetTheme (display->xdisplay, theme);
  XcursorSetDefaultSize (display->xdisplay, size);

  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;
	  	  
      meta_screen_update_cursor (screen);

      tmp = tmp->next;
    }

#endif
}

/**
 * Stores whether syncing is currently enabled.
 */
static gboolean is_syncing = FALSE;

/**
 * Returns whether X synchronisation is currently enabled.
 *
 * \return true if we must wait for events whenever we send X requests;
 * false otherwise.
 *
 * \bug This is *only* called by meta_display_open, but by that time
 * we have already turned syncing on or off on startup, and we don't
 * have any way to do so while Mutter is running, so it's rather
 * pointless.
 */
gboolean
meta_is_syncing (void)
{
  return is_syncing;
}

/**
 * A handy way to turn on synchronisation on or off for every display.
 *
 * \bug Of course there is only one display ever anyway, so this can
 * be rather hugely simplified.
 */
void
meta_set_syncing (gboolean setting)
{
  if (setting != is_syncing)
    {
      is_syncing = setting;

      XSynchronize (meta_get_display ()->xdisplay, is_syncing);
    }
}

/**
 * How long, in milliseconds, we should wait after pinging a window
 * before deciding it's not going to get back to us.
 */
#define PING_TIMEOUT_DELAY 2250

/**
 * Does whatever it is we decided to do when a window didn't respond
 * to a ping. We also remove the ping from the display's list of
 * pending pings. This function is called by the event loop when the timeout
 * times out which we created at the start of the ping.
 *
 * \param data All the information about this ping. It is a MetaPingData
 *             cast to a void* in order to be passable to a timeout function.
 *             This function will also free this parameter.
 *
 * \return Always returns false, because this function is called as a
 *         timeout and we don't want to run the timer again.
 *
 * \ingroup pings
 */
static gboolean
meta_display_ping_timeout (gpointer data)
{
  MetaPingData *ping_data;

  ping_data = data;

  ping_data->ping_timeout_id = 0;

  meta_topic (META_DEBUG_PING,
              "Ping %u on window %lx timed out\n",
              ping_data->timestamp, ping_data->xwindow);
  
  (* ping_data->ping_timeout_func) (ping_data->display, ping_data->xwindow,
                                    ping_data->timestamp, ping_data->user_data);

  ping_data->display->pending_pings =
    g_slist_remove (ping_data->display->pending_pings,
                    ping_data);
  ping_data_free (ping_data);
  
  return FALSE;
}

/**
 * Sends a ping request to a window. The window must respond to
 * the request within a certain amount of time. If it does, we
 * will call one callback; if the time passes and we haven't had
 * a response, we call a different callback. The window must have
 * the hint showing that it can respond to a ping; if it doesn't,
 * we call the "got a response" callback immediately and return.
 * This function returns straight away after setting things up;
 * the callbacks will be called from the event loop.
 *
 * \param display  The MetaDisplay that the window is on
 * \param window   The MetaWindow to send the ping to
 * \param timestamp The timestamp of the ping. Used for uniqueness.
 *                  Cannot be CurrentTime; use a real timestamp!
 * \param ping_reply_func The callback to call if we get a response.
 * \param ping_timeout_func The callback to call if we don't get a response.
 * \param user_data Arbitrary data that will be passed to the callback
 *                  function. (In practice it's often a pointer to
 *                  the window.)
 *
 * \bug This should probably be a method on windows, rather than displays
 *      for one of their windows.
 *
 * \ingroup pings
 */
void
meta_display_ping_window (MetaDisplay       *display,
			  MetaWindow        *window,
			  guint32            timestamp,
			  MetaWindowPingFunc ping_reply_func,
			  MetaWindowPingFunc ping_timeout_func,
			  gpointer           user_data)
{
  MetaPingData *ping_data;

  if (timestamp == CurrentTime)
    {
      meta_warning ("Tried to ping a window with CurrentTime! Not allowed.\n");
      return;
    }

  if (!window->net_wm_ping)
    {
      if (ping_reply_func)
        (* ping_reply_func) (display, window->xwindow, timestamp, user_data);

      return;
    }
  
  ping_data = g_new (MetaPingData, 1);
  ping_data->display = display;
  ping_data->xwindow = window->xwindow;
  ping_data->timestamp = timestamp;
  ping_data->ping_reply_func = ping_reply_func;
  ping_data->ping_timeout_func = ping_timeout_func;
  ping_data->user_data = user_data;
  ping_data->ping_timeout_id = g_timeout_add (PING_TIMEOUT_DELAY,
					      meta_display_ping_timeout,
					      ping_data);
  
  display->pending_pings = g_slist_prepend (display->pending_pings, ping_data);

  meta_topic (META_DEBUG_PING,
              "Sending ping with timestamp %u to window %s\n",
              timestamp, window->desc);
  meta_window_send_icccm_message (window,
                                  display->atom__NET_WM_PING,
                                  timestamp);
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
      int top = 0;
      int bottom = 0;
      int left = 0;
      int right = 0;

      MetaScreen *screen;

      screen = meta_display_screen_for_xwindow (display,
                                                event->xclient.window);
      if (screen == NULL)
        {
          meta_warning ("Received request to set _NET_FRAME_EXTENTS "
                        "on 0x%lx which is on a screen we are not managing\n",
                        event->xclient.window);
          meta_XFree (hints);
          return;
        }

      /* Return estimated frame extents for a normal window. */
      meta_ui_theme_get_frame_borders (screen->ui,
                                       META_FRAME_TYPE_NORMAL,
                                       0,
                                       &top,
                                       &bottom,
                                       &left,
                                       &right);

      data[0] = left;
      data[1] = right;
      data[2] = top;
      data[3] = bottom;
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
  meta_error_trap_pop (display, FALSE);

  meta_XFree (hints);
}

/**
 * Process the pong (the response message) from the ping we sent
 * to the window. This involves removing the timeout, calling the
 * reply handler function, and freeing memory.
 *
 * \param display  the display we got the pong from
 * \param event    the XEvent which is a pong; we can tell which
 *                 ping it corresponds to because it bears the
 *                 same timestamp.
 *
 * \ingroup pings
 */
static void
process_pong_message (MetaDisplay    *display,
                      XEvent         *event)
{
  GSList *tmp;
  guint32 timestamp = event->xclient.data.l[1];

  meta_topic (META_DEBUG_PING, "Received a pong with timestamp %u\n",
              timestamp);
  
  for (tmp = display->pending_pings; tmp; tmp = tmp->next)
    {
      MetaPingData *ping_data = tmp->data;
			  
      if (timestamp == ping_data->timestamp)
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
          (* ping_data->ping_reply_func) (display, 
                                          ping_data->xwindow,
                                          ping_data->timestamp, 
                                          ping_data->user_data);
			      
          ping_data_free (ping_data);

          break;
        }
    }
}

/**
 * Finds whether a window has any pings waiting on it.
 *
 * \param display The MetaDisplay of the window.
 * \param window  The MetaWindow whose pings we want to know about.
 *
 * \return True if there is at least one ping which has been sent
 *         to the window without getting a response; false otherwise.
 *
 * \bug This should probably be a method on windows, rather than displays
 *      for one of their windows.
 *
 * \ingroup pings
 */
gboolean
meta_display_window_has_pending_pings (MetaDisplay *display,
				       MetaWindow  *window)
{
  GSList *tmp;

  for (tmp = display->pending_pings; tmp; tmp = tmp->next)
    {
      MetaPingData *ping_data = tmp->data;

      if (ping_data->xwindow == window->xwindow) 
        return TRUE;
    }

  return FALSE;
}

MetaGroup*
get_focussed_group (MetaDisplay *display)
{
  if (display->focus_window)
    return display->focus_window->group;
  else
    return NULL;
}

#define IN_TAB_CHAIN(w,t) (((t) == META_TAB_LIST_NORMAL && META_WINDOW_IN_NORMAL_TAB_CHAIN (w)) \
    || ((t) == META_TAB_LIST_DOCKS && META_WINDOW_IN_DOCK_TAB_CHAIN (w)) \
    || ((t) == META_TAB_LIST_GROUP && META_WINDOW_IN_GROUP_TAB_CHAIN (w, get_focussed_group(w->display))))

static MetaWindow*
find_tab_forward (MetaDisplay   *display,
                  MetaTabList    type,
                  MetaScreen    *screen, 
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

      if (window->screen == screen &&
	  IN_TAB_CHAIN (window, type))
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
                   MetaScreen    *screen, 
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

      if (window->screen == screen &&
	  IN_TAB_CHAIN (window, type))
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

GList*
meta_display_get_tab_list (MetaDisplay   *display,
                           MetaTabList    type,
                           MetaScreen    *screen,
                           MetaWorkspace *workspace)
{
  GList *tab_list;

  g_return_val_if_fail (workspace != NULL, NULL);

  /* Windows sellout mode - MRU order. Collect unminimized windows
   * then minimized so minimized windows aren't in the way so much.
   */
  {
    GList *tmp;
    
    tab_list = NULL;
    tmp = workspace->mru_list;
    while (tmp != NULL)
      {
        MetaWindow *window = tmp->data;
        
        if (!window->minimized &&
            window->screen == screen &&
            IN_TAB_CHAIN (window, type))
          tab_list = g_list_prepend (tab_list, window);
        
        tmp = tmp->next;
      }
  }

  {
    GList *tmp;
    
    tmp = workspace->mru_list;
    while (tmp != NULL)
      {
        MetaWindow *window = tmp->data;
        
        if (window->minimized &&
            window->screen == screen &&
            IN_TAB_CHAIN (window, type))
          tab_list = g_list_prepend (tab_list, window);
        
        tmp = tmp->next;
      }
  }

  tab_list = g_list_reverse (tab_list);

  {
    GSList *tmp;
    MetaWindow *l_window;

    tmp = meta_display_list_windows (display, META_LIST_DEFAULT);

    /* Go through all windows */
    while (tmp != NULL)
      {
        l_window=tmp->data;

        /* Check to see if it demands attention */
        if (l_window->wm_state_demands_attention && 
            l_window->workspace!=workspace &&
            IN_TAB_CHAIN (l_window, type)) 
          {
            /* if it does, add it to the popup */
            tab_list = g_list_prepend (tab_list, l_window);
          }

        tmp = tmp->next;
      } /* End while tmp!=NULL */
  }
  
  return tab_list;
}

MetaWindow*
meta_display_get_tab_next (MetaDisplay   *display,
                           MetaTabList    type,
                           MetaScreen    *screen,
                           MetaWorkspace *workspace,
                           MetaWindow    *window,
                           gboolean       backward)
{
  gboolean skip;
  GList *tab_list;
  MetaWindow *ret;
  tab_list = meta_display_get_tab_list(display,
                                       type,
                                       screen,
                                       workspace);

  if (tab_list == NULL)
    return NULL;
  
  if (window != NULL)
    {
      g_assert (window->display == display);
      
      if (backward)
        ret = find_tab_backward (display, type, screen, workspace,
                                 g_list_find (tab_list,
                                              window),
                                 TRUE);
      else
        ret = find_tab_forward (display, type, screen, workspace,
                                g_list_find (tab_list,
                                             window),
                                TRUE);
    }
  else
    {
      skip = display->focus_window != NULL && 
             tab_list->data == display->focus_window;
      if (backward)
        ret = find_tab_backward (display, type, screen, workspace,
                                 tab_list, skip);
      else
        ret = find_tab_forward (display, type, screen, workspace,
                                tab_list, skip);
    }

  g_list_free (tab_list);
  return ret;
}

MetaWindow*
meta_display_get_tab_current (MetaDisplay   *display,
                              MetaTabList    type,
                              MetaScreen    *screen,
                              MetaWorkspace *workspace)
{
  MetaWindow *window;

  window = display->focus_window;
  
  if (window != NULL &&
      window->screen == screen &&
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

static MetaScreen*
find_screen_for_selection (MetaDisplay *display,
                           Window       owner,
                           Atom         selection)
{  
  GSList *tmp;  
  
  tmp = display->screens;
  while (tmp != NULL)
    {
      MetaScreen *screen = tmp->data;
      
      if (screen->wm_sn_selection_window == owner &&
          screen->wm_sn_atom == selection)
        return screen;
  
      tmp = tmp->next;
    }

  return NULL;
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

  meta_error_trap_push_with_return (display);
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
      meta_error_trap_pop_with_return (display, FALSE);
      return FALSE;
    }
  
  if (meta_error_trap_pop_with_return (display, FALSE) != Success)
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
  XSelectionEvent reply;
  MetaScreen *screen;

  screen = find_screen_for_selection (display,
                                      event->xselectionrequest.owner,
                                      event->xselectionrequest.selection);

  if (screen == NULL)
    {
      char *str;
      
      meta_error_trap_push (display);
      str = XGetAtomName (display->xdisplay,
                          event->xselectionrequest.selection);
      meta_error_trap_pop (display, TRUE);
      
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

          meta_error_trap_push_with_return (display);
          if (XGetWindowProperty (display->xdisplay,
                                  event->xselectionrequest.requestor,
                                  event->xselectionrequest.property, 0, 256, False,
                                  display->atom_ATOM_PAIR,
                                  &type, &format, &num, &rest, &data) != Success)
            {
              meta_error_trap_pop_with_return (display, TRUE);
              return;
            }
          
          if (meta_error_trap_pop_with_return (display, TRUE) == Success)
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
              meta_error_trap_pop (display, FALSE);
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
  /* We need to unmanage the screen on which we lost the selection */
  MetaScreen *screen;

  screen = find_screen_for_selection (display,
                                      event->xselectionclear.window,
                                      event->xselectionclear.selection);
  

  if (screen != NULL)
    {
      meta_verbose ("Got selection clear for screen %d on display %s\n",
                    screen->number, display->name);
      
      meta_display_unmanage_screen (display, 
                                    screen,
                                    event->xselectionclear.time);

      /* display and screen may both be invalid memory... */
      
      return;
    }

  {
    char *str;
            
    meta_error_trap_push (display);
    str = XGetAtomName (display->xdisplay,
                        event->xselectionclear.selection);
    meta_error_trap_pop (display, TRUE);

    meta_verbose ("Selection clear with selection %s window 0x%lx not a WM_Sn selection we recognize\n",
                  str ? str : "(bad atom)", event->xselectionclear.window);

    meta_XFree (str);
  }
}

void
meta_display_unmanage_screen (MetaDisplay *display,
                              MetaScreen  *screen,
                              guint32      timestamp)
{
  meta_verbose ("Unmanaging screen %d on display %s\n",
                screen->number, display->name);
  
  g_return_if_fail (g_slist_find (display->screens, screen) != NULL);
  
  meta_screen_free (screen, timestamp);
  display->screens = g_slist_remove (display->screens, screen);

  if (display->screens == NULL)
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

  /* Unmanage all windows */
  tmp = winlist;
  while (tmp != NULL)
    {
      meta_window_unmanage (tmp->data, timestamp);
      
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

  if (aw->screen == bw->screen)
    return meta_stack_windows_cmp (aw->screen->stack, aw, bw);
  /* Then assume screens are stacked by number */
  else if (aw->screen->number < bw->screen->number)
    return -1;
  else if (aw->screen->number > bw->screen->number)
    return 1;
  else
    return 0; /* not reached in theory, if windows on same display */
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
  else if (pref == META_PREF_COMPOSITING_MANAGER)
    {
      gboolean cm = meta_prefs_get_compositing_manager ();

      if (cm)
        enable_compositor (display, TRUE);
      else
	disable_compositor (display);
    }
}

void
meta_display_increment_focus_sentinel (MetaDisplay *display)
{
  unsigned long data[1];

  data[0] = meta_display_get_current_time (display);
  
  XChangeProperty (display->xdisplay,
                   ((MetaScreen*) display->screens->data)->xroot,
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

static void
sanity_check_timestamps (MetaDisplay *display,
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

static gboolean
timestamp_too_old (MetaDisplay *display,
                   MetaWindow  *window,
                   guint32     *timestamp)
{
  /* FIXME: If Soeren's suggestion in bug 151984 is implemented, it will allow
   * us to sanity check the timestamp here and ensure it doesn't correspond to
   * a future time (though we would want to rename to 
   * timestamp_too_old_or_in_future).
   */

  if (*timestamp == CurrentTime)
    {
      meta_warning ("Got a request to focus %s with a timestamp of 0.  This "
                    "shouldn't happen!\n",
                    window ? window->desc : "the no_focus_window");
      meta_print_backtrace ();
      *timestamp = meta_display_get_current_time_roundtrip (display);
      return FALSE;
    }
  else if (XSERVER_TIME_IS_BEFORE (*timestamp, display->last_focus_time))
    {
      if (XSERVER_TIME_IS_BEFORE (*timestamp, display->last_user_time))
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Ignoring focus request for %s since %u "
                      "is less than %u and %u.\n",
                      window ? window->desc : "the no_focus_window",
                      *timestamp,
                      display->last_user_time,
                      display->last_focus_time);
          return TRUE;
        }
      else
        {
          meta_topic (META_DEBUG_FOCUS,
                      "Received focus request for %s which is newer than most "
                      "recent user_time, but less recent than "
                      "last_focus_time (%u < %u < %u); adjusting "
                      "accordingly.  (See bug 167358)\n",
                      window ? window->desc : "the no_focus_window",
                      display->last_user_time,
                      *timestamp,
                      display->last_focus_time);
          *timestamp = display->last_focus_time;
          return FALSE;
        }
    }

  return FALSE;
}

void
meta_display_set_input_focus_window (MetaDisplay *display, 
                                     MetaWindow  *window,
                                     gboolean     focus_frame,
                                     guint32      timestamp)
{
  if (timestamp_too_old (display, window, &timestamp))
    return;

  meta_error_trap_push (display);
  XSetInputFocus (display->xdisplay,
                  focus_frame ? window->frame->xwindow : window->xwindow,
                  RevertToPointerRoot,
                  timestamp);
  meta_error_trap_pop (display, FALSE);

  display->expected_focus_window = window;
  display->last_focus_time = timestamp;
  display->active_screen = window->screen;

  if (window != display->autoraise_window)
    meta_display_remove_autoraise_callback (window->display);
}

void
meta_display_focus_the_no_focus_window (MetaDisplay *display, 
                                        MetaScreen  *screen,
                                        guint32      timestamp)
{
  if (timestamp_too_old (display, NULL, &timestamp))
    return;

  XSetInputFocus (display->xdisplay,
                  screen->no_focus_window,
                  RevertToPointerRoot,
                  timestamp);
  display->expected_focus_window = NULL;
  display->last_focus_time = timestamp;
  display->active_screen = screen;

  meta_display_remove_autoraise_callback (display);
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
meta_display_get_compositor_version (MetaDisplay *display,
                                     int         *major,
                                     int         *minor)
{
  *major = display->composite_major_version;
  *minor = display->composite_minor_version;
}

Display *
meta_display_get_xdisplay (MetaDisplay *display)
{
  return display->xdisplay;
}

MetaCompositor *
meta_display_get_compositor (MetaDisplay *display)
{
  return display->compositor;
}

GSList *
meta_display_get_screens (MetaDisplay *display)
{
  return display->screens;
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
 * Get the window that, according to events received from X server,
 * currently has the input focus. We may have already sent a request
 * to the X server to move the focus window elsewhere. (The
 * expected_focus_window records where we've last set the input
 * focus.)
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

Atom meta_display_get_atom (MetaDisplay *display, MetaAtom meta_atom)
{
  Atom *atoms = & display->atom_WM_PROTOCOLS;

  return atoms[meta_atom - 1];
}
