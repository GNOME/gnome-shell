/* shell-workspace-dot.h
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

G_BEGIN_DECLS


#define SHELL_TYPE_WORKSPACE_DOT (shell_workspace_dot_get_type ())
G_DECLARE_DERIVABLE_TYPE (ShellWorkspaceDot, shell_workspace_dot, SHELL, WORKSPACE_DOT, ClutterActor)


struct _ShellWorkspaceDotClass {
  ClutterActorClass parent_class;

  void        (*scale_in)                       (ShellWorkspaceDot *dot);
  void        (*scale_out_and_destroy)          (ShellWorkspaceDot *dot);
};


void         shell_workspace_dot_set_state             (ShellWorkspaceDot *self,
                                                        float              expansion,
                                                        float              width_multiplier);
gboolean     shell_workspace_dot_is_destroying         (ShellWorkspaceDot *self);
void         shell_workspace_dot_scale_in              (ShellWorkspaceDot *self);
void         shell_workspace_dot_scale_out_and_destroy (ShellWorkspaceDot *self);


G_END_DECLS
