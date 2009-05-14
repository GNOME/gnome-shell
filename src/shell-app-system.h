#ifndef __SHELL_APP_SYSTEM_H__
#define __SHELL_APP_SYSTEM_H__

#include <glib-object.h>

#define SHELL_TYPE_APP_SYSTEM                 (shell_app_system_get_type ())
#define SHELL_APP_SYSTEM(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_APP_SYSTEM, ShellAppSystem))
#define SHELL_APP_SYSTEM_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_APP_SYSTEM, ShellAppSystemClass))
#define SHELL_IS_APP_SYSTEM(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_APP_SYSTEM))
#define SHELL_IS_APP_SYSTEM_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_APP_SYSTEM))
#define SHELL_APP_SYSTEM_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_APP_SYSTEM, ShellAppSystemClass))

typedef struct _ShellAppSystem ShellAppSystem;
typedef struct _ShellAppSystemClass ShellAppSystemClass;
typedef struct _ShellAppSystemPrivate ShellAppSystemPrivate;

struct _ShellAppSystem
{
  GObject parent;

  ShellAppSystemPrivate *priv;
};

struct _ShellAppSystemClass
{
  GObjectClass parent_class;

  void (*changed)(ShellAppSystem *appsys, gpointer data);
};

GType shell_app_system_get_type (void) G_GNUC_CONST;
ShellAppSystem* shell_app_system_new(void);

GSList *shell_app_system_get_applications_for_menu (ShellAppSystem *system, const char *menu);

typedef struct _ShellAppMenuEntry ShellAppMenuEntry;

struct _ShellAppMenuEntry {
  char *name;
  char *id;
  char *icon;
};

GType shell_app_menu_entry_get_type (void);

GSList *shell_app_system_get_menus (ShellAppSystem *system);

GSList *shell_app_system_get_all_settings (ShellAppSystem *system);

#endif /* __SHELL_APP_SYSTEM_H__ */
