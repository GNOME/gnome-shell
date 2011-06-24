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
 * The arguments to keybind() are:
 *   1) the name of the binding; a bareword identifier
 *              (it's fine if it happens to clash with a C reserved word)
 *   2) the name of the function which implements it.
 *              Clearly we could have guessed this from the binding very often,
 *              but we choose to write it in full for the benefit of grep.
 *   3) an integer parameter to pass to the handler
 *   4) a set of boolean flags, ORed together:
 *       BINDING_PER_WINDOW  - this is a window-based binding.
 *                             It is only valid if there is a
 *                             current window, and will operate in
 *                             some way on that window.
 *       BINDING_REVERSES    - the binding can reverse if you hold down Shift
 *       BINDING_IS_REVERSED - the same, but the senses are reversed from the
 *                             handler's point of view (let me know if I should
 *                             explain this better)
 *      or 0 if no flag applies.
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

keybind (switch-to-workspace-1,  handle_switch_to_workspace, 0, 0)
keybind (switch-to-workspace-2,  handle_switch_to_workspace, 1, 0)
keybind (switch-to-workspace-3,  handle_switch_to_workspace, 2, 0)
keybind (switch-to-workspace-4,  handle_switch_to_workspace, 3, 0)
keybind (switch-to-workspace-5,  handle_switch_to_workspace, 4, 0)
keybind (switch-to-workspace-6,  handle_switch_to_workspace, 5, 0)
keybind (switch-to-workspace-7,  handle_switch_to_workspace, 6, 0)
keybind (switch-to-workspace-8,  handle_switch_to_workspace, 7, 0)
keybind (switch-to-workspace-9,  handle_switch_to_workspace, 8, 0)
keybind (switch-to-workspace-10, handle_switch_to_workspace, 9, 0)
keybind (switch-to-workspace-11, handle_switch_to_workspace, 10, 0)
keybind (switch-to-workspace-12, handle_switch_to_workspace, 11, 0)

/* META_MOTION_* are negative, and so distinct from workspace numbers,
 * which are always zero or positive.
 * If you make use of these constants, you will need to include workspace.h
 * (which you're probably using already for other reasons anyway).
 * If your definition of keybind() throws them away, you don't need to include
 * workspace.h, of course.
 */

keybind (switch-to-workspace-left, handle_switch_to_workspace,
         META_MOTION_LEFT, 0)

keybind (switch-to-workspace-right, handle_switch_to_workspace,
         META_MOTION_RIGHT, 0)

keybind (switch-to-workspace-up, handle_switch_to_workspace,
         META_MOTION_UP, 0)

keybind (switch-to-workspace-down, handle_switch_to_workspace,
         META_MOTION_DOWN, 0)

/***********************************/

/* The ones which have inverses.  These can't be bound to any keystroke
 * containing Shift because Shift will invert their "backward" state.
 *
 * TODO: "NORMAL" and "DOCKS" should be renamed to the same name as their
 * action, for obviousness.
 *
 * TODO: handle_switch and handle_cycle should probably really be the
 * same function checking a bit in the parameter for difference.
 */

keybind (switch-group, handle_switch, META_TAB_LIST_GROUP, BINDING_REVERSES)
keybind (switch-group-backward, handle_switch,
         META_TAB_LIST_GROUP, REVERSES_AND_REVERSED)
keybind (switch-windows, handle_switch, META_TAB_LIST_NORMAL, BINDING_REVERSES)
keybind (switch-windows-backward, handle_switch, META_TAB_LIST_NORMAL,
         REVERSES_AND_REVERSED)
keybind (switch-panels, handle_switch, META_TAB_LIST_DOCKS, BINDING_REVERSES)
keybind (switch-panels-backward, handle_switch, META_TAB_LIST_DOCKS,
         REVERSES_AND_REVERSED)

keybind (cycle-group, handle_cycle, META_TAB_LIST_GROUP, BINDING_REVERSES)
keybind (cycle-group-backward, handle_cycle, META_TAB_LIST_GROUP,
        REVERSES_AND_REVERSED)
keybind (cycle-windows, handle_cycle, META_TAB_LIST_NORMAL, BINDING_REVERSES)
keybind (cycle-windows-backward, handle_cycle, META_TAB_LIST_NORMAL,
        REVERSES_AND_REVERSED)
keybind (cycle-panels, handle_cycle, META_TAB_LIST_DOCKS, BINDING_REVERSES)
keybind (cycle-panels-backward, handle_cycle, META_TAB_LIST_DOCKS,
        REVERSES_AND_REVERSED)

/***********************************/

/* These two are special pseudo-bindings that are provided for allowing
 * custom handlers, but will never be bound to a key. While a tab
 * grab is in effect, they are invoked for releasing the primary modifier
 * or pressing some unbound key, respectively.
 */
keybind (tab-popup-select, handle_tab_popup_select, 0, 0)
keybind (tab-popup-cancel, handle_tab_popup_cancel, 0, 0)

/***********************************/
     
keybind (show-desktop, handle_show_desktop, 0, 0)
keybind (panel-main-menu, handle_panel,
       META_KEYBINDING_ACTION_PANEL_MAIN_MENU, 0)
keybind (panel-run-dialog, handle_panel,
       META_KEYBINDING_ACTION_PANEL_RUN_DIALOG, 0)
keybind (toggle-recording, handle_toggle_recording, 0, 0)

/* FIXME: No description because this is undocumented */
keybind (set-spew-mark, handle_set_spew_mark, 0, 0)

#undef REVERSES_AND_REVERSED

/************************ PER WINDOW BINDINGS ************************/

/* These take a window as an extra parameter; they have no effect
 * if no window is active.
 */
 
keybind (activate-window-menu, handle_activate_window_menu, 0,
        BINDING_PER_WINDOW)
keybind (toggle-fullscreen, handle_toggle_fullscreen, 0, BINDING_PER_WINDOW)
keybind (toggle-maximized, handle_toggle_maximized, 0, BINDING_PER_WINDOW)
keybind (toggle-above, handle_toggle_above, 0, BINDING_PER_WINDOW)
keybind (maximize, handle_maximize, 0, BINDING_PER_WINDOW)
keybind (unmaximize, handle_unmaximize, 0, BINDING_PER_WINDOW)
keybind (toggle-shaded, handle_toggle_shaded, 0, BINDING_PER_WINDOW)
keybind (minimize, handle_minimize, 0, BINDING_PER_WINDOW)
keybind (close, handle_close, 0, BINDING_PER_WINDOW)
keybind (begin-move, handle_begin_move, 0, BINDING_PER_WINDOW)
keybind (begin-resize, handle_begin_resize, 0, BINDING_PER_WINDOW)
keybind (toggle-on-all-workspaces, handle_toggle_on_all_workspaces, 0,
         BINDING_PER_WINDOW)

keybind (move-to-workspace-1, handle_move_to_workspace, 0, BINDING_PER_WINDOW)
keybind (move-to-workspace-2, handle_move_to_workspace, 1, BINDING_PER_WINDOW)
keybind (move-to-workspace-3, handle_move_to_workspace, 2, BINDING_PER_WINDOW)
keybind (move-to-workspace-4, handle_move_to_workspace, 3, BINDING_PER_WINDOW)
keybind (move-to-workspace-5, handle_move_to_workspace, 4, BINDING_PER_WINDOW)
keybind (move-to-workspace-6, handle_move_to_workspace, 5, BINDING_PER_WINDOW)
keybind (move-to-workspace-7, handle_move_to_workspace, 6, BINDING_PER_WINDOW)
keybind (move-to-workspace-8, handle_move_to_workspace, 7, BINDING_PER_WINDOW)
keybind (move-to-workspace-9, handle_move_to_workspace, 8, BINDING_PER_WINDOW)
keybind (move-to-workspace-10, handle_move_to_workspace, 9, BINDING_PER_WINDOW)
keybind (move-to-workspace-11, handle_move_to_workspace, 10, BINDING_PER_WINDOW)
keybind (move-to-workspace-12, handle_move_to_workspace, 11, BINDING_PER_WINDOW)


/* META_MOTION_* are negative, and so distinct from workspace numbers,
 * which are always zero or positive.
 * If you make use of these constants, you will need to include workspace.h
 * (which you're probably using already for other reasons anyway).
 * If your definition of keybind() throws them away, you don't need to include
 * workspace.h, of course.
 */

keybind (move-to-workspace-left, handle_move_to_workspace,
         META_MOTION_LEFT, BINDING_PER_WINDOW)
keybind (move-to-workspace-right, handle_move_to_workspace,
         META_MOTION_RIGHT, BINDING_PER_WINDOW)
keybind (move-to-workspace-up, handle_move_to_workspace,
         META_MOTION_UP, BINDING_PER_WINDOW)
keybind (move-to-workspace-down, handle_move_to_workspace,
         META_MOTION_DOWN, BINDING_PER_WINDOW)

keybind (raise-or-lower, handle_raise_or_lower, 0, BINDING_PER_WINDOW)
keybind (raise, handle_raise, 0, BINDING_PER_WINDOW)
keybind (lower, handle_lower, 0, BINDING_PER_WINDOW)

keybind (maximize-vertically, handle_maximize_vertically, 0, BINDING_PER_WINDOW)

keybind (maximize-horizontally, handle_maximize_horizontally, 0,
        BINDING_PER_WINDOW)

keybind (move-to-corner-nw, handle_move_to_corner_nw, 0, BINDING_PER_WINDOW)
keybind (move-to-corner-ne, handle_move_to_corner_ne, 0, BINDING_PER_WINDOW)
keybind (move-to-corner-sw, handle_move_to_corner_sw, 0, BINDING_PER_WINDOW)
keybind (move-to-corner-se, handle_move_to_corner_se, 0, BINDING_PER_WINDOW)

keybind (move-to-side-n, handle_move_to_side_n, 0, BINDING_PER_WINDOW)
keybind (move-to-side-s, handle_move_to_side_s, 0, BINDING_PER_WINDOW)
keybind (move-to-side-e, handle_move_to_side_e, 0, BINDING_PER_WINDOW)
keybind (move-to-side-w, handle_move_to_side_w, 0, BINDING_PER_WINDOW)
keybind (move-to-center, handle_move_to_center, 0, BINDING_PER_WINDOW)

/* eof all-keybindings.h */

