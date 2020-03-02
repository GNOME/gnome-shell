/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_APP_USAGE_H__
#define __SHELL_APP_USAGE_H__

#include "shell-app.h"
#include "shell-window-tracker.h"

G_BEGIN_DECLS

#define SHELL_TYPE_APP_USAGE              (shell_app_usage_get_type ())
G_DECLARE_FINAL_TYPE (ShellAppUsage, shell_app_usage,
                      SHELL, APP_USAGE, GObject)

ShellAppUsage* shell_app_usage_get_default(void);

GSList *shell_app_usage_get_most_used (ShellAppUsage *usage);
int shell_app_usage_compare (ShellAppUsage *self,
                             const char    *id_a,
                             const char    *id_b);

G_END_DECLS

#endif /* __SHELL_APP_USAGE_H__ */
