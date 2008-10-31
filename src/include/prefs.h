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
  META_PREF_ACTION_MIDDLE_CLICK_TITLEBAR,
  META_PREF_ACTION_RIGHT_CLICK_TITLEBAR,
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
  META_PREF_GNOME_ANIMATIONS,
  META_PREF_CURSOR_THEME,
  META_PREF_CURSOR_SIZE,
  META_PREF_COMPOSITING_MANAGER,
#ifdef WITH_CLUTTER
  META_PREF_CLUTTER_DISABLED,
  META_PREF_CLUTTER_PLUGINS,
#endif
  META_PREF_LIVE_HIDDEN_WINDOWS,
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
gboolean                    meta_prefs_get_gnome_animations   (void);

const char*                 meta_prefs_get_command            (int i);

char*                       meta_prefs_get_gconf_key_for_command (int i);

const char*                 meta_prefs_get_terminal_command   (void);
const char*                 meta_prefs_get_gconf_key_for_terminal_command (void);

void                        meta_prefs_get_button_layout (MetaButtonLayout *button_layout);

/* Double, right, middle click can be configured to any titlebar meta-action */
MetaActionTitlebar          meta_prefs_get_action_double_click_titlebar (void);
MetaActionTitlebar          meta_prefs_get_action_middle_click_titlebar (void);
MetaActionTitlebar          meta_prefs_get_action_right_click_titlebar (void);

void meta_prefs_set_num_workspaces (int n_workspaces);

const char* meta_prefs_get_workspace_name    (int         i);
void        meta_prefs_change_workspace_name (int         i,
                                              const char *name);

const char* meta_prefs_get_cursor_theme      (void);
int         meta_prefs_get_cursor_size       (void);
gboolean    meta_prefs_get_compositing_manager (void);

/**
 * Sets whether the compositor is turned on.
 *
 * \param whether   TRUE to turn on, FALSE to turn off
 */
void meta_prefs_set_compositing_manager (gboolean whether);

#ifdef WITH_CLUTTER

gboolean meta_prefs_get_clutter_disabled (void);
void     meta_prefs_set_clutter_disabled (gboolean whether);

GSList * meta_prefs_get_clutter_plugins (void);

/**
 * Sets the list of plugins persistently in GConf
 *
 * \param list   list of plugin:option pairs
 */
void meta_prefs_set_clutter_plugins (GSList *list);

/**
 * Sets the list of plugins temporarily for this process. The value
 * from GConf will be ignored.
 *
 * \param list   list of plugin:option pairs
 */
void meta_prefs_override_clutter_plugins (GSList *list);

#endif

gboolean meta_prefs_get_live_hidden_windows (void);
void     meta_prefs_set_live_hidden_windows (gboolean whether);


/* XXX FIXME This should be x-macroed, but isn't yet because it would be
 * difficult (or perhaps impossible) to add the suffixes using the current
 * system.  It needs some more thought, perhaps after the current system
 * evolves a little.
 */
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




