/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_APP_SYSTEM_H__
#define __SHELL_APP_SYSTEM_H__

#include <gio/gio.h>
#include <clutter/clutter.h>
#include <meta/window.h>

#include "shell-app.h"

#define SHELL_TYPE_APP_SYSTEM (shell_app_system_get_type ())
G_DECLARE_FINAL_TYPE (ShellAppSystem, shell_app_system,
                      SHELL, APP_SYSTEM, GObject)

ShellAppSystem *shell_app_system_get_default (void);

ShellApp       *shell_app_system_lookup_app                   (ShellAppSystem  *system,
                                                               const char      *id);
ShellApp       *shell_app_system_lookup_heuristic_basename    (ShellAppSystem  *system,
                                                               const char      *id);

ShellApp       *shell_app_system_lookup_startup_wmclass       (ShellAppSystem *system,
                                                               const char     *wmclass);
ShellApp       *shell_app_system_lookup_desktop_wmclass       (ShellAppSystem *system,
                                                               const char     *wmclass);

GSList         *shell_app_system_get_running               (ShellAppSystem  *self);
char         ***shell_app_system_search                    (const char *search_string);

GList          *shell_app_system_get_installed             (ShellAppSystem  *self);

#endif /* __SHELL_APP_SYSTEM_H__ */
