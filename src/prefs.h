/* Metacity preferences */

/* 
 * Copyright (C) 2001 Havoc Pennington
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
  META_PREF_FOCUS_MODE,
  META_PREF_THEME,
  META_PREF_TITLEBAR_FONT,
  META_PREF_TITLEBAR_FONT_SIZE,
  META_PREF_NUM_WORKSPACES,
  META_PREF_APPLICATION_BASED,
  META_PREF_WINDOW_KEYBINDINGS,
  META_PREF_SCREEN_KEYBINDINGS,
  META_PREF_DISABLE_WORKAROUNDS
} MetaPreference;

typedef void (* MetaPrefsChangedFunc) (MetaPreference pref,
                                       gpointer       data);

void meta_prefs_add_listener    (MetaPrefsChangedFunc func,
                                 gpointer             data);
void meta_prefs_remove_listener (MetaPrefsChangedFunc func,
                                 gpointer             data);

void meta_prefs_init (void);
const char* meta_preference_to_string (MetaPreference pref);

MetaFocusMode               meta_prefs_get_focus_mode         (void);
const char*                 meta_prefs_get_theme              (void);
/* returns NULL if GTK default should be used */
const PangoFontDescription* meta_prefs_get_titlebar_font      (void);
/* returns 0 if default should be used */
int                         meta_prefs_get_titlebar_font_size (void);
int                         meta_prefs_get_num_workspaces     (void);
gboolean                    meta_prefs_get_application_based  (void);
gboolean                    meta_prefs_get_disable_workarounds (void);

void meta_prefs_set_num_workspaces (int n_workspaces);

/* Screen bindings */
#define META_KEYBINDING_WORKSPACE_1          "switch_to_workspace_1"
#define META_KEYBINDING_WORKSPACE_2          "switch_to_workspace_2"
#define META_KEYBINDING_WORKSPACE_3          "switch_to_workspace_3"
#define META_KEYBINDING_WORKSPACE_4          "switch_to_workspace_4"
#define META_KEYBINDING_WORKSPACE_5          "switch_to_workspace_5"
#define META_KEYBINDING_WORKSPACE_6          "switch_to_workspace_6"
#define META_KEYBINDING_WORKSPACE_7          "switch_to_workspace_7"
#define META_KEYBINDING_WORKSPACE_8          "switch_to_workspace_8"
#define META_KEYBINDING_WORKSPACE_9          "switch_to_workspace_9"
#define META_KEYBINDING_WORKSPACE_10         "switch_to_workspace_10"
#define META_KEYBINDING_WORKSPACE_11         "switch_to_workspace_11"
#define META_KEYBINDING_WORKSPACE_12         "switch_to_workspace_12"
#define META_KEYBINDING_WORKSPACE_LEFT       "switch_to_workspace_left"
#define META_KEYBINDING_WORKSPACE_RIGHT      "switch_to_workspace_right"
#define META_KEYBINDING_WORKSPACE_UP         "switch_to_workspace_up"
#define META_KEYBINDING_WORKSPACE_DOWN       "switch_to_workspace_down"
#define META_KEYBINDING_SWITCH_WINDOWS       "switch_windows"
#define META_KEYBINDING_SWITCH_PANELS        "switch_panels"
#define META_KEYBINDING_FOCUS_PREVIOUS       "focus_previous_window"
#define META_KEYBINDING_SHOW_DESKTOP         "show_desktop"

/* Window bindings */
#define META_KEYBINDING_WINDOW_MENU          "activate_window_menu"
#define META_KEYBINDING_TOGGLE_FULLSCREEN    "toggle_fullscreen"
#define META_KEYBINDING_TOGGLE_MAXIMIZE      "toggle_maximized"
#define META_KEYBINDING_TOGGLE_SHADE         "toggle_shaded"
#define META_KEYBINDING_CLOSE                "close"
#define META_KEYBINDING_BEGIN_MOVE           "begin_move"
#define META_KEYBINDING_BEGIN_RESIZE         "begin_resize"
#define META_KEYBINDING_TOGGLE_STICKY        "toggle_on_all_workspaces"
#define META_KEYBINDING_MOVE_WORKSPACE_1     "move_to_workspace_1"
#define META_KEYBINDING_MOVE_WORKSPACE_2     "move_to_workspace_2"
#define META_KEYBINDING_MOVE_WORKSPACE_3     "move_to_workspace_3"
#define META_KEYBINDING_MOVE_WORKSPACE_4     "move_to_workspace_4"
#define META_KEYBINDING_MOVE_WORKSPACE_5     "move_to_workspace_5"
#define META_KEYBINDING_MOVE_WORKSPACE_6     "move_to_workspace_6"
#define META_KEYBINDING_MOVE_WORKSPACE_7     "move_to_workspace_7"
#define META_KEYBINDING_MOVE_WORKSPACE_8     "move_to_workspace_8"
#define META_KEYBINDING_MOVE_WORKSPACE_9     "move_to_workspace_9"
#define META_KEYBINDING_MOVE_WORKSPACE_10    "move_to_workspace_10"
#define META_KEYBINDING_MOVE_WORKSPACE_11    "move_to_workspace_11"
#define META_KEYBINDING_MOVE_WORKSPACE_12    "move_to_workspace_12"
#define META_KEYBINDING_MOVE_WORKSPACE_LEFT  "move_to_workspace_left"
#define META_KEYBINDING_MOVE_WORKSPACE_RIGHT "move_to_workspace_right"
#define META_KEYBINDING_MOVE_WORKSPACE_UP    "move_to_workspace_up"
#define META_KEYBINDING_MOVE_WORKSPACE_DOWN  "move_to_workspace_down"

typedef struct
{
  const char   *name;
  unsigned int  keysym;
  unsigned long mask;
} MetaKeyPref;

void meta_prefs_get_screen_bindings (const MetaKeyPref **bindings,
                                     int                *n_bindings);
void meta_prefs_get_window_bindings (const MetaKeyPref **bindings,
                                     int                *n_bindings);

#endif




