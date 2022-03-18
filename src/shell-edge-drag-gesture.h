/*
 * Copyright (C) 2025 Jonas Dreßler <verdre@v0yd.nl>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

G_BEGIN_DECLS

#include <clutter/clutter.h>
#include <st/st.h>

#define SHELL_TYPE_EDGE_DRAG_GESTURE (shell_edge_drag_gesture_get_type ())
G_DECLARE_FINAL_TYPE (ShellEdgeDragGesture, shell_edge_drag_gesture,
                      SHELL, EDGE_DRAG_GESTURE, ClutterGesture)

void shell_edge_drag_gesture_set_side (ShellEdgeDragGesture *self,
                                       StSide                side);

StSide shell_edge_drag_gesture_get_side (ShellEdgeDragGesture *self);

G_END_DECLS
