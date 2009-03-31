#ifndef __SHELL_GLOBAL_H__
#define __SHELL_GLOBAL_H__

#include "mutter-plugin.h"
#include <clutter/clutter.h>
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

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

  void (*panel_run_dialog) (ShellGlobal *global,
			    int      timestamp);
  void (*panel_main_menu) (ShellGlobal *global,
			   int      timestamp);
};

GType            shell_global_get_type            (void) G_GNUC_CONST;

gboolean shell_clutter_texture_set_from_pixbuf (ClutterTexture *texture,
                                                GdkPixbuf      *pixbuf);

GdkPixbuf *shell_get_thumbnail_for_recent_info(GtkRecentInfo  *recent_info);

GSList *shell_get_categories_for_desktop_file(const char *desktop_file_name);

guint16 shell_get_event_key_symbol(ClutterEvent *event);

guint16 shell_get_button_event_click_count(ClutterEvent *event);

ShellGlobal *shell_global_get (void);

void shell_global_grab_dbus_service (ShellGlobal *global);

void shell_global_start_task_panel (ShellGlobal *global);

void shell_global_set_stage_input_area (ShellGlobal *global,
					int          x,
					int          y,
					int          width,
					int          height);

GList *shell_global_get_windows (ShellGlobal *global);

void _shell_global_set_plugin (ShellGlobal  *global,
			       MutterPlugin *plugin);

MetaScreen * shell_global_get_screen (ShellGlobal *global);

gboolean shell_global_grab_keyboard   (ShellGlobal *global);
void     shell_global_ungrab_keyboard (ShellGlobal *global);

void shell_global_reexec_self (ShellGlobal *global);

ClutterCairoTexture *shell_global_create_vertical_gradient (ClutterColor *top,
							    ClutterColor *bottom);

G_END_DECLS

#endif /* __SHELL_GLOBAL_H__ */
