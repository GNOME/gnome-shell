/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity preferences */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2006 Elijah Newren
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

#ifndef META_PREFS_H
#define META_PREFS_H

/* This header is a "common" one between the UI and core side */
#include "common.h"
#include <pango/pango-font.h>

typedef enum
{
  META_PREF_MOUSE_BUTTON_MODS,
  META_PREF_FOCUS_MODE,
  META_PREF_FOCUS_NEW_WINDOWS,
  META_PREF_RAISE_ON_CLICK,
  META_PREF_ACTION_DOUBLE_CLICK_TITLEBAR,
  META_PREF_AUTO_RAISE,
  META_PREF_AUTO_RAISE_DELAY,
  META_PREF_THEME,
  META_PREF_TITLEBAR_FONT,
  META_PREF_NUM_WORKSPACES,
  META_PREF_APPLICATION_BASED,
  META_PREF_WINDOW_KEYBINDINGS,
  META_PREF_SCREEN_KEYBINDINGS,
  META_PREF_DISABLE_WORKAROUNDS,
  META_PREF_COMMANDS,
  META_PREF_TERMINAL_COMMAND,
  META_PREF_BUTTON_LAYOUT,
  META_PREF_WORKSPACE_NAMES,
  META_PREF_VISUAL_BELL,
  META_PREF_AUDIBLE_BELL,
  META_PREF_VISUAL_BELL_TYPE,
  META_PREF_REDUCED_RESOURCES,
  META_PREF_GNOME_ACCESSIBILITY,
  META_PREF_CURSOR_THEME,
  META_PREF_CURSOR_SIZE,
  META_PREF_COMPOSITING_MANAGER
} MetaPreference;

typedef void (* MetaPrefsChangedFunc) (MetaPreference pref,
                                       gpointer       data);

void meta_prefs_add_listener    (MetaPrefsChangedFunc func,
                                 gpointer             data);
void meta_prefs_remove_listener (MetaPrefsChangedFunc func,
                                 gpointer             data);

void meta_prefs_init (void);
const char* meta_preference_to_string (MetaPreference pref);

MetaVirtualModifier         meta_prefs_get_mouse_button_mods  (void);
MetaFocusMode               meta_prefs_get_focus_mode         (void);
MetaFocusNewWindows         meta_prefs_get_focus_new_windows  (void);
gboolean                    meta_prefs_get_raise_on_click     (void);
const char*                 meta_prefs_get_theme              (void);
/* returns NULL if GTK default should be used */
const PangoFontDescription* meta_prefs_get_titlebar_font      (void);
int                         meta_prefs_get_num_workspaces     (void);
gboolean                    meta_prefs_get_application_based  (void);
gboolean                    meta_prefs_get_disable_workarounds (void);
gboolean                    meta_prefs_get_auto_raise         (void);
int                         meta_prefs_get_auto_raise_delay   (void);
gboolean                    meta_prefs_get_reduced_resources  (void);
gboolean                    meta_prefs_get_gnome_accessibility (void);

const char*                 meta_prefs_get_command            (int i);

char*                       meta_prefs_get_gconf_key_for_command (int i);

const char*                 meta_prefs_get_terminal_command   (void);
const char*                 meta_prefs_get_gconf_key_for_terminal_command (void);

void                        meta_prefs_get_button_layout (MetaButtonLayout *button_layout);
MetaActionDoubleClickTitlebar meta_prefs_get_action_double_click_titlebar (void);

void meta_prefs_set_num_workspaces (int n_workspaces);

const char* meta_prefs_get_workspace_name    (int         i);
void        meta_prefs_change_workspace_name (int         i,
                                              const char *name);

const char* meta_prefs_get_cursor_theme      (void);
int         meta_prefs_get_cursor_size       (void);
gboolean    meta_prefs_get_compositing_manager (void);

/* Screen bindings */
#define META_KEYBINDING_WORKSPACE_1              "switch_to_workspace_1"
#define META_KEYBINDING_WORKSPACE_2              "switch_to_workspace_2"
#define META_KEYBINDING_WORKSPACE_3              "switch_to_workspace_3"
#define META_KEYBINDING_WORKSPACE_4              "switch_to_workspace_4"
#define META_KEYBINDING_WORKSPACE_5              "switch_to_workspace_5"
#define META_KEYBINDING_WORKSPACE_6              "switch_to_workspace_6"
#define META_KEYBINDING_WORKSPACE_7              "switch_to_workspace_7"
#define META_KEYBINDING_WORKSPACE_8              "switch_to_workspace_8"
#define META_KEYBINDING_WORKSPACE_9              "switch_to_workspace_9"
#define META_KEYBINDING_WORKSPACE_10             "switch_to_workspace_10"
#define META_KEYBINDING_WORKSPACE_11             "switch_to_workspace_11"
#define META_KEYBINDING_WORKSPACE_12             "switch_to_workspace_12"
#define META_KEYBINDING_WORKSPACE_LEFT           "switch_to_workspace_left"
#define META_KEYBINDING_WORKSPACE_RIGHT          "switch_to_workspace_right"
#define META_KEYBINDING_WORKSPACE_UP             "switch_to_workspace_up"
#define META_KEYBINDING_WORKSPACE_DOWN           "switch_to_workspace_down"
#define META_KEYBINDING_SWITCH_GROUP             "switch_group"
#define META_KEYBINDING_SWITCH_GROUP_BACKWARD    "switch_group_backward"
#define META_KEYBINDING_SWITCH_WINDOWS           "switch_windows"
#define META_KEYBINDING_SWITCH_WINDOWS_BACKWARD  "switch_windows_backward"
#define META_KEYBINDING_SWITCH_PANELS            "switch_panels"
#define META_KEYBINDING_SWITCH_PANELS_BACKWARD   "switch_panels_backward"
#define META_KEYBINDING_CYCLE_GROUP              "cycle_group"
#define META_KEYBINDING_CYCLE_GROUP_BACKWARD     "cycle_group_backward"
#define META_KEYBINDING_CYCLE_WINDOWS            "cycle_windows"
#define META_KEYBINDING_CYCLE_WINDOWS_BACKWARD   "cycle_windows_backward"
#define META_KEYBINDING_CYCLE_PANELS             "cycle_panels"
#define META_KEYBINDING_CYCLE_PANELS_BACKWARD    "cycle_panels_backward"
#define META_KEYBINDING_SHOW_DESKTOP             "show_desktop"
#define META_KEYBINDING_PANEL_MAIN_MENU          "panel_main_menu"
#define META_KEYBINDING_PANEL_RUN_DIALOG         "panel_run_dialog"
#define META_KEYBINDING_COMMAND_1                "run_command_1"
#define META_KEYBINDING_COMMAND_2                "run_command_2"
#define META_KEYBINDING_COMMAND_3                "run_command_3"
#define META_KEYBINDING_COMMAND_4                "run_command_4"
#define META_KEYBINDING_COMMAND_5                "run_command_5"
#define META_KEYBINDING_COMMAND_6                "run_command_6"
#define META_KEYBINDING_COMMAND_7                "run_command_7"
#define META_KEYBINDING_COMMAND_8                "run_command_8"
#define META_KEYBINDING_COMMAND_9                "run_command_9"
#define META_KEYBINDING_COMMAND_10               "run_command_10"
#define META_KEYBINDING_COMMAND_11               "run_command_11"
#define META_KEYBINDING_COMMAND_12               "run_command_12"
#define META_KEYBINDING_COMMAND_13               "run_command_13"
#define META_KEYBINDING_COMMAND_14               "run_command_14"
#define META_KEYBINDING_COMMAND_15               "run_command_15"
#define META_KEYBINDING_COMMAND_16               "run_command_16"
#define META_KEYBINDING_COMMAND_17               "run_command_17"
#define META_KEYBINDING_COMMAND_18               "run_command_18"
#define META_KEYBINDING_COMMAND_19               "run_command_19"
#define META_KEYBINDING_COMMAND_20               "run_command_20"
#define META_KEYBINDING_COMMAND_21               "run_command_21"
#define META_KEYBINDING_COMMAND_22               "run_command_22"
#define META_KEYBINDING_COMMAND_23               "run_command_23"
#define META_KEYBINDING_COMMAND_24               "run_command_24"
#define META_KEYBINDING_COMMAND_25               "run_command_25"
#define META_KEYBINDING_COMMAND_26               "run_command_26"
#define META_KEYBINDING_COMMAND_27               "run_command_27"
#define META_KEYBINDING_COMMAND_28               "run_command_28"
#define META_KEYBINDING_COMMAND_29               "run_command_29"
#define META_KEYBINDING_COMMAND_30               "run_command_30"
#define META_KEYBINDING_COMMAND_31               "run_command_31"
#define META_KEYBINDING_COMMAND_32               "run_command_32"
#define META_KEYBINDING_COMMAND_SCREENSHOT       "run_command_screenshot"
#define META_KEYBINDING_COMMAND_WIN_SCREENSHOT   "run_command_window_screenshot"
#define META_KEYBINDING_RUN_COMMAND_TERMINAL     "run_command_terminal"

/* Window bindings */
#define META_KEYBINDING_WINDOW_MENU              "activate_window_menu"
#define META_KEYBINDING_TOGGLE_FULLSCREEN        "toggle_fullscreen"
#define META_KEYBINDING_TOGGLE_MAXIMIZE          "toggle_maximized"
#define META_KEYBINDING_TOGGLE_ABOVE             "toggle_above"
#define META_KEYBINDING_MAXIMIZE                 "maximize"
#define META_KEYBINDING_UNMAXIMIZE               "unmaximize"
#define META_KEYBINDING_TOGGLE_SHADE             "toggle_shaded"
#define META_KEYBINDING_MINIMIZE                 "minimize"
#define META_KEYBINDING_CLOSE                    "close"
#define META_KEYBINDING_BEGIN_MOVE               "begin_move"
#define META_KEYBINDING_BEGIN_RESIZE             "begin_resize"
#define META_KEYBINDING_TOGGLE_STICKY            "toggle_on_all_workspaces"
#define META_KEYBINDING_MOVE_WORKSPACE_1         "move_to_workspace_1"
#define META_KEYBINDING_MOVE_WORKSPACE_2         "move_to_workspace_2"
#define META_KEYBINDING_MOVE_WORKSPACE_3         "move_to_workspace_3"
#define META_KEYBINDING_MOVE_WORKSPACE_4         "move_to_workspace_4"
#define META_KEYBINDING_MOVE_WORKSPACE_5         "move_to_workspace_5"
#define META_KEYBINDING_MOVE_WORKSPACE_6         "move_to_workspace_6"
#define META_KEYBINDING_MOVE_WORKSPACE_7         "move_to_workspace_7"
#define META_KEYBINDING_MOVE_WORKSPACE_8         "move_to_workspace_8"
#define META_KEYBINDING_MOVE_WORKSPACE_9         "move_to_workspace_9"
#define META_KEYBINDING_MOVE_WORKSPACE_10        "move_to_workspace_10"
#define META_KEYBINDING_MOVE_WORKSPACE_11        "move_to_workspace_11"
#define META_KEYBINDING_MOVE_WORKSPACE_12        "move_to_workspace_12"
#define META_KEYBINDING_MOVE_WORKSPACE_LEFT      "move_to_workspace_left"
#define META_KEYBINDING_MOVE_WORKSPACE_RIGHT     "move_to_workspace_right"
#define META_KEYBINDING_MOVE_WORKSPACE_UP        "move_to_workspace_up"
#define META_KEYBINDING_MOVE_WORKSPACE_DOWN      "move_to_workspace_down"
#define META_KEYBINDING_RAISE_OR_LOWER           "raise_or_lower"
#define META_KEYBINDING_RAISE                    "raise"
#define META_KEYBINDING_LOWER                    "lower"
#define META_KEYBINDING_MAXIMIZE_VERTICALLY      "maximize_vertically"
#define META_KEYBINDING_MAXIMIZE_HORIZONTALLY    "maximize_horizontally"
#define META_KEYBINDING_MOVE_TO_CORNER_NW        "move_to_corner_nw"
#define META_KEYBINDING_MOVE_TO_CORNER_NE        "move_to_corner_ne"
#define META_KEYBINDING_MOVE_TO_CORNER_SW        "move_to_corner_sw"
#define META_KEYBINDING_MOVE_TO_CORNER_SE        "move_to_corner_se"
#define META_KEYBINDING_MOVE_TO_SIDE_N           "move_to_side_n"
#define META_KEYBINDING_MOVE_TO_SIDE_S           "move_to_side_s"
#define META_KEYBINDING_MOVE_TO_SIDE_E           "move_to_side_e"
#define META_KEYBINDING_MOVE_TO_SIDE_W           "move_to_side_w"

typedef enum _MetaKeyBindingAction
{
  META_KEYBINDING_ACTION_NONE = -1,
  META_KEYBINDING_ACTION_WORKSPACE_1,
  META_KEYBINDING_ACTION_WORKSPACE_2,
  META_KEYBINDING_ACTION_WORKSPACE_3,
  META_KEYBINDING_ACTION_WORKSPACE_4,
  META_KEYBINDING_ACTION_WORKSPACE_5,
  META_KEYBINDING_ACTION_WORKSPACE_6,
  META_KEYBINDING_ACTION_WORKSPACE_7,
  META_KEYBINDING_ACTION_WORKSPACE_8,
  META_KEYBINDING_ACTION_WORKSPACE_9,
  META_KEYBINDING_ACTION_WORKSPACE_10,
  META_KEYBINDING_ACTION_WORKSPACE_11,
  META_KEYBINDING_ACTION_WORKSPACE_12,
  META_KEYBINDING_ACTION_WORKSPACE_LEFT,
  META_KEYBINDING_ACTION_WORKSPACE_RIGHT,
  META_KEYBINDING_ACTION_WORKSPACE_UP,
  META_KEYBINDING_ACTION_WORKSPACE_DOWN,
  META_KEYBINDING_ACTION_SWITCH_GROUP,
  META_KEYBINDING_ACTION_SWITCH_GROUP_BACKWARD,
  META_KEYBINDING_ACTION_SWITCH_WINDOWS,
  META_KEYBINDING_ACTION_SWITCH_WINDOWS_BACKWARD,
  META_KEYBINDING_ACTION_SWITCH_PANELS,
  META_KEYBINDING_ACTION_SWITCH_PANELS_BACKWARD,
  META_KEYBINDING_ACTION_CYCLE_GROUP,
  META_KEYBINDING_ACTION_CYCLE_GROUP_BACKWARD,
  META_KEYBINDING_ACTION_CYCLE_WINDOWS,
  META_KEYBINDING_ACTION_CYCLE_WINDOWS_BACKWARD,
  META_KEYBINDING_ACTION_CYCLE_PANELS,
  META_KEYBINDING_ACTION_CYCLE_PANELS_BACKWARD,
  META_KEYBINDING_ACTION_SHOW_DESKTOP,
  META_KEYBINDING_ACTION_PANEL_MAIN_MENU,
  META_KEYBINDING_ACTION_PANEL_RUN_DIALOG,
  META_KEYBINDING_ACTION_COMMAND_1,
  META_KEYBINDING_ACTION_COMMAND_2,
  META_KEYBINDING_ACTION_COMMAND_3,
  META_KEYBINDING_ACTION_COMMAND_4,
  META_KEYBINDING_ACTION_COMMAND_5,
  META_KEYBINDING_ACTION_COMMAND_6,
  META_KEYBINDING_ACTION_COMMAND_7,
  META_KEYBINDING_ACTION_COMMAND_8,
  META_KEYBINDING_ACTION_COMMAND_9,
  META_KEYBINDING_ACTION_COMMAND_10,
  META_KEYBINDING_ACTION_COMMAND_11,
  META_KEYBINDING_ACTION_COMMAND_12
} MetaKeyBindingAction;

typedef struct
{
  unsigned int keysym;
  unsigned int keycode;
  MetaVirtualModifier modifiers;
} MetaKeyCombo;

typedef struct
{
  const char   *name;
  /* a list of MetaKeyCombos. Each of them is bound to
   * this keypref. If one has keysym==modifiers==0, it is
   * ignored. For historical reasons, the first entry is
   * governed by the pref FOO and the remainder are
   * governed by the pref FOO_list.
   */
  GSList *bindings;

  /* for keybindings that can have shift or not like Alt+Tab */
  gboolean      add_shift;
} MetaKeyPref;

void meta_prefs_get_screen_bindings (const MetaKeyPref **bindings,
                                     int                *n_bindings);
void meta_prefs_get_window_bindings (const MetaKeyPref **bindings,
                                     int                *n_bindings);

MetaKeyBindingAction meta_prefs_get_keybinding_action (const char *name);

void meta_prefs_get_window_binding (const char          *name,
                                    unsigned int        *keysym,
                                    MetaVirtualModifier *modifiers);

typedef enum
{
  META_VISUAL_BELL_INVALID = 0,
  META_VISUAL_BELL_FULLSCREEN_FLASH,
  META_VISUAL_BELL_FRAME_FLASH

} MetaVisualBellType;

gboolean           meta_prefs_get_visual_bell      (void);
gboolean           meta_prefs_bell_is_audible      (void);
MetaVisualBellType meta_prefs_get_visual_bell_type (void);

#endif




