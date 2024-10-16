/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#pragma once

#include "shell-window-tracker.h"

void _shell_window_tracker_add_child_process_app (ShellWindowTracker *tracker,
                                                  GPid                pid,
                                                  ShellApp           *app);
