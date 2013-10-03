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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_KEYBINDINGS_PRIVATE_H
#define META_KEYBINDINGS_PRIVATE_H

#include <gio/gio.h>
#include <meta/keybindings.h>

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
  MetaKeyHandler *handler;
};

void     meta_display_init_keys             (MetaDisplay *display);
void     meta_display_shutdown_keys         (MetaDisplay *display);
void     meta_screen_grab_keys              (MetaScreen  *screen);
void     meta_screen_ungrab_keys            (MetaScreen  *screen);
gboolean meta_screen_grab_all_keys          (MetaScreen  *screen,
                                             guint32      timestamp);
void     meta_screen_ungrab_all_keys        (MetaScreen  *screen, 
                                             guint32      timestamp);
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


#endif
