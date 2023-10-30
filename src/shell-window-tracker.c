/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <meta/display.h>
#include <meta/group.h>
#include <meta/util.h>
#include <meta/window.h>
#include <meta/meta-workspace-manager.h>
#include <meta/meta-startup-notification.h>

#include "shell-window-tracker-private.h"
#include "shell-app-private.h"
#include "shell-global.h"
#include "st.h"

/* This file includes modified code from
 * desktop-data-engine/engine-dbus/hippo-application-monitor.c
 * in the functions collecting application usage data.
 * Written by Owen Taylor, originally licensed under LGPL 2.1.
 * Copyright Red Hat, Inc. 2006-2008
 */

/**
 * SECTION:shell-window-tracker
 * @short_description: Associate windows with applications
 *
 * Maintains a mapping from windows to applications (.desktop file ids).
 * It currently implements this with some heuristics on the WM_CLASS X11
 * property (and some static override regexps); in the future, we want to
 * have it also track through startup-notification.
 */

struct _ShellWindowTracker
{
  GObject parent;

  ShellApp *focus_app;

  /* <MetaWindow * window, ShellApp *app> */
  GHashTable *window_to_app;
};

G_DEFINE_TYPE (ShellWindowTracker, shell_window_tracker, G_TYPE_OBJECT);

enum {
  PROP_0,

  PROP_FOCUS_APP,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

enum {
  STARTUP_SEQUENCE_CHANGED,
  TRACKED_WINDOWS_CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void shell_window_tracker_finalize (GObject *object);
static void set_focus_app (ShellWindowTracker  *tracker,
                           ShellApp            *new_focus_app);
static void on_focus_window_changed (MetaDisplay *display, GParamSpec *spec, ShellWindowTracker *tracker);

static void track_window (ShellWindowTracker *tracker, MetaWindow *window);
static void disassociate_window (ShellWindowTracker *tracker, MetaWindow *window);

static ShellApp * shell_startup_sequence_get_app (MetaStartupSequence *sequence);

static void
shell_window_tracker_get_property (GObject    *gobject,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ShellWindowTracker *tracker = SHELL_WINDOW_TRACKER (gobject);

  switch (prop_id)
    {
    case PROP_FOCUS_APP:
      g_value_set_object (value, tracker->focus_app);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
shell_window_tracker_class_init (ShellWindowTrackerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = shell_window_tracker_get_property;
  gobject_class->finalize = shell_window_tracker_finalize;

  props[PROP_FOCUS_APP] =
    g_param_spec_object ("focus-app",
                         "Focus App",
                         "Focused application",
                         SHELL_TYPE_APP,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, props);

  signals[STARTUP_SEQUENCE_CHANGED] = g_signal_new ("startup-sequence-changed",
                                   SHELL_TYPE_WINDOW_TRACKER,
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL, NULL, NULL,
                                   G_TYPE_NONE, 1, META_TYPE_STARTUP_SEQUENCE);
  signals[TRACKED_WINDOWS_CHANGED] = g_signal_new ("tracked-windows-changed",
                                                   SHELL_TYPE_WINDOW_TRACKER,
                                                   G_SIGNAL_RUN_LAST,
                                                   0,
                                                   NULL, NULL, NULL,
                                                   G_TYPE_NONE, 0);
}

static gboolean
check_app_id_prefix (ShellApp   *app,
                     const char *prefix)
{
  if (prefix == NULL)
    return TRUE;

  return g_str_has_prefix (shell_app_get_id (app), prefix);
}

/*
 * get_app_from_window_wmclass:
 *
 * Looks only at the given window, and attempts to determine
 * an application based on WM_CLASS.  If one can't be determined,
 * return %NULL.
 *
 * Return value: (transfer full): A newly-referenced #ShellApp, or %NULL
 */
static ShellApp *
get_app_from_window_wmclass (MetaWindow  *window)
{
  ShellApp *app;
  ShellAppSystem *appsys;
  const char *wm_class;
  const char *wm_instance;
  const char *sandbox_id;
  g_autofree char *app_prefix = NULL;

  appsys = shell_app_system_get_default ();

  sandbox_id = meta_window_get_sandboxed_app_id (window);
  if (sandbox_id)
    app_prefix = g_strdup_printf ("%s.", sandbox_id);

  /* Notes on the heuristics used here:
     much of the complexity here comes from the desire to support
     Chrome apps.

     From https://bugzilla.gnome.org/show_bug.cgi?id=673657#c13

     Currently chrome sets WM_CLASS as follows (the first string is the 'instance',
     the second one is the 'class':

     For the normal browser:
     WM_CLASS(STRING) = "chromium", "Chromium"

     For a bookmarked page (through 'Tools -> Create application shortcuts')
     WM_CLASS(STRING) = "wiki.gnome.org__GnomeShell_ApplicationBased", "Chromium"

     For an application from the chrome store (with a .desktop file created through
     right click, "Create shortcuts" from Chrome's apps overview)
     WM_CLASS(STRING) = "crx_blpcfgokakmgnkcojhhkbfbldkacnbeo", "Chromium"

     The .desktop file has a matching StartupWMClass, but the name differs, e.g. for
     the store app (youtube) there is

     .local/share/applications/chrome-blpcfgokakmgnkcojhhkbfbldkacnbeo-Default.desktop

     with

     StartupWMClass=crx_blpcfgokakmgnkcojhhkbfbldkacnbeo

     Note that chromium (but not google-chrome!) includes a StartupWMClass=chromium
     in their .desktop file, so we must match the instance first.

     Also note that in the good case (regular gtk+ app without hacks), instance and
     class are the same except for case and there is no StartupWMClass at all.
  */

  /* first try a match from WM_CLASS (instance part) to StartupWMClass */
  wm_instance = meta_window_get_wm_class_instance (window);
  app = shell_app_system_lookup_startup_wmclass (appsys, wm_instance);
  if (app != NULL && check_app_id_prefix (app, app_prefix))
    return g_object_ref (app);

  /* then try a match from WM_CLASS to StartupWMClass */
  wm_class = meta_window_get_wm_class (window);
  app = shell_app_system_lookup_startup_wmclass (appsys, wm_class);
  if (app != NULL && check_app_id_prefix (app, app_prefix))
    return g_object_ref (app);

  /* then try a match from WM_CLASS (instance part) to .desktop */
  app = shell_app_system_lookup_desktop_wmclass (appsys, wm_instance);
  if (app != NULL && check_app_id_prefix (app, app_prefix))
    return g_object_ref (app);

  /* finally, try a match from WM_CLASS to .desktop */
  app = shell_app_system_lookup_desktop_wmclass (appsys, wm_class);
  if (app != NULL && check_app_id_prefix (app, app_prefix))
    return g_object_ref (app);

  return NULL;
}

/*
 * get_app_from_id:
 * @window: a #MetaWindow
 *
 * Looks only at the given window, and attempts to determine
 * an application based on %id.  If one can't be determined,
 * return %NULL.
 *
 * Return value: (transfer full): A newly-referenced #ShellApp, or %NULL
 */
static ShellApp *
get_app_from_id (MetaWindow  *window,
                 const char  *id)
{
  ShellApp *app;
  ShellAppSystem *appsys;
  g_autofree char *desktop_file = NULL;

  g_return_val_if_fail (id != NULL, NULL);

  appsys = shell_app_system_get_default ();

  desktop_file = g_strconcat (id, ".desktop", NULL);
  app = shell_app_system_lookup_app (appsys, desktop_file);
  if (app)
    return g_object_ref (app);

  return NULL;
}

/*
 * get_app_from_gapplication_id:
 * @window: a #MetaWindow
 *
 * Looks only at the given window, and attempts to determine
 * an application based on _GTK_APPLICATION_ID.  If one can't be determined,
 * return %NULL.
 *
 * Return value: (transfer full): A newly-referenced #ShellApp, or %NULL
 */
static ShellApp *
get_app_from_gapplication_id (MetaWindow  *window)
{
  const char *id;

  id = meta_window_get_gtk_application_id (window);
  if (!id)
    return NULL;

  return get_app_from_id (window, id);
}

/*
 * get_app_from_sandboxed_app_id:
 * @window: a #MetaWindow
 *
 * Looks only at the given window, and attempts to determine
 * an application based on its Flatpak or Snap ID.  If one can't be determined,
 * return %NULL.
 *
 * Return value: (transfer full): A newly-referenced #ShellApp, or %NULL
 */
static ShellApp *
get_app_from_sandboxed_app_id (MetaWindow  *window)
{
  const char *id;

  id = meta_window_get_sandboxed_app_id (window);
  if (!id)
    return NULL;

  return get_app_from_id (window, id);
}

/*
 * get_app_from_window_group:
 * @monitor: a #ShellWindowTracker
 * @window: a #MetaWindow
 *
 * Check other windows in the group for @window to see if we have
 * an application for one of them.
 *
 * Return value: (transfer full): A newly-referenced #ShellApp, or %NULL
 */
static ShellApp*
get_app_from_window_group (ShellWindowTracker  *tracker,
                           MetaWindow          *window)
{
  ShellApp *result;
  GSList *group_windows;
  MetaGroup *group;
  GSList *iter;

  group = meta_window_get_group (window);
  if (group == NULL)
    return NULL;

  group_windows = meta_group_list_windows (group);

  result = NULL;
  /* Try finding a window in the group of type NORMAL; if we
   * succeed, use that as our source. */
  for (iter = group_windows; iter; iter = iter->next)
    {
      MetaWindow *group_window = iter->data;

      if (meta_window_get_window_type (group_window) != META_WINDOW_NORMAL)
        continue;

      result = g_hash_table_lookup (tracker->window_to_app, group_window);
      if (result)
        break;
    }

  g_slist_free (group_windows);

  if (result)
    g_object_ref (result);

  return result;
}

/*
 * get_app_from_window_pid:
 * @tracker: a #ShellWindowTracker
 * @window: a #MetaWindow
 *
 * Check if the pid associated with @window corresponds to an
 * application.
 *
 * Return value: (transfer full): A newly-referenced #ShellApp, or %NULL
 */
static ShellApp *
get_app_from_window_pid (ShellWindowTracker  *tracker,
                         MetaWindow          *window)
{
  ShellApp *result;
  pid_t pid;

  if (meta_window_is_remote (window))
    return NULL;

  pid = meta_window_get_pid (window);

  if (pid < 1)
    return NULL;

  result = shell_window_tracker_get_app_from_pid (tracker, pid);
  if (result != NULL)
    g_object_ref (result);

  return result;
}

/**
 * get_app_for_window:
 *
 * Determines the application associated with a window, using
 * all available information such as the window's MetaGroup,
 * and what we know about other windows.
 *
 * Returns: (transfer full): a #ShellApp, or NULL if none is found
 */
static ShellApp *
get_app_for_window (ShellWindowTracker    *tracker,
                    MetaWindow            *window)
{
  ShellApp *result = NULL;
  MetaWindow *transient_for;
  const char *startup_id;

  transient_for = meta_window_get_transient_for (window);
  if (transient_for != NULL)
    return get_app_for_window (tracker, transient_for);

  /* First, we check whether we already know about this window,
   * if so, just return that.
   */
  if (meta_window_get_window_type (window) == META_WINDOW_NORMAL
      || meta_window_is_remote (window))
    {
      result = g_hash_table_lookup (tracker->window_to_app, window);
      if (result != NULL)
        {
          g_object_ref (result);
          return result;
        }
    }

  if (meta_window_is_remote (window))
    return _shell_app_new_for_window (window);

  /* Check if the app's WM_CLASS specifies an app; this is
   * canonical if it does.
   */
  result = get_app_from_window_wmclass (window);
  if (result != NULL)
    return result;

  /* Check if the window was opened from within a sandbox; if this
   * is the case, a corresponding .desktop file is guaranteed to match;
   */
  result = get_app_from_sandboxed_app_id (window);
  if (result != NULL)
    return result;

  /* Check if the window has a GApplication ID attached; this is
   * canonical if it does
   */
  result = get_app_from_gapplication_id (window);
  if (result != NULL)
    return result;

  result = get_app_from_window_pid (tracker, window);
  if (result != NULL)
    return result;

  /* Now we check whether we have a match through startup-notification */
  startup_id = meta_window_get_startup_id (window);
  if (startup_id)
    {
      GSList *iter, *sequences;

      sequences = shell_window_tracker_get_startup_sequences (tracker);
      for (iter = sequences; iter; iter = iter->next)
        {
          MetaStartupSequence *sequence = iter->data;
          const char *id = meta_startup_sequence_get_id (sequence);
          if (strcmp (id, startup_id) != 0)
            continue;

          result = shell_startup_sequence_get_app (sequence);
          if (result)
            {
              result = g_object_ref (result);
              break;
            }
        }
    }

  /* If we didn't get a startup-notification match, see if we matched
   * any other windows in the group.
   */
  if (result == NULL)
    result = get_app_from_window_group (tracker, window);

  /* Our last resort - we create a fake app from the window */
  if (result == NULL)
    result = _shell_app_new_for_window (window);

  return result;
}

static void
update_focus_app (ShellWindowTracker *self)
{
  MetaWindow *new_focus_win;
  ShellApp *new_focus_app;

  new_focus_win = meta_display_get_focus_window (shell_global_get_display (shell_global_get ()));

  /* we only consider an app focused if the focus window can be clearly
   * associated with a running app; this is the case if the focus window
   * or one of its parents is visible in the taskbar, e.g.
   *   - 'nautilus' should appear focused when its about dialog has focus
   *   - 'nautilus' should not appear focused when the DESKTOP has focus
   */
  while (new_focus_win && meta_window_is_skip_taskbar (new_focus_win))
    new_focus_win = meta_window_get_transient_for (new_focus_win);

  new_focus_app = new_focus_win ? shell_window_tracker_get_window_app (self, new_focus_win) : NULL;

  if (new_focus_app)
    {
      shell_app_update_window_actions (new_focus_app, new_focus_win);
      shell_app_update_app_actions (new_focus_app, new_focus_win);
    }

  set_focus_app (self, new_focus_app);

  g_clear_object (&new_focus_app);
}

static void
tracked_window_changed (ShellWindowTracker *self,
                        MetaWindow         *window)
{
  /* It's simplest to just treat this as a remove + add. */
  disassociate_window (self, window);
  track_window (self, window);
  /* also just recalculate the focused app, in case it was the focused
     window that changed */
  update_focus_app (self);
}

static void
on_wm_class_changed (MetaWindow  *window,
                     GParamSpec  *pspec,
                     gpointer     user_data)
{
  ShellWindowTracker *self = SHELL_WINDOW_TRACKER (user_data);
  tracked_window_changed (self, window);
}

static void
on_title_changed (MetaWindow  *window,
                  GParamSpec  *pspec,
                  gpointer     user_data)
{
  ShellWindowTracker *self = SHELL_WINDOW_TRACKER (user_data);
  g_signal_emit (self, signals[TRACKED_WINDOWS_CHANGED], 0);
}

static void
on_gtk_application_id_changed (MetaWindow  *window,
                               GParamSpec  *pspec,
                               gpointer     user_data)
{
  ShellWindowTracker *self = SHELL_WINDOW_TRACKER (user_data);
  tracked_window_changed (self, window);
}

static void
on_window_unmanaged (MetaWindow *window,
                     gpointer    user_data)
{
  disassociate_window (SHELL_WINDOW_TRACKER (user_data), window);
}

static void
track_window (ShellWindowTracker *self,
              MetaWindow      *window)
{
  ShellApp *app;

  app = get_app_for_window (self, window);
  if (!app)
    return;

  /* At this point we've stored the association from window -> application */
  g_hash_table_insert (self->window_to_app, window, app);

  g_signal_connect (window, "notify::wm-class", G_CALLBACK (on_wm_class_changed), self);
  g_signal_connect (window, "notify::title", G_CALLBACK (on_title_changed), self);
  g_signal_connect (window, "notify::gtk-application-id", G_CALLBACK (on_gtk_application_id_changed), self);
  g_signal_connect (window, "unmanaged", G_CALLBACK (on_window_unmanaged), self);

  _shell_app_add_window (app, window);

  g_signal_emit (self, signals[TRACKED_WINDOWS_CHANGED], 0);
}

static void
on_window_created (MetaDisplay *display,
                   MetaWindow  *window,
                   gpointer     user_data)
{
  track_window (SHELL_WINDOW_TRACKER (user_data), window);
}

static void
disassociate_window (ShellWindowTracker   *self,
                     MetaWindow        *window)
{
  ShellApp *app;

  app = g_hash_table_lookup (self->window_to_app, window);
  if (!app)
    return;

  g_object_ref (app);

  g_hash_table_remove (self->window_to_app, window);

  _shell_app_remove_window (app, window);
  g_signal_handlers_disconnect_by_func (window, G_CALLBACK (on_wm_class_changed), self);
  g_signal_handlers_disconnect_by_func (window, G_CALLBACK (on_title_changed), self);
  g_signal_handlers_disconnect_by_func (window, G_CALLBACK (on_gtk_application_id_changed), self);
  g_signal_handlers_disconnect_by_func (window, G_CALLBACK (on_window_unmanaged), self);

  g_signal_emit (self, signals[TRACKED_WINDOWS_CHANGED], 0);

  g_object_unref (app);
}

static void
load_initial_windows (ShellWindowTracker *tracker)
{
  MetaDisplay *display = shell_global_get_display (shell_global_get ());
  g_autoptr (GList) windows = NULL;
  GList *l;

  windows = meta_display_list_all_windows (display);
  for (l = windows; l; l = l->next)
    track_window (tracker, l->data);
}

static void
init_window_tracking (ShellWindowTracker *self)
{
  MetaDisplay *display = shell_global_get_display (shell_global_get ());

  g_signal_connect_object (display, "notify::focus-window",
                           G_CALLBACK (on_focus_window_changed), self,
                           G_CONNECT_DEFAULT);
  g_signal_connect_object (display, "window-created",
                           G_CALLBACK (on_window_created), self,
                           G_CONNECT_DEFAULT);
}

static void
on_startup_sequence_changed (MetaStartupNotification *sn,
                             MetaStartupSequence     *sequence,
                             ShellWindowTracker      *self)
{
  ShellApp *app;

  app = shell_startup_sequence_get_app (sequence);
  if (app)
    _shell_app_handle_startup_sequence (app, sequence);

  g_signal_emit (G_OBJECT (self), signals[STARTUP_SEQUENCE_CHANGED], 0, sequence);
}

static void
on_shutdown (ShellGlobal        *shell_global,
             ShellWindowTracker *tracker)
{
  g_autoptr (GList) windows;
  GList *l;

  windows = g_hash_table_get_keys (tracker->window_to_app);
  for (l = windows; l; l = l->next)
    {
      MetaWindow *window = l->data;

      disassociate_window (tracker, window);
    }
  g_assert (g_hash_table_size (tracker->window_to_app) == 0);
}

static void
shell_window_tracker_init (ShellWindowTracker *self)
{
  MetaDisplay *display = shell_global_get_display (shell_global_get ());
  MetaStartupNotification *sn = meta_display_get_startup_notification (display);

  self->window_to_app = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                               NULL, (GDestroyNotify) g_object_unref);


  g_signal_connect (sn, "changed",
                    G_CALLBACK (on_startup_sequence_changed), self);

  load_initial_windows (self);
  init_window_tracking (self);

  g_signal_connect (shell_global_get (),
                    "shutdown", G_CALLBACK (on_shutdown), self);
}

static void
shell_window_tracker_finalize (GObject *object)
{
  ShellWindowTracker *self = SHELL_WINDOW_TRACKER (object);

  g_hash_table_destroy (self->window_to_app);

  G_OBJECT_CLASS (shell_window_tracker_parent_class)->finalize(object);
}

/**
 * shell_window_tracker_get_window_app:
 * @tracker: An app monitor instance
 * @metawin: A #MetaWindow
 *
 * Returns: (transfer full): Application associated with window
 */
ShellApp *
shell_window_tracker_get_window_app (ShellWindowTracker *tracker,
                                     MetaWindow         *metawin)
{
  ShellApp *app;

  app = g_hash_table_lookup (tracker->window_to_app, metawin);
  if (app)
    g_object_ref (app);

  return app;
}


/**
 * shell_window_tracker_get_app_from_pid:
 * @tracker: A #ShellAppSystem
 * @pid: A Unix process identifier
 *
 * Look up the application corresponding to a process.
 *
 * Returns: (transfer none): A #ShellApp, or %NULL if none
 */
ShellApp *
shell_window_tracker_get_app_from_pid (ShellWindowTracker *tracker,
                                       int                 pid)
{
  GSList *running = shell_app_system_get_running (shell_app_system_get_default());
  GSList *iter;
  ShellApp *result = NULL;

  for (iter = running; iter; iter = iter->next)
    {
      ShellApp *app = iter->data;
      GSList *pids = shell_app_get_pids (app);
      GSList *pids_iter;

      for (pids_iter = pids; pids_iter; pids_iter = pids_iter->next)
        {
          int app_pid = GPOINTER_TO_INT (pids_iter->data);
          if (app_pid == pid)
            {
              result = app;
              break;
            }
        }
      g_slist_free (pids);

      if (result != NULL)
        break;
    }

  g_slist_free (running);

  return result;
}

static void
set_focus_app (ShellWindowTracker  *tracker,
               ShellApp            *new_focus_app)
{
  if (new_focus_app == tracker->focus_app)
    return;

  if (tracker->focus_app != NULL)
    g_object_unref (tracker->focus_app);

  tracker->focus_app = new_focus_app;

  if (tracker->focus_app != NULL)
    g_object_ref (tracker->focus_app);

  g_object_notify_by_pspec (G_OBJECT (tracker), props[PROP_FOCUS_APP]);
}

static void
on_focus_window_changed (MetaDisplay        *display,
                         GParamSpec         *spec,
                         ShellWindowTracker *tracker)
{
  update_focus_app (tracker);
}

/**
 * shell_window_tracker_get_startup_sequences:
 * @tracker:
 *
 * Returns: (transfer none) (element-type MetaStartupSequence): Currently active startup sequences
 */
GSList *
shell_window_tracker_get_startup_sequences (ShellWindowTracker *self)
{
  ShellGlobal *global = shell_global_get ();
  MetaDisplay *display = shell_global_get_display (global);
  MetaStartupNotification *sn = meta_display_get_startup_notification (display);

  return meta_startup_notification_get_sequences (sn);
}

static ShellApp *
shell_startup_sequence_get_app (MetaStartupSequence *sequence)
{
  const char *appid;
  char *basename;
  ShellAppSystem *appsys;
  ShellApp *app;

  appid = meta_startup_sequence_get_application_id (sequence);
  if (!appid)
    return NULL;

  basename = g_path_get_basename (appid);
  appsys = shell_app_system_get_default ();
  app = shell_app_system_lookup_app (appsys, basename);
  g_free (basename);
  return app;
}

/**
 * shell_window_tracker_get_default:
 *
 * Return Value: (transfer none): The global #ShellWindowTracker instance
 */
ShellWindowTracker *
shell_window_tracker_get_default (void)
{
  return shell_global_get_window_tracker (shell_global_get ());
}
