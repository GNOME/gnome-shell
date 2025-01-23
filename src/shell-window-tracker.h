/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#pragma once

#include <glib-object.h>
#include <glib.h>
#include <meta/window.h>
#include <meta/meta-startup-notification.h>

#include "shell-app.h"
#include "shell-app-system.h"

G_BEGIN_DECLS

#define SHELL_TYPE_WINDOW_TRACKER (shell_window_tracker_get_type ())
G_DECLARE_FINAL_TYPE (ShellWindowTracker, shell_window_tracker,
                      SHELL, WINDOW_TRACKER, GObject)

ShellWindowTracker* shell_window_tracker_get_default(void);

ShellApp *shell_window_tracker_get_window_app (ShellWindowTracker *tracker, MetaWindow *metawin);

ShellApp *shell_window_tracker_get_app_from_pid (ShellWindowTracker *tracker, int pid);

ShellApp *shell_window_tracker_get_focus_app (ShellWindowTracker *tracker);

GSList *shell_window_tracker_get_startup_sequences (ShellWindowTracker *tracker);

G_END_DECLS
