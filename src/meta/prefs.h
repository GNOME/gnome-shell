/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter preferences */

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
#include <meta/common.h>
#include <meta/types.h>
#include <pango/pango-font.h>
#include <gdesktop-enums.h>
#include <gio/gio.h>

/**
 * MetaPreference:
 * @META_PREF_MOUSE_BUTTON_MODS: mouse button modifiers
 * @META_PREF_FOCUS_MODE: focus mode
 * @META_PREF_FOCUS_NEW_WINDOWS: focus new windows
 * @META_PREF_ATTACH_MODAL_DIALOGS: attach modal dialogs
 * @META_PREF_RAISE_ON_CLICK: raise on click
 * @META_PREF_ACTION_DOUBLE_CLICK_TITLEBAR: action double click titlebar
 * @META_PREF_ACTION_MIDDLE_CLICK_TITLEBAR: action middle click titlebar
 * @META_PREF_ACTION_RIGHT_CLICK_TITLEBAR: action right click titlebar
 * @META_PREF_AUTO_RAISE: auto-raise
 * @META_PREF_AUTO_RAISE_DELAY: auto-raise delay
 * @META_PREF_FOCUS_CHANGE_ON_POINTER_REST: focus change on pointer rest
 * @META_PREF_THEME: theme
 * @META_PREF_TITLEBAR_FONT: title-bar font
 * @META_PREF_NUM_WORKSPACES: number of workspaces
 * @META_PREF_DYNAMIC_WORKSPACES: dynamic workspaces
 * @META_PREF_KEYBINDINGS: keybindings
 * @META_PREF_DISABLE_WORKAROUNDS: disable workarounds
 * @META_PREF_BUTTON_LAYOUT: button layout
 * @META_PREF_WORKSPACE_NAMES: workspace names
 * @META_PREF_VISUAL_BELL: visual bell
 * @META_PREF_AUDIBLE_BELL: audible bell
 * @META_PREF_VISUAL_BELL_TYPE: visual bell type
 * @META_PREF_GNOME_ACCESSIBILITY: GNOME accessibility
 * @META_PREF_GNOME_ANIMATIONS: GNOME animations
 * @META_PREF_CURSOR_THEME: cursor theme
 * @META_PREF_CURSOR_SIZE: cursor size
 * @META_PREF_RESIZE_WITH_RIGHT_BUTTON: resize with right button
 * @META_PREF_EDGE_TILING: edge tiling
 * @META_PREF_FORCE_FULLSCREEN: force fullscreen
 * @META_PREF_WORKSPACES_ONLY_ON_PRIMARY: workspaces only on primary
 * @META_PREF_NO_TAB_POPUP: no tab popup
 * @META_PREF_DRAGGABLE_BORDER_WIDTH: draggable border width
 * @META_PREF_AUTO_MAXIMIZE: auto-maximize
 */

/* Keep in sync with GSettings schemas! */
typedef enum
{
  META_PREF_MOUSE_BUTTON_MODS,
  META_PREF_FOCUS_MODE,
  META_PREF_FOCUS_NEW_WINDOWS,
  META_PREF_ATTACH_MODAL_DIALOGS,
  META_PREF_RAISE_ON_CLICK,
  META_PREF_ACTION_DOUBLE_CLICK_TITLEBAR,
  META_PREF_ACTION_MIDDLE_CLICK_TITLEBAR,
  META_PREF_ACTION_RIGHT_CLICK_TITLEBAR,
  META_PREF_AUTO_RAISE,
  META_PREF_AUTO_RAISE_DELAY,
  META_PREF_FOCUS_CHANGE_ON_POINTER_REST,
  META_PREF_THEME,
  META_PREF_TITLEBAR_FONT,
  META_PREF_NUM_WORKSPACES,
  META_PREF_DYNAMIC_WORKSPACES,
  META_PREF_KEYBINDINGS,
  META_PREF_DISABLE_WORKAROUNDS,
  META_PREF_BUTTON_LAYOUT,
  META_PREF_WORKSPACE_NAMES,
  META_PREF_VISUAL_BELL,
  META_PREF_AUDIBLE_BELL,
  META_PREF_VISUAL_BELL_TYPE,
  META_PREF_GNOME_ACCESSIBILITY,
  META_PREF_GNOME_ANIMATIONS,
  META_PREF_CURSOR_THEME,
  META_PREF_CURSOR_SIZE,
  META_PREF_RESIZE_WITH_RIGHT_BUTTON,
  META_PREF_EDGE_TILING,
  META_PREF_FORCE_FULLSCREEN,
  META_PREF_WORKSPACES_ONLY_ON_PRIMARY,
  META_PREF_NO_TAB_POPUP,
  META_PREF_DRAGGABLE_BORDER_WIDTH,
  META_PREF_AUTO_MAXIMIZE
} MetaPreference;

typedef void (* MetaPrefsChangedFunc) (MetaPreference pref,
                                       gpointer       user_data);

void meta_prefs_add_listener    (MetaPrefsChangedFunc func,
                                 gpointer             user_data);
void meta_prefs_remove_listener (MetaPrefsChangedFunc func,
                                 gpointer             user_data);

void meta_prefs_init (void);

void meta_prefs_override_preference_schema (const char *key,
                                            const char *schema);

const char* meta_preference_to_string (MetaPreference pref);

MetaVirtualModifier         meta_prefs_get_mouse_button_mods  (void);
gint                        meta_prefs_get_mouse_button_resize (void);
gint                        meta_prefs_get_mouse_button_menu  (void);
GDesktopFocusMode           meta_prefs_get_focus_mode         (void);
GDesktopFocusNewWindows     meta_prefs_get_focus_new_windows  (void);
gboolean                    meta_prefs_get_attach_modal_dialogs (void);
gboolean                    meta_prefs_get_raise_on_click     (void);
const char*                 meta_prefs_get_theme              (void);
/* returns NULL if GTK default should be used */
const PangoFontDescription* meta_prefs_get_titlebar_font      (void);
int                         meta_prefs_get_num_workspaces     (void);
gboolean                    meta_prefs_get_dynamic_workspaces (void);
gboolean                    meta_prefs_get_disable_workarounds (void);
gboolean                    meta_prefs_get_auto_raise         (void);
int                         meta_prefs_get_auto_raise_delay   (void);
gboolean                    meta_prefs_get_focus_change_on_pointer_rest (void);
gboolean                    meta_prefs_get_gnome_accessibility (void);
gboolean                    meta_prefs_get_gnome_animations   (void);
gboolean                    meta_prefs_get_edge_tiling        (void);
gboolean                    meta_prefs_get_auto_maximize      (void);

void                        meta_prefs_get_button_layout (MetaButtonLayout *button_layout);

/* Double, right, middle click can be configured to any titlebar meta-action */
GDesktopTitlebarAction      meta_prefs_get_action_double_click_titlebar (void);
GDesktopTitlebarAction      meta_prefs_get_action_middle_click_titlebar (void);
GDesktopTitlebarAction      meta_prefs_get_action_right_click_titlebar (void);

void meta_prefs_set_num_workspaces (int n_workspaces);

const char* meta_prefs_get_workspace_name    (int         i);
void        meta_prefs_change_workspace_name (int         i,
                                              const char *name);

const char* meta_prefs_get_cursor_theme      (void);
int         meta_prefs_get_cursor_size       (void);
gboolean    meta_prefs_get_compositing_manager (void);
gboolean    meta_prefs_get_force_fullscreen  (void);

void meta_prefs_set_force_fullscreen (gboolean whether);

gboolean meta_prefs_get_workspaces_only_on_primary (void);

gboolean meta_prefs_get_no_tab_popup (void);
void     meta_prefs_set_no_tab_popup (gboolean whether);

int      meta_prefs_get_draggable_border_width (void);

gboolean meta_prefs_get_ignore_request_hide_titlebar (void);
void     meta_prefs_set_ignore_request_hide_titlebar (gboolean whether);

/**
 * MetaKeyBindingAction:
 * @META_KEYBINDING_ACTION_NONE: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_1: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_2: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_3: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_4: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_5: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_6: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_7: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_8: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_9: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_10: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_11: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_12: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_LEFT: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_RIGHT: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_UP: FILLME 
 * @META_KEYBINDING_ACTION_WORKSPACE_DOWN: FILLME 
 * @META_KEYBINDING_ACTION_SWITCH_APPLICATIONS: FILLME 
 * @META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD: FILLME 
 * @META_KEYBINDING_ACTION_SWITCH_GROUP: FILLME 
 * @META_KEYBINDING_ACTION_SWITCH_GROUP_BACKWARD: FILLME 
 * @META_KEYBINDING_ACTION_SWITCH_WINDOWS: FILLME 
 * @META_KEYBINDING_ACTION_SWITCH_WINDOWS_BACKWARD: FILLME 
 * @META_KEYBINDING_ACTION_SWITCH_PANELS: FILLME 
 * @META_KEYBINDING_ACTION_SWITCH_PANELS_BACKWARD: FILLME 
 * @META_KEYBINDING_ACTION_CYCLE_GROUP: FILLME 
 * @META_KEYBINDING_ACTION_CYCLE_GROUP_BACKWARD: FILLME 
 * @META_KEYBINDING_ACTION_CYCLE_WINDOWS: FILLME 
 * @META_KEYBINDING_ACTION_CYCLE_WINDOWS_BACKWARD: FILLME 
 * @META_KEYBINDING_ACTION_CYCLE_PANELS: FILLME 
 * @META_KEYBINDING_ACTION_CYCLE_PANELS_BACKWARD: FILLME 
 * @META_KEYBINDING_ACTION_TAB_POPUP_SELECT: FILLME 
 * @META_KEYBINDING_ACTION_TAB_POPUP_CANCEL: FILLME 
 * @META_KEYBINDING_ACTION_SHOW_DESKTOP: FILLME 
 * @META_KEYBINDING_ACTION_PANEL_MAIN_MENU: FILLME 
 * @META_KEYBINDING_ACTION_PANEL_RUN_DIALOG: FILLME 
 * @META_KEYBINDING_ACTION_TOGGLE_RECORDING: FILLME 
 * @META_KEYBINDING_ACTION_SET_SPEW_MARK: FILLME 
 * @META_KEYBINDING_ACTION_ACTIVATE_WINDOW_MENU: FILLME 
 * @META_KEYBINDING_ACTION_TOGGLE_FULLSCREEN: FILLME 
 * @META_KEYBINDING_ACTION_TOGGLE_MAXIMIZED: FILLME 
 * @META_KEYBINDING_ACTION_TOGGLE_TILED_LEFT: FILLME 
 * @META_KEYBINDING_ACTION_TOGGLE_TILED_RIGHT: FILLME 
 * @META_KEYBINDING_ACTION_TOGGLE_ABOVE: FILLME 
 * @META_KEYBINDING_ACTION_MAXIMIZE: FILLME 
 * @META_KEYBINDING_ACTION_UNMAXIMIZE: FILLME 
 * @META_KEYBINDING_ACTION_TOGGLE_SHADED: FILLME 
 * @META_KEYBINDING_ACTION_MINIMIZE: FILLME 
 * @META_KEYBINDING_ACTION_CLOSE: FILLME 
 * @META_KEYBINDING_ACTION_BEGIN_MOVE: FILLME 
 * @META_KEYBINDING_ACTION_BEGIN_RESIZE: FILLME 
 * @META_KEYBINDING_ACTION_TOGGLE_ON_ALL_WORKSPACES: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_1: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_2: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_3: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_4: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_5: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_6: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_7: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_8: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_9: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_10: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_11: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_12: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_LEFT: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_RIGHT: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_UP: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_DOWN: FILLME 
 * @META_KEYBINDING_ACTION_RAISE_OR_LOWER: FILLME 
 * @META_KEYBINDING_ACTION_RAISE: FILLME 
 * @META_KEYBINDING_ACTION_LOWER: FILLME 
 * @META_KEYBINDING_ACTION_MAXIMIZE_VERTICALLY: FILLME 
 * @META_KEYBINDING_ACTION_MAXIMIZE_HORIZONTALLY: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_CORNER_NW: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_CORNER_NE: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_CORNER_SW: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_CORNER_SE: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_SIDE_N: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_SIDE_S: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_SIDE_E: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_SIDE_W: FILLME 
 * @META_KEYBINDING_ACTION_MOVE_TO_CENTER: FILLME 
 * @META_KEYBINDING_ACTION_OVERLAY_KEY: FILLME 
 * @META_KEYBINDING_ACTION_LAST: FILLME 
 */
/* XXX FIXME This should be x-macroed, but isn't yet because it would be
 * difficult (or perhaps impossible) to add the suffixes using the current
 * system.  It needs some more thought, perhaps after the current system
 * evolves a little.
 */
typedef enum _MetaKeyBindingAction
{
  META_KEYBINDING_ACTION_NONE,
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
  META_KEYBINDING_ACTION_SWITCH_APPLICATIONS,
  META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD,
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
  META_KEYBINDING_ACTION_TAB_POPUP_SELECT,
  META_KEYBINDING_ACTION_TAB_POPUP_CANCEL,
  META_KEYBINDING_ACTION_SHOW_DESKTOP,
  META_KEYBINDING_ACTION_PANEL_MAIN_MENU,
  META_KEYBINDING_ACTION_PANEL_RUN_DIALOG,
  META_KEYBINDING_ACTION_TOGGLE_RECORDING,
  META_KEYBINDING_ACTION_SET_SPEW_MARK,
  META_KEYBINDING_ACTION_ACTIVATE_WINDOW_MENU,
  META_KEYBINDING_ACTION_TOGGLE_FULLSCREEN,
  META_KEYBINDING_ACTION_TOGGLE_MAXIMIZED,
  META_KEYBINDING_ACTION_TOGGLE_TILED_LEFT,
  META_KEYBINDING_ACTION_TOGGLE_TILED_RIGHT,
  META_KEYBINDING_ACTION_TOGGLE_ABOVE,
  META_KEYBINDING_ACTION_MAXIMIZE,
  META_KEYBINDING_ACTION_UNMAXIMIZE,
  META_KEYBINDING_ACTION_TOGGLE_SHADED,
  META_KEYBINDING_ACTION_MINIMIZE,
  META_KEYBINDING_ACTION_CLOSE,
  META_KEYBINDING_ACTION_BEGIN_MOVE,
  META_KEYBINDING_ACTION_BEGIN_RESIZE,
  META_KEYBINDING_ACTION_TOGGLE_ON_ALL_WORKSPACES,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_1,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_2,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_3,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_4,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_5,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_6,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_7,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_8,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_9,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_10,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_11,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_12,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_LEFT,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_RIGHT,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_UP,
  META_KEYBINDING_ACTION_MOVE_TO_WORKSPACE_DOWN,
  META_KEYBINDING_ACTION_RAISE_OR_LOWER,
  META_KEYBINDING_ACTION_RAISE,
  META_KEYBINDING_ACTION_LOWER,
  META_KEYBINDING_ACTION_MAXIMIZE_VERTICALLY,
  META_KEYBINDING_ACTION_MAXIMIZE_HORIZONTALLY,
  META_KEYBINDING_ACTION_MOVE_TO_CORNER_NW,
  META_KEYBINDING_ACTION_MOVE_TO_CORNER_NE,
  META_KEYBINDING_ACTION_MOVE_TO_CORNER_SW,
  META_KEYBINDING_ACTION_MOVE_TO_CORNER_SE,
  META_KEYBINDING_ACTION_MOVE_TO_SIDE_N,
  META_KEYBINDING_ACTION_MOVE_TO_SIDE_S,
  META_KEYBINDING_ACTION_MOVE_TO_SIDE_E,
  META_KEYBINDING_ACTION_MOVE_TO_SIDE_W,
  META_KEYBINDING_ACTION_MOVE_TO_CENTER,
  META_KEYBINDING_ACTION_OVERLAY_KEY,
  META_KEYBINDING_ACTION_ISO_NEXT_GROUP,

  META_KEYBINDING_ACTION_LAST
} MetaKeyBindingAction;

/**
 * MetaKeyBindingFlags:
 * @META_KEY_BINDING_NONE: none
 * @META_KEY_BINDING_PER_WINDOW: per-window
 * @META_KEY_BINDING_BUILTIN: built-in
 * @META_KEY_BINDING_REVERSES: reverses
 * @META_KEY_BINDING_IS_REVERSED: is reversed
 */
typedef enum
{
  META_KEY_BINDING_NONE,
  META_KEY_BINDING_PER_WINDOW  = 1 << 0,
  META_KEY_BINDING_BUILTIN     = 1 << 1,
  META_KEY_BINDING_REVERSES    = 1 << 2,
  META_KEY_BINDING_IS_REVERSED = 1 << 3
} MetaKeyBindingFlags;

/**
 * MetaKeyCombo:
 * @keysym: keysym
 * @keycode: keycode
 * @modifiers: modifiers
 */
typedef struct _MetaKeyCombo MetaKeyCombo;
struct _MetaKeyCombo
{
  unsigned int keysym;
  unsigned int keycode;
  MetaVirtualModifier modifiers;
};

/**
 * MetaKeyHandlerFunc:
 * @display: a #MetaDisplay
 * @screen: a #MetaScreen
 * @window: a #MetaWindow
 * @event: (type gpointer): a #XIDeviceEvent
 * @binding: a #MetaKeyBinding
 * @user_data: data passed to the function
 *
 */
typedef void (* MetaKeyHandlerFunc) (MetaDisplay    *display,
                                     MetaScreen     *screen,
                                     MetaWindow     *window,
                                     XIDeviceEvent  *event,
                                     MetaKeyBinding *binding,
                                     gpointer        user_data);

typedef struct _MetaKeyHandler MetaKeyHandler;

typedef struct
{
  char *name;
  GSettings *settings;

  MetaKeyBindingAction action;

  /*
   * A list of MetaKeyCombos. Each of them is bound to
   * this keypref. If one has keysym==modifiers==0, it is
   * ignored.
   */
  GSList *bindings;

  /** for keybindings that can have shift or not like Alt+Tab */
  gboolean      add_shift:1;

  /** for keybindings that apply only to a window */
  gboolean      per_window:1;

  /** for keybindings not added with meta_display_add_keybinding() */
  gboolean      builtin:1;
} MetaKeyPref;

GType meta_key_binding_get_type    (void);

GList *meta_prefs_get_keybindings (void);

MetaKeyBindingAction meta_prefs_get_keybinding_action (const char *name);

void meta_prefs_get_window_binding (const char          *name,
                                    unsigned int        *keysym,
                                    MetaVirtualModifier *modifiers);

void meta_prefs_get_overlay_binding (MetaKeyCombo *combo);
const char *meta_prefs_get_iso_next_group_option (void);

gboolean           meta_prefs_get_visual_bell      (void);
gboolean           meta_prefs_bell_is_audible      (void);
GDesktopVisualBellType meta_prefs_get_visual_bell_type (void);

#endif




