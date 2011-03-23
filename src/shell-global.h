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

GType            shell_global_get_type            (void) G_GNUC_CONST;

ShellGlobal *shell_global_get (void);

typedef enum {
  SHELL_CURSOR_DND_IN_DRAG,
  SHELL_CURSOR_DND_UNSUPPORTED_TARGET,
  SHELL_CURSOR_DND_MOVE,
  SHELL_CURSOR_DND_COPY,
  SHELL_CURSOR_POINTING_HAND
} ShellCursor;

void shell_global_set_cursor (ShellGlobal *global,
                              ShellCursor type);

void shell_global_unset_cursor (ShellGlobal  *global);

MetaScreen *shell_global_get_screen (ShellGlobal  *global);

GdkScreen *shell_global_get_gdk_screen (ShellGlobal  *global);

gboolean shell_global_add_extension_importer (ShellGlobal *global,
                                              const char  *target_object_script,
                                              const char  *target_property,
                                              const char  *directory,
                                              GError     **error);

typedef enum {
  SHELL_STAGE_INPUT_MODE_NONREACTIVE,
  SHELL_STAGE_INPUT_MODE_NORMAL,
  SHELL_STAGE_INPUT_MODE_FOCUSED,
  SHELL_STAGE_INPUT_MODE_FULLSCREEN
} ShellStageInputMode;

void shell_global_set_stage_input_mode   (ShellGlobal         *global,
					  ShellStageInputMode  mode);
void shell_global_set_stage_input_region (ShellGlobal         *global,
					  GSList              *rectangles);

GList *shell_global_get_window_actors (ShellGlobal *global);

gboolean shell_global_begin_modal (ShellGlobal *global,
				   guint32      timestamp);
void     shell_global_end_modal   (ShellGlobal *global,
				   guint32      timestamp);

void shell_global_reexec_self (ShellGlobal *global);

void shell_global_breakpoint (ShellGlobal *global);

gboolean shell_global_parse_search_provider (ShellGlobal   *global,
                                             const char    *data,
                                             char         **name,
                                             char         **url,
                                             GList        **langs,
                                             char         **icon_data_uri,
                                             GError       **error);

void shell_global_gc (ShellGlobal *global);

void shell_global_maybe_gc (ShellGlobal *global);

GSList       *shell_global_get_monitors        (ShellGlobal  *global);
MetaRectangle *shell_global_get_primary_monitor (ShellGlobal  *global);
int            shell_global_get_primary_monitor_index (ShellGlobal  *global);
MetaRectangle *shell_global_get_focus_monitor   (ShellGlobal  *global);

guint32 shell_global_create_pointer_barrier (ShellGlobal *global,
                                             int x1, int y1, int x2, int y2,
                                             int directions);
void    shell_global_destroy_pointer_barrier (ShellGlobal *global,
                                              guint32 barrier);

void shell_global_get_pointer  (ShellGlobal         *global,
                                int                 *x,
                                int                 *y,
                                ClutterModifierType *mods);
void shell_global_sync_pointer (ShellGlobal         *global);

GSettings *shell_global_get_settings (ShellGlobal *global);

ClutterModifierType shell_get_event_state (ClutterEvent *event);

gboolean shell_write_string_to_stream (GOutputStream *stream,
                                       const char    *str,
                                       GError       **error);

guint32 shell_global_get_current_time (ShellGlobal *global);

GAppLaunchContext *shell_global_create_app_launch_context (ShellGlobal *global);

gboolean shell_global_set_property_mutable (ShellGlobal *global,
                                            const char  *object,
                                            const char  *property,
                                            gboolean     mutable);

void shell_global_begin_work     (ShellGlobal         *global);
void shell_global_end_work       (ShellGlobal         *global);

typedef void (*ShellLeisureFunction) (gpointer data);

void shell_global_run_at_leisure (ShellGlobal         *global,
                                  ShellLeisureFunction func,
                                  gpointer             user_data,
                                  GDestroyNotify       notify);

void shell_global_play_theme_sound (ShellGlobal       *global,
                                    guint              id,
                                    const char        *name);
void shell_global_cancel_theme_sound (ShellGlobal     *global,
                                      guint            id);


void shell_global_notify_error (ShellGlobal  *global,
                                const char   *msg,
                                const char   *details);

void shell_global_init_xdnd (ShellGlobal *global);

void shell_global_launch_calendar_server (ShellGlobal *global);

char *shell_get_file_contents_utf8_sync (const char *path,
                                         GError    **error);

G_END_DECLS

#endif /* __SHELL_GLOBAL_H__ */
