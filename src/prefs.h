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
  META_PREF_APPLICATION_BASED
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

void meta_prefs_set_num_workspaces (int n_workspaces);

#endif




