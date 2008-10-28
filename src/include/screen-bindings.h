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
 * A list of screen keybinding information.
 *
 * Each action which can have a keystroke bound to it is listed below.
 * To use this file, define keybind() to be a seven-argument macro (you can
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
 * (This may merge with window-bindings.h at some point, but traditionally
 * they have been separate concerns.  Meanwhile, it is important that actions
 * don't have names which clash between the two.)
 *
 * Currently, the GConf schemas in src/metacity.schemas also need to be
 * updated separately.  There is a program called schema-bindings.c in this
 * directory which will fix that, but it needs integrating into the build
 * process.
 *
 * The arguments to keybind() are:
 *   1) the name of the binding; a bareword identifier
 *              (it's fine if it happens to clash with a C reserved word)
 *   2) the name of the function which implements it.
 *              Clearly we could have guessed this from the binding very often,
 *              but we choose to write it in full for the benefit of grep.
 *   3) an integer parameter to pass to the handler
 *   4) a set of boolean flags, ORed together:
 *       BINDING_PER_WINDOW  - this is a window-based binding
 *                             (all in window-bindings.h use this,
 *                             and none in screen-bindings.h)
 *       BINDING_REVERSES    - the binding can reverse if you hold down Shift
 *       BINDING_IS_REVERSED - the same, but the senses are reversed from the
 *                             handler's point of view (let me know if I should
 *                             explain this better)
 *      or 0 if no flag applies.
 *
 *   5) a string representing the default binding.
 *          If this is NULL, the action is unbound by default.
 *          Please use NULL and not "disabled".
 *   6) a short description.
 *          It must be marked translatable (i.e. inside "_(...)").
 *
 * Don't try to do XML entity escaping anywhere in the strings.
 */

#ifndef keybind
#error "keybind () must be defined when you include screen-bindings.h"
#endif

/***********************************/

#ifndef _BINDINGS_DEFINED_CONSTANTS
#define _BINDINGS_DEFINED_CONSTANTS 1

#define BINDING_PER_WINDOW    0x01
#define BINDING_REVERSES      0x02
#define BINDING_IS_REVERSED   0x04

#endif /* _BINDINGS_DEFINED_CONSTANTS */

/***********************************/

/* convenience, since in this file they must always be set together */
#define REVERSES_AND_REVERSED (BINDING_REVERSES | BINDING_IS_REVERSED)

keybind (switch_to_workspace_1,  handle_switch_to_workspace, 0, 0, NULL,
        _("Switch to workspace 1"))
keybind (switch_to_workspace_2,  handle_switch_to_workspace, 1, 0, NULL,
        _("Switch to workspace 2"))
keybind (switch_to_workspace_3,  handle_switch_to_workspace, 2, 0, NULL,
        _("Switch to workspace 3"))
keybind (switch_to_workspace_4,  handle_switch_to_workspace, 3, 0, NULL,
        _("Switch to workspace 4"))
keybind (switch_to_workspace_5,  handle_switch_to_workspace, 4, 0, NULL,
        _("Switch to workspace 5"))
keybind (switch_to_workspace_6,  handle_switch_to_workspace, 5, 0, NULL,
        _("Switch to workspace 6"))
keybind (switch_to_workspace_7,  handle_switch_to_workspace, 6, 0, NULL,
        _("Switch to workspace 7"))
keybind (switch_to_workspace_8,  handle_switch_to_workspace, 7, 0, NULL,
        _("Switch to workspace 8"))
keybind (switch_to_workspace_9,  handle_switch_to_workspace, 8, 0, NULL,
        _("Switch to workspace 9"))
keybind (switch_to_workspace_10, handle_switch_to_workspace, 9, 0, NULL,
        _("Switch to workspace 10"))
keybind (switch_to_workspace_11, handle_switch_to_workspace, 10, 0, NULL,
        _("Switch to workspace 11"))
keybind (switch_to_workspace_12, handle_switch_to_workspace, 11, 0, NULL,
        _("Switch to workspace 12"))

/* META_MOTION_* are negative, and so distinct from workspace numbers,
 * which are always zero or positive.
 * If you make use of these constants, you will need to include workspace.h
 * (which you're probably using already for other reasons anyway).
 * If your definition of keybind() throws them away, you don't need to include
 * workspace.h, of course.
 */

keybind (switch_to_workspace_left, handle_switch_to_workspace,
         META_MOTION_LEFT, 0, "<Control><Alt>Left",
        _("Switch to workspace on the left of the current workspace"))

keybind (switch_to_workspace_right, handle_switch_to_workspace,
         META_MOTION_RIGHT, 0, "<Control><Alt>Right",
        _("Switch to workspace on the right of the current workspace"))

keybind (switch_to_workspace_up, handle_switch_to_workspace,
         META_MOTION_UP, 0, "<Control><Alt>Up",
        _("Switch to workspace above the current workspace"))

keybind (switch_to_workspace_down, handle_switch_to_workspace,
         META_MOTION_DOWN, 0, "<Control><Alt>Down",
        _("Switch to workspace below the current workspace"))

/***********************************/

/* The ones which have inverses.  These can't be bound to any keystroke
 * containing Shift because Shift will invert their "backwards" state.
 *
 * TODO: "NORMAL" and "DOCKS" should be renamed to the same name as their
 * action, for obviousness.
 *
 * TODO: handle_switch and handle_cycle should probably really be the
 * same function checking a bit in the parameter for difference.
 */

keybind (switch_group,              handle_switch,        META_TAB_LIST_GROUP,
         BINDING_REVERSES,       NULL,
        _("Move between windows of an application, using a popup window"))
keybind (switch_group_backwards,    handle_switch,        META_TAB_LIST_GROUP,
         REVERSES_AND_REVERSED,  NULL,
        _("Move backwards between windows of an application, "
          "using a popup window"))
keybind (switch_windows,            handle_switch,        META_TAB_LIST_NORMAL,
         BINDING_REVERSES,       "<Alt>Tab",
        _("Move between windows, using a popup window"))
keybind (switch_windows_backwards,  handle_switch,        META_TAB_LIST_NORMAL,
         REVERSES_AND_REVERSED,  NULL,
        _("Move backwards between windows, using a popup window"))
keybind (switch_panels,             handle_switch,        META_TAB_LIST_DOCKS,
         BINDING_REVERSES,       "<Control><Alt>Tab",
        _("Move between panels and the desktop, using a popup window"))
keybind (switch_panels_backwards,   handle_switch,        META_TAB_LIST_DOCKS,
         REVERSES_AND_REVERSED,  NULL,
         _("Move backwards between panels and the desktop, "
          "using a popup window"))

keybind (cycle_group,               handle_cycle,         META_TAB_LIST_GROUP,
        BINDING_REVERSES,        "<Alt>F6",
        _("Move between windows of an application immediately"))
keybind (cycle_group_backwards,     handle_cycle,         META_TAB_LIST_GROUP,
        REVERSES_AND_REVERSED,   NULL,
        _("Move backwards between windows of an application immediately"))
keybind (cycle_windows,             handle_cycle,         META_TAB_LIST_NORMAL,
        BINDING_REVERSES,        "<Alt>Escape",
        _("Move between windows immediately"))
keybind (cycle_windows_backwards,   handle_cycle,         META_TAB_LIST_NORMAL,
        REVERSES_AND_REVERSED,   NULL,
        _("Move backwards between windows immediately"))
keybind (cycle_panels,              handle_cycle,         META_TAB_LIST_DOCKS,
        BINDING_REVERSES,        "<Control><Alt>Escape",
        _("Move between panels and the desktop immediately"))
keybind (cycle_panels_backwards,    handle_cycle,         META_TAB_LIST_DOCKS,
        REVERSES_AND_REVERSED,   NULL,
        _("Move backwards between panels and the desktop immediately"))

/***********************************/
     
keybind (show_desktop, handle_show_desktop, 0, 0, "<Control><Alt>d",
      _("Hide all normal windows and set focus to the desktop background"))
keybind (panel_main_menu, handle_panel,
       META_KEYBINDING_ACTION_PANEL_MAIN_MENU, 0, "<Alt>F1",
      _("Show the panel's main menu"))
keybind (panel_run_dialog, handle_panel,
       META_KEYBINDING_ACTION_PANEL_RUN_DIALOG, 0, "<Alt>F2",
      _("Show the panel's \"Run Application\" dialog box"))

/* Yes, the param is offset by one.  Historical reasons.  (Maybe worth fixing
 * at some point.)  The description is NULL here because the stanza is
 * irregularly shaped in metacity.schemas.in.  This will probably be fixed
 * as well.
 */
keybind (run_command_1,  handle_run_command,  0, 0, NULL, NULL)
keybind (run_command_2,  handle_run_command,  1, 0, NULL, NULL)
keybind (run_command_3,  handle_run_command,  2, 0, NULL, NULL)
keybind (run_command_4,  handle_run_command,  3, 0, NULL, NULL)
keybind (run_command_5,  handle_run_command,  4, 0, NULL, NULL)
keybind (run_command_6,  handle_run_command,  5, 0, NULL, NULL)
keybind (run_command_7,  handle_run_command,  6, 0, NULL, NULL)
keybind (run_command_8,  handle_run_command,  7, 0, NULL, NULL)
keybind (run_command_9,  handle_run_command,  8, 0, NULL, NULL)
keybind (run_command_10, handle_run_command,  9, 0, NULL, NULL)
keybind (run_command_11, handle_run_command, 10, 0, NULL, NULL)
keybind (run_command_12, handle_run_command, 11, 0, NULL, NULL)
keybind (run_command_13, handle_run_command, 12, 0, NULL, NULL)
keybind (run_command_14, handle_run_command, 13, 0, NULL, NULL)
keybind (run_command_15, handle_run_command, 14, 0, NULL, NULL)
keybind (run_command_16, handle_run_command, 15, 0, NULL, NULL)
keybind (run_command_17, handle_run_command, 16, 0, NULL, NULL)
keybind (run_command_18, handle_run_command, 17, 0, NULL, NULL)
keybind (run_command_19, handle_run_command, 18, 0, NULL, NULL)
keybind (run_command_20, handle_run_command, 19, 0, NULL, NULL)
keybind (run_command_21, handle_run_command, 20, 0, NULL, NULL)
keybind (run_command_22, handle_run_command, 21, 0, NULL, NULL)
keybind (run_command_23, handle_run_command, 22, 0, NULL, NULL)
keybind (run_command_24, handle_run_command, 23, 0, NULL, NULL)
keybind (run_command_25, handle_run_command, 24, 0, NULL, NULL)
keybind (run_command_26, handle_run_command, 25, 0, NULL, NULL)
keybind (run_command_27, handle_run_command, 26, 0, NULL, NULL)
keybind (run_command_28, handle_run_command, 27, 0, NULL, NULL)
keybind (run_command_29, handle_run_command, 28, 0, NULL, NULL)
keybind (run_command_30, handle_run_command, 29, 0, NULL, NULL)
keybind (run_command_31, handle_run_command, 30, 0, NULL, NULL)
keybind (run_command_32, handle_run_command, 31, 0, NULL, NULL)

keybind (run_command_screenshot, handle_run_command, 32, 0, "Print",
      _("Take a screenshot"))
keybind (run_command_window_screenshot, handle_run_command, 33, 0,"<Alt>Print",
      _("Take a screenshot of a window"))

keybind (run_command_terminal, handle_run_terminal, 0, 0, NULL, _("Run a terminal"))

/* No description because this is undocumented */
keybind (set_spew_mark, handle_set_spew_mark, 0, 0, NULL, NULL)

#undef REVERSES_AND_REVERSED

/* eof screen-bindings.h */

