/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_THEME_H__
#define __SHELL_THEME_H__

#include <glib-object.h>

#include "shell-theme-node.h"

G_BEGIN_DECLS

typedef struct _ShellThemeClass ShellThemeClass;

#define SHELL_TYPE_THEME              (shell_theme_get_type ())
#define SHELL_THEME(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), SHELL_TYPE_THEME, ShellTheme))
#define SHELL_THEME_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_THEME, ShellThemeClass))
#define SHELL_IS_THEME(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), SHELL_TYPE_THEME))
#define SHELL_IS_THEME_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_THEME))
#define SHELL_THEME_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_THEME, ShellThemeClass))

GType  shell_theme_get_type (void) G_GNUC_CONST;

ShellTheme *shell_theme_new (const char *application_stylesheet,
                             const char *theme_stylesheet,
                             const char *default_stylesheet);

G_END_DECLS

#endif /* __SHELL_THEME_H__ */
