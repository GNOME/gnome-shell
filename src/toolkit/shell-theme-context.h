/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_THEME_CONTEXT_H__
#define __SHELL_THEME_CONTEXT_H__

#include <clutter/clutter.h>
#include <pango/pango.h>
#include "shell-theme-node.h"

G_BEGIN_DECLS

typedef struct _ShellThemeContextClass ShellThemeContextClass;

#define SHELL_TYPE_THEME_CONTEXT             (shell_theme_context_get_type ())
#define SHELL_THEME_CONTEXT(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), SHELL_TYPE_THEME_CONTEXT, ShellThemeContext))
#define SHELL_THEME_CONTEXT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_THEME_CONTEXT, ShellThemeContextClass))
#define SHELL_IS_THEME_CONTEXT(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), SHELL_TYPE_THEME_CONTEXT))
#define SHELL_IS_THEME_CONTEXT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_THEME_CONTEXT))
#define SHELL_THEME_CONTEXT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_THEME_CONTEXT, ShellThemeContextClass))

GType shell_theme_context_get_type (void) G_GNUC_CONST;

ShellThemeContext *shell_theme_context_new           (void);
ShellThemeContext *shell_theme_context_get_for_stage (ClutterStage *stage);

void                        shell_theme_context_set_theme      (ShellThemeContext          *context,
                                                                ShellTheme                 *theme);
ShellTheme *                shell_theme_context_get_theme      (ShellThemeContext          *context);

void                        shell_theme_context_set_resolution (ShellThemeContext          *context,
                                                                gdouble                     resolution);
double                      shell_theme_context_get_resolution (ShellThemeContext          *context);
void                        shell_theme_context_set_font       (ShellThemeContext          *context,
                                                                const PangoFontDescription *font);
const PangoFontDescription *shell_theme_context_get_font       (ShellThemeContext          *context);

ShellThemeNode *            shell_theme_context_get_root_node  (ShellThemeContext          *context);

G_END_DECLS

#endif /* __SHELL_THEME_CONTEXT_H__ */
