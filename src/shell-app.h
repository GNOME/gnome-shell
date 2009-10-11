/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_APP_H__
#define __SHELL_APP_H__

#include <glib-object.h>
#include <glib.h>

#include "window.h"
#include "shell-app-system.h"

G_BEGIN_DECLS

typedef struct _ShellApp ShellApp;
typedef struct _ShellAppClass ShellAppClass;
typedef struct _ShellAppPrivate ShellAppPrivate;

#define SHELL_TYPE_APP              (shell_app_get_type ())
#define SHELL_APP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), SHELL_TYPE_APP, ShellApp))
#define SHELL_APP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_APP, ShellAppClass))
#define SHELL_IS_APP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), SHELL_TYPE_APP))
#define SHELL_IS_APP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_APP))
#define SHELL_APP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_APP, ShellAppClass))

struct _ShellAppClass
{
  GObjectClass parent_class;

};

GType shell_app_get_type (void) G_GNUC_CONST;

const char *shell_app_get_id (ShellApp *app);

ClutterActor *shell_app_create_icon_texture (ShellApp *app, float size);
char *shell_app_get_name (ShellApp *app);
char *shell_app_get_description (ShellApp *app);

ShellAppInfo *shell_app_get_info (ShellApp *app);

GSList *shell_app_get_windows (ShellApp *app);

gboolean shell_app_is_on_workspace (ShellApp *app, MetaWorkspace *workspace);

int shell_app_compare (ShellApp *app, ShellApp *other);

ShellApp* _shell_app_new_for_window (MetaWindow *window);

ShellApp* _shell_app_new (ShellAppInfo *appinfo);

void _shell_app_add_window (ShellApp *app, MetaWindow *window);

void _shell_app_remove_window (ShellApp *app, MetaWindow *window);

G_END_DECLS

#endif /* __SHELL_APP_H__ */
