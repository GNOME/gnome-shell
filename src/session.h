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

typedef struct _MetaWindowSessionInfo MetaWindowSessionInfo;

struct _MetaWindowSessionInfo
{
  int workspace;

};

void meta_window_lookup_saved_state (MetaWindow *window,
                                     MetaWindowSessionInfo *info);

void meta_session_init             (const char *previous_id);


#endif




