/* Metacity utilities */

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

#ifndef META_UTIL_H
#define META_UTIL_H

#include <glib.h>

gboolean meta_is_verbose  (void);
void     meta_set_verbose (gboolean setting);
gboolean meta_is_debugging (void);
void     meta_set_debugging (gboolean setting);
gboolean meta_is_syncing (void);
void     meta_set_syncing (gboolean setting);
gboolean meta_get_replace_current_wm (void);
void     meta_set_replace_current_wm (gboolean setting);

void meta_debug_spew (const char *format,
                      ...) G_GNUC_PRINTF (1, 2);
void meta_verbose    (const char *format,
                      ...) G_GNUC_PRINTF (1, 2);
void meta_bug        (const char *format,
                      ...) G_GNUC_PRINTF (1, 2);
void meta_warning    (const char *format,
                      ...) G_GNUC_PRINTF (1, 2);
void meta_fatal      (const char *format,
                      ...) G_GNUC_PRINTF (1, 2);

typedef enum
{
  META_DEBUG_FOCUS        = 1 << 0,
  META_DEBUG_WORKAREA     = 1 << 1,
  META_DEBUG_STACK        = 1 << 2,
  META_DEBUG_THEMES       = 1 << 3,
  META_DEBUG_SM           = 1 << 4,
  META_DEBUG_EVENTS       = 1 << 5,
  META_DEBUG_WINDOW_STATE = 1 << 6,
  META_DEBUG_WINDOW_OPS   = 1 << 7,
  META_DEBUG_GEOMETRY     = 1 << 8,
  META_DEBUG_PLACEMENT    = 1 << 9,
  META_DEBUG_PING         = 1 << 10,
  META_DEBUG_XINERAMA     = 1 << 11,
  META_DEBUG_KEYBINDINGS  = 1 << 12

} MetaDebugTopic;

void meta_topic      (MetaDebugTopic topic,
                      const char    *format,
                      ...) G_GNUC_PRINTF (2, 3);

void meta_push_no_msg_prefix (void);
void meta_pop_no_msg_prefix  (void);

#include <libintl.h>
#define _(x) dgettext (GETTEXT_PACKAGE, x)
#define N_(x) x

#endif


