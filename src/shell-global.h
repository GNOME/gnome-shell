/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_GLOBAL_H__
#define __SHELL_GLOBAL_H__

#include <clutter/clutter.h>
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <meta/meta-plugin.h>

G_BEGIN_DECLS

typedef struct _ShellGlobal      ShellGlobal;
typedef struct _ShellGlobalClass ShellGlobalClass;

#define SHELL_TYPE_GLOBAL              (shell_global_get_type ())
#define SHELL_GLOBAL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), SHELL_TYPE_GLOBAL, ShellGlobal))
#define SHELL_GLOBAL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_GLOBAL, ShellGlobalClass))
#define SHELL_IS_GLOBAL(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), SHELL_TYPE_GLOBAL))
#define SHELL_IS_GLOBAL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_GLOBAL))
#define SHELL_GLOBAL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_GLOBAL, ShellGlobalClass))

struct _ShellGlobalClass
{
  GObjectClass parent_class;
};

GType shell_global_get_type (void) G_GNUC_CONST;

ShellGlobal   *shell_global_get                       (void);

ClutterStage  *shell_global_get_stage                 (ShellGlobal *global);
MetaScreen    *shell_global_get_screen                (ShellGlobal *global);
GdkScreen     *shell_global_get_gdk_screen            (ShellGlobal *global);
MetaDisplay   *shell_global_get_display               (ShellGlobal *global);
GList         *shell_global_get_window_actors         (ShellGlobal *global);
GSettings     *shell_global_get_settings              (ShellGlobal *global);
guint32        shell_global_get_current_time          (ShellGlobal *global);


/* Input/event handling */
gboolean shell_global_begin_modal            (ShellGlobal         *global,
                                              guint32             timestamp,
                                              MetaModalOptions    options);
void     shell_global_end_modal              (ShellGlobal         *global,
                                              guint32              timestamp);
void     shell_global_freeze_keyboard        (ShellGlobal         *global,
                                              guint32              timestamp);

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

void     shell_global_get_memory_info      (ShellGlobal     *global,
                                            ShellMemoryInfo *meminfo);


/* Run-at-leisure API */
void shell_global_begin_work     (ShellGlobal          *global);
void shell_global_end_work       (ShellGlobal          *global);

typedef void (*ShellLeisureFunction) (gpointer data);

void shell_global_run_at_leisure (ShellGlobal          *global,
                                  ShellLeisureFunction  func,
                                  gpointer              user_data,
                                  GDestroyNotify        notify);


/* Misc utilities / Shell API */
void     shell_global_sync_pointer              (ShellGlobal  *global);

GAppLaunchContext *
         shell_global_create_app_launch_context (ShellGlobal  *global);

void     shell_global_play_theme_sound          (ShellGlobal *global,
                                                 guint        id,
                                                 const char   *name,
                                                 const char   *description,
                                                 ClutterEvent *for_event);
void     shell_global_play_theme_sound_full     (ShellGlobal  *global,
                                                 guint         id,
                                                 const char   *name,
                                                 const char   *description,
                                                 ClutterEvent *for_event,
                                                 const char   *application_id,
                                                 const char   *application_name);
void     shell_global_play_sound_file           (ShellGlobal  *global,
                                                 guint         id,
                                                 const char   *file_name,
                                                 const char   *description,
                                                 ClutterEvent *for_event);
void     shell_global_play_sound_file_full      (ShellGlobal  *global,
                                                 guint         id,
                                                 const char   *file_name,
                                                 const char   *description,
                                                 ClutterEvent *for_event,
                                                 const char   *application_id,
                                                 const char   *application_name);

void     shell_global_cancel_theme_sound        (ShellGlobal  *global,
                                                 guint         id);

void     shell_global_notify_error              (ShellGlobal  *global,
                                                 const char   *msg,
                                                 const char   *details);

void     shell_global_init_xdnd                 (ShellGlobal  *global);

void     shell_global_reexec_self               (ShellGlobal  *global);

const char *     shell_global_get_session_mode  (ShellGlobal  *global);

void     shell_global_set_runtime_state      (ShellGlobal  *global,
                                              const char   *property_name,
                                              GVariant     *variant);
GVariant * shell_global_get_runtime_state       (ShellGlobal  *global,
                                                 const char   *property_type,
                                                 const char   *property_name);


G_END_DECLS

#endif /* __SHELL_GLOBAL_H__ */
