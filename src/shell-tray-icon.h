/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#pragma once

#include <clutter/clutter.h>

#define SHELL_TYPE_TRAY_ICON (shell_tray_icon_get_type ())
G_DECLARE_FINAL_TYPE (ShellTrayIcon, shell_tray_icon,
                      SHELL, TRAY_ICON, ClutterClone)

pid_t shell_tray_icon_get_pid (ShellTrayIcon *icon);
const char * shell_tray_icon_get_title (ShellTrayIcon *icon);
const char * shell_tray_icon_get_wm_class (ShellTrayIcon *icon);

void shell_tray_icon_click (ShellTrayIcon *icon,
                            ClutterEvent  *event);
