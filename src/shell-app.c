/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include <meta/display.h>

#include "shell-app-private.h"
#include "shell-enum-types.h"
#include "shell-global.h"
#include "shell-util.h"
#include "shell-app-system-private.h"
#include "shell-window-tracker-private.h"
#include "st.h"
#include "gtkactionmuxer.h"

#ifdef HAVE_SYSTEMD
#include <systemd/sd-journal.h>
#include <errno.h>
#include <unistd.h>
#endif

typedef enum {
  MATCH_NONE,
  MATCH_SUBSTRING, /* Not prefix, substring */
  MATCH_PREFIX, /* Strict prefix */
} ShellAppSearchMatch;

/* This is mainly a memory usage optimization - the user is going to
 * be running far fewer of the applications at one time than they have
 * installed.  But it also just helps keep the code more logically
 * separated.
 */
typedef struct {
  guint refcount;

  /* Signal connection to dirty window sort list on workspace changes */
  guint workspace_switch_id;

  GSList *windows;

  guint interesting_windows;

  /* Whether or not we need to resort the windows; this is done on demand */
  guint window_sort_stale : 1;

  /* DBus property notification subscription */
  guint properties_changed_id : 1;

  /* See GApplication documentation */
  GDBusMenuModel   *remote_menu;
  GtkActionMuxer   *muxer;
  char             *unique_bus_name;
  GDBusConnection  *session;
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

  ShellAppRunningState *running_state;

  char *window_id_string;
  char *name_collation_key;
};

enum {
  PROP_0,
  PROP_STATE,
  PROP_ID,
  PROP_DBUS_ID,
  PROP_ACTION_GROUP,
  PROP_MENU
};

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
    case PROP_ID:
      g_value_set_string (value, shell_app_get_id (app));
      break;
    case PROP_ACTION_GROUP:
      if (app->running_state)
        g_value_set_object (value, app->running_state->muxer);
      break;
    case PROP_MENU:
      if (app->running_state)
        g_value_set_object (value, app->running_state->remote_menu);
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
  g_assert (app->running_state);
  g_assert (app->running_state->windows);
  return app->running_state->windows->data;
}

static ClutterActor *
window_backed_app_get_icon (ShellApp *app,
                            int       size)
{
  MetaWindow *window;
  ClutterActor *actor;

  /* During a state transition from running to not-running for
   * window-backend apps, it's possible we get a request for the icon.
   * Avoid asserting here and just return an empty image.
   */
  if (app->running_state == NULL)
    {
      actor = clutter_texture_new ();
      g_object_set (actor, "opacity", 0, "width", (float) size, "height", (float) size, NULL);
      return actor;
    }

  window = window_backed_app_get_window (app);
  actor = st_texture_cache_bind_pixbuf_property (st_texture_cache_get_default (),
                                                               G_OBJECT (window),
                                                               "icon");
  g_object_set (actor, "width", (float) size, "height", (float) size, NULL);
  return actor;
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
                               int         size)
{
  GIcon *icon;
  ClutterActor *ret;

  ret = NULL;

  if (app->info == NULL)
    return window_backed_app_get_icon (app, size);

  icon = g_app_info_get_icon (G_APP_INFO (app->info));
  if (icon != NULL)
    ret = st_texture_cache_load_gicon (st_texture_cache_get_default (), NULL, icon, size);

  if (ret == NULL)
    {
      icon = g_themed_icon_new ("application-x-executable");
      ret = st_texture_cache_load_gicon (st_texture_cache_get_default (), NULL, icon, size);
      g_object_unref (icon);
    }

  return ret;
}

typedef struct {
  ShellApp *app;
  int size;
  ClutterTextDirection direction;
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
  gint fade_end;
  guint i, j;
  guint pixbuf_byte_size;
  guint8 *orig_pixels;
  guint8 *pixels;
  GIcon *icon;
  GtkIconInfo *info;

  app = data->app;
  size = data->size;

  info = NULL;

  icon = g_app_info_get_icon (G_APP_INFO (app->info));
  if (icon != NULL)
    {
      info = gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (),
                                             icon, size,
                                             GTK_ICON_LOOKUP_FORCE_SIZE);
    }

  if (info == NULL)
    {
      icon = g_themed_icon_new ("application-x-executable");
      info = gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (),
                                             icon, size,
                                             GTK_ICON_LOOKUP_FORCE_SIZE);
      g_object_unref (icon);
    }

  if (info == NULL)
    return COGL_INVALID_HANDLE;

  pixbuf = gtk_icon_info_load_icon (info, NULL);
  g_object_unref (info);

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

  /* fade on the right side for LTR, left side for RTL */
  if (data->direction == CLUTTER_TEXT_DIRECTION_LTR)
    {
      fade_start = width / 2;
      fade_end = width;
    }
  else
    {
      fade_start = 0;
      fade_end = width / 2;
    }

  for (i = fade_start; i < fade_end; i++)
    {
      for (j = 0; j < height; j++)
        {
          guchar *pixel = &pixels[j * rowstride + i * n_channels];
          float fade = ((float) i - fade_start) / (fade_end - fade_start);
          if (data->direction == CLUTTER_TEXT_DIRECTION_LTR)
            fade = 1.0 - fade;
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
 * @direction: Whether to fade on the left or right
 *
 * Return an actor with a horizontally faded look.
 *
 * Return value: (transfer none): A floating #ClutterActor, or %NULL if no icon
 */
ClutterActor *
shell_app_get_faded_icon (ShellApp *app, int size, ClutterTextDirection direction)
{
  CoglHandle texture;
  ClutterActor *result;
  char *cache_key;
  CreateFadedIconData data;

  /* Don't fade for window backed apps for now...easier to reuse the
   * property tracking bits, and this helps us visually distinguish
   * app-tracked from not.
   */
  if (!app->info)
    return window_backed_app_get_icon (app, size);

  /* Use icon: prefix so that we get evicted from the cache on
   * icon theme changes. */
  cache_key = g_strdup_printf ("icon:%s,size=%d,faded-%s",
                               shell_app_get_id (app),
                               size,
                               direction == CLUTTER_TEXT_DIRECTION_RTL ? "rtl" : "ltr");
  data.app = app;
  data.size = size;
  data.direction = direction;
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
      g_object_set (result, "opacity", 0, "width", (float) size, "height", (float) size, NULL);

    }
  return result;
}

const char *
shell_app_get_name (ShellApp *app)
{
  if (app->info)
    return g_app_info_get_name (G_APP_INFO (app->info));
  else
    {
      MetaWindow *window = window_backed_app_get_window (app);
      const char *name;

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
 * window, i.e. there's no .desktop file assocation, so we don't know
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
      GSList *windows_reversed, *iter;
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
      windows_reversed = g_slist_copy (windows);
      windows_reversed = g_slist_reverse (windows_reversed);
      for (iter = windows_reversed; iter; iter = iter->next)
        {
          MetaWindow *other_window = iter->data;

          if (other_window != window)
            meta_window_raise (other_window);
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

      if (!app->running_state->muxer)
        app->running_state->muxer = gtk_action_muxer_new ();

      gtk_action_muxer_insert (app->running_state->muxer, "win", actions);
      g_object_notify (G_OBJECT (app), "action-group");
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
          if (!shell_app_launch (app, timestamp, workspace, &error))
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
      case SHELL_APP_STATE_BUSY:
        shell_app_activate_window (app, NULL, timestamp);
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
  g_return_if_fail (app->info != NULL);

  /* Here we just always launch the application again, even if we know
   * it was already running.  For most applications this
   * should have the effect of creating a new window, whether that's
   * a second process (in the case of Calculator) or IPC to existing
   * instance (Firefox).  There are a few less-sensical cases such
   * as say Pidgin.  Ideally, we have the application express to us
   * that it supports an explicit new-window action.
   */
  shell_app_launch (app, 0, workspace, NULL);
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
  int last_user_time;

  last_user_time = 0;

  if (app->running_state != NULL)
    {
      for (iter = app->running_state->windows; iter; iter = iter->next)
        last_user_time = MAX (last_user_time, meta_window_get_user_time (iter->data));
    }

  return last_user_time;
}

/**
 * shell_app_compare:
 * @app:
 * @other: A #ShellApp
 *
 * Compare one #ShellApp instance to another, in the following way:
 *   - Running applications sort before not-running applications.
 *   - The application which the user interacted with most recently
 *     compares earlier.
 */
int
shell_app_compare (ShellApp *app,
                   ShellApp *other)
{
  if (app->state != other->state)
    {
      if (app->state == SHELL_APP_STATE_RUNNING)
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

  app = g_object_new (SHELL_TYPE_APP, NULL);

  _shell_app_set_app_info (app, info);

  return app;
}

void
_shell_app_set_app_info (ShellApp        *app,
                         GDesktopAppInfo *info)
{
  g_clear_object (&app->info);
  app->info = g_object_ref (info);

  if (app->name_collation_key != NULL)
    g_free (app->name_collation_key);
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

  if (app->state == SHELL_APP_STATE_STOPPED && app->running_state)
    {
      unref_running_state (app->running_state);
      app->running_state = NULL;
    }

  _shell_app_system_notify_app_state_changed (shell_app_system_get_default (), app);

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

static void
application_properties_changed (GDBusConnection *connection,
                                const gchar     *sender_name,
                                const gchar     *object_path,
                                const gchar     *interface_name,
                                const gchar     *signal_name,
                                GVariant        *parameters,
                                gpointer         user_data)
{
  ShellApp *app = user_data;
  GVariant *changed_properties;
  gboolean busy = FALSE;
  const gchar *interface_name_for_signal;

  g_variant_get (parameters,
                 "(&s@a{sv}as)",
                 &interface_name_for_signal,
                 &changed_properties,
                 NULL);

  if (g_strcmp0 (interface_name_for_signal, "org.gtk.Application") != 0)
    return;

  g_variant_lookup (changed_properties, "Busy", "b", &busy);

  if (busy)
    shell_app_state_transition (app, SHELL_APP_STATE_BUSY);
  else
    shell_app_state_transition (app, SHELL_APP_STATE_RUNNING);

  if (changed_properties != NULL)
    g_variant_unref (changed_properties);
}

static void
shell_app_ensure_busy_watch (ShellApp *app)
{
  ShellAppRunningState *running_state = app->running_state;
  MetaWindow *window;
  const gchar *object_path;

  if (running_state->properties_changed_id != 0)
    return;

  if (running_state->unique_bus_name == NULL)
    return;

  window = g_slist_nth_data (running_state->windows, 0);
  object_path = meta_window_get_gtk_application_object_path (window);

  if (object_path == NULL)
    return;

  running_state->properties_changed_id =
    g_dbus_connection_signal_subscribe (running_state->session,
                                        running_state->unique_bus_name,
                                        "org.freedesktop.DBus.Properties",
                                        "PropertiesChanged",
                                        object_path,
                                        "org.gtk.Application",
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        application_properties_changed, app, NULL);
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
  g_signal_connect (window, "unmanaged", G_CALLBACK(shell_app_on_unmanaged), app);
  g_signal_connect (window, "notify::user-time", G_CALLBACK(shell_app_on_user_time_changed), app);

  shell_app_update_app_menu (app, window);
  shell_app_ensure_busy_watch (app);

  if (shell_window_tracker_is_window_interesting (window))
    app->running_state->interesting_windows++;

  if (app->state != SHELL_APP_STATE_STARTING &&
      app->running_state->interesting_windows > 0)
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

  if (shell_window_tracker_is_window_interesting (window))
    app->running_state->interesting_windows--;

  if (app->running_state->interesting_windows == 0)
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
_gather_pid_callback (GDesktopAppInfo   *gapp,
                      GPid               pid,
                      gpointer           data)
{
  ShellApp *app;
  ShellWindowTracker *tracker;

  g_return_if_fail (data != NULL);

  app = SHELL_APP (data);
  tracker = shell_window_tracker_get_default ();

  _shell_window_tracker_add_child_process_app (tracker,
                                               pid,
                                               app);
}

#ifdef HAVE_SYSTEMD
/* This sets up the launched application to log to the journal
 * using its own identifier, instead of just "gnome-session".
 */
static void
app_child_setup (gpointer user_data)
{
  const char *appid = user_data;
  int res;
  int journalfd = sd_journal_stream_fd (appid, LOG_INFO, FALSE);
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
}
#endif

/**
 * shell_app_launch:
 * @timestamp: Event timestamp, or 0 for current event timestamp
 * @workspace: Start on this workspace, or -1 for default
 * @error: A #GError
 */
gboolean
shell_app_launch (ShellApp     *app,
                  guint         timestamp,
                  int           workspace,
                  GError      **error)
{
  ShellGlobal *global;
  GAppLaunchContext *context;
  gboolean ret;

  if (app->info == NULL)
    {
      MetaWindow *window = window_backed_app_get_window (app);
      meta_window_activate (window, timestamp);
      return TRUE;
    }

  global = shell_global_get ();
  context = shell_global_create_app_launch_context (global, timestamp, workspace);

  ret = g_desktop_app_info_launch_uris_as_manager (app->info, NULL,
                                                   context,
                                                   G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
#ifdef HAVE_SYSTEMD
                                                   app_child_setup, (gpointer)shell_app_get_id (app),
#else
                                                   NULL, NULL,
#endif
                                                   _gather_pid_callback, app,
                                                   error);
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
  MetaScreen *screen;

  g_assert (app->running_state == NULL);

  screen = shell_global_get_screen (shell_global_get ());
  app->running_state = g_slice_new0 (ShellAppRunningState);
  app->running_state->refcount = 1;
  app->running_state->workspace_switch_id =
    g_signal_connect (screen, "workspace-switched", G_CALLBACK(shell_app_on_ws_switch), app);

  app->running_state->session = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (app->running_state->session != NULL);
  app->running_state->muxer = gtk_action_muxer_new ();
}

void
shell_app_update_app_menu (ShellApp   *app,
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

  if (app->running_state->remote_menu == NULL ||
      g_strcmp0 (app->running_state->unique_bus_name, unique_bus_name) != 0)
    {
      const gchar *application_object_path;
      const gchar *app_menu_object_path;
      GDBusActionGroup *actions;

      application_object_path = meta_window_get_gtk_application_object_path (window);
      app_menu_object_path = meta_window_get_gtk_app_menu_object_path (window);

      if (application_object_path == NULL || app_menu_object_path == NULL || unique_bus_name == NULL)
        return;

      g_clear_pointer (&app->running_state->unique_bus_name, g_free);
      app->running_state->unique_bus_name = g_strdup (unique_bus_name);
      g_clear_object (&app->running_state->remote_menu);
      app->running_state->remote_menu = g_dbus_menu_model_get (app->running_state->session, unique_bus_name, app_menu_object_path);
      actions = g_dbus_action_group_get (app->running_state->session, unique_bus_name, application_object_path);
      gtk_action_muxer_insert (app->running_state->muxer, "app", G_ACTION_GROUP (actions));
      g_object_unref (actions);
    }
}

static void
unref_running_state (ShellAppRunningState *state)
{
  MetaScreen *screen;

  g_assert (state->refcount > 0);

  state->refcount--;
  if (state->refcount > 0)
    return;

  screen = shell_global_get_screen (shell_global_get ());
  g_signal_handler_disconnect (screen, state->workspace_switch_id);

  if (state->properties_changed_id != 0)
    g_dbus_connection_signal_unsubscribe (state->session, state->properties_changed_id);

  g_clear_object (&state->remote_menu);
  g_clear_object (&state->muxer);
  g_clear_object (&state->session);
  g_clear_pointer (&state->unique_bus_name, g_free);
  g_clear_pointer (&state->remote_menu, g_free);

  g_slice_free (ShellAppRunningState, state);
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
}

static void
shell_app_dispose (GObject *object)
{
  ShellApp *app = SHELL_APP (object);

  g_clear_object (&app->info);

  if (app->running_state)
    {
      while (app->running_state->windows)
        _shell_app_remove_window (app, app->running_state->windows->data);
    }
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
  g_object_class_install_property (gobject_class,
                                   PROP_STATE,
                                   g_param_spec_enum ("state",
                                                      "State",
                                                      "Application state",
                                                      SHELL_TYPE_APP_STATE,
                                                      SHELL_APP_STATE_STOPPED,
                                                      G_PARAM_READABLE));

  /**
   * ShellApp:id:
   *
   * The id of this application (a desktop filename, or a special string
   * like window:0xabcd1234)
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ID,
                                   g_param_spec_string ("id",
                                                        "Application id",
                                                        "The desktop file id of this ShellApp",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * ShellApp:action-group:
   *
   * The #GDBusActionGroup associated with this ShellApp, if any. See the
   * documentation of #GApplication and #GActionGroup for details.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACTION_GROUP,
                                   g_param_spec_object ("action-group",
                                                        "Application Action Group",
                                                        "The action group exported by the remote application",
                                                        G_TYPE_ACTION_GROUP,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  /**
   * ShellApp:menu:
   *
   * The #GMenuProxy associated with this ShellApp, if any. See the
   * documentation of #GMenuModel for details.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MENU,
                                   g_param_spec_object ("menu",
                                                        "Application Menu",
                                                        "The primary menu exported by the remote application",
                                                        G_TYPE_MENU_MODEL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

}
