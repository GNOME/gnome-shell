/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_GLOBAL_PRIVATE_H__
#define __SHELL_GLOBAL_PRIVATE_H__

#include "shell-global.h"

#include <gjs/gjs.h>

typedef ShellGlobal ShellGlobalSingleton;

ShellGlobalSingleton *_shell_global_init (const char *first_property_name,
                                          ...);
void _shell_global_set_plugin      (ShellGlobal  *global,
                                    MetaPlugin   *plugin);

void        _shell_global_destroy (ShellGlobal  *global);

GjsContext *_shell_global_get_gjs_context (ShellGlobal  *global);

gboolean _shell_global_check_xdnd_event (ShellGlobal  *global,
                                         XEvent       *xev);

void _shell_global_locate_pointer (ShellGlobal  *global);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ShellGlobalSingleton, _shell_global_destroy)

#endif /* __SHELL_GLOBAL_PRIVATE_H__ */
