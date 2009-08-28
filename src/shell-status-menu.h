/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __SHELL_STATUS_MENU_H__
#define __SHELL_STATUS_MENU_H__

#include <clutter/clutter.h>
#include "big/box.h"

G_BEGIN_DECLS

#define SHELL_TYPE_STATUS_MENU			(shell_status_menu_get_type ())
#define SHELL_STATUS_MENU(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_STATUS_MENU, ShellStatusMenu))
#define SHELL_STATUS_MENU_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_STATUS_MENU, ShellStatusMenuClass))
#define SHELL_IS_STATUS_MENU(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_STATUS_MENU))
#define SHELL_IS_STATUS_MENU_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_STATUS_MENU))
#define SHELL_STATUS_MENU_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_STATUS_MENU, ShellStatusMenuClass))

typedef struct _ShellStatusMenu        ShellStatusMenu;
typedef struct _ShellStatusMenuPrivate ShellStatusMenuPrivate;
typedef struct _ShellStatusMenuClass   ShellStatusMenuClass;

struct _ShellStatusMenu
{
  BigBox parent_instance;

  ShellStatusMenuPrivate *priv;
};

struct _ShellStatusMenuClass
{
  BigBoxClass parent_class;

  void (*deactivated) (ShellStatusMenu *status, gpointer user_data);
};

GType             shell_status_menu_get_type     (void);

void              shell_status_menu_toggle       (ShellStatusMenu *menu, ClutterEvent *event);
gboolean          shell_status_menu_is_active    (ShellStatusMenu *menu);
ClutterText      *shell_status_menu_get_name     (ShellStatusMenu *menu);
ClutterTexture   *shell_status_menu_get_icon     (ShellStatusMenu *menu);

G_END_DECLS

#endif /* __SHELL_STATUS_MENU_H__ */
