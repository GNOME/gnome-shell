/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#pragma once

#include <clutter/clutter.h>
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <meta/meta-plugin.h>
#include <st/st.h>

G_BEGIN_DECLS

#include "shell-window-tracker.h"
#include "shell-app-system.h"
#include "shell-app-usage.h"
#include "shell-wm.h"

#define SHELL_TYPE_GLOBAL (shell_global_get_type ())
G_DECLARE_FINAL_TYPE (ShellGlobal, shell_global, SHELL, GLOBAL, GObject)

ShellGlobal          *shell_global_get                       (void);
MetaBackend          *shell_global_get_backend               (ShellGlobal *global);
MetaCompositor       *shell_global_get_compositor            (ShellGlobal *global);
MetaContext          *shell_global_get_context               (ShellGlobal *global);
ClutterStage         *shell_global_get_stage                 (ShellGlobal *global);
MetaDisplay          *shell_global_get_display               (ShellGlobal *global);
GList                *shell_global_get_window_actors         (ShellGlobal *global);
GSettings            *shell_global_get_settings              (ShellGlobal *global);
guint32               shell_global_get_current_time          (ShellGlobal *global);
MetaWorkspaceManager *shell_global_get_workspace_manager     (ShellGlobal *global);
ShellWM              *shell_global_get_window_manager        (ShellGlobal *global);
StFocusManager       *shell_global_get_focus_manager         (ShellGlobal *global);

int shell_global_get_screen_width  (ShellGlobal *global);
int shell_global_get_screen_height (ShellGlobal *global);

ClutterActor * shell_global_get_window_group (ShellGlobal *global);
ClutterActor * shell_global_get_top_window_group (ShellGlobal *global);

const char * shell_global_get_datadir (ShellGlobal *global);
const char * shell_global_get_userdatadir (ShellGlobal *global);

GFile * shell_global_get_automation_script (ShellGlobal *global);

gboolean shell_global_get_force_animations (ShellGlobal *global);
void     shell_global_set_force_animations (ShellGlobal *global,
                                            gboolean     force);

/* Input/event handling */
void    shell_global_get_pointer             (ShellGlobal         *global,
                                              int                 *x,
                                              int                 *y,
                                              ClutterModifierType *mods);

typedef struct {
  guint glibc_uordblks;

  guint js_bytes;

  guint gjs_boxed;
  guint gjs_gobject;
  guint gjs_function;
  guint gjs_closure;

  /* 32 bit to avoid js conversion problems with 64 bit */
  guint  last_gc_seconds_ago;
} ShellMemoryInfo;

/* Run-at-leisure API */
void shell_global_begin_work     (ShellGlobal          *global);
void shell_global_end_work       (ShellGlobal          *global);

typedef void (*ShellLeisureFunction) (gpointer data);

void shell_global_run_at_leisure (ShellGlobal          *global,
                                  ShellLeisureFunction  func,
                                  gpointer              user_data,
                                  GDestroyNotify        notify);


/* Misc utilities / Shell API */
GDBusProxy *
         shell_global_get_switcheroo_control    (ShellGlobal  *global);

GAppLaunchContext *
         shell_global_create_app_launch_context (ShellGlobal  *global,
                                                 guint32       timestamp,
                                                 int           workspace);

void     shell_global_notify_error              (ShellGlobal  *global,
                                                 const char   *msg,
                                                 const char   *details);

void     shell_global_reexec_self               (ShellGlobal  *global);

const char *     shell_global_get_session_mode  (ShellGlobal  *global);

void     shell_global_set_runtime_state         (ShellGlobal  *global,
                                                 const char   *property_name,
                                                 GVariant     *variant);
GVariant * shell_global_get_runtime_state       (ShellGlobal  *global,
                                                 const char   *property_type,
                                                 const char   *property_name);

void     shell_global_set_persistent_state      (ShellGlobal  *global,
                                                 const char   *property_name,
                                                 GVariant     *variant);
GVariant * shell_global_get_persistent_state    (ShellGlobal  *global,
                                                 const char   *property_type,
                                                 const char   *property_name);

ShellWindowTracker * shell_global_get_window_tracker (ShellGlobal *global);

ShellAppSystem *     shell_global_get_app_system     (ShellGlobal *global);

ShellAppUsage *      shell_global_get_app_usage      (ShellGlobal *global);

gboolean shell_global_get_frame_timestamps (ShellGlobal *global);
void shell_global_set_frame_timestamps (ShellGlobal *global,
                                        gboolean     enable);

gboolean shell_global_get_frame_finish_timestamp (ShellGlobal *global);
void shell_global_set_frame_finish_timestamp (ShellGlobal *global,
                                              gboolean     enable);

G_END_DECLS
