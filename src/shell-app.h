/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_APP_H__
#define __SHELL_APP_H__

#include <clutter/clutter.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <meta/window.h>

G_BEGIN_DECLS

#define SHELL_TYPE_APP (shell_app_get_type ())
G_DECLARE_FINAL_TYPE (ShellApp, shell_app, SHELL, APP, GObject)

typedef enum {
  SHELL_APP_STATE_STOPPED,
  SHELL_APP_STATE_STARTING,
  SHELL_APP_STATE_RUNNING
} ShellAppState;

typedef enum {
  SHELL_APP_LAUNCH_GPU_APP_PREF = 0,
  SHELL_APP_LAUNCH_GPU_DISCRETE,
  SHELL_APP_LAUNCH_GPU_DEFAULT
} ShellAppLaunchGpu;

const char *shell_app_get_id (ShellApp *app);

GDesktopAppInfo *shell_app_get_app_info (ShellApp *app);

ClutterActor *shell_app_create_icon_texture (ShellApp *app, int size);
GIcon *shell_app_get_icon (ShellApp *app);
const char *shell_app_get_name (ShellApp *app);
const char *shell_app_get_description (ShellApp *app);
gboolean shell_app_is_window_backed (ShellApp *app);

void shell_app_activate_window (ShellApp *app, MetaWindow *window, guint32 timestamp);

void shell_app_activate (ShellApp      *app);

void shell_app_activate_full (ShellApp      *app,
                              int            workspace,
                              guint32        timestamp);

void shell_app_open_new_window (ShellApp *app,
                                int       workspace);
gboolean shell_app_can_open_new_window (ShellApp *app);

ShellAppState shell_app_get_state (ShellApp *app);

gboolean shell_app_request_quit (ShellApp *app);

guint shell_app_get_n_windows (ShellApp *app);

GSList *shell_app_get_windows (ShellApp *app);

GSList *shell_app_get_pids (ShellApp *app);

gboolean shell_app_is_on_workspace (ShellApp *app, MetaWorkspace *workspace);

gboolean shell_app_launch (ShellApp           *app,
                           guint               timestamp,
                           int                 workspace,
                           ShellAppLaunchGpu   gpu_pref,
                           GError            **error);

void shell_app_launch_action (ShellApp        *app,
                              const char      *action_name,
                              guint            timestamp,
                              int              workspace);

int shell_app_compare_by_name (ShellApp *app, ShellApp *other);

int shell_app_compare (ShellApp *app, ShellApp *other);

void shell_app_update_window_actions (ShellApp *app, MetaWindow *window);
void shell_app_update_app_actions    (ShellApp *app, MetaWindow *window);

gboolean shell_app_get_busy          (ShellApp *app);

G_END_DECLS

#endif /* __SHELL_APP_H__ */
