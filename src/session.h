/* Metacity Session Management */

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

#ifndef META_SESSION_H
#define META_SESSION_H

#include "window.h"

typedef struct _MetaSessionInfo MetaSessionInfo;

struct _MetaSessionInfo
{
  /* In -geometry format; x, y are affected by gravity, width, height
   * are to be multiplied by resize increments, etc.  This way we're
   * robust against theme changes, client resize inc changes, client
   * base size changes, and so on.
   */
  MetaRectangle rect;

  /* A per-screen index (_NET_WM_DESKTOP) */
  int workspace;
};

MetaSessionInfo* meta_window_lookup_session_info (MetaWindow *window);

void meta_session_init (const char *previous_id);

#endif




