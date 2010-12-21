/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_WINDOW_TRACKER_PRIVATE_H__
#define __SHELL_WINDOW_TRACKER_PRIVATE_H__

#include "shell-window-tracker.h"

void _shell_window_tracker_notify_app_state_changed (ShellWindowTracker *tracker, ShellApp *self);

void _shell_window_tracker_add_child_process_app (ShellWindowTracker *tracker,
                                                  GPid                pid,
                                                  ShellApp           *app);

#endif
