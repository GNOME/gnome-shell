/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * \file keybindings.h  Grab and ungrab keys, and process the key events
 *
 * Performs global X grabs on the keys we need to be told about, like
 * the one to close a window.  It also deals with incoming key events.
 */

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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_KEYBINDINGS_PRIVATE_H
#define META_KEYBINDINGS_PRIVATE_H

#include <gio/gio.h>
#include <meta/keybindings.h>

typedef struct _MetaKeyHandler MetaKeyHandler;
struct _MetaKeyHandler
{
  char *name;
  MetaKeyHandlerFunc func;
  MetaKeyHandlerFunc default_func;
  gint data, flags;
  gpointer user_data;
  GDestroyNotify user_data_free_func;
};

struct _MetaKeyBinding
{
  const char *name;
  KeySym keysym;
  KeyCode keycode;
  unsigned int mask;
  MetaVirtualModifier modifiers;
  gint flags;
  MetaKeyHandler *handler;
};

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
  GSList *combos;

  /* for keybindings that can have shift or not like Alt+Tab */
  gboolean      add_shift:1;

  /* for keybindings that apply only to a window */
  gboolean      per_window:1;

  /* for keybindings not added with meta_display_add_keybinding() */
  gboolean      builtin:1;
} MetaKeyPref;

void     meta_display_init_keys             (MetaDisplay *display);
void     meta_display_shutdown_keys         (MetaDisplay *display);
void     meta_screen_grab_keys              (MetaScreen  *screen);
void     meta_screen_ungrab_keys            (MetaScreen  *screen);
void     meta_window_grab_keys              (MetaWindow  *window);
void     meta_window_ungrab_keys            (MetaWindow  *window);
gboolean meta_window_grab_all_keys          (MetaWindow  *window,
                                             guint32      timestamp);
void     meta_window_ungrab_all_keys        (MetaWindow  *window,
                                             guint32      timestamp);
gboolean meta_display_process_key_event     (MetaDisplay     *display,
                                             MetaWindow      *window,
                                             ClutterKeyEvent *event);
void     meta_display_process_mapping_event (MetaDisplay *display,
                                             XEvent      *event);

gboolean meta_prefs_add_keybinding          (const char           *name,
                                             GSettings            *settings,
                                             MetaKeyBindingAction  action,
                                             MetaKeyBindingFlags   flags);

gboolean meta_prefs_remove_keybinding       (const char    *name);

GList *meta_prefs_get_keybindings (void);
void meta_prefs_get_overlay_binding (MetaKeyCombo *combo);
const char *meta_prefs_get_iso_next_group_option (void);

#endif
