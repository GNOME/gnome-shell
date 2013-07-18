/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity Theme Rendering */

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

#ifndef META_THEME_H
#define META_THEME_H

#include <glib.h>

/**
 * MetaTheme:
 *
 */
typedef struct _MetaTheme MetaTheme;

MetaTheme* meta_theme_get_current (void);
void       meta_theme_set_current (const char *name);

MetaTheme* meta_theme_new      (void);
void       meta_theme_free     (MetaTheme *theme);
gboolean   meta_theme_validate (MetaTheme *theme,
                                GError   **error);

MetaTheme* meta_theme_load (const char *theme_name,
                            GError    **err);

#endif
