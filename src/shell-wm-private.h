/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_WM_PRIVATE_H__
#define __SHELL_WM_PRIVATE_H__

#include "shell-wm.h"

G_BEGIN_DECLS

/* These forward along the different effects from GnomeShellPlugin */

void _shell_wm_minimize   (ShellWM         *wm,
                           MetaWindowActor *actor);
void _shell_wm_unminimize (ShellWM         *wm,
                           MetaWindowActor *actor);
void _shell_wm_size_changed(ShellWM         *wm,
                            MetaWindowActor *actor);
void _shell_wm_size_change(ShellWM         *wm,
                           MetaWindowActor *actor,
                           MetaSizeChange   which_change,
                           MetaRectangle   *old_frame_rect,
                           MetaRectangle   *old_buffer_rect);
void _shell_wm_map        (ShellWM         *wm,
                           MetaWindowActor *actor);
void _shell_wm_destroy    (ShellWM         *wm,
                           MetaWindowActor *actor);

void _shell_wm_switch_workspace      (ShellWM             *wm,
                                      gint                 from,
                                      gint                 to,
                                      MetaMotionDirection  direction);
void _shell_wm_kill_window_effects   (ShellWM             *wm,
                                      MetaWindowActor     *actor);
void _shell_wm_kill_switch_workspace (ShellWM             *wm);

void _shell_wm_show_tile_preview     (ShellWM             *wm,
                                      MetaWindow          *window,
                                      MetaRectangle       *tile_rect,
                                      int                  tile_monitor);
void _shell_wm_hide_tile_preview     (ShellWM             *wm);
void _shell_wm_show_window_menu      (ShellWM             *wm,
                                      MetaWindow          *window,
                                      MetaWindowMenuType   menu,
                                      int                  x,
                                      int                  y);
void _shell_wm_show_window_menu_for_rect (ShellWM             *wm,
                                          MetaWindow          *window,
                                          MetaWindowMenuType   menu,
                                          MetaRectangle       *rect);

gboolean _shell_wm_filter_keybinding (ShellWM             *wm,
                                      MetaKeyBinding      *binding);

void _shell_wm_confirm_display_change (ShellWM            *wm);

MetaCloseDialog * _shell_wm_create_close_dialog (ShellWM     *wm,
                                                 MetaWindow  *window);

MetaInhibitShortcutsDialog * _shell_wm_create_inhibit_shortcuts_dialog (ShellWM     *wm,
                                                                        MetaWindow  *window);

G_END_DECLS

#endif /* __SHELL_WM_PRIVATE_H__ */
