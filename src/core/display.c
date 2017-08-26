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
#include "window-private.h"
#include "boxes-private.h"
#include "frame.h"
#include <meta/errors.h>
#include "keybindings-private.h"
#include <meta/prefs.h>
#include "workspace-private.h"
#include "bell.h"
#include <meta/compositor.h>
#include <meta/compositor-mutter.h>
#include <X11/Xatom.h>
#include <meta/meta-enum-types.h>
#include "meta-idle-monitor-dbus.h"
#include "meta-cursor-tracker-private.h"
#include <meta/meta-backend.h>
#include "backends/meta-logical-monitor.h"
#include "backends/native/meta-backend-native.h"
#include "backends/x11/meta-backend-x11.h"
#include "backends/meta-stage-private.h"
#include "backends/meta-input-settings-private.h"
#include <clutter/x11/clutter-x11.h>

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif
#include <X11/extensions/shape.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "x11/events.h"
#include "x11/window-x11.h"
#include "x11/xprops.h"
#include "x11/meta-x11-display-private.h"

#ifdef HAVE_WAYLAND
#include "wayland/meta-xwayland-private.h"
#include "wayland/meta-wayland-tablet-seat.h"
#include "wayland/meta-wayland-tablet-pad.h"
#endif

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
  MetaWindow *window;
  guint32     serial;
  guint       ping_timeout_id;
} MetaPingData;

G_DEFINE_TYPE(MetaDisplay, meta_display, G_TYPE_OBJECT);

/* Signals */
enum
{
  CURSOR_UPDATED,
  X11_DISPLAY_OPENED,
  X11_DISPLAY_CLOSING,
  OVERLAY_KEY,
  ACCELERATOR_ACTIVATED,
  MODIFIERS_ACCELERATOR_ACTIVATED,
  FOCUS_WINDOW,
  WINDOW_CREATED,
  WINDOW_DEMANDS_ATTENTION,
  WINDOW_MARKED_URGENT,
  GRAB_OP_BEGIN,
  GRAB_OP_END,
  SHOW_RESTART_MESSAGE,
  RESTART,
  SHOW_RESIZE_POPUP,
  GL_VIDEO_MEMORY_PURGED,
  SHOW_PAD_OSD,
  SHOW_OSD,
  PAD_MODE_SWITCH,
  WINDOW_ENTERED_MONITOR,
  WINDOW_LEFT_MONITOR,
  WORKSPACE_ADDED,
  WORKSPACE_REMOVED,
  WORKSPACE_SWITCHED,
  IN_FULLSCREEN_CHANGED,
  STARTUP_SEQUENCE_CHANGED,
  MONITORS_CHANGED,
  RESTACKED,
  WORKAREAS_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,

  PROP_FOCUS_WINDOW,
  PROP_N_WORKSPACES
};

static guint display_signals [LAST_SIGNAL] = { 0 };

/*
 * The display we're managing.  This is a singleton object.  (Historically,
 * this was a list of displays, but there was never any way to add more
 * than one element to it.)  The goofy name is because we don't want it
 * to shadow the parameter in its object methods.
 */
static MetaDisplay *the_display = NULL;

static void on_monitors_changed_internal (MetaMonitorManager *monitor_manager,
                                          MetaDisplay        *display);

static void on_monitors_changed (MetaMonitorManager *monitor_manager,
                                 MetaDisplay        *display);

static void    prefs_changed_callback    (MetaPreference pref,
                                          void          *data);

static void set_workspace_names   (MetaDisplay *display);
static void update_num_workspaces (MetaDisplay *display,
                                   guint32     timestamp);

static int mru_cmp (gconstpointer a,
                    gconstpointer b);

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
    case PROP_N_WORKSPACES:
      g_value_set_int (value, meta_display_get_n_workspaces (display));
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

  display_signals[CURSOR_UPDATED] =
    g_signal_new ("cursor-updated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  display_signals[X11_DISPLAY_OPENED] =
    g_signal_new ("x11-display-opened",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  display_signals[X11_DISPLAY_CLOSING] =
    g_signal_new ("x11-display-closing",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

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
                  META_TYPE_DISPLAY,
                  META_TYPE_WINDOW,
                  META_TYPE_GRAB_OP);

  display_signals[GRAB_OP_END] =
    g_signal_new ("grab-op-end",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 3,
                  META_TYPE_DISPLAY,
                  META_TYPE_WINDOW,
                  META_TYPE_GRAB_OP);

  /**
   * MetaDisplay::show-restart-message:
   * @display: the #MetaDisplay instance
   * @message: (allow-none): The message to display, or %NULL
   *  to clear a previous restart message.
   *
   * The ::show-restart-message signal will be emitted to indicate
   * that the compositor should show a message during restart. This is
   * emitted when meta_restart() is called, either by Mutter
   * internally or by the embedding compositor.  The message should be
   * immediately added to the Clutter stage in its final form -
   * ::restart will be emitted to exit the application and leave the
   * stage contents frozen as soon as the the stage is painted again.
   *
   * On case of failure to restart, this signal will be emitted again
   * with %NULL for @message.
   *
   * Returns: %TRUE means the message was added to the stage; %FALSE
   *   indicates that the compositor did not show the message.
   */
  display_signals[SHOW_RESTART_MESSAGE] =
    g_signal_new ("show-restart-message",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_true_handled,
                  NULL, NULL,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_STRING);

  /**
   * MetaDisplay::restart:
   * @display: the #MetaDisplay instance
   *
   * The ::restart signal is emitted to indicate that compositor
   * should reexec the process. This is
   * emitted when meta_restart() is called, either by Mutter
   * internally or by the embedding compositor. See also
   * ::show-restart-message.
   *
   * Returns: %FALSE to indicate that the compositor could not
   *  be restarted. When the compositor is restarted, the signal
   *  should not return.
   */
  display_signals[RESTART] =
    g_signal_new ("restart",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_true_handled,
                  NULL, NULL,
                  G_TYPE_BOOLEAN, 0);

  display_signals[SHOW_RESIZE_POPUP] =
    g_signal_new ("show-resize-popup",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_true_handled,
                  NULL, NULL,
                  G_TYPE_BOOLEAN, 4,
                  G_TYPE_BOOLEAN, META_TYPE_RECTANGLE, G_TYPE_INT, G_TYPE_INT);

  display_signals[GL_VIDEO_MEMORY_PURGED] =
    g_signal_new ("gl-video-memory-purged",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /**
   * MetaDisplay::show-pad-osd:
   * @display: the #MetaDisplay instance
   * @pad: the pad device
   * @settings: the pad device settings
   * @layout_path: path to the layout image
   * @edition_mode: Whether the OSD should be shown in edition mode
   * @monitor_idx: Monitor to show the OSD on
   *
   * Requests the pad button mapping OSD to be shown.
   *
   * Returns: (transfer none) (nullable): The OSD actor
   */
  display_signals[SHOW_PAD_OSD] =
    g_signal_new ("show-pad-osd",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  CLUTTER_TYPE_ACTOR, 5, CLUTTER_TYPE_INPUT_DEVICE,
                  G_TYPE_SETTINGS, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INT);

  display_signals[SHOW_OSD] =
    g_signal_new ("show-osd",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 3, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);

  display_signals[PAD_MODE_SWITCH] =
    g_signal_new ("pad-mode-switch",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 3, CLUTTER_TYPE_INPUT_DEVICE,
                  G_TYPE_UINT, G_TYPE_UINT);

  display_signals[WINDOW_ENTERED_MONITOR] =
    g_signal_new ("window-entered-monitor",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_INT,
                  META_TYPE_WINDOW);

  display_signals[WINDOW_LEFT_MONITOR] =
    g_signal_new ("window-left-monitor",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_INT,
                  META_TYPE_WINDOW);

  display_signals[WORKSPACE_ADDED] =
    g_signal_new ("workspace-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_INT);

  display_signals[WORKSPACE_REMOVED] =
    g_signal_new ("workspace-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_INT);

  display_signals[WORKSPACE_SWITCHED] =
    g_signal_new ("workspace-switched",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_INT,
                  G_TYPE_INT,
                  META_TYPE_MOTION_DIRECTION);

  display_signals[IN_FULLSCREEN_CHANGED] =
    g_signal_new ("in-fullscreen-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  display_signals[STARTUP_SEQUENCE_CHANGED] =
    g_signal_new ("startup-sequence-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  display_signals[MONITORS_CHANGED] =
    g_signal_new ("monitors-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  display_signals[RESTACKED] =
    g_signal_new ("restacked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  display_signals[WORKAREAS_CHANGED] =
    g_signal_new ("workareas-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
                                   PROP_FOCUS_WINDOW,
                                   g_param_spec_object ("focus-window",
                                                        "Focus window",
                                                        "Currently focused window",
                                                        META_TYPE_WINDOW,
                                                        G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_N_WORKSPACES,
                                   g_param_spec_int ("n-workspaces",
                                                     "N Workspaces",
                                                     "Number of workspaces",
                                                     1, G_MAXINT, 1,
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

void
meta_display_remove_pending_pings_for_window (MetaDisplay *display,
                                              MetaWindow  *window)
{
  GSList *tmp;
  GSList *dead;

  /* could obviously be more efficient, don't care */

  /* build list to be removed */
  dead = NULL;
  for (tmp = display->pending_pings; tmp; tmp = tmp->next)
    {
      MetaPingData *ping_data = tmp->data;

      if (ping_data->window == window)
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


static void
enable_compositor (MetaDisplay *display)
{
  MetaX11Display *x11_display = display->x11_display;

  if (!META_X11_DISPLAY_HAS_COMPOSITE (x11_display) ||
      !META_X11_DISPLAY_HAS_DAMAGE (x11_display))
    {
      meta_warning ("Missing %s extension required for compositing",
                    !META_X11_DISPLAY_HAS_COMPOSITE (x11_display) ?
                    "composite" : "damage");
      return;
    }

  int version = (x11_display->composite_major_version * 10) +
                 x11_display->composite_minor_version;
  if (version < 3)
    {
      meta_warning ("Your version of COMPOSITE is too old.");
      return;
    }

  if (!display->compositor)
      display->compositor = meta_compositor_new (display);

  meta_compositor_manage (display->compositor);
}

static void
meta_display_init (MetaDisplay *disp)
{
  /* Some stuff could go in here that's currently in _open,
   * but it doesn't really matter. */
}

void
meta_display_cancel_touch (MetaDisplay *display)
{
#ifdef HAVE_WAYLAND
  MetaWaylandCompositor *compositor;

  if (!meta_is_wayland_compositor ())
    return;

  compositor = meta_wayland_compositor_get_default ();
  meta_wayland_touch_cancel (compositor->seat->touch);
#endif
}

static void
gesture_tracker_state_changed (MetaGestureTracker   *tracker,
                               ClutterEventSequence *sequence,
                               MetaSequenceState     state,
                               MetaDisplay          *display)
{
  if (meta_is_wayland_compositor ())
    {
      if (state == META_SEQUENCE_ACCEPTED)
        meta_display_cancel_touch (display);
    }
  else
    {
      MetaBackendX11 *backend = META_BACKEND_X11 (meta_get_backend ());
      int event_mode;

      if (state == META_SEQUENCE_ACCEPTED)
        event_mode = XIAcceptTouch;
      else if (state == META_SEQUENCE_REJECTED)
        event_mode = XIRejectTouch;
      else
        return;

      XIAllowTouchEvents (meta_backend_x11_get_xdisplay (backend),
                          META_VIRTUAL_CORE_POINTER_ID,
                          clutter_x11_event_sequence_get_touch_detail (sequence),
                          DefaultRootWindow (display->x11_display->xdisplay), event_mode);
    }
}

static void
on_startup_notification_changed (MetaStartupNotification *sn,
                                 gpointer                 sequence,
                                 MetaDisplay             *display)
{
  g_slist_free (display->startup_sequences);
  display->startup_sequences =
    meta_startup_notification_get_sequences (display->startup_notification);
  g_signal_emit_by_name (display, "startup-sequence-changed", sequence);
}

static void
reload_logical_monitors (MetaDisplay *display)
{
  GList *l;

  for (l = display->workspaces; l != NULL; l = l->next)
    {
      MetaWorkspace *space = l->data;
      meta_workspace_invalidate_work_area (space);
    }
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
  GError *error = NULL;
  MetaDisplay *display;
  MetaX11Display *x11_display;
  int i;
  guint32 timestamp;
  Window old_active_xwindow = None;
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager;

  g_assert (the_display == NULL);
  display = the_display = g_object_new (META_TYPE_DISPLAY, NULL);

  display->closing = 0;
  display->display_opening = TRUE;

  display->pending_pings = NULL;
  display->autoraise_timeout_id = 0;
  display->autoraise_window = NULL;
  display->focus_window = NULL;
  display->x11_display = NULL;

  display->rect.x = display->rect.y = 0;
  display->current_cursor = -1; /* invalid/unset */
  display->tile_preview_timeout_id = 0;
  display->check_fullscreen_later = 0;
  display->work_area_later = 0;

  display->mouse_mode = TRUE; /* Only relevant for mouse or sloppy focus */
  display->allow_terminal_deactivation = TRUE; /* Only relevant for when a
                                                  terminal has the focus */

  i = 0;
  while (i < N_IGNORED_CROSSING_SERIALS)
    {
      display->ignored_crossing_serials[i] = 0;
      ++i;
    }

  display->current_time = CurrentTime;
  display->sentinel_counter = 0;

  display->grab_resize_timeout_id = 0;
  display->grab_have_keyboard = FALSE;
  display->last_bell_time = 0;

  display->grab_op = META_GRAB_OP_NONE;
  display->grab_window = NULL;
  display->grab_tile_mode = META_TILE_NONE;
  display->grab_tile_monitor_number = -1;

  display->active_workspace = NULL;
  display->workspaces = NULL;
  display->rows_of_workspaces = 1;
  display->columns_of_workspaces = -1;
  display->vertical_workspaces = FALSE;
  display->starting_corner = META_DISPLAY_TOPLEFT;

  display->grab_edge_resistance_data = NULL;

  meta_display_init_keys (display);

  meta_prefs_add_listener (prefs_changed_callback, display);

  /* Get events */
  meta_display_init_events (display);

  display->stamps = g_hash_table_new (g_int64_hash,
                                      g_int64_equal);
  display->wayland_windows = g_hash_table_new (NULL, NULL);

  monitor_manager = meta_backend_get_monitor_manager (backend);
  g_signal_connect (monitor_manager, "monitors-changed-internal",
                    G_CALLBACK (on_monitors_changed_internal), display);
  g_signal_connect (monitor_manager, "monitors-changed",
                    G_CALLBACK (on_monitors_changed), display);

  meta_monitor_manager_get_screen_size (monitor_manager,
                                        &display->rect.width,
                                        &display->rect.height);

  meta_display_set_cursor (display, META_CURSOR_DEFAULT);

  x11_display = meta_x11_display_new (display, &error);
  g_assert (x11_display != NULL); /* Required, for now */
  display->x11_display = x11_display;
  g_signal_emit (display, display_signals[X11_DISPLAY_OPENED], 0);

  timestamp = display->x11_display->timestamp;

  display->stack = meta_stack_new (display);
  display->stack_tracker = meta_stack_tracker_new (display);

  reload_logical_monitors (display);

  meta_display_update_workspace_layout (display);

  /* There must be at least one workspace at all times,
   * so create that required workspace.
   */
  meta_workspace_new (display);

  meta_bell_init (display);

  display->last_focus_time = timestamp;
  display->last_user_time = timestamp;
  display->compositor = NULL;

  if (!meta_is_wayland_compositor ())
    meta_prop_get_window (display->x11_display,
                          display->x11_display->xroot,
                          display->x11_display->atom__NET_ACTIVE_WINDOW,
                          &old_active_xwindow);

  display->startup_notification = meta_startup_notification_get (display);
  g_signal_connect (display->startup_notification, "changed",
                    G_CALLBACK (on_startup_notification_changed), display);

  meta_display_init_workspaces (display);

  enable_compositor (display);

  meta_x11_display_create_guard_window (display->x11_display);

  /* Set up touch support */
  display->gesture_tracker = meta_gesture_tracker_new ();
  g_signal_connect (display->gesture_tracker, "state-changed",
                    G_CALLBACK (gesture_tracker_state_changed), display);

  /* We know that if mutter is running as a Wayland compositor,
   * we start out with no windows.
   */
  if (!meta_is_wayland_compositor ())
    meta_display_manage_all_windows (display);

  if (old_active_xwindow != None)
    {
      MetaWindow *old_active_window;
      old_active_window = meta_x11_display_lookup_x_window (display->x11_display,
                                                            old_active_xwindow);
      if (old_active_window)
        meta_window_focus (old_active_window, timestamp);
      else
        meta_x11_display_focus_the_no_focus_window (display->x11_display, timestamp);
    }
  else
    meta_x11_display_focus_the_no_focus_window (display->x11_display, timestamp);

  meta_idle_monitor_init_dbus ();

  /* Done opening new display */
  display->display_opening = FALSE;

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

  if (display->x11_display)
    {
      g_hash_table_iter_init (&iter, display->x11_display->xids);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          MetaWindow *window = value;

          if (!META_IS_WINDOW (window) || window->unmanaging)
            continue;

          if (!window->override_redirect ||
              (flags & META_LIST_INCLUDE_OVERRIDE_REDIRECT) != 0)
            winlist = g_slist_prepend (winlist, window);
        }
    }

  g_hash_table_iter_init (&iter, display->wayland_windows);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      MetaWindow *window = value;

      if (!META_IS_WINDOW (window) || window->unmanaging)
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

  if (flags & META_LIST_SORTED)
    winlist = g_slist_sort (winlist, mru_cmp);

  return winlist;
}

void
meta_display_close (MetaDisplay *display,
                    guint32      timestamp)
{
  g_assert (display != NULL);
  g_assert (display == the_display);

  if (display->closing != 0)
    {
      /* The display's already been closed. */
      return;
    }

  display->closing += 1;

  meta_compositor_unmanage (display->compositor);

  meta_display_unmanage_windows (display, timestamp);

  meta_prefs_remove_listener (prefs_changed_callback, display);

  meta_display_remove_autoraise_callback (display);

  g_clear_object (&display->startup_notification);
  g_clear_object (&display->gesture_tracker);

  g_clear_pointer (&display->stack, (GDestroyNotify) meta_stack_free);
  g_clear_pointer (&display->stack_tracker,
                   (GDestroyNotify) meta_stack_tracker_free);

  if (display->focus_timeout_id)
    g_source_remove (display->focus_timeout_id);
  display->focus_timeout_id = 0;

  if (display->tile_preview_timeout_id)
    g_source_remove (display->tile_preview_timeout_id);
  display->tile_preview_timeout_id = 0;

  if (display->work_area_later != 0)
    meta_later_remove (display->work_area_later);
  if (display->check_fullscreen_later != 0)
    meta_later_remove (display->check_fullscreen_later);

  /* Stop caring about events */
  meta_display_free_events (display);

  /* Must be after all calls to meta_window_unmanage() since they
   * unregister windows
   */
  g_hash_table_destroy (display->wayland_windows);
  g_hash_table_destroy (display->stamps);

  if (display->compositor)
    meta_compositor_destroy (display->compositor);

  if (display->x11_display)
    {
      g_signal_emit (display, display_signals[X11_DISPLAY_CLOSING], 0);
      g_object_run_dispose (G_OBJECT (display->x11_display));
      g_clear_object (&display->x11_display);
    }

  meta_display_shutdown_keys (display);

  g_object_unref (display);
  the_display = NULL;

  meta_quit (META_EXIT_SUCCESS);
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
  if (the_display->x11_display->xdisplay == xdisplay)
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

static inline gboolean
grab_op_is_window (MetaGrabOp op)
{
  return GRAB_OP_GET_BASE_TYPE (op) == META_GRAB_OP_WINDOW_BASE;
}

gboolean
meta_grab_op_is_mouse (MetaGrabOp op)
{
  if (!grab_op_is_window (op))
    return FALSE;

  return (op & META_GRAB_OP_WINDOW_FLAG_KEYBOARD) == 0;
}

gboolean
meta_grab_op_is_keyboard (MetaGrabOp op)
{
  if (!grab_op_is_window (op))
    return FALSE;

  return (op & META_GRAB_OP_WINDOW_FLAG_KEYBOARD) != 0;
}

gboolean
meta_grab_op_is_resizing (MetaGrabOp op)
{
  if (!grab_op_is_window (op))
    return FALSE;

  return (op & META_GRAB_OP_WINDOW_DIR_MASK) != 0 || op == META_GRAB_OP_KEYBOARD_RESIZING_UNKNOWN;
}

gboolean
meta_grab_op_is_moving (MetaGrabOp op)
{
  if (!grab_op_is_window (op))
    return FALSE;

  return !meta_grab_op_is_resizing (op);
}

/**
 * meta_display_windows_are_interactable:
 * @op: A #MetaGrabOp
 *
 * Whether windows can be interacted with.
 */
gboolean
meta_display_windows_are_interactable (MetaDisplay *display)
{
  switch (display->event_route)
    {
    case META_EVENT_ROUTE_NORMAL:
    case META_EVENT_ROUTE_WAYLAND_POPUP:
      return TRUE;
    default:
      return FALSE;
    }
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

guint32
meta_display_get_current_time_roundtrip (MetaDisplay *display)
{
  return meta_x11_display_get_current_time_roundtrip (display->x11_display);
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
  if (meta_stack_get_top (window->display->stack) != window)
    {
      if (meta_window_has_pointer (window))
	meta_window_raise (window);
      else
	meta_topic (META_DEBUG_FOCUS,
		    "Pointer not inside window, not raising %s\n",
		    window->desc);
    }

  return G_SOURCE_REMOVE;
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

void
meta_display_sync_wayland_input_focus (MetaDisplay *display)
{
#ifdef HAVE_WAYLAND
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  MetaWindow *focus_window = NULL;
  MetaBackend *backend = meta_get_backend ();
  MetaStage *stage = META_STAGE (meta_backend_get_stage (backend));
  gboolean is_focus_xwindow =
    meta_x11_display_xwindow_is_a_no_focus_window (display->x11_display,
                                                   display->x11_display->focus_xwindow);

  if (!meta_display_windows_are_interactable (display))
    focus_window = NULL;
  else if (is_focus_xwindow)
    focus_window = NULL;
  else if (display->focus_window && display->focus_window->surface)
    focus_window = display->focus_window;
  else
    meta_topic (META_DEBUG_FOCUS, "Focus change has no effect, because there is no matching wayland surface");

  meta_stage_set_active (stage, focus_window == NULL);
  meta_wayland_compositor_set_input_focus (compositor, focus_window);

  meta_wayland_seat_repick (compositor->seat);
#endif
}

void
meta_display_update_focus_window (MetaDisplay *display,
                                  MetaWindow  *window,
                                  Window       xwindow,
                                  gulong       serial,
                                  gboolean     focused_by_us)
{
  display->x11_display->focus_serial = serial;
  display->focused_by_us = focused_by_us;

  if (display->x11_display->focus_xwindow == xwindow &&
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
      display->x11_display->focus_xwindow = None;

      meta_window_set_focused_internal (previous, FALSE);
    }

  display->focus_window = window;
  display->x11_display->focus_xwindow = xwindow;

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
  meta_x11_display_update_active_window_hint (display->x11_display);
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

MetaWindow*
meta_display_lookup_stamp (MetaDisplay *display,
                           guint64       stamp)
{
  return g_hash_table_lookup (display->stamps, &stamp);
}

void
meta_display_register_stamp (MetaDisplay *display,
                             guint64     *stampp,
                             MetaWindow  *window)
{
  g_return_if_fail (g_hash_table_lookup (display->stamps, stampp) == NULL);

  g_hash_table_insert (display->stamps, stampp, window);
}

void
meta_display_unregister_stamp (MetaDisplay *display,
                               guint64      stamp)
{
  g_return_if_fail (g_hash_table_lookup (display->stamps, &stamp) != NULL);

  g_hash_table_remove (display->stamps, &stamp);
}

MetaWindow*
meta_display_lookup_stack_id (MetaDisplay *display,
                              guint64      stack_id)
{
  if (META_STACK_ID_IS_X11 (stack_id))
    return meta_x11_display_lookup_x_window (display->x11_display,
                                             (Window)stack_id);
  else
    return meta_display_lookup_stamp (display, stack_id);
}

/* We return a pointer into a ring of static buffers. This is to make
 * using this function for debug-logging convenient and avoid tempory
 * strings that must be freed. */
const char *
meta_display_describe_stack_id (MetaDisplay *display,
                                guint64      stack_id)
{
  /* 0x<64-bit: 16 characters> (<10 characters of title>)\0' */
  static char buffer[5][32];
  MetaWindow *window;
  static int pos = 0;
  char *result;

  result = buffer[pos];
  pos = (pos + 1) % 5;

  window = meta_display_lookup_stack_id (display, stack_id);

  if (window && window->title)
    snprintf (result, sizeof(buffer[0]), "%#" G_GINT64_MODIFIER "x (%.10s)", stack_id, window->title);
  else
    snprintf (result, sizeof(buffer[0]), "%#" G_GINT64_MODIFIER "x", stack_id);

  return result;
}

void
meta_display_notify_window_created (MetaDisplay  *display,
                                    MetaWindow   *window)
{
  g_signal_emit (display, display_signals[WINDOW_CREATED], 0, window);
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

static int
find_highest_logical_monitor_scale (MetaBackend      *backend,
                                    MetaCursorSprite *cursor_sprite)
{
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaCursorRenderer *cursor_renderer =
    meta_backend_get_cursor_renderer (backend);
  ClutterRect cursor_rect;
  GList *logical_monitors;
  GList *l;
  int highest_scale = 0.0;

  cursor_rect = meta_cursor_renderer_calculate_rect (cursor_renderer,
                                                     cursor_sprite);

  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);
  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;
      ClutterRect logical_monitor_rect =
        meta_rectangle_to_clutter_rect (&logical_monitor->rect);

      if (!clutter_rect_intersection (&cursor_rect,
                                      &logical_monitor_rect,
                                      NULL))
        continue;

      highest_scale = MAX (highest_scale, logical_monitor->scale);
    }

  return highest_scale;
}

static void
root_cursor_prepare_at (MetaCursorSpriteXcursor *sprite_xcursor,
                        int                      x,
                        int                      y,
                        MetaDisplay             *display)
{
  MetaCursorSprite *cursor_sprite = META_CURSOR_SPRITE (sprite_xcursor);
  MetaBackend *backend = meta_get_backend ();

  if (meta_is_stage_views_scaled ())
    {
      int scale;

      scale = find_highest_logical_monitor_scale (backend, cursor_sprite);
      if (scale != 0.0)
        {
          meta_cursor_sprite_xcursor_set_theme_scale (sprite_xcursor, scale);
          meta_cursor_sprite_set_texture_scale (cursor_sprite, 1.0 / scale);
        }
    }
  else
    {
      MetaMonitorManager *monitor_manager =
        meta_backend_get_monitor_manager (backend);
      MetaLogicalMonitor *logical_monitor;

      logical_monitor =
        meta_monitor_manager_get_logical_monitor_at (monitor_manager, x, y);

      /* Reload the cursor texture if the scale has changed. */
      if (logical_monitor)
        {
          meta_cursor_sprite_xcursor_set_theme_scale (sprite_xcursor,
                                                      logical_monitor->scale);
          meta_cursor_sprite_set_texture_scale (cursor_sprite, 1.0);
        }
    }
}

static void
manage_root_cursor_sprite_scale (MetaDisplay             *display,
                                 MetaCursorSpriteXcursor *sprite_xcursor)
{
  g_signal_connect_object (sprite_xcursor,
                           "prepare-at",
                           G_CALLBACK (root_cursor_prepare_at),
                           display,
                           0);
}

void
meta_display_reload_cursor (MetaDisplay *display)
{
  MetaCursor cursor = display->current_cursor;
  MetaCursorSpriteXcursor *sprite_xcursor;
  MetaBackend *backend = meta_get_backend ();
  MetaCursorTracker *cursor_tracker = meta_backend_get_cursor_tracker (backend);

  sprite_xcursor = meta_cursor_sprite_xcursor_new (cursor);

  if (meta_is_wayland_compositor ())
    manage_root_cursor_sprite_scale (display, sprite_xcursor);

  meta_cursor_tracker_set_root_cursor (cursor_tracker,
                                       META_CURSOR_SPRITE (sprite_xcursor));
  g_object_unref (sprite_xcursor);

  g_signal_emit (display, display_signals[CURSOR_UPDATED], 0, display);
}

void
meta_display_set_cursor (MetaDisplay *display,
                         MetaCursor   cursor)
{
  if (cursor == display->current_cursor)
    return;

  display->current_cursor = cursor;
  meta_display_reload_cursor (display);
}

void
meta_display_update_cursor (MetaDisplay *display)
{
  meta_display_set_cursor (display, meta_cursor_for_grab_op (display->grab_op));
}

static MetaWindow *
get_first_freefloating_window (MetaWindow *window)
{
  while (meta_window_is_attached_dialog (window))
    window = meta_window_get_transient_for (window);

  /* Attached dialogs should always have a non-NULL transient-for */
  g_assert (window != NULL);

  return window;
}

static MetaEventRoute
get_event_route_from_grab_op (MetaGrabOp op)
{
  switch (GRAB_OP_GET_BASE_TYPE (op))
    {
    case META_GRAB_OP_NONE:
      /* begin_grab_op shouldn't be called with META_GRAB_OP_NONE. */
      g_assert_not_reached ();

    case META_GRAB_OP_WINDOW_BASE:
      return META_EVENT_ROUTE_WINDOW_OP;

    case META_GRAB_OP_COMPOSITOR:
      /* begin_grab_op shouldn't be called with META_GRAB_OP_COMPOSITOR. */
      g_assert_not_reached ();

    case META_GRAB_OP_WAYLAND_POPUP:
      return META_EVENT_ROUTE_WAYLAND_POPUP;

    case META_GRAB_OP_FRAME_BUTTON:
      return META_EVENT_ROUTE_FRAME_BUTTON;

    default:
      g_assert_not_reached ();
    }
}

gboolean
meta_display_begin_grab_op (MetaDisplay *display,
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
  MetaBackend *backend = meta_get_backend ();
  MetaWindow *grab_window = NULL;
  MetaEventRoute event_route;

  g_assert (window != NULL);

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Doing grab op %u on window %s button %d pointer already grabbed: %d pointer pos %d,%d\n",
              op, window->desc, button, pointer_already_grabbed,
              root_x, root_y);

  if (display->grab_op != META_GRAB_OP_NONE)
    {
      meta_warning ("Attempt to perform window operation %u on window %s when operation %u on %s already in effect\n",
                    op, window->desc, display->grab_op,
                    display->grab_window ? display->grab_window->desc : "none");
      return FALSE;
    }

  event_route = get_event_route_from_grab_op (op);

  if (event_route == META_EVENT_ROUTE_WINDOW_OP)
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

  grab_window = window;

  /* If we're trying to move a window, move the first
   * non-attached dialog instead.
   */
  if (meta_grab_op_is_moving (op))
    grab_window = get_first_freefloating_window (window);

  g_assert (grab_window != NULL);
  g_assert (op != META_GRAB_OP_NONE);

  display->grab_have_pointer = FALSE;

  if (pointer_already_grabbed)
    display->grab_have_pointer = TRUE;

  /* Since grab operations often happen as a result of implicit
   * pointer operations on the display X11 connection, we need
   * to ungrab here to ensure that the backend's X11 can take
   * the device grab. */
  XIUngrabDevice (display->x11_display->xdisplay,
                  META_VIRTUAL_CORE_POINTER_ID,
                  timestamp);
  XSync (display->x11_display->xdisplay, False);

  if (meta_backend_grab_device (backend, META_VIRTUAL_CORE_POINTER_ID, timestamp))
    display->grab_have_pointer = TRUE;

  if (!display->grab_have_pointer && !meta_grab_op_is_keyboard (op))
    {
      meta_topic (META_DEBUG_WINDOW_OPS, "XIGrabDevice() failed\n");
      return FALSE;
    }

  /* Grab keys when beginning window ops; see #126497 */
  if (event_route == META_EVENT_ROUTE_WINDOW_OP)
    {
      display->grab_have_keyboard = meta_window_grab_all_keys (grab_window, timestamp);

      if (!display->grab_have_keyboard)
        {
          meta_topic (META_DEBUG_WINDOW_OPS, "grabbing all keys failed, ungrabbing pointer\n");
          meta_backend_ungrab_device (backend, META_VIRTUAL_CORE_POINTER_ID, timestamp);
          display->grab_have_pointer = FALSE;
          return FALSE;
        }
    }

  display->event_route = event_route;
  display->grab_op = op;
  display->grab_window = grab_window;
  display->grab_button = button;
  display->grab_tile_mode = grab_window->tile_mode;
  display->grab_tile_monitor_number = grab_window->tile_monitor_number;
  display->grab_anchor_root_x = root_x;
  display->grab_anchor_root_y = root_y;
  display->grab_latest_motion_x = root_x;
  display->grab_latest_motion_y = root_y;
  display->grab_last_moveresize_time.tv_sec = 0;
  display->grab_last_moveresize_time.tv_usec = 0;
  display->grab_last_user_action_was_snap = FALSE;
  display->grab_frame_action = frame_action;

  meta_display_update_cursor (display);

  if (display->grab_resize_timeout_id)
    {
      g_source_remove (display->grab_resize_timeout_id);
      display->grab_resize_timeout_id = 0;
    }

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Grab op %u on window %s successful\n",
              display->grab_op, window ? window->desc : "(null)");

  meta_window_get_frame_rect (display->grab_window,
                              &display->grab_initial_window_pos);
  display->grab_anchor_window_pos = display->grab_initial_window_pos;

  if (meta_is_wayland_compositor ())
    {
      meta_display_sync_wayland_input_focus (display);
      meta_display_cancel_touch (display);
    }

  g_signal_emit (display, display_signals[GRAB_OP_BEGIN], 0,
                 display, display->grab_window, display->grab_op);

  if (display->event_route == META_EVENT_ROUTE_WINDOW_OP)
    meta_window_grab_op_began (display->grab_window, display->grab_op);

  return TRUE;
}

void
meta_display_end_grab_op (MetaDisplay *display,
                          guint32      timestamp)
{
  MetaWindow *grab_window = display->grab_window;
  MetaGrabOp grab_op = display->grab_op;

  meta_topic (META_DEBUG_WINDOW_OPS,
              "Ending grab op %u at time %u\n", grab_op, timestamp);

  if (display->event_route == META_EVENT_ROUTE_NORMAL ||
      display->event_route == META_EVENT_ROUTE_COMPOSITOR_GRAB)
    return;

  g_assert (grab_window != NULL);

  g_signal_emit (display, display_signals[GRAB_OP_END], 0,
                 display, grab_window, grab_op);

  /* We need to reset this early, since the
   * meta_window_grab_op_ended callback relies on this being
   * up to date. */
  display->grab_op = META_GRAB_OP_NONE;

  if (display->event_route == META_EVENT_ROUTE_WINDOW_OP)
    {
      /* Clear out the edge cache */
      meta_display_cleanup_edges (display);

      /* Only raise the window in orthogonal raise
       * ('do-not-raise-on-click') mode if the user didn't try to move
       * or resize the given window by at least a threshold amount.
       * For raise on click mode, the window was raised at the
       * beginning of the grab_op.
       */
      if (!meta_prefs_get_raise_on_click () &&
          !display->grab_threshold_movement_reached)
        meta_window_raise (display->grab_window);

      meta_window_grab_op_ended (grab_window, grab_op);
    }

  if (display->grab_have_pointer)
    {
      MetaBackend *backend = meta_get_backend ();
      meta_backend_ungrab_device (backend, META_VIRTUAL_CORE_POINTER_ID, timestamp);
    }

  if (display->grab_have_keyboard)
    {
      meta_topic (META_DEBUG_WINDOW_OPS,
                  "Ungrabbing all keys timestamp %u\n", timestamp);
      meta_window_ungrab_all_keys (grab_window, timestamp);
    }

  display->event_route = META_EVENT_ROUTE_NORMAL;
  display->grab_window = NULL;
  display->grab_tile_mode = META_TILE_NONE;
  display->grab_tile_monitor_number = -1;

  meta_display_update_cursor (display);

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
        XSynchronize (meta_get_display ()->x11_display->xdisplay, is_syncing);
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
  MetaWindow *window = ping_data->window;
  MetaDisplay *display = window->display;

  meta_window_set_alive (window, FALSE);

  ping_data->ping_timeout_id = 0;

  meta_topic (META_DEBUG_PING,
              "Ping %u on window %s timed out\n",
              ping_data->serial, ping_data->window->desc);

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
meta_display_ping_window (MetaWindow *window,
			  guint32     serial)
{
  MetaDisplay *display = window->display;
  MetaPingData *ping_data;

  if (serial == 0)
    {
      meta_warning ("Tried to ping a window with a bad serial! Not allowed.\n");
      return;
    }

  if (!window->can_ping)
    return;

  ping_data = g_new (MetaPingData, 1);
  ping_data->window = window;
  ping_data->serial = serial;
  ping_data->ping_timeout_id = g_timeout_add (PING_TIMEOUT_DELAY,
					      meta_display_ping_timeout,
					      ping_data);
  g_source_set_name_by_id (ping_data->ping_timeout_id, "[mutter] meta_display_ping_timeout");

  display->pending_pings = g_slist_prepend (display->pending_pings, ping_data);

  meta_topic (META_DEBUG_PING,
              "Sending ping with serial %u to window %s\n",
              serial, window->desc);

  META_WINDOW_GET_CLASS (window)->ping (window, serial);
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

      if (serial == ping_data->serial)
        {
          meta_topic (META_DEBUG_PING,
                      "Matching ping found for pong %u\n",
                      ping_data->serial);

          /* Remove the ping data from the list */
          display->pending_pings = g_slist_remove (display->pending_pings,
                                                   ping_data);

          /* Remove the timeout */
          if (ping_data->ping_timeout_id != 0)
            {
              g_source_remove (ping_data->ping_timeout_id);
              ping_data->ping_timeout_id = 0;
            }

          meta_window_set_alive (ping_data->window, TRUE);
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
 * @workspace: (nullable): origin workspace
 *
 * Determine the list of windows that should be displayed for Alt-TAB
 * functionality.  The windows are returned in most recently used order.
 * If @workspace is not %NULL, the list only conains windows that are on
 * @workspace or have the demands-attention hint set; otherwise it contains
 * all windows.
 *
 * Returns: (transfer container) (element-type Meta.Window): List of windows
 */
GList*
meta_display_get_tab_list (MetaDisplay   *display,
                           MetaTabList    type,
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

  /* Windows sellout mode - MRU order. Collect unminimized windows
   * then minimized so minimized windows aren't in the way so much.
   */
  for (tmp = mru_list; tmp; tmp = tmp->next)
    {
      MetaWindow *window = tmp->data;

      if (!window->minimized && IN_TAB_CHAIN (window, type))
        tab_list = g_list_prepend (tab_list, window);
    }

  for (tmp = mru_list; tmp; tmp = tmp->next)
    {
      MetaWindow *window = tmp->data;

      if (window->minimized && IN_TAB_CHAIN (window, type))
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
 * @window: (nullable): starting window
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
  tab_list = meta_display_get_tab_list (display, type, workspace);

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
meta_display_manage_all_windows (MetaDisplay *display)
{
  guint64 *_children;
  guint64 *children;
  int n_children, i;

  meta_stack_freeze (display->stack);
  meta_stack_tracker_get_stack (display->stack_tracker, &_children, &n_children);

  /* Copy the stack as it will be modified as part of the loop */
  children = g_memdup (_children, sizeof (guint64) * n_children);

  for (i = 0; i < n_children; ++i)
    {
      g_assert (META_STACK_ID_IS_X11 (children[i]));
      meta_window_x11_new (display, children[i], TRUE,
                           META_COMP_EFFECT_NONE);
    }

  g_free (children);
  meta_stack_thaw (display->stack);
}

void
meta_display_unmanage_windows (MetaDisplay *display,
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

  return meta_stack_windows_cmp (aw->display->stack, aw, bw);
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

static void
prefs_changed_callback (MetaPreference pref,
                        void          *data)
{
  MetaDisplay *display = data;

  if (pref == META_PREF_AUDIBLE_BELL)
    {
      meta_bell_set_audible (display, meta_prefs_bell_is_audible ());
    }
  else if (pref == META_PREF_CURSOR_THEME ||
           pref == META_PREF_CURSOR_SIZE)
    {
      meta_display_reload_cursor (display);
    }
  else if ((pref == META_PREF_NUM_WORKSPACES ||
            pref == META_PREF_DYNAMIC_WORKSPACES) &&
            !meta_prefs_get_dynamic_workspaces ())
    {
      /* GSettings doesn't provide timestamps, but luckily update_num_workspaces
       * often doesn't need it...
       */
      guint32 timestamp =
        meta_display_get_current_time_roundtrip (display);
      update_num_workspaces (display, timestamp);
    }
  else if (pref == META_PREF_WORKSPACE_NAMES)
    {
      set_workspace_names (display);
    }
}

void
meta_display_increment_focus_sentinel (MetaDisplay *display)
{
  unsigned long data[1];

  data[0] = meta_display_get_current_time (display);

  XChangeProperty (display->x11_display->xdisplay,
                   display->x11_display->xroot,
                   display->x11_display->atom__MUTTER_SENTINEL,
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

/**
 * meta_display_supports_extended_barriers:
 * @display: a #MetaDisplay
 *
 * Returns: whether pointer barriers can be supported.
 *
 * When running as an X compositor the X server needs XInput 2
 * version 2.3. When running as a display server it is supported
 * when running on the native backend.
 *
 * Clients should use this method to determine whether their
 * interfaces should depend on new barrier features.
 */
gboolean
meta_display_supports_extended_barriers (MetaDisplay *display)
{
#ifdef HAVE_NATIVE_BACKEND
  if (META_IS_BACKEND_NATIVE (meta_get_backend ()))
    return TRUE;
#endif

  if (META_IS_BACKEND_X11 (meta_get_backend ()))
    {
      return (META_X11_DISPLAY_HAS_XINPUT_23 (display->x11_display) &&
              !meta_is_wayland_compositor());
    }

  g_assert_not_reached ();
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

/**
 * meta_display_get_x11_display: (skip)
 * @display: a #MetaDisplay
 *
 */
MetaX11Display *
meta_display_get_x11_display (MetaDisplay *display)
{
  return display->x11_display;
}

/**
 * meta_display_get_size:
 * @display: A #MetaDisplay
 * @width: (out): The width of the screen
 * @height: (out): The height of the screen
 *
 * Retrieve the size of the display.
 */
void
meta_display_get_size (MetaDisplay *display,
                       int         *width,
                       int         *height)
{
  if (width != NULL)
    *width = display->rect.width;

  if (height != NULL)
    *height = display->rect.height;
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

MetaGestureTracker *
meta_display_get_gesture_tracker (MetaDisplay *display)
{
  return display->gesture_tracker;
}

gboolean
meta_display_show_restart_message (MetaDisplay *display,
                                   const char  *message)
{
  gboolean result = FALSE;

  g_signal_emit (display,
                 display_signals[SHOW_RESTART_MESSAGE], 0,
                 message, &result);

  return result;
}

gboolean
meta_display_request_restart (MetaDisplay *display)
{
  gboolean result = FALSE;

  g_signal_emit (display,
                 display_signals[RESTART], 0,
                 &result);

  return result;
}

gboolean
meta_display_show_resize_popup (MetaDisplay *display,
                                gboolean show,
                                MetaRectangle *rect,
                                int display_w,
                                int display_h)
{
  gboolean result = FALSE;

  g_signal_emit (display,
                 display_signals[SHOW_RESIZE_POPUP], 0,
                 show, rect, display_w, display_h, &result);

  return result;
}

/**
 * meta_display_is_pointer_emulating_sequence:
 * @display: the display
 * @sequence: (nullable): a #ClutterEventSequence
 *
 * Tells whether the event sequence is the used for pointer emulation
 * and single-touch interaction.
 *
 * Returns: #TRUE if the sequence emulates pointer behavior
 **/
gboolean
meta_display_is_pointer_emulating_sequence (MetaDisplay          *display,
                                            ClutterEventSequence *sequence)
{
  if (!sequence)
    return FALSE;

  return display->pointer_emulating_sequence == sequence;
}

void
meta_display_request_pad_osd (MetaDisplay        *display,
                              ClutterInputDevice *pad,
                              gboolean            edition_mode)
{
  MetaBackend *backend = meta_get_backend ();
  MetaInputSettings *input_settings;
  const gchar *layout_path = NULL;
  ClutterActor *osd;
  MetaLogicalMonitor *logical_monitor;
  GSettings *settings;
#ifdef HAVE_LIBWACOM
  WacomDevice *wacom_device;
#endif

  /* Avoid emitting the signal while there is an OSD being currently
   * displayed, the first OSD will have to be dismissed before showing
   * any other one.
   */
  if (display->current_pad_osd)
    return;

  input_settings = meta_backend_get_input_settings (meta_get_backend ());

  if (input_settings)
    {
      settings = meta_input_settings_get_tablet_settings (input_settings, pad);
      logical_monitor =
        meta_input_settings_get_tablet_logical_monitor (input_settings, pad);
#ifdef HAVE_LIBWACOM
      wacom_device = meta_input_settings_get_tablet_wacom_device (input_settings,
                                                                  pad);
      layout_path = libwacom_get_layout_filename (wacom_device);
#endif
    }

  if (!layout_path || !settings)
    return;

  if (!logical_monitor)
    logical_monitor = meta_backend_get_current_logical_monitor (backend);

  g_signal_emit (display, display_signals[SHOW_PAD_OSD], 0,
                 pad, settings, layout_path,
                 edition_mode, logical_monitor->number, &osd);

  if (osd)
    {
      display->current_pad_osd = osd;
      g_object_add_weak_pointer (G_OBJECT (display->current_pad_osd),
                                 (gpointer *) &display->current_pad_osd);
    }

  g_object_unref (settings);
}

gchar *
meta_display_get_pad_action_label (MetaDisplay        *display,
                                   ClutterInputDevice *pad,
                                   MetaPadActionType   action_type,
                                   guint               action_number)
{
  MetaInputSettings *settings;
  gchar *label;

  /* First, lookup the action, as imposed by settings */
  settings = meta_backend_get_input_settings (meta_get_backend ());
  label = meta_input_settings_get_pad_action_label (settings, pad, action_type, action_number);
  if (label)
    return label;

#ifdef HAVE_WAYLAND
  /* Second, if this wayland, lookup the actions set by the clients */
  if (meta_is_wayland_compositor ())
    {
      MetaWaylandCompositor *compositor;
      MetaWaylandTabletSeat *tablet_seat;
      MetaWaylandTabletPad *tablet_pad = NULL;

      compositor = meta_wayland_compositor_get_default ();
      tablet_seat = meta_wayland_tablet_manager_ensure_seat (compositor->tablet_manager,
                                                             compositor->seat);
      if (tablet_seat)
        tablet_pad = meta_wayland_tablet_seat_lookup_pad (tablet_seat, pad);

      if (tablet_pad)
        {
          label = meta_wayland_tablet_pad_get_label (tablet_pad, action_type,
                                                     action_number);
        }

      if (label)
        return label;
    }
#endif

  return NULL;
}

static void
meta_display_show_osd (MetaDisplay *display,
                       gint         monitor_idx,
                       const gchar *icon_name,
                       const gchar *message)
{
  g_signal_emit (display, display_signals[SHOW_OSD], 0,
                 monitor_idx, icon_name, message);
}

static gint
lookup_tablet_monitor (MetaDisplay        *display,
                       ClutterInputDevice *device)
{
  MetaInputSettings *input_settings;
  MetaLogicalMonitor *monitor;
  gint monitor_idx = -1;

  input_settings = meta_backend_get_input_settings (meta_get_backend ());
  if (!input_settings)
    return -1;

  monitor = meta_input_settings_get_tablet_logical_monitor (input_settings, device);

  if (monitor)
    {
      monitor_idx = meta_display_get_monitor_index_for_rect (display,
                                                             &monitor->rect);
    }

  return monitor_idx;
}

void
meta_display_show_tablet_mapping_notification (MetaDisplay        *display,
                                               ClutterInputDevice *pad,
                                               const gchar        *pretty_name)
{
  if (!pretty_name)
    pretty_name = clutter_input_device_get_device_name (pad);
  meta_display_show_osd (display, lookup_tablet_monitor (display, pad),
                         "input-tablet-symbolic", pretty_name);
}

void
meta_display_notify_pad_group_switch (MetaDisplay        *display,
                                      ClutterInputDevice *pad,
                                      const gchar        *pretty_name,
                                      guint               n_group,
                                      guint               n_mode,
                                      guint               n_modes)
{
  GString *message;
  guint i;

  if (!pretty_name)
    pretty_name = clutter_input_device_get_device_name (pad);

  message = g_string_new (pretty_name);
  g_string_append_c (message, '\n');
  for (i = 0; i < n_modes; i++)
    g_string_append (message, (i == n_mode) ? "⚫" : "⚪");

  meta_display_show_osd (display, lookup_tablet_monitor (display, pad),
                         "input-tablet-symbolic", message->str);

  g_signal_emit (display, display_signals[PAD_MODE_SWITCH], 0, pad,
                 n_group, n_mode);

  g_string_free (message, TRUE);
}

void
meta_display_foreach_window (MetaDisplay           *display,
                             MetaListWindowsFlags   flags,
                             MetaDisplayWindowFunc  func,
                             gpointer               data)
{
  GSList *windows;

  /* If we end up doing this often, just keeping a list
   * of windows might be sensible.
   */

  windows = meta_display_list_windows (display, flags);

  g_slist_foreach (windows, (GFunc) func, data);

  g_slist_free (windows);
}

static void
meta_display_resize_func (MetaWindow *window,
                          gpointer    user_data)
{
  if (window->struts)
    {
      meta_window_update_struts (window);
    }
  meta_window_queue (window, META_QUEUE_MOVE_RESIZE);

  meta_window_recalc_features (window);
}

static void
on_monitors_changed_internal (MetaMonitorManager *monitor_manager,
                              MetaDisplay        *display)
{
  MetaBackend *backend;
  MetaCursorRenderer *cursor_renderer;

  meta_monitor_manager_get_screen_size (monitor_manager,
                                        &display->rect.width,
                                        &display->rect.height);

  reload_logical_monitors (display);

  /* Fix up monitor for all windows on this display */
  meta_display_foreach_window (display, META_LIST_INCLUDE_OVERRIDE_REDIRECT,
                               (MetaDisplayWindowFunc)
                               meta_window_update_for_monitors_changed, 0);

  /* Queue a resize on all the windows */
  meta_display_foreach_window (display, META_LIST_DEFAULT,
                               meta_display_resize_func, 0);

  meta_display_queue_check_fullscreen (display);

  backend = meta_get_backend ();
  cursor_renderer = meta_backend_get_cursor_renderer (backend);
  meta_cursor_renderer_force_update (cursor_renderer);
}

static void
on_monitors_changed (MetaMonitorManager *monitor_manager,
                     MetaDisplay        *display)
{
  g_signal_emit (display, display_signals[MONITORS_CHANGED], 0);
}

void
meta_display_restacked (MetaDisplay *display)
{
  g_signal_emit (display, display_signals[RESTACKED], 0);
}

static gboolean
meta_display_update_tile_preview_timeout (gpointer data)
{
  MetaDisplay *display = data;
  MetaWindow *window = display->grab_window;
  gboolean needs_preview = FALSE;

  display->tile_preview_timeout_id = 0;

  if (window)
    {
      switch (display->preview_tile_mode)
        {
        case META_TILE_LEFT:
        case META_TILE_RIGHT:
          if (!META_WINDOW_TILED_SIDE_BY_SIDE (window))
            needs_preview = TRUE;
          break;

        case META_TILE_MAXIMIZED:
          if (!META_WINDOW_MAXIMIZED (window))
            needs_preview = TRUE;
          break;

        default:
          needs_preview = FALSE;
          break;
        }
    }

  if (needs_preview)
    {
      MetaRectangle tile_rect;
      int monitor;

      monitor = meta_window_get_current_tile_monitor_number (window);
      meta_window_get_tile_area (window, display->preview_tile_mode,
                                 &tile_rect);
      meta_compositor_show_tile_preview (display->compositor,
                                         window, &tile_rect, monitor);
    }
  else
    meta_compositor_hide_tile_preview (display->compositor);

  return FALSE;
}

#define TILE_PREVIEW_TIMEOUT_MS 200

void
meta_display_update_tile_preview (MetaDisplay *display,
                                  gboolean     delay)
{
  if (delay)
    {
      if (display->tile_preview_timeout_id > 0)
        return;

      display->tile_preview_timeout_id =
        g_timeout_add (TILE_PREVIEW_TIMEOUT_MS,
                       meta_display_update_tile_preview_timeout,
                       display);
      g_source_set_name_by_id (display->tile_preview_timeout_id,
                               "[mutter] meta_display_update_tile_preview_timeout");
    }
  else
    {
      if (display->tile_preview_timeout_id > 0)
        g_source_remove (display->tile_preview_timeout_id);

      meta_display_update_tile_preview_timeout ((gpointer)display);
    }
}

void
meta_display_hide_tile_preview (MetaDisplay *display)
{
  if (display->tile_preview_timeout_id > 0)
    g_source_remove (display->tile_preview_timeout_id);

  display->preview_tile_mode = META_TILE_NONE;
  meta_compositor_hide_tile_preview (display->compositor);
}

/**
 * meta_display_get_startup_sequences: (skip)
 * @display:
 *
 * Return value: (transfer none): Currently active #SnStartupSequence items
 */
GSList *
meta_display_get_startup_sequences (MetaDisplay *display)
{
  return display->startup_sequences;
}

/* Sets the initial_timestamp and initial_workspace properties
 * of a window according to information given us by the
 * startup-notification library.
 *
 * Returns TRUE if startup properties have been applied, and
 * FALSE if they have not (for example, if they had already
 * been applied.)
 */
gboolean
meta_display_apply_startup_properties (MetaDisplay *display,
                                       MetaWindow  *window)
{
#ifdef HAVE_STARTUP_NOTIFICATION
  const char *startup_id;
  GSList *l;
  SnStartupSequence *sequence;

  /* Does the window have a startup ID stored? */
  startup_id = meta_window_get_startup_id (window);

  meta_topic (META_DEBUG_STARTUP,
              "Applying startup props to %s id \"%s\"\n",
              window->desc,
              startup_id ? startup_id : "(none)");

  sequence = NULL;
  if (!startup_id)
    {
      /* No startup ID stored for the window. Let's ask the
       * startup-notification library whether there's anything
       * stored for the resource name or resource class hints.
       */
      for (l = display->startup_sequences; l; l = l->next)
        {
          const char *wmclass;
          SnStartupSequence *seq = l->data;

          wmclass = sn_startup_sequence_get_wmclass (seq);

          if (wmclass != NULL &&
              ((window->res_class &&
                strcmp (wmclass, window->res_class) == 0) ||
               (window->res_name &&
                strcmp (wmclass, window->res_name) == 0)))
            {
              sequence = seq;

              g_assert (window->startup_id == NULL);
              window->startup_id = g_strdup (sn_startup_sequence_get_id (sequence));
              startup_id = window->startup_id;

              meta_topic (META_DEBUG_STARTUP,
                          "Ending legacy sequence %s due to window %s\n",
                          sn_startup_sequence_get_id (sequence),
                          window->desc);

              sn_startup_sequence_complete (sequence);
              break;
            }
        }
    }

  /* Still no startup ID? Bail. */
  if (!startup_id)
    return FALSE;

  /* We might get this far and not know the sequence ID (if the window
   * already had a startup ID stored), so let's look for one if we don't
   * already know it.
   */
  if (sequence == NULL)
    {
      for (l = display->startup_sequences; l != NULL; l = l->next)
        {
          SnStartupSequence *seq = l->data;
          const char *id;

          id = sn_startup_sequence_get_id (seq);

          if (strcmp (id, startup_id) == 0)
            {
              sequence = seq;
              break;
            }
        }
    }

  if (sequence != NULL)
    {
      gboolean changed_something = FALSE;

      meta_topic (META_DEBUG_STARTUP,
                  "Found startup sequence for window %s ID \"%s\"\n",
                  window->desc, startup_id);

      if (!window->initial_workspace_set)
        {
          int space = sn_startup_sequence_get_workspace (sequence);
          if (space >= 0)
            {
              meta_topic (META_DEBUG_STARTUP,
                          "Setting initial window workspace to %d based on startup info\n",
                          space);

              window->initial_workspace_set = TRUE;
              window->initial_workspace = space;
              changed_something = TRUE;
            }
        }

      if (!window->initial_timestamp_set)
        {
          guint32 timestamp = sn_startup_sequence_get_timestamp (sequence);
          meta_topic (META_DEBUG_STARTUP,
                      "Setting initial window timestamp to %u based on startup info\n",
                      timestamp);

          window->initial_timestamp_set = TRUE;
          window->initial_timestamp = timestamp;
          changed_something = TRUE;
        }

      return changed_something;
    }
  else
    {
      meta_topic (META_DEBUG_STARTUP,
                  "Did not find startup sequence for window %s ID \"%s\"\n",
                  window->desc, startup_id);
    }

#endif /* HAVE_STARTUP_NOTIFICATION */

  return FALSE;
}

static void
set_work_area_hint (MetaDisplay *display)
{
  MetaX11Display *x11_display = display->x11_display;
  int num_workspaces;
  GList *l;
  unsigned long *data, *tmp;
  MetaRectangle area;

  num_workspaces = meta_display_get_n_workspaces (display);
  data = g_new (unsigned long, num_workspaces * 4);
  tmp = data;

  for (l = display->workspaces; l; l = l->next)
    {
      MetaWorkspace *workspace = l->data;

      meta_workspace_get_work_area_all_monitors (workspace, &area);
      tmp[0] = area.x;
      tmp[1] = area.y;
      tmp[2] = area.width;
      tmp[3] = area.height;

      tmp += 4;
    }

  meta_error_trap_push (x11_display);
  XChangeProperty (x11_display->xdisplay,
                   x11_display->xroot,
                   x11_display->atom__NET_WORKAREA,
                   XA_CARDINAL, 32, PropModeReplace,
                   (guchar*) data, num_workspaces*4);
  meta_error_trap_pop (x11_display);

  g_free (data);

  g_signal_emit (display, display_signals[WORKAREAS_CHANGED], 0);
}

static gboolean
set_work_area_later_func (MetaDisplay *display)
{
  meta_topic (META_DEBUG_WORKAREA,
              "Running work area hint computation function\n");

  display->work_area_later = 0;

  set_work_area_hint (display);

  return FALSE;
}

void
meta_display_queue_workarea_recalc (MetaDisplay *display)
{
  /* Recompute work area later before redrawing */
  if (display->work_area_later == 0)
    {
      meta_topic (META_DEBUG_WORKAREA,
                  "Adding work area hint computation function\n");
      display->work_area_later =
        meta_later_add (META_LATER_BEFORE_REDRAW,
                        (GSourceFunc) set_work_area_later_func,
                        display,
                        NULL);
    }
}

static gboolean
check_fullscreen_func (gpointer data)
{
  MetaDisplay *display = data;
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  GList *logical_monitors, *l;
  MetaWindow *window;
  GSList *fullscreen_monitors = NULL;
  GSList *obscured_monitors = NULL;
  gboolean in_fullscreen_changed = FALSE;

  display->check_fullscreen_later = 0;

  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);

  /* We consider a monitor in fullscreen if it contains a fullscreen window;
   * however we make an exception for maximized windows above the fullscreen
   * one, as in that case window+chrome fully obscure the fullscreen window.
   */
  for (window = meta_stack_get_top (display->stack);
       window;
       window = meta_stack_get_below (display->stack, window, FALSE))
    {
      gboolean covers_monitors = FALSE;

      if (window->hidden)
        continue;

      if (window->fullscreen)
        {
          covers_monitors = TRUE;
        }
      else if (window->override_redirect)
        {
          /* We want to handle the case where an application is creating an
           * override-redirect window the size of the screen (monitor) and treat
           * it similarly to a fullscreen window, though it doesn't have fullscreen
           * window management behavior. (Being O-R, it's not managed at all.)
           */
          if (meta_window_is_monitor_sized (window))
            covers_monitors = TRUE;
        }
      else if (window->maximized_horizontally &&
               window->maximized_vertically)
        {
          MetaLogicalMonitor *logical_monitor;

          logical_monitor = meta_window_get_main_logical_monitor (window);
          if (!g_slist_find (obscured_monitors, logical_monitor))
            obscured_monitors = g_slist_prepend (obscured_monitors,
                                                 logical_monitor);
        }

      if (covers_monitors)
        {
          MetaRectangle window_rect;

          meta_window_get_frame_rect (window, &window_rect);

          for (l = logical_monitors; l; l = l->next)
            {
              MetaLogicalMonitor *logical_monitor = l->data;

              if (meta_rectangle_overlap (&window_rect,
                                          &logical_monitor->rect) &&
                  !g_slist_find (fullscreen_monitors, logical_monitor) &&
                  !g_slist_find (obscured_monitors, logical_monitor))
                fullscreen_monitors = g_slist_prepend (fullscreen_monitors,
                                                       logical_monitor);
            }
        }
    }

  g_slist_free (obscured_monitors);

  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;
      gboolean in_fullscreen;

      in_fullscreen = g_slist_find (fullscreen_monitors,
                                    logical_monitor) != NULL;
      if (in_fullscreen != logical_monitor->in_fullscreen)
        {
          logical_monitor->in_fullscreen = in_fullscreen;
          in_fullscreen_changed = TRUE;
        }
    }

  g_slist_free (fullscreen_monitors);

  if (in_fullscreen_changed)
    {
      /* DOCK window stacking depends on the monitor's fullscreen
         status so we need to trigger a re-layering. */
      MetaWindow *window = meta_stack_get_top (display->stack);
      if (window)
        meta_stack_update_layer (display->stack, window);

      g_signal_emit (display, display_signals[IN_FULLSCREEN_CHANGED], 0, NULL);
    }

  return FALSE;
}

void
meta_display_queue_check_fullscreen (MetaDisplay *display)
{
  if (!display->check_fullscreen_later)
    display->check_fullscreen_later = meta_later_add (META_LATER_CHECK_FULLSCREEN,
                                                      check_fullscreen_func,
                                                      display, NULL);
}

int
meta_display_get_monitor_index_for_rect (MetaDisplay   *display,
                                         MetaRectangle *rect)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaLogicalMonitor *logical_monitor;

  logical_monitor =
    meta_monitor_manager_get_logical_monitor_from_rect (monitor_manager, rect);
  if (!logical_monitor)
    return -1;

  return logical_monitor->number;
}

int
meta_display_get_monitor_neighbor_index (MetaDisplay         *display,
                                         int                  which_monitor,
                                         MetaDisplayDirection direction)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaLogicalMonitor *logical_monitor;
  MetaLogicalMonitor *neighbor;

  logical_monitor =
    meta_monitor_manager_get_logical_monitor_from_number (monitor_manager,
                                                          which_monitor);
  neighbor = meta_monitor_manager_get_logical_monitor_neighbor (monitor_manager,
                                                                logical_monitor,
                                                                direction);
  return neighbor ? neighbor->number : -1;
}

/**
 * meta_display_get_current_monitor:
 * @display: a #MetaDisplay
 *
 * Gets the index of the monitor that currently has the mouse pointer.
 *
 * Return value: a monitor index
 */
int
meta_display_get_current_monitor (MetaDisplay *display)
{
  MetaBackend *backend = meta_get_backend ();
  MetaLogicalMonitor *logical_monitor;

  logical_monitor = meta_backend_get_current_logical_monitor (backend);

  /* Pretend its the first when there is no actual current monitor. */
  if (!logical_monitor)
    return 0;

  return logical_monitor->number;
}

/**
 * meta_display_get_n_monitors:
 * @display: a #MetaDisplay
 *
 * Gets the number of monitors that are joined together to form @display.
 *
 * Return value: the number of monitors
 */
int
meta_display_get_n_monitors (MetaDisplay *display)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);

  g_return_val_if_fail (META_IS_DISPLAY (display), 0);

  return meta_monitor_manager_get_num_logical_monitors (monitor_manager);
}

/**
 * meta_display_get_primary_monitor:
 * @display: a #MetaDisplay
 *
 * Gets the index of the primary monitor on this @display.
 *
 * Return value: a monitor index
 */
int
meta_display_get_primary_monitor (MetaDisplay *display)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaLogicalMonitor *logical_monitor;

  g_return_val_if_fail (META_IS_DISPLAY (display), 0);

  logical_monitor =
    meta_monitor_manager_get_primary_logical_monitor (monitor_manager);
  if (logical_monitor)
    return logical_monitor->number;
  else
    return 0;
}

/**
 * meta_display_get_monitor_geometry:
 * @display: a #MetaDisplay
 * @monitor: the monitor number
 * @geometry: (out): location to store the monitor geometry
 *
 * Stores the location and size of the indicated monitor in @geometry.
 */
void
meta_display_get_monitor_geometry (MetaDisplay   *display,
                                   int            monitor,
                                   MetaRectangle *geometry)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaLogicalMonitor *logical_monitor;
#ifndef G_DISABLE_CHECKS
  int n_logical_monitors =
    meta_monitor_manager_get_num_logical_monitors (monitor_manager);
#endif

  g_return_if_fail (META_IS_DISPLAY (display));
  g_return_if_fail (monitor >= 0 && monitor < n_logical_monitors);
  g_return_if_fail (geometry != NULL);

  logical_monitor =
    meta_monitor_manager_get_logical_monitor_from_number (monitor_manager,
                                                          monitor);
  *geometry = logical_monitor->rect;
}

/**
 * meta_display_get_monitor_in_fullscreen:
 * @display: a #MetaDisplay
 * @monitor: the monitor number
 *
 * Determines whether there is a fullscreen window obscuring the specified
 * monitor. If there is a fullscreen window, the desktop environment will
 * typically hide any controls that might obscure the fullscreen window.
 *
 * You can get notification when this changes by connecting to
 * MetaDisplay::in-fullscreen-changed.
 *
 * Returns: %TRUE if there is a fullscreen window covering the specified monitor.
 */
gboolean
meta_display_get_monitor_in_fullscreen (MetaDisplay *display,
                                        int          monitor)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaLogicalMonitor *logical_monitor;
#ifndef G_DISABLE_CHECKS
  int n_logical_monitors =
    meta_monitor_manager_get_num_logical_monitors (monitor_manager);
#endif

  g_return_val_if_fail (META_IS_DISPLAY (display), FALSE);
  g_return_val_if_fail (monitor >= 0 &&
                        monitor < n_logical_monitors, FALSE);

  logical_monitor =
    meta_monitor_manager_get_logical_monitor_from_number (monitor_manager,
                                                          monitor);

  /* We use -1 as a flag to mean "not known yet" for notification
  purposes */ return logical_monitor->in_fullscreen == TRUE;
}

MetaWindow *
meta_display_get_pointer_window (MetaDisplay *display,
                                 MetaWindow  *not_this_one)
{
  MetaBackend *backend = meta_get_backend ();
  MetaCursorTracker *cursor_tracker = meta_backend_get_cursor_tracker (backend);
  MetaWindow *window;
  int x, y;

  if (not_this_one)
    meta_topic (META_DEBUG_FOCUS,
                "Focusing mouse window excluding %s\n", not_this_one->desc);

  meta_cursor_tracker_get_pointer (cursor_tracker, &x, &y, NULL);

  window = meta_stack_get_default_focus_window_at_point (display->stack,
                                                         display->active_workspace,
                                                         not_this_one,
                                                         x, y);

  return window;
}

void
meta_display_init_workspaces (MetaDisplay *display)
{
  MetaWorkspace *current_workspace;
  uint32_t current_workspace_index = 0;
  guint32 timestamp;

  g_return_if_fail (META_IS_DISPLAY (display));

  timestamp = display->x11_display->wm_sn_timestamp;

  /* Get current workspace */
  if (meta_prop_get_cardinal (display->x11_display,
                              display->x11_display->xroot,
                              display->x11_display->atom__NET_CURRENT_DESKTOP,
                              &current_workspace_index))
    meta_verbose ("Read existing _NET_CURRENT_DESKTOP = %d\n",
                  (int) current_workspace_index);
  else
    meta_verbose ("No _NET_CURRENT_DESKTOP present\n");

  update_num_workspaces (display, timestamp);

  set_workspace_names (display);

  /* Switch to the _NET_CURRENT_DESKTOP workspace */
  current_workspace = meta_display_get_workspace_by_index (display,
                                                           current_workspace_index);

  if (current_workspace != NULL)
    meta_workspace_activate (current_workspace, timestamp);
  else
    meta_workspace_activate (display->workspaces->data, timestamp);
}

int
meta_display_get_n_workspaces (MetaDisplay *display)
{
  return g_list_length (display->workspaces);
}

/**
 * meta_display_get_workspace_by_index:
 * @display: a #MetaDisplay
 * @index: index of one of the display's workspaces
 *
 * Gets the workspace object for one of a display's workspaces given the workspace
 * index. It's valid to call this function with an out-of-range index and it
 * will robustly return %NULL.
 *
 * Return value: (transfer none): the workspace object with specified index, or %NULL
 *   if the index is out of range.
 */
MetaWorkspace *
meta_display_get_workspace_by_index (MetaDisplay  *display,
                                     int           idx)
{
  return g_list_nth_data (display->workspaces, idx);
}

void
meta_display_remove_workspace (MetaDisplay   *display,
                               MetaWorkspace *workspace,
                               guint32        timestamp)
{
  GList *l;
  GList *next;
  MetaWorkspace *neighbour = NULL;
  int index;
  gboolean active_index_changed;
  int new_num;

  l = g_list_find (display->workspaces, workspace);
  if (!l)
    return;

  next = l->next;

  if (l->prev)
    neighbour = l->prev->data;
  else if (l->next)
    neighbour = l->next->data;
  else
    {
      /* Cannot remove the only workspace! */
      return;
    }

  meta_workspace_relocate_windows (workspace, neighbour);

  if (workspace == display->active_workspace)
    meta_workspace_activate (neighbour, timestamp);

  /* To emit the signal after removing the workspace */
  index = meta_workspace_index (workspace);
  active_index_changed = index < meta_display_get_active_workspace_index (display);

  /* This also removes the workspace from the displays list */
  meta_workspace_remove (workspace);

  new_num = g_list_length (display->workspaces);

  meta_x11_display_set_number_of_spaces_hint (display->x11_display, new_num);

  if (!meta_prefs_get_dynamic_workspaces ())
    meta_prefs_set_num_workspaces (new_num);

  /* If deleting a workspace before the current workspace, the active
   * workspace index changes, so we need to update that hint */
  if (active_index_changed)
      meta_x11_display_set_active_workspace_hint (display->x11_display);

  for (l = next; l; l = l->next)
    {
      MetaWorkspace *w = l->data;

      meta_workspace_index_changed (w);
    }

  meta_display_queue_workarea_recalc (display);

  g_signal_emit (display, display_signals[WORKSPACE_REMOVED], 0, index);
  g_object_notify (G_OBJECT (display), "n-workspaces");
}

/**
 * meta_display_append_new_workspace:
 * @display: a #MetaDisplay
 * @activate: %TRUE if the workspace should be switched to after creation
 * @timestamp: if switching to a new workspace, timestamp to be used when
 *   focusing a window on the new workspace. (Doesn't hurt to pass a valid
 *   timestamp when available even if not switching workspaces.)
 *
 * Append a new workspace to the display and (optionally) switch to that
 * display.
 *
 * Return value: (transfer none): the newly appended workspace.
 */
MetaWorkspace *
meta_display_append_new_workspace (MetaDisplay *display,
                                   gboolean     activate,
                                   guint32      timestamp)
{
  MetaWorkspace *w;
  int new_num;

  /* This also adds the workspace to the display list */
  w = meta_workspace_new (display);

  if (!w)
    return NULL;

  if (activate)
    meta_workspace_activate (w, timestamp);

  new_num = g_list_length (display->workspaces);

  meta_x11_display_set_number_of_spaces_hint (display->x11_display, new_num);

  if (!meta_prefs_get_dynamic_workspaces ())
    meta_prefs_set_num_workspaces (new_num);

  meta_display_queue_workarea_recalc (display);

  g_signal_emit (display, display_signals[WORKSPACE_ADDED],
                 0, meta_workspace_index (w));
  g_object_notify (G_OBJECT (display), "n-workspaces");

  return w;
}

static void
update_num_workspaces (MetaDisplay *display,
                       guint32     timestamp)
{
  int new_num, old_num;
  GList *l;
  int i;
  GList *extras;
  MetaWorkspace *last_remaining;
  gboolean need_change_space;

  if (meta_prefs_get_dynamic_workspaces ())
    {
      int n_items;
      uint32_t *list;

      n_items = 0;
      list = NULL;

      if (meta_prop_get_cardinal_list (display->x11_display,
                                       display->x11_display->xroot,
                                       display->x11_display->atom__NET_NUMBER_OF_DESKTOPS,
                                       &list, &n_items))
        {
          new_num = list[0];
          meta_XFree (list);
        }
      else
        {
          new_num = 1;
        }
    }
  else
    {
      new_num = meta_prefs_get_num_workspaces ();
    }

  g_assert (new_num > 0);

  if (g_list_length (display->workspaces) == (guint) new_num)
    return;

  last_remaining = NULL;
  extras = NULL;
  i = 0;
  for (l = display->workspaces; l != NULL; l = l->next)
    {
      MetaWorkspace *w = l->data;

      if (i >= new_num)
        extras = g_list_prepend (extras, w);
      else
        last_remaining = w;

      ++i;
    }
  old_num = i;

  g_assert (last_remaining);

  /* Get rid of the extra workspaces by moving all their windows
   * to last_remaining, then activating last_remaining if
   * one of the removed workspaces was active. This will be a bit
   * wacky if the config tool for changing number of workspaces
   * is on a removed workspace ;-)
   */
  need_change_space = FALSE;
  for (l = extras; l != NULL; l = l->next)
    {
      MetaWorkspace *w = l->data;

      meta_workspace_relocate_windows (w, last_remaining);

      if (w == display->active_workspace)
        need_change_space = TRUE;
    }

  if (need_change_space)
    meta_workspace_activate (last_remaining, timestamp);

  /* Should now be safe to free the workspaces */
  for (l = extras; l != NULL; l = l->next)
    {
      MetaWorkspace *w = l->data;

      meta_workspace_remove (w);
    }

  g_list_free (extras);

  for (i = old_num; i < new_num; i++)
    meta_workspace_new (display);

  meta_x11_display_set_number_of_spaces_hint (display->x11_display, new_num);

  meta_display_queue_workarea_recalc (display);

  for (i = old_num; i < new_num; i++)
    g_signal_emit (display, display_signals[WORKSPACE_ADDED], 0, i);

  g_object_notify (G_OBJECT (display), "n-workspaces");
}

#define _NET_WM_ORIENTATION_HORZ 0
#define _NET_WM_ORIENTATION_VERT 1

#define _NET_WM_TOPLEFT     0
#define _NET_WM_TOPRIGHT    1
#define _NET_WM_BOTTOMRIGHT 2
#define _NET_WM_BOTTOMLEFT  3

void
meta_display_update_workspace_layout (MetaDisplay *display)
{
  uint32_t *list;
  int n_items;

  if (display->workspace_layout_overridden)
    return;

  list = NULL;
  n_items = 0;

  if (meta_prop_get_cardinal_list (display->x11_display,
                                   display->x11_display->xroot,
                                   display->x11_display->atom__NET_DESKTOP_LAYOUT,
                                   &list, &n_items))
    {
      if (n_items == 3 || n_items == 4)
        {
          int cols, rows;

          switch (list[0])
            {
            case _NET_WM_ORIENTATION_HORZ:
              display->vertical_workspaces = FALSE;
              break;
            case _NET_WM_ORIENTATION_VERT:
              display->vertical_workspaces = TRUE;
              break;
            default:
              meta_warning ("Someone set a weird orientation in _NET_DESKTOP_LAYOUT\n");
              break;
            }

          cols = list[1];
          rows = list[2];

          if (rows <= 0 && cols <= 0)
            {
              meta_warning ("Columns = %d rows = %d in _NET_DESKTOP_LAYOUT makes no sense\n", rows, cols);
            }
          else
            {
              if (rows > 0)
                display->rows_of_workspaces = rows;
              else
                display->rows_of_workspaces = -1;

              if (cols > 0)
                display->columns_of_workspaces = cols;
              else
                display->columns_of_workspaces = -1;
            }

          if (n_items == 4)
            {
              switch (list[3])
                {
                case _NET_WM_TOPLEFT:
                  display->starting_corner = META_DISPLAY_TOPLEFT;
                  break;
                case _NET_WM_TOPRIGHT:
                  display->starting_corner = META_DISPLAY_TOPRIGHT;
                  break;
                case _NET_WM_BOTTOMRIGHT:
                  display->starting_corner = META_DISPLAY_BOTTOMRIGHT;
                  break;
                case _NET_WM_BOTTOMLEFT:
                  display->starting_corner = META_DISPLAY_BOTTOMLEFT;
                  break;
                default:
                  meta_warning ("Someone set a weird starting corner in _NET_DESKTOP_LAYOUT\n");
                  break;
                }
            }
          else
            display->starting_corner = META_DISPLAY_TOPLEFT;
        }
      else
        {
          meta_warning ("Someone set _NET_DESKTOP_LAYOUT to %d integers instead of 4 "
                        "(3 is accepted for backwards compat)\n", n_items);
        }

      meta_XFree (list);
    }

  meta_verbose ("Workspace layout rows = %d cols = %d orientation = %d starting corner = %u\n",
                display->rows_of_workspaces,
                display->columns_of_workspaces,
                display->vertical_workspaces,
                display->starting_corner);
}

/**
 * meta_display_override_workspace_layout:
 * @display: a #MetaDisplay
 * @starting_corner: the corner at which the first workspace is found
 * @vertical_layout: if %TRUE the workspaces are laid out in columns rather than rows
 * @n_rows: number of rows of workspaces, or -1 to determine the number of rows from
 *   @n_columns and the total number of workspaces
 * @n_columns: number of columns of workspaces, or -1 to determine the number of columns from
 *   @n_rows and the total number of workspaces
 *
 * Explicitly set the layout of workspaces. Once this has been called, the contents of the
 * _NET_DESKTOP_LAYOUT property on the root window are completely ignored.
 */
void
meta_display_override_workspace_layout (MetaDisplay       *display,
                                        MetaDisplayCorner  starting_corner,
                                        gboolean           vertical_layout,
                                        int                n_rows,
                                        int                n_columns)
{
  g_return_if_fail (META_IS_DISPLAY (display));
  g_return_if_fail (n_rows > 0 || n_columns > 0);
  g_return_if_fail (n_rows != 0 && n_columns != 0);

  display->workspace_layout_overridden = TRUE;
  display->vertical_workspaces = vertical_layout != FALSE;
  display->starting_corner = starting_corner;
  display->rows_of_workspaces = n_rows;
  display->columns_of_workspaces = n_columns;

  /* In theory we should remove _NET_DESKTOP_LAYOUT from _NET_SUPPORTED at this
   * point, but it's unlikely that anybody checks that, and it's unlikely that
   * anybody who checks that handles changes, so we'd probably just create
   * a race condition. And it's hard to implement with the code in set_supported_hint()
   */
}

static void
set_workspace_names (MetaDisplay *display)
{
  /* This updates names on root window when the pref changes,
   * note we only get prefs change notify if things have
   * really changed.
   */
  MetaX11Display *x11_display = display->x11_display;
  GString *flattened;
  int i;
  int n_spaces;

  /* flatten to nul-separated list */
  n_spaces = meta_display_get_n_workspaces (display);
  flattened = g_string_new ("");
  i = 0;
  while (i < n_spaces)
    {
      const char *name;

      name = meta_prefs_get_workspace_name (i);

      if (name)
        g_string_append_len (flattened, name,
                             strlen (name) + 1);
      else
        g_string_append_len (flattened, "", 1);

      ++i;
    }

  meta_error_trap_push (x11_display);
  XChangeProperty (x11_display->xdisplay,
                   x11_display->xroot,
                   x11_display->atom__NET_DESKTOP_NAMES,
		   x11_display->atom_UTF8_STRING,
                   8, PropModeReplace,
		   (unsigned char *)flattened->str, flattened->len);
  meta_error_trap_pop (x11_display);

  g_string_free (flattened, TRUE);
}

#ifdef WITH_VERBOSE_MODE
static const char *
meta_display_corner_to_string (MetaDisplayCorner corner)
{
  switch (corner)
    {
    case META_DISPLAY_TOPLEFT:
      return "TopLeft";
    case META_DISPLAY_TOPRIGHT:
      return "TopRight";
    case META_DISPLAY_BOTTOMLEFT:
      return "BottomLeft";
    case META_DISPLAY_BOTTOMRIGHT:
      return "BottomRight";
    }

  return "Unknown";
}
#endif /* WITH_VERBOSE_MODE */

void
meta_display_calc_workspace_layout (MetaDisplay         *display,
                                    int                  num_workspaces,
                                    int                  current_space,
                                    MetaWorkspaceLayout *layout)
{
  int rows, cols;
  int grid_area;
  int *grid;
  int i, r, c;
  int current_row, current_col;

  rows = display->rows_of_workspaces;
  cols = display->columns_of_workspaces;
  if (rows <= 0 && cols <= 0)
    cols = num_workspaces;

  if (rows <= 0)
    rows = num_workspaces / cols + ((num_workspaces % cols) > 0 ? 1 : 0);
  if (cols <= 0)
    cols = num_workspaces / rows + ((num_workspaces % rows) > 0 ? 1 : 0);

  /* paranoia */
  if (rows < 1)
    rows = 1;
  if (cols < 1)
    cols = 1;

  g_assert (rows != 0 && cols != 0);

  grid_area = rows * cols;

  meta_verbose ("Getting layout rows = %d cols = %d current = %d "
                "num_spaces = %d vertical = %s corner = %s\n",
                rows, cols, current_space, num_workspaces,
                display->vertical_workspaces ? "(true)" : "(false)",
                meta_display_corner_to_string (display->starting_corner));

  /* ok, we want to setup the distances in the workspace array to go
   * in each direction. Remember, there are many ways that a workspace
   * array can be setup.
   * see http://www.freedesktop.org/standards/wm-spec/1.2/html/x109.html
   * and look at the _NET_DESKTOP_LAYOUT section for details.
   * For instance:
   */
  /* starting_corner = META_DISPLAY_TOPLEFT
   *  vertical_workspaces = 0                 vertical_workspaces=1
   *       1234                                    1357
   *       5678                                    2468
   *
   * starting_corner = META_DISPLAY_TOPRIGHT
   *  vertical_workspaces = 0                 vertical_workspaces=1
   *       4321                                    7531
   *       8765                                    8642
   *
   * starting_corner = META_DISPLAY_BOTTOMLEFT
   *  vertical_workspaces = 0                 vertical_workspaces=1
   *       5678                                    2468
   *       1234                                    1357
   *
   * starting_corner = META_DISPLAY_BOTTOMRIGHT
   *  vertical_workspaces = 0                 vertical_workspaces=1
   *       8765                                    8642
   *       4321                                    7531
   *
   */
  /* keep in mind that we could have a ragged layout, e.g. the "8"
   * in the above grids could be missing
   */


  grid = g_new (int, grid_area);

  current_row = -1;
  current_col = -1;
  i = 0;

  switch (display->starting_corner)
    {
    case META_DISPLAY_TOPLEFT:
      if (display->vertical_workspaces)
        {
          c = 0;
          while (c < cols)
            {
              r = 0;
              while (r < rows)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++r;
                }
              ++c;
            }
        }
      else
        {
          r = 0;
          while (r < rows)
            {
              c = 0;
              while (c < cols)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++c;
                }
              ++r;
            }
        }
      break;
    case META_DISPLAY_TOPRIGHT:
      if (display->vertical_workspaces)
        {
          c = cols - 1;
          while (c >= 0)
            {
              r = 0;
              while (r < rows)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++r;
                }
              --c;
            }
        }
      else
        {
          r = 0;
          while (r < rows)
            {
              c = cols - 1;
              while (c >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --c;
                }
              ++r;
            }
        }
      break;
    case META_DISPLAY_BOTTOMLEFT:
      if (display->vertical_workspaces)
        {
          c = 0;
          while (c < cols)
            {
              r = rows - 1;
              while (r >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --r;
                }
              ++c;
            }
        }
      else
        {
          r = rows - 1;
          while (r >= 0)
            {
              c = 0;
              while (c < cols)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++c;
                }
              --r;
            }
        }
      break;
    case META_DISPLAY_BOTTOMRIGHT:
      if (display->vertical_workspaces)
        {
          c = cols - 1;
          while (c >= 0)
            {
              r = rows - 1;
              while (r >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --r;
                }
              --c;
            }
        }
      else
        {
          r = rows - 1;
          while (r >= 0)
            {
              c = cols - 1;
              while (c >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --c;
                }
              --r;
            }
        }
      break;
    }

  if (i != grid_area)
    meta_bug ("did not fill in the whole workspace grid in %s (%d filled)\n",
              G_STRFUNC, i);

  current_row = 0;
  current_col = 0;
  r = 0;
  while (r < rows)
    {
      c = 0;
      while (c < cols)
        {
          if (grid[r*cols+c] == current_space)
            {
              current_row = r;
              current_col = c;
            }
          else if (grid[r*cols+c] >= num_workspaces)
            {
              /* flag nonexistent spaces with -1 */
              grid[r*cols+c] = -1;
            }
          ++c;
        }
      ++r;
    }

  layout->rows = rows;
  layout->cols = cols;
  layout->grid = grid;
  layout->grid_area = grid_area;
  layout->current_row = current_row;
  layout->current_col = current_col;

#ifdef WITH_VERBOSE_MODE
  if (meta_is_verbose ())
    {
      r = 0;
      while (r < layout->rows)
        {
          meta_verbose (" ");
          meta_push_no_msg_prefix ();
          c = 0;
          while (c < layout->cols)
            {
              if (r == layout->current_row &&
                  c == layout->current_col)
                meta_verbose ("*%2d ", layout->grid[r*layout->cols+c]);
              else
                meta_verbose ("%3d ", layout->grid[r*layout->cols+c]);
              ++c;
            }
          meta_verbose ("\n");
          meta_pop_no_msg_prefix ();
          ++r;
        }
    }
#endif /* WITH_VERBOSE_MODE */
}

void
meta_display_free_workspace_layout (MetaWorkspaceLayout *layout)
{
  g_free (layout->grid);
}

static void
queue_windows_showing (MetaDisplay *display)
{
  GSList *windows, *l;

  /* Must operate on all windows on display instead of just on the
   * active_workspace's window list, because the active_workspace's
   * window list may not contain the on_all_workspace windows.
   */
  windows = meta_display_list_windows (display, META_LIST_DEFAULT);

  for (l = windows; l != NULL; l = l->next)
    {
      MetaWindow *w = l->data;
      meta_window_queue (w, META_QUEUE_CALC_SHOWING);
    }

  g_slist_free (windows);
}

void
meta_display_minimize_all_on_active_workspace_except (MetaDisplay *display,
                                                      MetaWindow  *keep)
{
  GList *l;

  for (l = display->active_workspace->windows; l != NULL; l = l->next)
    {
      MetaWindow *w = l->data;

      if (w->has_minimize_func && w != keep)
	meta_window_minimize (w);
    }
}

void
meta_display_show_desktop (MetaDisplay *display,
                           guint32      timestamp)
{
  GList *l;

  if (display->active_workspace->showing_desktop)
    return;

  display->active_workspace->showing_desktop = TRUE;

  queue_windows_showing (display);

  /* Focus the most recently used META_WINDOW_DESKTOP window, if there is one;
   * see bug 159257.
   */
  for (l = display->active_workspace->mru_list; l != NULL; l = l->next)
    {
      MetaWindow *w = l->data;

      if (w->type == META_WINDOW_DESKTOP)
        {
          meta_window_focus (w, timestamp);
          break;
        }
    }

  meta_x11_display_update_showing_desktop_hint (display->x11_display);
}

void
meta_display_unshow_desktop (MetaDisplay *display)
{
  if (!display->active_workspace->showing_desktop)
    return;

  display->active_workspace->showing_desktop = FALSE;

  queue_windows_showing (display);

  meta_x11_display_update_showing_desktop_hint (display->x11_display);
}

/**
 * meta_display_get_workspaces: (skip)
 * @display: a #MetaDisplay
 *
 * Returns: (transfer none) (element-type Meta.Workspace): The workspaces for @display
 */
GList *
meta_display_get_workspaces (MetaDisplay *display)
{
  return display->workspaces;
}

int
meta_display_get_active_workspace_index (MetaDisplay *display)
{
  MetaWorkspace *active = display->active_workspace;

  if (!active)
    return -1;

  return meta_workspace_index (active);
}

/**
 * meta_display_get_active_workspace:
 * @display: A #MetaDisplay
 *
 * Returns: (transfer none): The current workspace
 */
MetaWorkspace *
meta_display_get_active_workspace (MetaDisplay *display)
{
  return display->active_workspace;
}

void
meta_display_focus_default_window (MetaDisplay *display,
                                   guint32      timestamp)
{
  meta_workspace_focus_default_window (display->active_workspace,
                                       NULL,
                                       timestamp);
}

void
meta_display_workspace_switched (MetaDisplay        *display,
                                 int                 from,
                                 int                 to,
                                 MetaMotionDirection direction)
{
  g_signal_emit (display, display_signals[WORKSPACE_SWITCHED], 0,
                 from, to, direction);
}
