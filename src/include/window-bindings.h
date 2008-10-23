/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* 
 * Copyright (C) 2008 Thomas Thurman
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/**
 * A list of window keybinding information.
 *
 * Information about how this file works is in screen-bindings.h.
 */

#ifndef keybind
#error "keybind () must be defined when you include window-bindings.h"
#endif

/***********************************/
/* FIXME: this is duplicated from screen-bindings.h; find a better
 * solution, which may involve merging the two files */

#ifndef _BINDINGS_DEFINED_CONSTANTS
#define _BINDINGS_DEFINED_CONSTANTS 1

#define BINDING_PER_WINDOW    0x01
#define BINDING_REVERSES      0x02
#define BINDING_IS_REVERSED   0x04

#endif /* _BINDINGS_DEFINED_CONSTANTS */


keybind (activate_window_menu, handle_activate_window_menu, 0,
        BINDING_PER_WINDOW, "<Alt>space",
        _("Activate the window menu"))
keybind (toggle_fullscreen, handle_toggle_fullscreen, 0, BINDING_PER_WINDOW,
        NULL,
        _("Toggle fullscreen mode"))
keybind (toggle_maximized, handle_toggle_maximized, 0, BINDING_PER_WINDOW, NULL,
        _("Toggle maximization state"))
keybind (toggle_above, handle_toggle_above, 0, BINDING_PER_WINDOW, NULL,
        _("Toggle whether a window will always be visible over other windows"))
keybind (maximize, handle_maximize, 0, BINDING_PER_WINDOW, "<Alt>F10",
        _("Maximize window"))
keybind (unmaximize, handle_unmaximize, 0, BINDING_PER_WINDOW, "<Alt>F5",
        _("Unmaximize window"))
keybind (toggle_shaded, handle_toggle_shaded, 0, BINDING_PER_WINDOW, NULL,
        _("Toggle shaded state"))
keybind (minimize, handle_minimize, 0, BINDING_PER_WINDOW, "<Alt>F9",
        _("Minimize window"))
keybind (close, handle_close, 0, BINDING_PER_WINDOW, "<Alt>F4",
        _("Close window"))
keybind (begin_move, handle_begin_move, 0, BINDING_PER_WINDOW, "<Alt>F7",
        _("Move window"))
keybind (begin_resize, handle_begin_resize, 0, BINDING_PER_WINDOW, "<Alt>F8",
        _("Resize window"))
keybind (toggle_on_all_workspaces, handle_toggle_on_all_workspaces, 0,
         BINDING_PER_WINDOW, NULL,
        _("Toggle whether window is on all workspaces or just one"))

keybind (move_to_workspace_1, handle_move_to_workspace, 0, BINDING_PER_WINDOW,
        NULL,
        _("Move window to workspace 1"))
keybind (move_to_workspace_2, handle_move_to_workspace, 1, BINDING_PER_WINDOW,
        NULL,
        _("Move window to workspace 2"))
keybind (move_to_workspace_3, handle_move_to_workspace, 2, BINDING_PER_WINDOW,
        NULL,
        _("Move window to workspace 3"))
keybind (move_to_workspace_4, handle_move_to_workspace, 3, BINDING_PER_WINDOW,
        NULL,
        _("Move window to workspace 4"))
keybind (move_to_workspace_5, handle_move_to_workspace, 4, BINDING_PER_WINDOW,
        NULL,
        _("Move window to workspace 5"))
keybind (move_to_workspace_6, handle_move_to_workspace, 5, BINDING_PER_WINDOW,
        NULL,
        _("Move window to workspace 6"))
keybind (move_to_workspace_7, handle_move_to_workspace, 6, BINDING_PER_WINDOW,
        NULL,
        _("Move window to workspace 7"))
keybind (move_to_workspace_8, handle_move_to_workspace, 7, BINDING_PER_WINDOW,
        NULL,
        _("Move window to workspace 8"))
keybind (move_to_workspace_9, handle_move_to_workspace, 8, BINDING_PER_WINDOW,
        NULL,
        _("Move window to workspace 9"))
keybind (move_to_workspace_10, handle_move_to_workspace, 9, BINDING_PER_WINDOW,
        NULL,
        _("Move window to workspace 10"))
keybind (move_to_workspace_11, handle_move_to_workspace, 10, BINDING_PER_WINDOW,
        NULL,
        _("Move window to workspace 11"))
keybind (move_to_workspace_12, handle_move_to_workspace, 11, BINDING_PER_WINDOW,
        NULL,
        _("Move window to workspace 12"))

/* META_MOTION_* are negative, and so distinct from workspace numbers,
 * which are always zero or positive.
 * If you make use of these constants, you will need to include workspace.h
 * (which you're probably using already for other reasons anyway).
 * If your definition of keybind() throws them away, you don't need to include
 * workspace.h, of course.
 */

keybind (move_to_workspace_left, handle_move_to_workspace,
         META_MOTION_LEFT, BINDING_PER_WINDOW, "<Control><Shift><Alt>Left",
        _("Move window one workspace to the left"))
keybind (move_to_workspace_right, handle_move_to_workspace,
         META_MOTION_RIGHT, BINDING_PER_WINDOW, "<Control><Shift><Alt>Right",
        _("Move window one workspace to the right"))
keybind (move_to_workspace_up, handle_move_to_workspace,
         META_MOTION_UP, BINDING_PER_WINDOW, "<Control><Shift><Alt>Up",
        _("Move window one workspace up"))
keybind (move_to_workspace_down, handle_move_to_workspace,
         META_MOTION_DOWN, BINDING_PER_WINDOW, "<Control><Shift><Alt>Down",
        _("Move window one workspace down"))

keybind (raise_or_lower, handle_raise_or_lower, 0, BINDING_PER_WINDOW, NULL,
        _("Raise window if it's covered by another window, otherwise lower it"))
keybind (raise, handle_raise, 0, BINDING_PER_WINDOW, NULL,
        _("Raise window above other windows"))
keybind (lower, handle_lower, 0, BINDING_PER_WINDOW, NULL,
        _("Lower window below other windows"))

keybind (maximize_vertically, handle_maximize_vertically, 0,
        BINDING_PER_WINDOW, NULL,
        _("Maximize window vertically"))

keybind (maximize_horizontally, handle_maximize_horizontally, 0,
        BINDING_PER_WINDOW, NULL,
        _("Maximize window horizontally"))

keybind (move_to_corner_nw, handle_move_to_corner_nw, 0,
        BINDING_PER_WINDOW, NULL,
        _("Move window to north-west (top left) corner"))
keybind (move_to_corner_ne, handle_move_to_corner_ne, 0,
        BINDING_PER_WINDOW, NULL,
        _("Move window to north-east (top right) corner"))
keybind (move_to_corner_sw, handle_move_to_corner_sw, 0,
        BINDING_PER_WINDOW, NULL,
        _("Move window to south-west (bottom left) corner"))
keybind (move_to_corner_nw, handle_move_to_corner_se, 0,
        BINDING_PER_WINDOW, NULL,
        _("Move window to south-east (bottom right) corner"))

keybind (move_to_side_n, handle_move_to_side_n, 0,
        BINDING_PER_WINDOW, NULL,
        _("Move window to north (top) side of screen"))
keybind (move_to_side_s, handle_move_to_side_s, 0,
        BINDING_PER_WINDOW, NULL,
        _("Move window to south (bottom) side of screen"))
keybind (move_to_side_e, handle_move_to_side_e, 0,
        BINDING_PER_WINDOW, NULL,
        _("Move window to east (right) side of screen"))
keybind (move_to_side_w, handle_move_to_side_w, 0,
        BINDING_PER_WINDOW, NULL,
        _("Move window to west (left) side of screen"))
keybind (move_to_center, handle_move_to_center, 0,
        BINDING_PER_WINDOW, NULL,
        _("Move window to center of screen"))

/* eof window-bindings.h */

