#ifndef __SHELL_PANEL_WINDOW_H__
#define __SHELL_PANEL_WINDOW_H__

#include <gtk/gtk.h>

#define SHELL_TYPE_PANEL_WINDOW                 (shell_panel_window_get_type ())
#define SHELL_PANEL_WINDOW(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_PANEL_WINDOW, ShellPanelWindow))
#define SHELL_PANEL_WINDOW_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_PANEL_WINDOW, ShellPanelWindowClass))
#define SHELL_IS_PANEL_WINDOW(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_PANEL_WINDOW))
#define SHELL_IS_PANEL_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_PANEL_WINDOW))
#define SHELL_PANEL_WINDOW_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_PANEL_WINDOW, ShellPanelWindowClass))

typedef struct _ShellPanelWindow ShellPanelWindow;
typedef struct _ShellPanelWindowClass ShellPanelWindowClass;

typedef struct ShellPanelWindowPrivate ShellPanelWindowPrivate;

struct _ShellPanelWindow
{
    GtkWindow parent;

    ShellPanelWindowPrivate *priv;
};

struct _ShellPanelWindowClass
{
    GtkWindowClass parent_class;

};

GType shell_panel_window_get_type (void) G_GNUC_CONST;
ShellPanelWindow* shell_panel_window_new(void);


#endif /* __SHELL_PANEL_WINDOW_H__ */
