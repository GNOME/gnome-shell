/* shell-monitor-group.h
 *
 * Copyright 2023 Zander Brown <zbrown@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <st/st.h>
#include <meta/workspace.h>

#include "shell-workspace-group.h"

G_BEGIN_DECLS

#define SHELL_TYPE_MONITOR_GROUP (shell_monitor_group_get_type ())
G_DECLARE_DERIVABLE_TYPE (ShellMonitorGroup, shell_monitor_group, SHELL, MONITOR_GROUP, StWidget)

struct _ShellMonitorGroupClass
{
  StWidgetClass parent_class;
};

float          shell_monitor_group_get_base_distance        (ShellMonitorGroup   *self);
float          shell_monitor_group_get_progress             (ShellMonitorGroup   *self);
void           shell_monitor_group_set_progress             (ShellMonitorGroup   *self,
                                                             float                progress);
float          shell_monitor_group_get_workspace_progress   (ShellMonitorGroup   *self,
                                                             MetaWorkspace       *workspace);
void           shell_monitor_group_get_snap_points          (ShellMonitorGroup   *self,
                                                             size_t              *n_points,
                                                             float               *points[]);
void           shell_monitor_group_add_group                (ShellMonitorGroup   *self,
                                                             ShellWorkspaceGroup *group,
                                                             float                x,
                                                             float                y);
void           shell_monitor_group_update_swipe_for_monitor (ShellMonitorGroup   *self,
                                                             float                progress,
                                                             ShellMonitorGroup   *monitor_group);
MetaWorkspace *shell_monitor_group_find_closest_workspace   (ShellMonitorGroup   *self,
                                                             float                progress);

G_END_DECLS
