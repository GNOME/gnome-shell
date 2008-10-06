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
 *   1) name: the name of the binding; a bareword identifier
 *   2) suffix: a string to add to the binding name to make the handler name
 *              (usually the empty string)
 *   3) param: an integer parameter to pass to the handler
 *   4) short: a short description.  Mostly, you won't use this.
 *             It must be marked translatable (i.e. inside "_(...)").
 *   5) long: like short, except long.  Don't include all the stuff about
 *            the parser being fairly liberal.
 *   6) keystroke: a string representing the default binding.
 *            If this is NULL, the action is unbound by default.
 *
 * Don't try to do XML entity escaping anywhere in the strings.
 *
 * Some code out there wants only the entries which have a default
 * binding (i.e. whose sixth parameter is not NULL).  You can get only these
 * by defining ONLY_BOUND_BY_DEFAULT before you include this file.
 */

#ifndef item
#error "item () must be defined when you include window-bindings.h"
#endif

item (activate_window_menu, "", 0,
        _("Activate window menu"),
        _("The keybinding used to activate the window menu."),
        "<Alt>Print")

#ifndef ONLY_BOUND_BY_DEFAULT

item (toggle_fullscreen, "", 0,
        _("Toggle fullscreen mode"),
        _("The keybinding used to toggle fullscreen mode."),
        NULL)
item (toggle_maximized, "", 0,
        _("Toggle maximization state"),
        _("The keybinding used to toggle maximization."),
        NULL)
item (toggle_above, "", 0,
        _("Toggle always on top state"),
        _("The keybinding used to toggle always on top.  A window that is "
          "always on top will always be visible over other overlapping "
          "windows."),
          NULL)
#endif /* ONLY_BOUND_BY_DEFAULT */

item (maximize, "", 0,
        _("Maximize window"),
        _("The keybinding used to maximize a window."),
        "<Alt>F10")
item (unmaximize, "", 0,
        _("Unmaximize window"),
        _("The keybinding used to unmaximize a window."),
        "<Alt>F5")

#ifndef ONLY_BOUND_BY_DEFAULT

item (toggle_shaded, "", 0,
        _("Toggle shaded state"),
        _("The keybinding used to toggle shaded/unshaded state."),
        NULL)
        
#endif /* ONLY_BOUND_BY_DEFAULT */

item (minimize, "", 0,
        _("Minimize window"),
        _("The keybinding used to minimize a window."),
        "<Alt>F9")
item (close, "", 0,
        _("Close window"),
        _("The keybinding used to close a window."),
        "<Alt>F4")
item (begin_move, "", 0,
        _("Move window"),
        _("The keybinding used to enter \"move mode\" "
        "and begin moving a window using the keyboard."),
        "<Alt>F7")
item (begin_resize, "", 0,
        _("Resize window"),
        ("The keybinding used to enter \"resize mode\" "
        "and begin resizing a window using the keyboard."),
        "<Alt>F8")

#ifndef ONLY_BOUND_BY_DEFAULT

item (toggle_on_all_workspaces, "", 0,
        _("Toggle window on all workspaces"),
        _("The keybinding used to toggle whether the window is on all "
          "workspaces or just one."),
          NULL)


item (move_to_workspace, "_1", 1,
        _("Move window to workspace 1"),
        _("The keybinding used to move a window to workspace 1."),
        NULL)
item (move_to_workspace, "_2", 2,
        _("Move window to workspace 2"),
        _("The keybinding used to move a window to workspace 2."),
        NULL)
item (move_to_workspace, "_3", 3,
        _("Move window to workspace 3"),
        _("The keybinding used to move a window to workspace 3."),
        NULL)
item (move_to_workspace, "_4", 4,
        _("Move window to workspace 4"),
        _("The keybinding used to move a window to workspace 4."),
        NULL)
item (move_to_workspace, "_5", 5,
        _("Move window to workspace 5"),
        _("The keybinding used to move a window to workspace 5."),
        NULL)
item (move_to_workspace, "_6", 6,
        _("Move window to workspace 6"),
        _("The keybinding used to move a window to workspace 6."),
        NULL)
item (move_to_workspace, "_7", 7,
        _("Move window to workspace 7"),
        _("The keybinding used to move a window to workspace 7."),
        NULL)
item (move_to_workspace, "_8", 8,
        _("Move window to workspace 8"),
        _("The keybinding used to move a window to workspace 8."),
        NULL)
item (move_to_workspace, "_9", 9,
        _("Move window to workspace 9"),
        _("The keybinding used to move a window to workspace 9."),
        NULL)
item (move_to_workspace, "_10", 10,
        _("Move window to workspace 10"),
        _("The keybinding used to move a window to workspace 10."),
        NULL)
item (move_to_workspace, "_11", 11,
        _("Move window to workspace 11"),
        _("The keybinding used to move a window to workspace 11."),
        NULL)
item (move_to_workspace, "_12", 12,
        _("Move window to workspace 12"),
        _("The keybinding used to move a window to workspace 12."),
        NULL)

#endif /* ONLY_BOUND_BY_DEFAULT */

/* META_MOTION_* are negative, and so distinct from workspace numbers,
 * which are always zero or positive.
 * If you make use of these constants, you will need to include workspace.h
 * (which you're probably using already for other reasons anyway).
 * If your definition of item() throws them away, you don't need to include
 * workspace.h, of course.
 */

item (move_to_workspace, "_left", META_MOTION_LEFT,
        _("Move window one workspace to the left"),
        _("The keybinding used to move a window one workspace to the left."),
        "<Control><Shift><Alt>Left")
item (move_to_workspace, "_right", META_MOTION_RIGHT,
        _("Move window one workspace to the right"),
        _("The keybinding used to move a window one workspace to the right."),
        "<Control><Shift><Alt>Right")
item (move_to_workspace, "_up", META_MOTION_UP,
        _("Move window one workspace up"),
        _("The keybinding used to move a window one workspace up."),
        "<Control><Shift><Alt>Up")
item (move_to_workspace, "_down", META_MOTION_DOWN,
        _("Move window one workspace down"),
        _("The keybinding used to move a window one workspace down."),
        "<Control><Shift><Alt>Down")

#ifndef ONLY_BOUND_BY_DEFAULT

item (raise_or_lower, "", 0,
        _("Raise obscured window, otherwise lower"),
        _("This keybinding changes whether a window is above or below "
           "other windows.  If the window is covered by another one, it "
           "raises the window above all others, and if the window is "
           "already fully visible, it lowers it below all others."),
           NULL)
item (raise, "", 0,
        _("Raise window above other windows"),
        _("This keybinding raises the window above other windows."),
        NULL)
item (lower, "", 0,
        _("Lower window below other windows"),
        _("This keybinding lowers a window below other windows."),
        NULL)

item (maximize_vertically, "", 0,
        _("Maximize window vertically"),
        _("This keybinding resizes a window to fill available "
          "vertical space."),
          NULL)
item (maximize_horizontally, "", 0,
        _("Maximize window horizontally"),
        _("This keybinding resizes a window to fill available "
          "horizontal space."),
          NULL)

item (move_to_corner_nw, "", 0,
        _("Move window to north-west corner"),
        _("This keybinding moves a window into the north-west (top left) "
          "corner of the screen."),
          NULL)
item (move_to_corner_ne, "", 0,
        _("Move window to north-east corner"),
        _("This keybinding moves a window into the north-east (top right) "
          "corner of the screen."),
          NULL)
item (move_to_corner_sw, "", 0,
        _("Move window to south-west corner"),
        _("This keybinding moves a window into the north-east (bottom left) "
          "corner of the screen."),
          NULL)
item (move_to_corner_se, "", 0,
        _("Move window to south-east corner"),
        _("This keybinding moves a window into the north-east (bottom right) "
          "corner of the screen."),
          NULL)

item (move_to_side_n, "", 0,
        _("Move window to north side of screen"),
        _("This keybinding moves a window against the north (top) "
          "side of the screen."),
          NULL)
item (move_to_side_s, "", 0,
        _("Move window to south side of screen"),
        _("This keybinding moves a window against the south (bottom) "
          "side of the screen."),
          NULL)
item (move_to_side_e, "", 0,
        _("Move window to east side of screen"),
        _("This keybinding moves a window against the east (right) "
          "side of the screen."),
          NULL)
item (move_to_side_w, "", 0,
        _("Move window to west side of screen"),
        _("This keybinding moves a window against the west (left) "
          "side of the screen."),
          NULL)

item (move_to_center, "", 0,
        _("Move window to center of screen"),
        _("This keybinding moves a window into the center "
          "of the screen."),
          NULL)

#endif /* ONLY_BOUND_BY_DEFAULT */

/* eof window-bindings.h */

