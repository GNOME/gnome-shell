/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __SHELL_TRAY_MANAGER_H__
#define __SHELL_TRAY_MANAGER_H__

#include <clutter/clutter.h>
#include "st.h"

G_BEGIN_DECLS

#define SHELL_TYPE_TRAY_MANAGER (shell_tray_manager_get_type ())
G_DECLARE_FINAL_TYPE (ShellTrayManager, shell_tray_manager,
                      SHELL, TRAY_MANAGER, GObject)

ShellTrayManager *shell_tray_manager_new          (void);
void              shell_tray_manager_manage_screen (ShellTrayManager *manager,
                                                    StWidget         *theme_widget);
void              shell_tray_manager_unmanage_screen (ShellTrayManager *manager);

G_END_DECLS

#endif /* __SHELL_TRAY_MANAGER_H__ */
