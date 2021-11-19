/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_GLOBAL_H__
#define __SHELL_GLOBAL_H__

#include <clutter/clutter.h>
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <meta/meta-plugin.h>

G_BEGIN_DECLS

#define SHELL_TYPE_GLOBAL (shell_global_get_type ())
G_DECLARE_FINAL_TYPE (ShellGlobal, shell_global, SHELL, GLOBAL, GObject)

ShellGlobal   *shell_global_get                       (void);

ClutterStage  *shell_global_get_stage                 (ShellGlobal *global);
MetaDisplay   *shell_global_get_display               (ShellGlobal *global);
GList         *shell_global_get_window_actors         (ShellGlobal *global);
GSettings     *shell_global_get_settings              (ShellGlobal *global);
guint32        shell_global_get_current_time          (ShellGlobal *global);


/* Input/event handling */
void     shell_global_set_stage_input_region (ShellGlobal         *global,
                                              GSList              *rectangles);

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

G_END_DECLS

#endif /* __SHELL_GLOBAL_H__ */
