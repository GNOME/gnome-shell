/* shell-workspace-group.h
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

#include <clutter/clutter.h>
#include <meta/workspace.h>

G_BEGIN_DECLS

#define SHELL_TYPE_WORKSPACE_GROUP (shell_workspace_group_get_type ())
G_DECLARE_DERIVABLE_TYPE (ShellWorkspaceGroup, shell_workspace_group, SHELL, WORKSPACE_GROUP, ClutterActor)

struct _ShellWorkspaceGroupClass
{
  ClutterActorClass parent_class;
};

MetaWorkspace   *shell_workspace_group_get_workspace   (ShellWorkspaceGroup *self);

G_END_DECLS
