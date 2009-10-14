/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "shell-app.h"
#include "shell-global.h"

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

  ShellAppInfo *info;

  guint workspace_switch_id;

  gboolean window_sort_stale;
  GSList *windows;
};

G_DEFINE_TYPE (ShellApp, shell_app, G_TYPE_OBJECT);

enum {
  WINDOWS_CHANGED,
  LAST_SIGNAL
};

static guint shell_app_signals[LAST_SIGNAL] = { 0 };

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

/**
 * shell_app_get_info:
 *
 * Returns: (transfer none): Associated app info
 */
ShellAppInfo *
shell_app_get_info (ShellApp *app)
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
  if (app->window_sort_stale)
    {
      CompareWindowsData data;
      data.app = app;
      data.active_workspace = meta_screen_get_active_workspace (shell_global_get_screen (shell_global_get ()));
      app->windows = g_slist_sort_with_data (app->windows, shell_app_compare_windows, &data);
      app->window_sort_stale = FALSE;
    }

  return app->windows;
}

static gboolean
shell_app_has_visible_windows (ShellApp   *app)
{
  GSList *iter;

  for (iter = app->windows; iter; iter = iter->next)
    {
      MetaWindow *window = iter->data;

      if (!meta_window_showing_on_its_workspace (window))
        return FALSE;
    }

  return TRUE;
}

gboolean
shell_app_is_on_workspace (ShellApp *app,
                           MetaWorkspace   *workspace)
{
  GSList *iter;

  for (iter = app->windows; iter; iter = iter->next)
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
 *   - If one of them has visible windows and the other does not, the one
 *     with visible windows is first.
 *   - If one has no windows at all (i.e. it's not running) and the other
 *     does, the one with windows is first.
 *   - Finally, the application which the user interacted with most recently
 *     compares earlier.
 */
int
shell_app_compare (ShellApp *app,
                   ShellApp *other)
{
  gboolean vis_app, vis_other;
  GSList *windows_app, *windows_other;

  vis_app = shell_app_has_visible_windows (app);
  vis_other = shell_app_has_visible_windows (other);

  if (vis_app && !vis_other)
    return -1;
  else if (!vis_app && vis_other)
    return 1;

  if (app->windows && !other->windows)
    return -1;
  else if (!app->windows && other->windows)
    return 1;

  windows_app = shell_app_get_windows (app);
  windows_other = shell_app_get_windows (other);

  return meta_window_get_user_time (windows_other->data) - meta_window_get_user_time (windows_app->data);
}

ShellApp *
_shell_app_new_for_window (MetaWindow      *window)
{
  ShellApp *app;

  app = g_object_new (SHELL_TYPE_APP, NULL);
  app->info = shell_app_system_create_from_window (shell_app_system_get_default (), window);
  _shell_app_add_window (app, window);

  return app;
}

ShellApp *
_shell_app_new (ShellAppInfo    *info)
{
  ShellApp *app;

  app = g_object_new (SHELL_TYPE_APP, NULL);
  app->info = shell_app_info_ref (info);

  return app;
}

static void
shell_app_on_unmanaged (MetaWindow      *window,
                        ShellApp *app)
{
  _shell_app_remove_window (app, window);
}

static void
shell_app_on_ws_switch (ShellApp   *self)
{
  self->window_sort_stale = TRUE;
  g_signal_emit (self, shell_app_signals[WINDOWS_CHANGED], 0);
}

void
_shell_app_add_window (ShellApp        *app,
                       MetaWindow      *window)
{
  if (g_slist_find (app->windows, window))
    return;

  app->windows = g_slist_prepend (app->windows, g_object_ref (window));
  g_signal_connect (window, "unmanaged", G_CALLBACK(shell_app_on_unmanaged), app);
  app->window_sort_stale = TRUE;

  g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);

  if (app->workspace_switch_id == 0)
    {
      MetaScreen *screen = shell_global_get_screen (shell_global_get ());

      app->workspace_switch_id =
        g_signal_connect (screen, "workspace-switched", G_CALLBACK(shell_app_on_ws_switch), app);
    }
}

static void
disconnect_workspace_switch (ShellApp  *app)
{
  MetaScreen *screen;

  if (app->workspace_switch_id == 0)
    return;

  screen = shell_global_get_screen (shell_global_get ());
  g_signal_handler_disconnect (screen, app->workspace_switch_id);
  app->workspace_switch_id = 0;
}

void
_shell_app_remove_window (ShellApp   *app,
                          MetaWindow *window)
{
  g_object_unref (window);
  app->windows = g_slist_remove (app->windows, window);
  if (app->windows == NULL)
    disconnect_workspace_switch (app);
}

static void
shell_app_init (ShellApp *self)
{
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

  if (app->windows)
    {
      g_slist_foreach (app->windows, (GFunc) g_object_unref, NULL);
      g_slist_free (app->windows);
      app->windows = NULL;
    }

  disconnect_workspace_switch (app);
}

static void
shell_app_class_init(ShellAppClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = shell_app_dispose;

  shell_app_signals[WINDOWS_CHANGED] = g_signal_new ("windows-changed",
                                     SHELL_TYPE_APP,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);
}
