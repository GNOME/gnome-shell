/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include <meta/display.h>

#include "shell-app-private.h"
#include "shell-enum-types.h"
#include "shell-global.h"
#include "shell-window-tracker-private.h"
#include "st.h"

/* This is mainly a memory usage optimization - the user is going to
 * be running far fewer of the applications at one time than they have
 * installed.  But it also just helps keep the code more logically
 * separated.
 */
typedef struct {
  guint refcount;

  /* Last time the user interacted with any of this application's windows */
  guint32 last_user_time;

  /* Signal connection to dirty window sort list on workspace changes */
  guint workspace_switch_id;

  GSList *windows;

  /* Whether or not we need to resort the windows; this is done on demand */
  gboolean window_sort_stale : 1;
} ShellAppRunningState;

/**
 * SECTION:shell-app
 * @short_description: Object representing an application
 *
 * This object wraps a #ShellAppInfo, providing methods and signals
 * primarily useful for running applications.
 */
struct _ShellApp
{
  GObject parent;

  int started_on_workspace;

  ShellAppState state;

  ShellAppInfo *info;

  ShellAppRunningState *running_state;
};

G_DEFINE_TYPE (ShellApp, shell_app, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_STATE
};

enum {
  WINDOWS_CHANGED,
  LAST_SIGNAL
};

static guint shell_app_signals[LAST_SIGNAL] = { 0 };

static void create_running_state (ShellApp *app);
static void unref_running_state (ShellAppRunningState *state);

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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

const char *
shell_app_get_id (ShellApp *app)
{
  return shell_app_info_get_id (app->info);
}

/**
 * shell_app_create_icon_texture:
 *
 * Look up the icon for this application, and create a #ClutterTexture
 * for it at the given size.
 *
 * Return value: (transfer none): A floating #ClutterActor
 */
ClutterActor *
shell_app_create_icon_texture (ShellApp   *app,
                               float size)
{
  return shell_app_info_create_icon_texture (app->info, size);
}
typedef struct {
  ShellApp *app;
  int size;
} CreateFadedIconData;

static CoglHandle
shell_app_create_faded_icon_cpu (StTextureCache *cache,
                                 const char     *key,
                                 void           *datap,
                                 GError        **error)
{
  CreateFadedIconData *data = datap;
  ShellApp *app;
  GdkPixbuf *pixbuf;
  int size;
  CoglHandle texture;
  gint width, height, rowstride;
  guint8 n_channels;
  gboolean have_alpha;
  gint fade_start;
  gint fade_range;
  guint i, j;
  guint pixbuf_byte_size;
  guint8 *orig_pixels;
  guint8 *pixels;
  GIcon *icon;
  GtkIconInfo *info;

  app = data->app;
  size = data->size;

  info = NULL;

  icon = shell_app_info_get_icon (app->info);
  if (icon != NULL)
    {
      info = gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (),
                                             icon, (int) (size + 0.5),
                                             GTK_ICON_LOOKUP_FORCE_SIZE);
      g_object_unref (icon);
    }

  if (info == NULL)
    {
      icon = g_themed_icon_new ("application-x-executable");
      info = gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (),
                                             icon, (int) (size + 0.5),
                                             GTK_ICON_LOOKUP_FORCE_SIZE);
      g_object_unref (icon);
    }

  if (info == NULL)
    return COGL_INVALID_HANDLE;

  pixbuf = gtk_icon_info_load_icon (info, NULL);
  gtk_icon_info_free (info);

  if (pixbuf == NULL)
    return COGL_INVALID_HANDLE;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  orig_pixels = gdk_pixbuf_get_pixels (pixbuf);
  have_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  pixbuf_byte_size = (height - 1) * rowstride +
    + width * ((n_channels * gdk_pixbuf_get_bits_per_sample (pixbuf) + 7) / 8);

  pixels = g_malloc0 (rowstride * height);
  memcpy (pixels, orig_pixels, pixbuf_byte_size);

  fade_start = width / 2;
  fade_range = width - fade_start;
  for (i = fade_start; i < width; i++)
    {
      for (j = 0; j < height; j++)
        {
          guchar *pixel = &pixels[j * rowstride + i * n_channels];
          float fade = 1.0 - ((float) i - fade_start) / fade_range;
          pixel[0] = 0.5 + pixel[0] * fade;
          pixel[1] = 0.5 + pixel[1] * fade;
          pixel[2] = 0.5 + pixel[2] * fade;
          if (have_alpha)
            pixel[3] = 0.5 + pixel[3] * fade;
        }
    }

  texture = cogl_texture_new_from_data (width,
                                        height,
                                        COGL_TEXTURE_NONE,
                                        have_alpha ? COGL_PIXEL_FORMAT_RGBA_8888 : COGL_PIXEL_FORMAT_RGB_888,
                                        COGL_PIXEL_FORMAT_ANY,
                                        rowstride,
                                        pixels);
  g_free (pixels);
  g_object_unref (pixbuf);

  return texture;
}

/**
 * shell_app_get_faded_icon:
 * @app: A #ShellApp
 * @size: Size in pixels
 *
 * Return an actor with a horizontally faded look.
 *
 * Return value: (transfer none): A floating #ClutterActor, or %NULL if no icon
 */
ClutterActor *
shell_app_get_faded_icon (ShellApp *app, float size)
{
  MetaWindow *window;
  CoglHandle texture;
  ClutterActor *result;
  char *cache_key;
  CreateFadedIconData data;

  /* Punt for WINDOW types for now...easier to reuse the property tracking bits,
   * and this helps us visually distinguish app-tracked from not.
   */
  window = shell_app_info_get_source_window (app->info);
  if (window)
    {
      return st_texture_cache_bind_pixbuf_property (st_texture_cache_get_default (),
                                                    G_OBJECT (window),
                                                    "icon");
    }

  cache_key = g_strdup_printf ("faded-icon:%s,size=%f", shell_app_get_id (app), size);
  data.app = app;
  data.size = (int) (0.5 + size);
  texture = st_texture_cache_load (st_texture_cache_get_default (),
                                   cache_key,
                                   ST_TEXTURE_CACHE_POLICY_FOREVER,
                                   shell_app_create_faded_icon_cpu,
                                   &data,
                                   NULL);
  g_free (cache_key);

  if (texture != COGL_INVALID_HANDLE)
    {
      result = clutter_texture_new ();
      clutter_texture_set_cogl_texture (CLUTTER_TEXTURE (result), texture);
    }
  else
    {
      result = clutter_texture_new ();
      g_object_set (result, "opacity", 0, "width", size, "height", size, NULL);

    }
  return result;
}

char *
shell_app_get_name (ShellApp *app)
{
  return shell_app_info_get_name (app->info);
}

char *
shell_app_get_description (ShellApp *app)
{
  return shell_app_info_get_description (app->info);
}

gboolean
shell_app_is_transient (ShellApp *app)
{
  return shell_app_info_is_transient (app->info);
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

/**
 * shell_app_activate_window:
 * @app: a #ShellApp
 * @window: (allow-none): Window to be focused
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
  GSList *windows;

  if (shell_app_get_state (app) != SHELL_APP_STATE_RUNNING)
    return;

  windows = shell_app_get_windows (app);
  if (window == NULL && windows)
    window = windows->data;

  if (!g_slist_find (windows, window))
    return;
  else
    {
      GSList *iter;
      ShellGlobal *global = shell_global_get ();
      MetaScreen *screen = shell_global_get_screen (global);
      MetaDisplay *display = meta_screen_get_display (screen);
      MetaWorkspace *active = meta_screen_get_active_workspace (screen);
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
      for (iter = windows; iter; iter = iter->next)
        {
          MetaWindow *other_window = iter->data;

          if (other_window != window)
            meta_window_raise (other_window);
        }

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

/**
 * shell_app_activate:
 * @app: a #ShellApp
 * @workspace: launch on this workspace, or -1 for default. Ignored if
 *   activating an existing window
 *
 * Perform an appropriate default action for operating on this application,
 * dependent on its current state.  For example, if the application is not
 * currently running, launch it.  If it is running, activate the most
 * recently used NORMAL window (or if that window has a transient, the most
 * recently used transient for that window).
 */
void
shell_app_activate (ShellApp      *app,
                    int            workspace)
{
  switch (app->state)
    {
      case SHELL_APP_STATE_STOPPED:
        {
          GError *error = NULL;
          if (!shell_app_info_launch_full (app->info,
                                           0,
                                           NULL,
                                           workspace,
                                           NULL,
                                           &error))
            {
              char *msg;
              msg = g_strdup_printf (_("Failed to launch '%s'"), shell_app_get_name (app));
              shell_global_notify_error (shell_global_get (),
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
        shell_app_activate_window (app, NULL, shell_global_get_current_time (shell_global_get ()));
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
  /* Here we just always launch the application again, even if we know
   * it was already running.  For most applications this
   * should have the effect of creating a new window, whether that's
   * a second process (in the case of Calculator) or IPC to existing
   * instance (Firefox).  There are a few less-sensical cases such
   * as say Pidgin.  Ideally, we have the application express to us
   * that it supports an explicit new-window action.
   */
  shell_app_info_launch_full (app->info,
                              0,
                              NULL,
                              workspace,
                              NULL,
                              NULL);
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

/**
 * _shell_app_get_info:
 *
 * Returns: (transfer none): Associated app info
 */
ShellAppInfo *
_shell_app_get_info (ShellApp *app)
{
  return app->info;
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
 * Get the toplevel, interesting windows which are associated with this
 * application.  The returned list will be sorted first by whether
 * they're on the active workspace, then by whether they're visible,
 * and finally by the time the user last interacted with them.
 *
 * Returns: (transfer none) (element-type MetaWindow): List of windows
 */
GSList *
shell_app_get_windows (ShellApp *app)
{
  if (app->running_state == NULL)
    return NULL;

  if (app->running_state->window_sort_stale)
    {
      CompareWindowsData data;
      data.app = app;
      data.active_workspace = meta_screen_get_active_workspace (shell_global_get_screen (shell_global_get ()));
      app->running_state->windows = g_slist_sort_with_data (app->running_state->windows, shell_app_compare_windows, &data);
      app->running_state->window_sort_stale = FALSE;
    }

  return app->running_state->windows;
}

guint
shell_app_get_n_windows (ShellApp *app)
{
  if (app->running_state == NULL)
    return 0;
  return g_slist_length (app->running_state->windows);
}

static gboolean
shell_app_has_visible_windows (ShellApp   *app)
{
  GSList *iter;

  if (app->running_state == NULL)
    return FALSE;

  for (iter = app->running_state->windows; iter; iter = iter->next)
    {
      MetaWindow *window = iter->data;

      if (meta_window_showing_on_its_workspace (window))
        return TRUE;
    }

  return FALSE;
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

/**
 * shell_app_compare:
 * @app:
 * @other: A #ShellApp
 *
 * Compare one #ShellApp instance to another, in the following way:
 *   - Running applications sort before not-running applications.
 *   - If one of them has visible windows and the other does not, the one
 *     with visible windows is first.
 *   - Finally, the application which the user interacted with most recently
 *     compares earlier.
 */
int
shell_app_compare (ShellApp *app,
                   ShellApp *other)
{
  gboolean vis_app, vis_other;

  if (app->state != other->state)
    {
      if (app->state == SHELL_APP_STATE_RUNNING)
        return -1;
      return 1;
    }

  vis_app = shell_app_has_visible_windows (app);
  vis_other = shell_app_has_visible_windows (other);

  if (vis_app && !vis_other)
    return -1;
  else if (!vis_app && vis_other)
    return 1;

  if (app->state == SHELL_APP_STATE_RUNNING)
    {
      if (app->running_state->windows && !other->running_state->windows)
        return -1;
      else if (!app->running_state->windows && other->running_state->windows)
        return 1;
      return other->running_state->last_user_time - app->running_state->last_user_time;
    }

  return 0;
}

ShellApp *
_shell_app_new_for_window (MetaWindow      *window)
{
  ShellApp *app;

  app = g_object_new (SHELL_TYPE_APP, NULL);
  app->info = shell_app_system_create_from_window (shell_app_system_get_default (), window);
  _shell_app_system_register_app (shell_app_system_get_default (), app);
  _shell_app_add_window (app, window);

  return app;
}

ShellApp *
_shell_app_new (ShellAppInfo    *info)
{
  ShellApp *app;

  app = g_object_new (SHELL_TYPE_APP, NULL);
  app->info = shell_app_info_ref (info);
  _shell_app_system_register_app (shell_app_system_get_default (), app);

  return app;
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

  if (app->state == SHELL_APP_STATE_STOPPED && app->running_state)
    {
      unref_running_state (app->running_state);
      app->running_state = NULL;
    }

  _shell_window_tracker_notify_app_state_changed (shell_window_tracker_get_default (), app);

  g_object_notify (G_OBJECT (app), "state");
}

static void
shell_app_on_unmanaged (MetaWindow      *window,
                        ShellApp *app)
{
  _shell_app_remove_window (app, window);
}

static void
shell_app_on_user_time_changed (MetaWindow *window,
                                GParamSpec *pspec,
                                ShellApp   *app)
{
  g_assert (app->running_state != NULL);

  app->running_state->last_user_time = meta_window_get_user_time (window);

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
shell_app_on_ws_switch (MetaScreen         *screen,
                        int                 from,
                        int                 to,
                        MetaMotionDirection direction,
                        gpointer            data)
{
  ShellApp *app = SHELL_APP (data);

  g_assert (app->running_state != NULL);

  app->running_state->window_sort_stale = TRUE;

  g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);
}

void
_shell_app_add_window (ShellApp        *app,
                       MetaWindow      *window)
{
  guint32 user_time;

  if (app->running_state && g_slist_find (app->running_state->windows, window))
    return;

  g_object_freeze_notify (G_OBJECT (app));

  if (!app->running_state)
      create_running_state (app);

  app->running_state->window_sort_stale = TRUE;
  app->running_state->windows = g_slist_prepend (app->running_state->windows, g_object_ref (window));
  g_signal_connect (window, "unmanaged", G_CALLBACK(shell_app_on_unmanaged), app);
  g_signal_connect (window, "notify::user-time", G_CALLBACK(shell_app_on_user_time_changed), app);

  user_time = meta_window_get_user_time (window);
  if (user_time > app->running_state->last_user_time)
    app->running_state->last_user_time = user_time;

  if (app->state != SHELL_APP_STATE_STARTING)
    shell_app_state_transition (app, SHELL_APP_STATE_RUNNING);

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

  g_signal_handlers_disconnect_by_func (window, G_CALLBACK(shell_app_on_unmanaged), app);
  g_signal_handlers_disconnect_by_func (window, G_CALLBACK(shell_app_on_user_time_changed), app);
  g_object_unref (window);
  app->running_state->windows = g_slist_remove (app->running_state->windows, window);

  if (app->running_state->windows == NULL)
    shell_app_state_transition (app, SHELL_APP_STATE_STOPPED);

  g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);
}

/**
 * shell_app_get_pids:
 * @app: a #ShellApp
 *
 * Returns: (transfer container) (element-type int): An unordered list of process identifers associated with this application.
 */
GSList *
shell_app_get_pids (ShellApp *app)
{
  GSList *result;
  GSList *iter;

  result = NULL;
  for (iter = shell_app_get_windows (app); iter; iter = iter->next)
    {
      MetaWindow *window = iter->data;
      int pid = meta_window_get_pid (window);
      /* Note in the (by far) common case, app will only have one pid, so
       * we'll hit the first element, so don't worry about O(N^2) here.
       */
      if (!g_slist_find (result, GINT_TO_POINTER (pid)))
        result = g_slist_prepend (result, GINT_TO_POINTER (pid));
    }
  return result;
}

void
_shell_app_handle_startup_sequence (ShellApp          *app,
                                    SnStartupSequence *sequence)
{
  gboolean starting = !sn_startup_sequence_get_completed (sequence);

  /* The Shell design calls for on application launch, the app title
   * appears at top, and no X window is focused.  So when we get
   * a startup-notification for this app, transition it to STARTING
   * if it's currently stopped, set it as our application focus,
   * but focus the no_focus window.
   */
  if (starting && shell_app_get_state (app) == SHELL_APP_STATE_STOPPED)
    {
      MetaScreen *screen = shell_global_get_screen (shell_global_get ());
      MetaDisplay *display = meta_screen_get_display (screen);

      shell_app_state_transition (app, SHELL_APP_STATE_STARTING);
      meta_display_focus_the_no_focus_window (display, screen,
                                              sn_startup_sequence_get_timestamp (sequence));
      app->started_on_workspace = sn_startup_sequence_get_workspace (sequence);
    }

  if (!starting)
    {
      if (app->running_state && app->running_state->windows)
        shell_app_state_transition (app, SHELL_APP_STATE_RUNNING);
      else /* application have > 1 .desktop file */
        shell_app_state_transition (app, SHELL_APP_STATE_STOPPED);
    }
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
  GSList *iter;

  if (shell_app_get_state (app) != SHELL_APP_STATE_RUNNING)
    return FALSE;

  /* TODO - check for an XSMP connection; we could probably use that */

  for (iter = app->running_state->windows; iter; iter = iter->next)
    {
      MetaWindow *win = iter->data;

      if (!shell_window_tracker_is_window_interesting (win))
        continue;

      meta_window_delete (win, shell_global_get_current_time (shell_global_get ()));
    }
  return TRUE;
}

static void
create_running_state (ShellApp *app)
{
  MetaScreen *screen;

  g_assert (app->running_state == NULL);

  screen = shell_global_get_screen (shell_global_get ());
  app->running_state = g_slice_new0 (ShellAppRunningState);
  app->running_state->refcount = 1;
  app->running_state->workspace_switch_id =
    g_signal_connect (screen, "workspace-switched", G_CALLBACK(shell_app_on_ws_switch), app);
}

static void
unref_running_state (ShellAppRunningState *state)
{
  MetaScreen *screen;

  state->refcount--;
  if (state->refcount > 0)
    return;

  screen = shell_global_get_screen (shell_global_get ());

  g_signal_handler_disconnect (screen, state->workspace_switch_id);
  g_slice_free (ShellAppRunningState, state);
}

static void
shell_app_init (ShellApp *self)
{
  self->state = SHELL_APP_STATE_STOPPED;
}

static void
shell_app_dispose (GObject *object)
{
  ShellApp *app = SHELL_APP (object);

  if (app->info)
    {
      shell_app_info_unref (app->info);
      app->info = NULL;
    }

  if (app->running_state)
    {
      while (app->running_state->windows)
        _shell_app_remove_window (app, app->running_state->windows->data);
    }

  G_OBJECT_CLASS(shell_app_parent_class)->dispose (object);
}

static void
shell_app_class_init(ShellAppClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = shell_app_get_property;
  gobject_class->dispose = shell_app_dispose;

  shell_app_signals[WINDOWS_CHANGED] = g_signal_new ("windows-changed",
                                     SHELL_TYPE_APP,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);

  /**
   * ShellApp:state:
   *
   * The high-level state of the application, effectively whether it's
   * running or not, or transitioning between those states.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STATE,
                                   g_param_spec_enum ("state",
                                                      "State",
                                                      "Application state",
                                                      SHELL_TYPE_APP_STATE,
                                                      SHELL_APP_STATE_STOPPED,
                                                      G_PARAM_READABLE));
}
