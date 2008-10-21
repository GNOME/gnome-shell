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
 * \file  A list of window keybinding information.
 *
 * Each action which can have a keystroke bound to it is listed below.
 * To use this file, define "item" to be a six-argument macro (you can
 * throw any of the arguments you please away), include this file,
 * and then undefine the macro again.
 *
 * (If you aren't familiar with this technique, sometimes called "x-macros",
 * see DDJ of May 2001: <http://www.ddj.com/cpp/184401387>.)
 *
 * This makes it possible to keep all information about all the keybindings
 * in the same place.  The only exception is the code to run when an action
 * is actually invoked; while we *could* have put that in this file, it would
 * have made debugging ridiculously difficult.  Instead, each action should
 * have a corresponding static function named handle_<name>() in
 * keybindings.c.
 *
 * Currently, the GConf schemas in src/metacity.schemas also need to be
 * updated separately.  There is a program called schema-bindings.c in this
 * directory which will fix that, but it needs integrating into the build
 * process.
 *
 * The arguments to item() are:
 *   1) the name of the binding; a bareword identifier
 *   2) a suffix to add to the binding name to make the handler name
 *              (usually the empty string)
 *   3) an integer parameter to pass to the handler
 *   4) a set of boolean flags, ORed together.
 *         This is used in *this* file for completeness, but at present
 *         is not checked anywhere.  We use the flag BINDING_PER_WINDOW
 *         on all window-based bindings (i.e. every binding in this file).
 *   5) a short description.  Mostly, you won't use this.
 *         It must be marked translatable (i.e. inside "_(...)").
 *   6) a string representing the default binding.
 *         If this is NULL, the action is unbound by default.
 *
 * Don't try to do XML entity escaping anywhere in the strings.
 *
 * Possible future work:
 *  - merge with screen-bindings.h somehow
 *  - "suffix" is confusing; write it out in full
 */

#ifndef item
#error "item () must be defined when you include window-bindings.h"
#endif

/***********************************/
/* FIXME: this is duplicated from screen-bindings.h; find a better
 * solution, which may involve merging the two files */

#ifndef _BINDINGS_DEFINED_CONSTANTS
#define _BINDINGS_DEFINED_CONSTANTS 1

#define BINDING_PER_WINDOW    0x01
#define BINDING_REVERSES      0x02
#define BINDING_IS_REVERSED   0x04

/* FIXME: There is somewhere better for these; remove them */
#define PANEL_MAIN_MENU            -1
#define PANEL_RUN_DIALOG           -2

#endif /* _BINDINGS_DEFINED_CONSTANTS */


item (activate_window_menu, "", 0, BINDING_PER_WINDOW,
        _("Activate the window menu"),
        "<Alt>Space")

item (toggle_fullscreen, "", 0, BINDING_PER_WINDOW,
        _("Toggle fullscreen mode"),
        NULL)
item (toggle_maximized, "", 0, BINDING_PER_WINDOW,
        _("Toggle maximization state"),
        NULL)
item (toggle_above, "", 0, BINDING_PER_WINDOW,
        _("Toggle whether a window will always be visible over other windows"),
          NULL)

item (maximize, "", 0, BINDING_PER_WINDOW,
        _("Maximize window"),
        "<Alt>F10")
item (unmaximize, "", 0, BINDING_PER_WINDOW,
        _("Unmaximize window"),
        "<Alt>F5")

item (toggle_shaded, "", 0, BINDING_PER_WINDOW,
        _("Toggle shaded state"),
        NULL)
        
item (minimize, "", 0, BINDING_PER_WINDOW,
        _("Minimize window"),
        "<Alt>F9")
item (close, "", 0, BINDING_PER_WINDOW,
        _("Close window"),
        "<Alt>F4")
item (begin_move, "", 0, BINDING_PER_WINDOW,
        _("Move window"),
        "<Alt>F7")
item (begin_resize, "", 0, BINDING_PER_WINDOW,
        _("Resize window"),
        "<Alt>F8")

item (toggle_on_all_workspaces, "", 0, BINDING_PER_WINDOW,
        _("Toggle whether window is on all workspaces or just one"),
          NULL)

item (move_to_workspace, "_1", 0, BINDING_PER_WINDOW,
        _("Move window to workspace 1"),
        NULL)
item (move_to_workspace, "_2", 1, BINDING_PER_WINDOW,
        _("Move window to workspace 2"),
        NULL)
item (move_to_workspace, "_3", 2, BINDING_PER_WINDOW,
        _("Move window to workspace 3"),
        NULL)
item (move_to_workspace, "_4", 3, BINDING_PER_WINDOW,
        _("Move window to workspace 4"),
        NULL)
item (move_to_workspace, "_5", 4, BINDING_PER_WINDOW,
        _("Move window to workspace 5"),
        NULL)
item (move_to_workspace, "_6", 5, BINDING_PER_WINDOW,
        _("Move window to workspace 6"),
        NULL)
item (move_to_workspace, "_7", 6, BINDING_PER_WINDOW,
        _("Move window to workspace 7"),
        NULL)
item (move_to_workspace, "_8", 7, BINDING_PER_WINDOW,
        _("Move window to workspace 8"),
        NULL)
item (move_to_workspace, "_9", 8, BINDING_PER_WINDOW,
        _("Move window to workspace 9"),
        NULL)
item (move_to_workspace, "_10", 9, BINDING_PER_WINDOW,
        _("Move window to workspace 10"),
        NULL)
item (move_to_workspace, "_11", 10, BINDING_PER_WINDOW,
        _("Move window to workspace 11"),
        NULL)
item (move_to_workspace, "_12", 11, BINDING_PER_WINDOW,
        _("Move window to workspace 12"),
        NULL)

/* META_MOTION_* are negative, and so distinct from workspace numbers,
 * which are always zero or positive.
 * If you make use of these constants, you will need to include workspace.h
 * (which you're probably using already for other reasons anyway).
 * If your definition of item() throws them away, you don't need to include
 * workspace.h, of course.
 */

item (move_to_workspace, "_left", META_MOTION_LEFT, BINDING_PER_WINDOW,
        _("Move window one workspace to the left"),
        "<Control><Shift><Alt>Left")
item (move_to_workspace, "_right", META_MOTION_RIGHT, BINDING_PER_WINDOW,
        _("Move window one workspace to the right"),
        "<Control><Shift><Alt>Right")
item (move_to_workspace, "_up", META_MOTION_UP, BINDING_PER_WINDOW,
        _("Move window one workspace up"),
        "<Control><Shift><Alt>Up")
item (move_to_workspace, "_down", META_MOTION_DOWN, BINDING_PER_WINDOW,
        _("Move window one workspace down"),
        "<Control><Shift><Alt>Down")

item (raise_or_lower, "", 0, BINDING_PER_WINDOW,
        _("Raise window if it's covered by another window, otherwise lower it"),
           NULL)
item (raise, "", 0, BINDING_PER_WINDOW,
        _("Raise window above other windows"),
        NULL)
item (lower, "", 0, BINDING_PER_WINDOW,
        _("Lower window below other windows"),
        NULL)

item (maximize_vertically, "", 0, BINDING_PER_WINDOW,
        _("Maximize window vertically"),
          NULL)
item (maximize_horizontally, "", 0, BINDING_PER_WINDOW,
        _("Maximize window horizontally"),
          NULL)

item (move_to_corner_nw, "", 0, BINDING_PER_WINDOW,
        _("Move window to north-west (top left) corner"),
          NULL)
item (move_to_corner_ne, "", 0, BINDING_PER_WINDOW,
        _("Move window to north-east (top right) corner"),
          NULL)
item (move_to_corner_sw, "", 0, BINDING_PER_WINDOW,
        _("Move window to south-west (bottom left) corner"),
          NULL)
item (move_to_corner_se, "", 0, BINDING_PER_WINDOW,
        _("Move window to south-east (bottom right) corner"),
          NULL)

item (move_to_side_n, "", 0, BINDING_PER_WINDOW,
        _("Move window to north (top) side of screen"),
          NULL)
item (move_to_side_s, "", 0, BINDING_PER_WINDOW,
        _("Move window to south (bottom) side of screen"),
          NULL)
item (move_to_side_e, "", 0, BINDING_PER_WINDOW,
        _("Move window to east (right) side of screen"),
          NULL)
item (move_to_side_w, "", 0, BINDING_PER_WINDOW,
        _("Move window to west (left) side of screen"),
          NULL)
item (move_to_center, "", 0, BINDING_PER_WINDOW,
        _("Move window to center of screen"),
          NULL)

/* eof window-bindings.h */

