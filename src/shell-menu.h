#ifndef __SHELL_MENU_H__
#define __SHELL_MENU_H__

#include <clutter/clutter.h>
#include "big/box.h"

#define SHELL_TYPE_MENU                 (shell_menu_get_type ())
#define SHELL_MENU(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_MENU, ShellMenu))
#define SHELL_MENU_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_MENU, ShellMenuClass))
#define SHELL_IS_MENU(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_MENU))
#define SHELL_IS_MENU_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_MENU))
#define SHELL_MENU_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_MENU, ShellMenuClass))

typedef struct _ShellMenu        ShellMenu;
typedef struct _ShellMenuClass   ShellMenuClass;

typedef struct _ShellMenuPrivate ShellMenuPrivate;

struct _ShellMenu
{
    BigBox parent;

    ShellMenuPrivate *priv;
};

struct _ShellMenuClass
{
    BigBoxClass parent_class;
};

GType shell_menu_get_type (void) G_GNUC_CONST;

void shell_menu_popup (ShellMenu *behavior, guint button, guint32 activate_time);

void shell_menu_append_separator (ShellMenu *behavior, ClutterActor *separator, BigBoxPackFlags flags);

void shell_menu_popdown (ShellMenu *behavior);

#endif /* __SHELL_MENU_H__ */
