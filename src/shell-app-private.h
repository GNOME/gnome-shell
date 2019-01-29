/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_APP_PRIVATE_H__
#define __SHELL_APP_PRIVATE_H__

#include "shell-app.h"
#include "shell-app-system.h"

G_BEGIN_DECLS

ShellApp* _shell_app_new_for_window (MetaWindow *window);

ShellApp* _shell_app_new (GDesktopAppInfo *info);

void _shell_app_set_app_info (ShellApp *app, GDesktopAppInfo *info);

void _shell_app_handle_startup_sequence (ShellApp *app, MetaStartupSequence *sequence);

void _shell_app_add_window (ShellApp *app, MetaWindow *window);

void _shell_app_remove_window (ShellApp *app, MetaWindow *window);

G_END_DECLS

#endif /* __SHELL_APP_PRIVATE_H__ */
