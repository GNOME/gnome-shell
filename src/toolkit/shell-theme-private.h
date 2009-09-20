/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_THEME_PRIVATE_H__
#define __SHELL_THEME_PRIVATE_H__

#include <libcroco/libcroco.h>
#include "shell-theme.h"

G_BEGIN_DECLS

GPtrArray *_shell_theme_get_matched_properties (ShellTheme       *theme,
                                                ShellThemeNode   *node);

/* Resolve an URL from the stylesheet to a filename */
char *_shell_theme_resolve_url (ShellTheme   *theme,
                                CRStyleSheet *base_stylesheet,
                                const char   *url);

G_END_DECLS

#endif /* __SHELL_THEME_PRIVATE_H__ */
