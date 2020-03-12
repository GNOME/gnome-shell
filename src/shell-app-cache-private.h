/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_APP_CACHE_PRIVATE_H__
#define __SHELL_APP_CACHE_PRIVATE_H__

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#define SHELL_TYPE_APP_CACHE (shell_app_cache_get_type())

G_DECLARE_FINAL_TYPE (ShellAppCache, shell_app_cache, SHELL, APP_CACHE, GObject)

ShellAppCache   *shell_app_cache_get_default      (void);
GList           *shell_app_cache_get_all          (ShellAppCache *cache);
GDesktopAppInfo *shell_app_cache_get_info         (ShellAppCache *cache,
                                                   const char    *id);
char            *shell_app_cache_translate_folder (ShellAppCache *cache,
                                                   const char    *name);

#endif /* __SHELL_APP_CACHE_PRIVATE_H__ */
