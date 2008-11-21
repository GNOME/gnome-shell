#ifndef __SHELL_GLOBAL_H__
#define __SHELL_GLOBAL_H__

#include "mutter-plugin.h"
#include <clutter/clutter.h>
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

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

gboolean
shell_clutter_texture_set_from_pixbuf (ClutterTexture *texture,
                                       GdkPixbuf      *pixbuf);

ShellGlobal *shell_global_get (void);

void shell_global_set_stage_input_area (ShellGlobal *global,
					int          x,
					int          y,
					int          width,
					int          height);

GList        *shell_global_get_windows       (ShellGlobal *global);
ClutterActor *shell_global_get_window_group  (ShellGlobal *global);
ClutterActor *shell_global_get_overlay_group (ShellGlobal *global);

void _shell_global_set_plugin (ShellGlobal  *global,
			       MutterPlugin *plugin);

MetaScreen * shell_global_get_screen (ShellGlobal *global);

void shell_global_focus_stage (ShellGlobal *global);

G_END_DECLS

#endif /* __SHELL_GLOBAL_H__ */
