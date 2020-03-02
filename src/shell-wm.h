/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_WM_H__
#define __SHELL_WM_H__

#include <glib-object.h>
#include <meta/meta-plugin.h>

G_BEGIN_DECLS

#define SHELL_TYPE_WM (shell_wm_get_type ())
G_DECLARE_FINAL_TYPE (ShellWM, shell_wm, SHELL, WM, GObject)

ShellWM *shell_wm_new                        (MetaPlugin      *plugin);

void     shell_wm_completed_minimize         (ShellWM         *wm,
                                              MetaWindowActor *actor);
void     shell_wm_completed_unminimize       (ShellWM         *wm,
                                              MetaWindowActor *actor);
void     shell_wm_completed_size_change      (ShellWM         *wm,
                                              MetaWindowActor *actor);
void     shell_wm_completed_map              (ShellWM         *wm,
                                              MetaWindowActor *actor);
void     shell_wm_completed_destroy          (ShellWM         *wm,
                                              MetaWindowActor *actor);
void     shell_wm_completed_switch_workspace (ShellWM         *wm);

void     shell_wm_complete_display_change    (ShellWM         *wm,
                                              gboolean         ok);

G_END_DECLS

#endif /* __SHELL_WM_H__ */
