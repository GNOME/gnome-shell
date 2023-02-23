/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include <meta/display.h>
#include <meta/meta-context.h>
#include <meta/meta-workspace-manager.h>
#include <meta/meta-x11-display.h>

#include "shell-app-private.h"
#include "shell-enum-types.h"
#include "shell-global.h"
#include "shell-util.h"
#include "shell-app-system-private.h"
#include "shell-window-tracker-private.h"
#include "st.h"
#include "gtkactionmuxer.h"
#include "org-gtk-application.h"
#include "switcheroo-control.h"

#ifdef HAVE_SYSTEMD
#include <systemd/sd-journal.h>
#include <errno.h>
#include <unistd.h>
#endif

/* This is mainly a memory usage optimization - the user is going to
 * be running far fewer of the applications at one time than they have
 * installed.  But it also just helps keep the code more logically
 * separated.
 */
typedef struct {
  guint refcount;

  /* Signal connection to dirty window sort list on workspace changes */
  gulong workspace_switch_id;

  GSList *windows;

  guint interesting_windows;

  /* Whether or not we need to resort the windows; this is done on demand */
  guint window_sort_stale : 1;

  /* See GApplication documentation */
  GtkActionMuxer   *muxer;
  char             *unique_bus_name;
  GDBusConnection  *session;

  /* GDBus Proxy for getting application busy state */
  ShellOrgGtkApplication *application_proxy;
  GCancellable           *cancellable;

} ShellAppRunningState;

/**
 * SECTION:shell-app
 * @short_description: Object representing an application
 *
 * This object wraps a #GDesktopAppInfo, providing methods and signals
 * primarily useful for running applications.
 */
struct _ShellApp
{
  GObject parent;

  int started_on_workspace;

  ShellAppState state;

  GDesktopAppInfo *info; /* If NULL, this app is backed by one or more
                          * MetaWindow.  For purposes of app title
                          * etc., we use the first window added,
                          * because it's most likely to be what we
                          * want (e.g. it will be of TYPE_NORMAL from
                          * the way shell-window-tracker.c works).
                          */
  GIcon *fallback_icon;
  MetaWindow *fallback_icon_window;

  ShellAppRunningState *running_state;

  char *window_id_string;
  char *name_collation_key;
};

enum {
  PROP_0,

  PROP_STATE,
  PROP_BUSY,
  PROP_ID,
  PROP_ACTION_GROUP,
  PROP_ICON,
  PROP_APP_INFO,

  N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

enum {
  WINDOWS_CHANGED,
  LAST_SIGNAL
};

static guint shell_app_signals[LAST_SIGNAL] = { 0 };

static void create_running_state (ShellApp *app);
static void unref_running_state (ShellAppRunningState *state);

G_DEFINE_TYPE (ShellApp, shell_app, G_TYPE_OBJECT)

static void
shell_app_get_property (GObject    *gobject,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  ShellApp *app = SHELL_APP (gobject);

  switch (prop_id)
    {
    case PROP_STATE:
      g_value_set_enum (value, app->state);
      break;
    case PROP_BUSY:
      g_value_set_boolean (value, shell_app_get_busy (app));
      break;
    case PROP_ID:
      g_value_set_string (value, shell_app_get_id (app));
      break;
    case PROP_ICON:
      g_value_set_object (value, shell_app_get_icon (app));
      break;
    case PROP_ACTION_GROUP:
      if (app->running_state)
        g_value_set_object (value, app->running_state->muxer);
      break;
    case PROP_APP_INFO:
      if (app->info)
        g_value_set_object (value, app->info);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
shell_app_set_property (GObject      *gobject,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  ShellApp *app = SHELL_APP (gobject);

  switch (prop_id)
    {
    case PROP_APP_INFO:
      _shell_app_set_app_info (app, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

const char *
shell_app_get_id (ShellApp *app)
{
  if (app->info)
    return g_app_info_get_id (G_APP_INFO (app->info));
  return app->window_id_string;
}

static MetaWindow *
window_backed_app_get_window (ShellApp     *app)
{
  g_assert (app->info == NULL);
  if (app->running_state)
    {
      g_assert (app->running_state->windows);
      return app->running_state->windows->data;
    }
  else
    return NULL;
}

static GIcon *
x11_window_create_fallback_gicon (MetaWindow *window)
{
  StTextureCache *texture_cache;
  cairo_surface_t *surface;

  g_object_get (window, "icon", &surface, NULL);

  texture_cache = st_texture_cache_get_default ();
  return st_texture_cache_load_cairo_surface_to_gicon (texture_cache, surface);
}

static void
on_window_icon_changed (GObject          *object,
                        const GParamSpec *pspec,
                        gpointer          user_data)
{
  MetaWindow *window = META_WINDOW (object);
  ShellApp *app = user_data;

  g_clear_object (&app->fallback_icon);
  app->fallback_icon = x11_window_create_fallback_gicon (window);

  if (!app->fallback_icon)
    app->fallback_icon = g_themed_icon_new ("application-x-executable");

  g_object_notify_by_pspec (G_OBJECT (app), props[PROP_ICON]);
}

/**
 * shell_app_get_icon:
 *
 * Look up the icon for this application
 *
 * Return value: (transfer none): A #GIcon
 */
GIcon *
shell_app_get_icon (ShellApp *app)
{
  MetaWindow *window = NULL;

  g_return_val_if_fail (SHELL_IS_APP (app), NULL);

  if (app->info)
    return g_app_info_get_icon (G_APP_INFO (app->info));

  if (app->fallback_icon)
    return app->fallback_icon;

  /* During a state transition from running to not-running for
   * window-backend apps, it's possible we get a request for the icon.
   * Avoid asserting here and just return a fallback icon
   */
  if (app->running_state != NULL)
    window = window_backed_app_get_window (app);

  if (window &&
      meta_window_get_client_type (window) == META_WINDOW_CLIENT_TYPE_X11)
    {
      app->fallback_icon_window = window;
      app->fallback_icon = x11_window_create_fallback_gicon (window);
      g_signal_connect (G_OBJECT (window),
                        "notify::icon", G_CALLBACK (on_window_icon_changed), app);
    }
  else
    {
      app->fallback_icon = g_themed_icon_new ("application-x-executable");
    }

  return app->fallback_icon;
}

/**
 * shell_app_create_icon_texture:
 *
 * Look up the icon for this application, and create a #ClutterActor
 * for it at the given size.
 *
 * Return value: (transfer none): A floating #ClutterActor
 */
ClutterActor *
shell_app_create_icon_texture (ShellApp   *app,
                               int         size)
{
  ClutterActor *ret;

  ret = st_icon_new ();
  st_icon_set_icon_size (ST_ICON (ret), size);
  st_icon_set_fallback_icon_name (ST_ICON (ret), "application-x-executable");

  g_object_bind_property (app, "icon", ret, "gicon", G_BINDING_SYNC_CREATE);

  if (shell_app_is_window_backed (app))
    st_widget_add_style_class_name (ST_WIDGET (ret), "fallback-app-icon");

  return ret;
}

const char *
shell_app_get_name (ShellApp *app)
{
  if (app->info)
    return g_app_info_get_name (G_APP_INFO (app->info));
  else
    {
      MetaWindow *window = window_backed_app_get_window (app);
      const char *name = NULL;

      if (window)
        name = meta_window_get_wm_class (window);
      if (!name)
        name = C_("program", "Unknown");
      return name;
    }
}

const char *
shell_app_get_description (ShellApp *app)
{
  if (app->info)
    return g_app_info_get_description (G_APP_INFO (app->info));
  else
    return NULL;
}

/**
 * shell_app_is_window_backed:
 *
 * A window backed application is one which represents just an open
 * window, i.e. there's no .desktop file association, so we don't know
 * how to launch it again.
 */
gboolean
shell_app_is_window_backed (ShellApp *app)
{
  return app->info == NULL;
}

typedef struct {
  MetaWorkspace *workspace;
  GSList **transients;
} CollectTransientsData;

static gboolean
collect_transients_on_workspace (MetaWindow *window,
                                 gpointer    datap)
{
  CollectTransientsData *data = datap;

  if (data->workspace && meta_window_get_workspace (window) != data->workspace)
    return TRUE;

  *data->transients = g_slist_prepend (*data->transients, window);
  return TRUE;
}

/* The basic idea here is that when we're targeting a window,
 * if it has transients we want to pick the most recent one
 * the user interacted with.
 * This function makes raising GEdit with the file chooser
 * open work correctly.
 */
static MetaWindow *
find_most_recent_transient_on_same_workspace (MetaDisplay *display,
                                              MetaWindow  *reference)
{
  GSList *transients, *transients_sorted, *iter;
  MetaWindow *result;
  CollectTransientsData data;

  transients = NULL;
  data.workspace = meta_window_get_workspace (reference);
  data.transients = &transients;

  meta_window_foreach_transient (reference, collect_transients_on_workspace, &data);

  transients_sorted = meta_display_sort_windows_by_stacking (display, transients);
  /* Reverse this so we're top-to-bottom (yes, we should probably change the order
   * returned from the sort_windows_by_stacking function)
   */
  transients_sorted = g_slist_reverse (transients_sorted);
  g_slist_free (transients);
  transients = NULL;

  result = NULL;
  for (iter = transients_sorted; iter; iter = iter->next)
    {
      MetaWindow *window = iter->data;
      MetaWindowType wintype = meta_window_get_window_type (window);

      /* Don't want to focus UTILITY types, like the Gimp toolbars */
      if (wintype == META_WINDOW_NORMAL ||
          wintype == META_WINDOW_DIALOG)
        {
          result = window;
          break;
        }
    }
  g_slist_free (transients_sorted);
  return result;
}

static MetaWorkspace *
get_active_workspace (void)
{
  ShellGlobal *global = shell_global_get ();
  MetaDisplay *display = shell_global_get_display (global);
  MetaWorkspaceManager *workspace_manager =
    meta_display_get_workspace_manager (display);

  return meta_workspace_manager_get_active_workspace (workspace_manager);
}

/**
 * shell_app_activate_window:
 * @app: a #ShellApp
 * @window: (nullable): Window to be focused
 * @timestamp: Event timestamp
 *
 * Bring all windows for the given app to the foreground,
 * but ensure that @window is on top.  If @window is %NULL,
 * the window with the most recent user time for the app
 * will be used.
 *
 * This function has no effect if @app is not currently running.
 */
void
shell_app_activate_window (ShellApp     *app,
                           MetaWindow   *window,
                           guint32       timestamp)
{
  g_autoptr (GSList) windows = NULL;

  if (shell_app_get_state (app) != SHELL_APP_STATE_RUNNING)
    return;

  windows = shell_app_get_windows (app);
  if (window == NULL && windows)
    window = windows->data;

  if (!g_slist_find (windows, window))
    return;
  else
    {
      GSList *windows_reversed, *iter;
      ShellGlobal *global = shell_global_get ();
      MetaDisplay *display = shell_global_get_display (global);
      MetaWorkspace *active = get_active_workspace ();
      MetaWorkspace *workspace = meta_window_get_workspace (window);
      guint32 last_user_timestamp = meta_display_get_last_user_time (display);
      MetaWindow *most_recent_transient;

      if (meta_display_xserver_time_is_before (display, timestamp, last_user_timestamp))
        {
          meta_window_set_demands_attention (window);
          return;
        }

      /* Now raise all the other windows for the app that are on
       * the same workspace, in reverse order to preserve the stacking.
       */
      windows_reversed = g_slist_copy (windows);
      windows_reversed = g_slist_reverse (windows_reversed);
      for (iter = windows_reversed; iter; iter = iter->next)
        {
          MetaWindow *other_window = iter->data;

          if (other_window != window && meta_window_get_workspace (other_window) == workspace)
            meta_window_raise_and_make_recent (other_window);
        }
      g_slist_free (windows_reversed);

      /* If we have a transient that the user's interacted with more recently than
       * the window, pick that.
       */
      most_recent_transient = find_most_recent_transient_on_same_workspace (display, window);
      if (most_recent_transient
          && meta_display_xserver_time_is_before (display,
                                                  meta_window_get_user_time (window),
                                                  meta_window_get_user_time (most_recent_transient)))
        window = most_recent_transient;


      if (active != workspace)
        meta_workspace_activate_with_focus (workspace, window, timestamp);
      else
        meta_window_activate (window, timestamp);
    }
}


void
shell_app_update_window_actions (ShellApp *app, MetaWindow *window)
{
  const char *object_path;

  object_path = meta_window_get_gtk_window_object_path (window);
  if (object_path != NULL)
    {
      GActionGroup *actions;

      actions = g_object_get_data (G_OBJECT (window), "actions");
      if (actions == NULL)
        {
          actions = G_ACTION_GROUP (g_dbus_action_group_get (app->running_state->session,
                                                             meta_window_get_gtk_unique_bus_name (window),
                                                             object_path));
          g_object_set_data_full (G_OBJECT (window), "actions", actions, g_object_unref);
        }

      g_assert (app->running_state->muxer);
      gtk_action_muxer_insert (app->running_state->muxer, "win", actions);
      g_object_notify_by_pspec (G_OBJECT (app), props[PROP_ACTION_GROUP]);
    }
}

/**
 * shell_app_activate:
 * @app: a #ShellApp
 *
 * Like shell_app_activate_full(), but using the default workspace and
 * event timestamp.
 */
void
shell_app_activate (ShellApp      *app)
{
  return shell_app_activate_full (app, -1, 0);
}

/**
 * shell_app_activate_full:
 * @app: a #ShellApp
 * @workspace: launch on this workspace, or -1 for default. Ignored if
 *   activating an existing window
 * @timestamp: Event timestamp
 *
 * Perform an appropriate default action for operating on this application,
 * dependent on its current state.  For example, if the application is not
 * currently running, launch it.  If it is running, activate the most
 * recently used NORMAL window (or if that window has a transient, the most
 * recently used transient for that window).
 */
void
shell_app_activate_full (ShellApp      *app,
                         int            workspace,
                         guint32        timestamp)
{
  ShellGlobal *global;

  global = shell_global_get ();

  if (timestamp == 0)
    timestamp = shell_global_get_current_time (global);

  switch (app->state)
    {
      case SHELL_APP_STATE_STOPPED:
        {
          GError *error = NULL;
          if (!shell_app_launch (app, timestamp, workspace, SHELL_APP_LAUNCH_GPU_APP_PREF, &error))
            {
              char *msg;
              msg = g_strdup_printf (_("Failed to launch “%s”"), shell_app_get_name (app));
              shell_global_notify_error (global,
                                         msg,
                                         error->message);
              g_free (msg);
              g_clear_error (&error);
            }
        }
        break;
      case SHELL_APP_STATE_STARTING:
        break;
      case SHELL_APP_STATE_RUNNING:
        shell_app_activate_window (app, NULL, timestamp);
        break;
      default:
        g_assert_not_reached();
        break;
    }
}

/**
 * shell_app_open_new_window:
 * @app: a #ShellApp
 * @workspace: open on this workspace, or -1 for default
 *
 * Request that the application create a new window.
 */
void
shell_app_open_new_window (ShellApp      *app,
                           int            workspace)
{
  GActionGroup *group = NULL;
  const char * const *actions;

  g_return_if_fail (app->info != NULL);

  /* First check whether the application provides a "new-window" desktop
   * action - it is a safe bet that it will open a new window, and activating
   * it will trigger startup notification if necessary
   */
  actions = g_desktop_app_info_list_actions (G_DESKTOP_APP_INFO (app->info));

  if (g_strv_contains (actions, "new-window"))
    {
      shell_app_launch_action (app, "new-window", 0, workspace);
      return;
    }

  /* Next, check whether the app exports an explicit "new-window" action
   * that we can activate on the bus - the muxer will add startup notification
   * information to the platform data, so this should work just as well as
   * desktop actions.
   */
  group = app->running_state ? G_ACTION_GROUP (app->running_state->muxer)
                             : NULL;

  if (group &&
      g_action_group_has_action (group, "app.new-window") &&
      g_action_group_get_action_parameter_type (group, "app.new-window") == NULL)
    {
      g_action_group_activate_action (group, "app.new-window", NULL);

      return;
    }

  /* Lastly, just always launch the application again, even if we know
   * it was already running.  For most applications this
   * should have the effect of creating a new window, whether that's
   * a second process (in the case of Calculator) or IPC to existing
   * instance (Firefox).  There are a few less-sensical cases such
   * as say Pidgin.
   */
  shell_app_launch (app, 0, workspace, SHELL_APP_LAUNCH_GPU_APP_PREF, NULL);
}

/**
 * shell_app_can_open_new_window:
 * @app: a #ShellApp
 *
 * Returns %TRUE if the app supports opening a new window through
 * shell_app_open_new_window() (ie, if calling that function will
 * result in actually opening a new window and not something else,
 * like presenting the most recently active one)
 */
gboolean
shell_app_can_open_new_window (ShellApp *app)
{
  ShellAppRunningState *state;
  MetaWindow *window;
  GDesktopAppInfo *desktop_info;
  const char * const *desktop_actions;

  /* Apps that are stopped can always open new windows, because
   * activating them would open the first one; if they are starting,
   * we cannot tell whether they can open additional windows until
   * they are running */
  if (app->state != SHELL_APP_STATE_RUNNING)
    return app->state == SHELL_APP_STATE_STOPPED;

  state = app->running_state;

  /* If the app has an explicit new-window action, then it can
     (or it should be able to) ...
  */
  if (g_action_group_has_action (G_ACTION_GROUP (state->muxer), "app.new-window"))
    return TRUE;

  /* If the app doesn't have a desktop file, then nothing is possible */
  if (!app->info)
    return FALSE;

  desktop_info = G_DESKTOP_APP_INFO (app->info);

  /* If the app is explicitly telling us via its desktop file, then we know
   * for sure
  */
  if (g_desktop_app_info_has_key (desktop_info, "SingleMainWindow"))
      return !g_desktop_app_info_get_boolean (desktop_info,
                                              "SingleMainWindow");

  /* GNOME-specific key, for backwards compatibility with apps that haven't
   * started using the XDG "SingleMainWindow" key yet
  */
  if (g_desktop_app_info_has_key (desktop_info, "X-GNOME-SingleWindow"))
    return !g_desktop_app_info_get_boolean (desktop_info,
                                            "X-GNOME-SingleWindow");

  /* If it has a new-window desktop action, it should be able to */
  desktop_actions = g_desktop_app_info_list_actions (desktop_info);
  if (desktop_actions && g_strv_contains (desktop_actions, "new-window"))
    return TRUE;

  /* If this is a unique GtkApplication, and we don't have a new-window, then
     probably we can't

     We don't consider non-unique GtkApplications here to handle cases like
     evince, which don't export a new-window action because each window is in
     a different process. In any case, in a non-unique GtkApplication each
     Activate() knows nothing about the other instances, so it will show a
     new window.
  */

  window = state->windows->data;

  if (state->unique_bus_name != NULL &&
      meta_window_get_gtk_application_object_path (window) != NULL)
    {
      if (meta_window_get_gtk_application_id (window) != NULL)
        return FALSE;
      else
        return TRUE;
    }

  /* In all other cases, we don't have a reliable source of information
     or a decent heuristic, so we err on the compatibility side and say
     yes.
  */
  return TRUE;
}

/**
 * shell_app_get_state:
 * @app: a #ShellApp
 *
 * Returns: State of the application
 */
ShellAppState
shell_app_get_state (ShellApp *app)
{
  return app->state;
}

typedef struct {
  ShellApp *app;
  MetaWorkspace *active_workspace;
} CompareWindowsData;

static int
shell_app_compare_windows (gconstpointer   a,
                           gconstpointer   b,
                           gpointer        datap)
{
  MetaWindow *win_a = (gpointer)a;
  MetaWindow *win_b = (gpointer)b;
  CompareWindowsData *data = datap;
  gboolean ws_a, ws_b;
  gboolean vis_a, vis_b;

  ws_a = meta_window_get_workspace (win_a) == data->active_workspace;
  ws_b = meta_window_get_workspace (win_b) == data->active_workspace;

  if (ws_a && !ws_b)
    return -1;
  else if (!ws_a && ws_b)
    return 1;

  vis_a = meta_window_showing_on_its_workspace (win_a);
  vis_b = meta_window_showing_on_its_workspace (win_b);

  if (vis_a && !vis_b)
    return -1;
  else if (!vis_a && vis_b)
    return 1;

  return meta_window_get_user_time (win_b) - meta_window_get_user_time (win_a);
}

/**
 * shell_app_get_windows:
 * @app:
 *
 * Get the windows which are associated with this application. The
 * returned list will be sorted first by whether they're on the
 * active workspace, then by whether they're visible, and finally
 * by the time the user last interacted with them.
 *
 * Returns: (transfer container) (element-type MetaWindow): List of windows
 */
GSList *
shell_app_get_windows (ShellApp *app)
{
  GSList *windows = NULL;
  GSList *l;

  if (app->running_state == NULL)
    return NULL;

  if (app->running_state->window_sort_stale)
    {
      CompareWindowsData data;
      data.app = app;
      data.active_workspace = get_active_workspace ();
      app->running_state->windows = g_slist_sort_with_data (app->running_state->windows, shell_app_compare_windows, &data);
      app->running_state->window_sort_stale = FALSE;
    }

  for (l = app->running_state->windows; l; l = l->next)
    if (!meta_window_is_override_redirect (META_WINDOW (l->data)))
      windows = g_slist_prepend (windows, l->data);

  return g_slist_reverse (windows);
}

guint
shell_app_get_n_windows (ShellApp *app)
{
  if (app->running_state == NULL)
    return 0;
  return g_slist_length (app->running_state->windows);
}

gboolean
shell_app_is_on_workspace (ShellApp *app,
                           MetaWorkspace   *workspace)
{
  GSList *iter;

  if (shell_app_get_state (app) == SHELL_APP_STATE_STARTING)
    {
      if (app->started_on_workspace == -1 ||
          meta_workspace_index (workspace) == app->started_on_workspace)
        return TRUE;
      else
        return FALSE;
    }

  if (app->running_state == NULL)
    return FALSE;

  for (iter = app->running_state->windows; iter; iter = iter->next)
    {
      if (meta_window_get_workspace (iter->data) == workspace)
        return TRUE;
    }

  return FALSE;
}

static int
shell_app_get_last_user_time (ShellApp *app)
{
  GSList *iter;
  guint32 last_user_time;

  last_user_time = 0;

  if (app->running_state != NULL)
    {
      for (iter = app->running_state->windows; iter; iter = iter->next)
        last_user_time = MAX (last_user_time, meta_window_get_user_time (iter->data));
    }

  return (int)last_user_time;
}

static gboolean
shell_app_is_minimized (ShellApp *app)
{
  GSList *iter;

  if (app->running_state == NULL)
    return FALSE;

  for (iter = app->running_state->windows; iter; iter = iter->next)
    {
      if (meta_window_showing_on_its_workspace (iter->data))
        return FALSE;
    }

  return TRUE;
}

/**
 * shell_app_compare:
 * @app:
 * @other: A #ShellApp
 *
 * Compare one #ShellApp instance to another, in the following way:
 *   - Running applications sort before not-running applications.
 *   - If one of them has non-minimized windows and the other does not,
 *     the one with visible windows is first.
 *   - Finally, the application which the user interacted with most recently
 *     compares earlier.
 */
int
shell_app_compare (ShellApp *app,
                   ShellApp *other)
{
  gboolean min_app, min_other;

  if (app->state != other->state)
    {
      if (app->state == SHELL_APP_STATE_RUNNING)
        return -1;
      return 1;
    }

  min_app = shell_app_is_minimized (app);
  min_other = shell_app_is_minimized (other);

  if (min_app != min_other)
    {
      if (min_other)
        return -1;
      return 1;
    }

  if (app->state == SHELL_APP_STATE_RUNNING)
    {
      if (app->running_state->windows && !other->running_state->windows)
        return -1;
      else if (!app->running_state->windows && other->running_state->windows)
        return 1;

      return shell_app_get_last_user_time (other) - shell_app_get_last_user_time (app);
    }

  return 0;
}

ShellApp *
_shell_app_new_for_window (MetaWindow      *window)
{
  ShellApp *app;

  app = g_object_new (SHELL_TYPE_APP, NULL);

  app->window_id_string = g_strdup_printf ("window:%d", meta_window_get_stable_sequence (window));

  _shell_app_add_window (app, window);

  return app;
}

ShellApp *
_shell_app_new (GDesktopAppInfo *info)
{
  ShellApp *app;

  app = g_object_new (SHELL_TYPE_APP,
                      "app-info", info,
                      NULL);

  return app;
}

void
_shell_app_set_app_info (ShellApp        *app,
                         GDesktopAppInfo *info)
{
  g_set_object (&app->info, info);

  g_clear_pointer (&app->name_collation_key, g_free);
  if (app->info)
    app->name_collation_key = g_utf8_collate_key (shell_app_get_name (app), -1);
}

static void
shell_app_state_transition (ShellApp      *app,
                            ShellAppState  state)
{
  if (app->state == state)
    return;
  g_return_if_fail (!(app->state == SHELL_APP_STATE_RUNNING &&
                      state == SHELL_APP_STATE_STARTING));
  app->state = state;

  _shell_app_system_notify_app_state_changed (shell_app_system_get_default (), app);

  g_object_notify_by_pspec (G_OBJECT (app), props[PROP_STATE]);
}

static void
shell_app_on_user_time_changed (MetaWindow *window,
                                GParamSpec *pspec,
                                ShellApp   *app)
{
  g_assert (app->running_state != NULL);

  /* Ideally we don't want to emit windows-changed if the sort order
   * isn't actually changing. This check catches most of those.
   */
  if (window != app->running_state->windows->data)
    {
      app->running_state->window_sort_stale = TRUE;
      g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);
    }
}

static void
shell_app_sync_running_state (ShellApp *app)
{
  g_return_if_fail (app->running_state != NULL);

  if (app->state != SHELL_APP_STATE_STARTING)
    {
      if (app->running_state->interesting_windows == 0)
        shell_app_state_transition (app, SHELL_APP_STATE_STOPPED);
      else
        shell_app_state_transition (app, SHELL_APP_STATE_RUNNING);
    }
}


static void
shell_app_on_skip_taskbar_changed (MetaWindow *window,
                                   GParamSpec *pspec,
                                   ShellApp   *app)
{
  g_assert (app->running_state != NULL);

  /* we rely on MetaWindow:skip-taskbar only being notified
   * when it actually changes; when that assumption breaks,
   * we'll have to track the "interesting" windows themselves
   */
  if (meta_window_is_skip_taskbar (window))
    app->running_state->interesting_windows--;
  else
    app->running_state->interesting_windows++;

  shell_app_sync_running_state (app);
}

static void
shell_app_on_ws_switch (MetaWorkspaceManager *workspace_manager,
                        int                   from,
                        int                   to,
                        MetaMotionDirection   direction,
                        gpointer              data)
{
  ShellApp *app = SHELL_APP (data);

  g_assert (app->running_state != NULL);

  app->running_state->window_sort_stale = TRUE;

  g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);
}

gboolean
shell_app_get_busy (ShellApp *app)
{
  if (app->running_state != NULL &&
      app->running_state->application_proxy != NULL &&
      shell_org_gtk_application_get_busy (app->running_state->application_proxy))
    return TRUE;

  return FALSE;
}

static void
busy_changed_cb (GObject    *object,
                 GParamSpec *pspec,
                 gpointer    user_data)
{
  ShellApp *app = user_data;

  g_assert (SHELL_IS_APP (app));

  g_object_notify_by_pspec (G_OBJECT (app), props[PROP_BUSY]);
}

static void
get_application_proxy (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  ShellApp *app = user_data;
  ShellOrgGtkApplication *proxy;
  g_autoptr (GError) error = NULL;

  g_assert (SHELL_IS_APP (app));

  proxy = shell_org_gtk_application_proxy_new_finish (result, &error);
  if (proxy != NULL)
    {
      app->running_state->application_proxy = proxy;
      g_signal_connect (proxy,
                        "notify::busy",
                        G_CALLBACK (busy_changed_cb),
                        app);
      if (shell_org_gtk_application_get_busy (proxy))
        g_object_notify_by_pspec (G_OBJECT (app), props[PROP_BUSY]);
    }

  if (app->running_state != NULL &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_clear_object (&app->running_state->cancellable);

  g_object_unref (app);
}

static void
shell_app_ensure_busy_watch (ShellApp *app)
{
  ShellAppRunningState *running_state = app->running_state;
  MetaWindow *window;
  const gchar *object_path;

  if (running_state->application_proxy != NULL ||
      running_state->cancellable != NULL)
    return;

  if (running_state->unique_bus_name == NULL)
    return;

  window = g_slist_nth_data (running_state->windows, 0);
  object_path = meta_window_get_gtk_application_object_path (window);

  if (object_path == NULL)
    return;

  running_state->cancellable = g_cancellable_new();
  /* Take a reference to app to make sure it isn't finalized before
     get_application_proxy runs */
  shell_org_gtk_application_proxy_new (running_state->session,
                                       G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                       running_state->unique_bus_name,
                                       object_path,
                                       running_state->cancellable,
                                       get_application_proxy,
                                       g_object_ref (app));
}

void
_shell_app_add_window (ShellApp        *app,
                       MetaWindow      *window)
{
  if (app->running_state && g_slist_find (app->running_state->windows, window))
    return;

  g_object_freeze_notify (G_OBJECT (app));

  if (!app->running_state)
      create_running_state (app);

  app->running_state->window_sort_stale = TRUE;
  app->running_state->windows = g_slist_prepend (app->running_state->windows, g_object_ref (window));
  g_signal_connect_object (window, "notify::user-time", G_CALLBACK(shell_app_on_user_time_changed), app, 0);
  g_signal_connect_object (window, "notify::skip-taskbar", G_CALLBACK(shell_app_on_skip_taskbar_changed), app, 0);

  shell_app_update_app_actions (app, window);
  shell_app_ensure_busy_watch (app);

  if (!meta_window_is_skip_taskbar (window))
    app->running_state->interesting_windows++;
  shell_app_sync_running_state (app);

  if (app->started_on_workspace >= 0 && !meta_window_is_on_all_workspaces (window))
    meta_window_change_workspace_by_index (window, app->started_on_workspace, FALSE);
  app->started_on_workspace = -1;

  g_object_thaw_notify (G_OBJECT (app));

  g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);
}

void
_shell_app_remove_window (ShellApp   *app,
                          MetaWindow *window)
{
  g_assert (app->running_state != NULL);

  if (!g_slist_find (app->running_state->windows, window))
    return;

  app->running_state->windows = g_slist_remove (app->running_state->windows, window);

  if (!meta_window_is_skip_taskbar (window))
    app->running_state->interesting_windows--;
  shell_app_sync_running_state (app);

  if (app->running_state->windows == NULL)
    g_clear_pointer (&app->running_state, unref_running_state);

  g_signal_handlers_disconnect_by_func (window, G_CALLBACK(shell_app_on_user_time_changed), app);
  g_signal_handlers_disconnect_by_func (window, G_CALLBACK(shell_app_on_skip_taskbar_changed), app);
  if (window == app->fallback_icon_window)
    {
      g_signal_handlers_disconnect_by_func (window, G_CALLBACK(on_window_icon_changed), app);
      app->fallback_icon_window = NULL;

      /* Select a new icon from a different window. */
      g_clear_object (&app->fallback_icon);
      g_object_notify_by_pspec (G_OBJECT (app), props[PROP_ICON]);
    }

  g_object_unref (window);

  g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);
}

/**
 * shell_app_get_pids:
 * @app: a #ShellApp
 *
 * Returns: (transfer container) (element-type int): An unordered list of process identifiers associated with this application.
 */
GSList *
shell_app_get_pids (ShellApp *app)
{
  GSList *result;
  g_autoptr (GSList) windows = NULL;
  GSList *iter;

  result = NULL;
  windows = shell_app_get_windows (app);
  for (iter = windows; iter; iter = iter->next)
    {
      MetaWindow *window = iter->data;
      pid_t pid = meta_window_get_pid (window);

      if (pid < 1)
        continue;

      /* Note in the (by far) common case, app will only have one pid, so
       * we'll hit the first element, so don't worry about O(N^2) here.
       */
      if (!g_slist_find (result, GINT_TO_POINTER (pid)))
        result = g_slist_prepend (result, GINT_TO_POINTER (pid));
    }
  return result;
}

void
_shell_app_handle_startup_sequence (ShellApp            *app,
                                    MetaStartupSequence *sequence)
{
  gboolean starting = !meta_startup_sequence_get_completed (sequence);

  /* The Shell design calls for on application launch, the app title
   * appears at top, and no X window is focused.  So when we get
   * a startup-notification for this app, transition it to STARTING
   * if it's currently stopped, set it as our application focus,
   * but focus the no_focus window.
   */
  if (starting && shell_app_get_state (app) == SHELL_APP_STATE_STOPPED)
    {
      MetaDisplay *display = shell_global_get_display (shell_global_get ());

      shell_app_state_transition (app, SHELL_APP_STATE_STARTING);
      meta_display_unset_input_focus (display,
                                      meta_startup_sequence_get_timestamp (sequence));
    }

  if (starting)
    app->started_on_workspace = meta_startup_sequence_get_workspace (sequence);
  else if (app->running_state && app->running_state->windows)
    shell_app_state_transition (app, SHELL_APP_STATE_RUNNING);
  else /* application have > 1 .desktop file */
    shell_app_state_transition (app, SHELL_APP_STATE_STOPPED);
}

/**
 * shell_app_request_quit:
 * @app: A #ShellApp
 *
 * Initiate an asynchronous request to quit this application.
 * The application may interact with the user, and the user
 * might cancel the quit request from the application UI.
 *
 * This operation may not be supported for all applications.
 *
 * Returns: %TRUE if a quit request is supported for this application
 */
gboolean
shell_app_request_quit (ShellApp   *app)
{
  GActionGroup *group = NULL;
  GSList *iter;

  if (shell_app_get_state (app) != SHELL_APP_STATE_RUNNING)
    return FALSE;

  /* First, check whether the app exports an explicit "quit" action
   * that we can activate on the bus
   */
  group = G_ACTION_GROUP (app->running_state->muxer);

  if (g_action_group_has_action (group, "app.quit") &&
      g_action_group_get_action_parameter_type (group, "app.quit") == NULL)
    {
      g_action_group_activate_action (group, "app.quit", NULL);

      return TRUE;
    }

  /* Otherwise, fall back to closing all the app's windows */
  for (iter = app->running_state->windows; iter; iter = iter->next)
    {
      MetaWindow *win = iter->data;

      if (!meta_window_can_close (win))
        continue;

      meta_window_delete (win, shell_global_get_current_time (shell_global_get ()));
    }
  return TRUE;
}

static void
child_context_setup (gpointer user_data)
{
  ShellGlobal *shell_global = user_data;
  MetaContext *meta_context;

  g_object_get (shell_global, "context", &meta_context, NULL);
  meta_context_restore_rlimit_nofile (meta_context, NULL);
}

#if !defined(HAVE_GIO_DESKTOP_LAUNCH_URIS_WITH_FDS) && defined(HAVE_SYSTEMD)
/* This sets up the launched application to log to the journal
 * using its own identifier, instead of just "gnome-session".
 */
static void
app_child_setup (gpointer user_data)
{
  const char *appid = user_data;
  int res;
  int journalfd = sd_journal_stream_fd (appid, LOG_INFO, FALSE);
  ShellGlobal *shell_global = shell_global_get ();

  if (journalfd >= 0)
    {
      do
        res = dup2 (journalfd, 1);
      while (G_UNLIKELY (res == -1 && errno == EINTR));
      do
        res = dup2 (journalfd, 2);
      while (G_UNLIKELY (res == -1 && errno == EINTR));
      (void) close (journalfd);
    }

  child_context_setup (shell_global);
}
#endif

static void
wait_pid (GDesktopAppInfo *appinfo,
          GPid             pid,
          gpointer         user_data)
{
  g_child_watch_add (pid, (GChildWatchFunc) g_spawn_close_pid, NULL);
}

static void
apply_discrete_gpu_env (GAppLaunchContext *context,
                        ShellGlobal       *global)
{
  GDBusProxy *proxy;
  GVariant* variant;
  guint num_children, i;

  proxy = shell_global_get_switcheroo_control (global);
  if (!proxy)
    {
      g_warning ("Could not apply discrete GPU environment, switcheroo-control not available");
      return;
    }

  variant = shell_net_hadess_switcheroo_control_get_gpus (SHELL_NET_HADESS_SWITCHEROO_CONTROL (proxy));
  if (!variant)
    {
      g_warning ("Could not apply discrete GPU environment, no GPUs in list");
      return;
    }

  num_children = g_variant_n_children (variant);
  for (i = 0; i < num_children; i++)
    {
      g_autoptr(GVariant) gpu = NULL;
      g_autoptr(GVariant) env = NULL;
      g_autoptr(GVariant) default_variant = NULL;
      g_autofree const char **env_s = NULL;
      guint j;

      gpu = g_variant_get_child_value (variant, i);
      if (!gpu ||
          !g_variant_is_of_type (gpu, G_VARIANT_TYPE ("a{s*}")))
        continue;

      /* Skip over the default GPU */
      default_variant = g_variant_lookup_value (gpu, "Default", NULL);
      if (!default_variant || g_variant_get_boolean (default_variant))
        continue;

      env = g_variant_lookup_value (gpu, "Environment", NULL);
      if (!env)
        continue;

      env_s = g_variant_get_strv (env, NULL);
      for (j = 0; env_s[j] != NULL; j = j + 2)
        g_app_launch_context_setenv (context, env_s[j], env_s[j+1]);
      return;
    }

  g_debug ("Could not find discrete GPU in switcheroo-control, not applying environment");
}

/**
 * shell_app_launch:
 * @timestamp: Event timestamp, or 0 for current event timestamp
 * @workspace: Start on this workspace, or -1 for default
 * @gpu_pref: the GPU to prefer launching on
 * @error: A #GError
 */
gboolean
shell_app_launch (ShellApp           *app,
                  guint               timestamp,
                  int                 workspace,
                  ShellAppLaunchGpu   gpu_pref,
                  GError            **error)
{
  ShellGlobal *global;
  GAppLaunchContext *context;
  gboolean ret;
  GSpawnFlags flags;
  gboolean discrete_gpu = FALSE;
  ShellGlobal *shell_global = shell_global_get ();

  if (app->info == NULL)
    {
      MetaWindow *window = window_backed_app_get_window (app);
      /* We don't use an error return if there no longer any windows, because the
       * user attempting to activate a stale window backed app isn't something
       * we would expect the caller to meaningfully handle or display an error
       * message to the user.
       */
      if (window)
        meta_window_activate (window, timestamp);
      return TRUE;
    }

  global = shell_global_get ();
  context = shell_global_create_app_launch_context (global, timestamp, workspace);
  if (gpu_pref == SHELL_APP_LAUNCH_GPU_APP_PREF)
    discrete_gpu = g_desktop_app_info_get_boolean (app->info, "PrefersNonDefaultGPU");
  else
    discrete_gpu = (gpu_pref == SHELL_APP_LAUNCH_GPU_DISCRETE);

  if (discrete_gpu)
    apply_discrete_gpu_env (context, global);

  /* Set LEAVE_DESCRIPTORS_OPEN in order to use an optimized gspawn
   * codepath. The shell's open file descriptors should be marked CLOEXEC
   * so that they are automatically closed even with this flag set.
   */
  flags = G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD |
          G_SPAWN_LEAVE_DESCRIPTORS_OPEN;

#ifdef HAVE_GIO_DESKTOP_LAUNCH_URIS_WITH_FDS
  /* Optimized spawn path, avoiding a child_setup function */
  {
    int journalfd = -1;

#ifdef HAVE_SYSTEMD
    journalfd = sd_journal_stream_fd (shell_app_get_id (app), LOG_INFO, FALSE);
#endif /* HAVE_SYSTEMD */

    ret = g_desktop_app_info_launch_uris_as_manager_with_fds (app->info, NULL,
                                                              context,
                                                              flags,
                                                              child_context_setup, shell_global,
                                                              wait_pid, NULL,
                                                              -1,
                                                              journalfd,
                                                              journalfd,
                                                              error);

    if (journalfd >= 0)
      (void) close (journalfd);
  }
#else /* !HAVE_GIO_DESKTOP_LAUNCH_URIS_WITH_FDS */
  ret = g_desktop_app_info_launch_uris_as_manager (app->info, NULL,
                                                   context,
                                                   flags,
#ifdef HAVE_SYSTEMD
                                                   app_child_setup, (gpointer)shell_app_get_id (app),
#else
                                                   child_context_setup, shell_global,
#endif
                                                   wait_pid, NULL,
                                                   error);
#endif /* HAVE_GIO_DESKTOP_LAUNCH_URIS_WITH_FDS */
  g_object_unref (context);

  return ret;
}

/**
 * shell_app_launch_action:
 * @app: the #ShellApp
 * @action_name: the name of the action to launch (as obtained by
 *               g_desktop_app_info_list_actions())
 * @timestamp: Event timestamp, or 0 for current event timestamp
 * @workspace: Start on this workspace, or -1 for default
 */
void
shell_app_launch_action (ShellApp        *app,
                         const char      *action_name,
                         guint            timestamp,
                         int              workspace)
{
  ShellGlobal *global;
  GAppLaunchContext *context;

  global = shell_global_get ();
  context = shell_global_create_app_launch_context (global, timestamp, workspace);

  g_desktop_app_info_launch_action (G_DESKTOP_APP_INFO (app->info),
                                    action_name, context);

  g_object_unref (context);
}

/**
 * shell_app_get_app_info:
 * @app: a #ShellApp
 *
 * Returns: (transfer none): The #GDesktopAppInfo for this app, or %NULL if backed by a window
 */
GDesktopAppInfo *
shell_app_get_app_info (ShellApp *app)
{
  return app->info;
}

static void
create_running_state (ShellApp *app)
{
  MetaDisplay *display = shell_global_get_display (shell_global_get ());
  MetaWorkspaceManager *workspace_manager =
    meta_display_get_workspace_manager (display);

  g_assert (app->running_state == NULL);

  app->running_state = g_new0 (ShellAppRunningState, 1);
  app->running_state->refcount = 1;
  app->running_state->workspace_switch_id =
    g_signal_connect (workspace_manager, "workspace-switched",
                      G_CALLBACK (shell_app_on_ws_switch), app);

  app->running_state->session = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (app->running_state->session != NULL);
  app->running_state->muxer = gtk_action_muxer_new ();
}

void
shell_app_update_app_actions (ShellApp   *app,
                              MetaWindow *window)
{
  const gchar *unique_bus_name;

  /* We assume that 'gtk-application-object-path' and
   * 'gtk-app-menu-object-path' are the same for all windows which
   * have it set.
   *
   * It could be possible, however, that the first window we see
   * belonging to the app didn't have them set.  For this reason, we
   * take the values from the first window that has them set and ignore
   * all the rest (until the app is stopped and restarted).
   */

  unique_bus_name = meta_window_get_gtk_unique_bus_name (window);

  if (g_strcmp0 (app->running_state->unique_bus_name, unique_bus_name) != 0)
    {
      const gchar *application_object_path;
      GDBusActionGroup *actions;

      application_object_path = meta_window_get_gtk_application_object_path (window);

      if (application_object_path == NULL || unique_bus_name == NULL)
        return;

      g_clear_pointer (&app->running_state->unique_bus_name, g_free);
      app->running_state->unique_bus_name = g_strdup (unique_bus_name);
      actions = g_dbus_action_group_get (app->running_state->session, unique_bus_name, application_object_path);
      gtk_action_muxer_insert (app->running_state->muxer, "app", G_ACTION_GROUP (actions));
      g_object_unref (actions);
    }
}

static void
unref_running_state (ShellAppRunningState *state)
{
  MetaDisplay *display = shell_global_get_display (shell_global_get ());
  MetaWorkspaceManager *workspace_manager =
    meta_display_get_workspace_manager (display);

  g_assert (state->refcount > 0);

  state->refcount--;
  if (state->refcount > 0)
    return;

  g_clear_signal_handler (&state->workspace_switch_id, workspace_manager);

  g_clear_object (&state->application_proxy);

  if (state->cancellable != NULL)
    {
      g_cancellable_cancel (state->cancellable);
      g_clear_object (&state->cancellable);
    }

  g_clear_object (&state->muxer);
  g_clear_object (&state->session);
  g_clear_pointer (&state->unique_bus_name, g_free);

  g_free (state);
}

/**
 * shell_app_compare_by_name:
 * @app: One app
 * @other: The other app
 *
 * Order two applications by name.
 *
 * Returns: -1, 0, or 1; suitable for use as a comparison function
 * for e.g. g_slist_sort()
 */
int
shell_app_compare_by_name (ShellApp *app, ShellApp *other)
{
  return strcmp (app->name_collation_key, other->name_collation_key);
}

static void
shell_app_init (ShellApp *self)
{
  self->state = SHELL_APP_STATE_STOPPED;
  self->started_on_workspace = -1;
}

static void
shell_app_dispose (GObject *object)
{
  ShellApp *app = SHELL_APP (object);

  g_clear_object (&app->info);
  g_clear_object (&app->fallback_icon);

  while (app->running_state)
    _shell_app_remove_window (app, app->running_state->windows->data);

  /* We should have been transitioned when we removed all of our windows */
  g_assert (app->state == SHELL_APP_STATE_STOPPED);
  g_assert (app->running_state == NULL);

  G_OBJECT_CLASS(shell_app_parent_class)->dispose (object);
}

static void
shell_app_finalize (GObject *object)
{
  ShellApp *app = SHELL_APP (object);

  g_free (app->window_id_string);

  g_free (app->name_collation_key);

  G_OBJECT_CLASS(shell_app_parent_class)->finalize (object);
}

static void
shell_app_class_init(ShellAppClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = shell_app_get_property;
  gobject_class->set_property = shell_app_set_property;
  gobject_class->dispose = shell_app_dispose;
  gobject_class->finalize = shell_app_finalize;

  shell_app_signals[WINDOWS_CHANGED] = g_signal_new ("windows-changed",
                                     SHELL_TYPE_APP,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL, NULL,
                                     G_TYPE_NONE, 0);

  /**
   * ShellApp:state:
   *
   * The high-level state of the application, effectively whether it's
   * running or not, or transitioning between those states.
   */
  props[PROP_STATE] =
    g_param_spec_enum ("state",
                       "State",
                       "Application state",
                       SHELL_TYPE_APP_STATE,
                       SHELL_APP_STATE_STOPPED,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * ShellApp:busy:
   *
   * Whether the application has marked itself as busy.
   */
  props[PROP_BUSY] =
    g_param_spec_boolean ("busy",
                          "Busy",
                          "Busy state",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * ShellApp:id:
   *
   * The id of this application (a desktop filename, or a special string
   * like window:0xabcd1234)
   */
  props[PROP_ID] =
    g_param_spec_string ("id",
                         "Application id",
                         "The desktop file id of this ShellApp",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * ShellApp:icon:
   *
   * The #GIcon representing this ShellApp
   */
  props[PROP_ICON] =
    g_param_spec_object ("icon",
                         "GIcon",
                         "The GIcon representing this app",
                         G_TYPE_ICON,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * ShellApp:action-group:
   *
   * The #GDBusActionGroup associated with this ShellApp, if any. See the
   * documentation of #GApplication and #GActionGroup for details.
   */
  props[PROP_ACTION_GROUP] =
    g_param_spec_object ("action-group",
                         "Application Action Group",
                         "The action group exported by the remote application",
                         G_TYPE_ACTION_GROUP,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * ShellApp:app-info:
   *
   * The #GDesktopAppInfo associated with this ShellApp, if any.
   */
  props[PROP_APP_INFO] =
    g_param_spec_object ("app-info",
                         "DesktopAppInfo",
                         "The DesktopAppInfo associated with this app",
                         G_TYPE_DESKTOP_APP_INFO,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, props);
}
