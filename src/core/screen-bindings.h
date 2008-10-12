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
 * To use this file, define "item" to be a seven-argument macro (you can
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
 * The arguments to item() are:
 *   1) the name of the binding; a bareword identifier
 *              (it's fine if it happens to clash with a C reserved word)
 *   2) a string to add to the binding name to make the handler name
 *              (usually the empty string)
 *   3) an integer parameter to pass to the handler
 *   4) a set of boolean flags, ORed together:
 *       BINDING_PER_WINDOW  - this is a window-based binding
 *                             (not used in this file)
 *       BINDING_REVERSES    - the binding can reverse if you hold down Shift
 *       BINDING_IS_REVERSED - the same, but the senses are reversed from the
 *                             handler's point of view (let me know if I should
 *                             explain this better)
 *       
 *   5) a short description.  Mostly, you won't use this.
 *          It must be marked translatable (i.e. inside "_(...)").
 *   6) like short, except long.  Don't include all the stuff about
 *          the parser being fairly liberal.
 *   7) a string representing the default binding.
 *          If this is NULL, the action is unbound by default.
 *
 * Don't try to do XML entity escaping anywhere in the strings.
 */

#ifndef item
#error "item () must be defined when you include screen-bindings.h"
#endif

/***********************************/

#ifndef _BINDINGS_DEFINED_CONSTANTS
#define _BINDINGS_DEFINED_CONSTANTS 1

#define BINDING_PER_WINDOW    0x01
#define BINDING_REVERSES      0x02
#define BINDING_IS_REVERSED   0x04

/* FIXME: There is somewhere better for these; remove them */
#define PANEL_MAIN_MENU            -1
#define PANEL_RUN_DIALOG           -2

#endif /* _BINDINGS_DEFINED_CONSTANTS */

/***********************************/

/* convenience, since in this file they must always be set together */
#define REVERSES_AND_REVERSED (BINDING_REVERSES | BINDING_IS_REVERSED)

item (switch_to_workspace, "_1", 1, 0,
        _("Switch to workspace 1"),
        _("The keybinding that switches to workspace 1."),
        NULL)
item (switch_to_workspace, "_2", 2, 0,
        _("Switch to workspace 2"),
        _("The keybinding that switches to workspace 2."),
        NULL)
item (switch_to_workspace, "_3", 3, 0,
        _("Switch to workspace 3"),
        _("The keybinding that switches to workspace 3."),
        NULL)
item (switch_to_workspace, "_4", 4, 0,
        _("Switch to workspace 4"),
        _("The keybinding that switches to workspace 4."),
        NULL)
item (switch_to_workspace, "_5", 5, 0,
        _("Switch to workspace 5"),
        _("The keybinding that switches to workspace 5."),
        NULL)
item (switch_to_workspace, "_6", 6, 0,
        _("Switch to workspace 6"),
        _("The keybinding that switches to workspace 6."),
        NULL)
item (switch_to_workspace, "_7", 7, 0,
        _("Switch to workspace 7"),
        _("The keybinding that switches to workspace 7."),
        NULL)
item (switch_to_workspace, "_8", 8, 0,
        _("Switch to workspace 8"),
        _("The keybinding that switches to workspace 8."),
        NULL)
item (switch_to_workspace, "_9", 9, 0,
        _("Switch to workspace 9"),
        _("The keybinding that switches to workspace 9."),
        NULL)
item (switch_to_workspace, "_10", 10, 0,
        _("Switch to workspace 10"),
        _("The keybinding that switches to workspace 10."),
        NULL)
item (switch_to_workspace, "_11", 11, 0,
        _("Switch to workspace 11"),
        _("The keybinding that switches to workspace 11."),
        NULL)
item (switch_to_workspace, "_12", 12, 0,
        _("Switch to workspace 12"),
        _("The keybinding that switches to workspace 12."),
        NULL)

/* META_MOTION_* are negative, and so distinct from workspace numbers,
 * which are always zero or positive.
 * If you make use of these constants, you will need to include workspace.h
 * (which you're probably using already for other reasons anyway).
 * If your definition of item() throws them away, you don't need to include
 * workspace.h, of course.
 */

item (switch_to_workspace, "_left",  META_MOTION_LEFT,  0,
        _("Switch to workspace on the left"),
        _("The keybinding that switches to the workspace on the left "
          "of the current workspace."),
        "<Control><Alt>Left")

item (switch_to_workspace, "_right", META_MOTION_RIGHT, 0,
        _("Switch to workspace on the right"),
        _("The keybinding that switches to the workspace on the right "
          "of the current workspace."),
        "<Control><Alt>Right")

item (switch_to_workspace, "_up",    META_MOTION_UP,    0,
        _("Switch to workspace above this one"),
        _("The keybinding that switches to the workspace above "
          "the current workspace."),
        "<Control><Alt>Up")

item (switch_to_workspace, "_down",  META_MOTION_DOWN,  0,
        _("Switch to workspace below this one"),
        _("The keybinding that switches to the workspace below "
          "the current workspace."),
        "<Control><Alt>Down")

/***********************************/

/* The ones which have inverses.  These can't be bound to any keystroke
 * containing Shift because Shift will invert their "backwards" state.
 *
 * TODO: "NORMAL" and "DOCKS" should be renamed to the same name as their
 * action, for obviousness.
 */

item (switch, "_group",            META_TAB_LIST_GROUP,    BINDING_REVERSES,
        _("Move between windows of an application with popup"),
        _("The keybinding used to move focus between windows of an"
          "application, using a popup window."),           NULL)
item (switch, "_group_backwards",  META_TAB_LIST_GROUP,    REVERSES_AND_REVERSED,
        _("Move backwards between windows of an application with popup"),
        _("The keybinding used to move focus backwards between windows"
          "of an application, using a popup window."),     NULL)
item (switch, "_windows",          META_TAB_LIST_NORMAL,   BINDING_REVERSES,
        _("Move between windows with popup"),
        _("The keybinding used to move focus between windows, "
          "using a popup window."),                        "<Alt>Tab")
item (switch, "_windows_backwards",META_TAB_LIST_NORMAL,   REVERSES_AND_REVERSED,
        _("Move focus backwards between windows using popup display"),
        _("The keybinding used to move focus backwards between windows, "
          "using a popup window."),                        NULL)
item (switch, "_panels",           META_TAB_LIST_DOCKS,    BINDING_REVERSES,
        _("Move between panels and the desktop with popup"),
        _("The keybinding used to move focus between panels and the desktop, "
          "using a popup window."),                        "<Control><Alt>Tab")
item (switch, "_panels_backwards", META_TAB_LIST_DOCKS,    REVERSES_AND_REVERSED,
        _("Move backwards between panels and the desktop with popup"),
        _("The keybinding used to move focus backwards between panels "
          "and the desktop, using a popup window."),       NULL)
item (cycle,  "_group",            META_TAB_LIST_GROUP,    BINDING_REVERSES,
        _("Move between windows of an application immediately"),
        _("The keybinding used to move focus between windows of an "
          "application without a popup window."),          "<Alt>F6")
item (cycle,  "_group_backwards",  META_TAB_LIST_GROUP,    REVERSES_AND_REVERSED,
        _("Move backwards between windows of an application immediately"),
        _("The keybinding used to move focus backwards between windows "
          "of an application without a popup window."),    NULL)
item (cycle,  "_windows",          META_TAB_LIST_NORMAL,   BINDING_REVERSES,
        _("Move between windows immediately"),
        _("The keybinding used to move focus between windows without "
          "a popup window."),                              "<Alt>Escape")
item (cycle,  "_windows_backwards",META_TAB_LIST_NORMAL,   REVERSES_AND_REVERSED,
        _("Move backwards between windows immediately"),
        _("The keybinding used to move focus backwards between windows "
          "without a popup window."),                       NULL)
item (cycle,  "_panels",           META_TAB_LIST_DOCKS,    BINDING_REVERSES,
        _("Move between panels and the desktop immediately"),
        _("The keybinding used to move focus between panels and "
          "the desktop, without a popup window."),      "<Control><Alt>Escape")
item (cycle,  "_panels_backwards", META_TAB_LIST_DOCKS,    REVERSES_AND_REVERSED,
        _("Move backward between panels and the desktop immediately"),
        _("The keybinding used to move focus backwards between panels and "
          "the desktop, without a popup window."),         NULL)

/***********************************/
     
item (show_desktop, "", 0, 0,
      _("Hide all windows and focus desktop"),
      _("The keybinding used to hide all normal windows and set the "
        "focus to the desktop background."),
      "<Control><Alt>d")
item (panel, "_main_menu", META_KEYBINDING_ACTION_PANEL_MAIN_MENU, 0,
      _("Show the panel menu"),
      _("The keybinding which shows the panel's main menu."),
      "<Alt>F1")
item (panel, "_run_dialog", META_KEYBINDING_ACTION_PANEL_RUN_DIALOG, 0,
      _("Show the panel run application dialog"),
      _("The keybinding which display's the panel's \"Run Application\" "
        "dialog box."),
      "<Alt>F2")

/* Yes, the param is offset by one.  Historical reasons.  (Maybe worth fixing
 * at some point.)  The short and long are NULL here because the stanza is
 * irregularly shaped in metacity.schemas.in.  This will probably be fixed
 * as well.
 */
item (run_command, "_1",   0, 0, NULL, NULL, NULL)
item (run_command, "_2",   1, 0, NULL, NULL, NULL)
item (run_command, "_3",   2, 0, NULL, NULL, NULL)
item (run_command, "_4",   3, 0, NULL, NULL, NULL)
item (run_command, "_5",   4, 0, NULL, NULL, NULL)
item (run_command, "_6",   5, 0, NULL, NULL, NULL)
item (run_command, "_7",   6, 0, NULL, NULL, NULL)
item (run_command, "_8",   7, 0, NULL, NULL, NULL)
item (run_command, "_9",   8, 0, NULL, NULL, NULL)
item (run_command, "_10",  9, 0, NULL, NULL, NULL)
item (run_command, "_11", 10, 0, NULL, NULL, NULL)
item (run_command, "_12", 11, 0, NULL, NULL, NULL)
item (run_command, "_13", 12, 0, NULL, NULL, NULL)
item (run_command, "_14", 13, 0, NULL, NULL, NULL)
item (run_command, "_15", 14, 0, NULL, NULL, NULL)
item (run_command, "_16", 15, 0, NULL, NULL, NULL)
item (run_command, "_17", 16, 0, NULL, NULL, NULL)
item (run_command, "_18", 17, 0, NULL, NULL, NULL)
item (run_command, "_19", 18, 0, NULL, NULL, NULL)
item (run_command, "_20", 19, 0, NULL, NULL, NULL)
item (run_command, "_21", 20, 0, NULL, NULL, NULL)
item (run_command, "_22", 21, 0, NULL, NULL, NULL)
item (run_command, "_23", 22, 0, NULL, NULL, NULL)
item (run_command, "_24", 23, 0, NULL, NULL, NULL)
item (run_command, "_25", 24, 0, NULL, NULL, NULL)
item (run_command, "_26", 25, 0, NULL, NULL, NULL)
item (run_command, "_27", 26, 0, NULL, NULL, NULL)
item (run_command, "_28", 27, 0, NULL, NULL, NULL)
item (run_command, "_29", 28, 0, NULL, NULL, NULL)
item (run_command, "_30", 29, 0, NULL, NULL, NULL)
item (run_command, "_31", 30, 0, NULL, NULL, NULL)
item (run_command, "_32", 31, 0, NULL, NULL, NULL)

item (run_command, "_screenshot", 32, 0,
      _("Take a screenshot"),
      _("The keybinding which invokes the panel's screenshot utility."),
      "Print")
item (run_command, "_window_screenshot", 33, 0,
      _("Take a screenshot of a window"),
      _("The keybinding which invokes the panel's screenshot utility "
        "to take a screenshot of a window."),
      "<Alt>Print")

item (run_terminal, "", 0, 0,
      _("Run a terminal"),
      _("The keybinding which invokes a terminal."),
      NULL)

/* No descriptions because this is undocumented */
item (set_spew_mark, "", 0, 0, NULL, NULL, NULL)

#undef REVERSES_AND_REVERSED

/* eof screen-bindings.h */

