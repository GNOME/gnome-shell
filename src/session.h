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
  /* Fields we use to match against */

  char *id;
  char *res_class;
  char *res_name;
  char *title;
  char *role;
  MetaWindowType type;

  /* Information we restore */
  
  GSList *workspace_indices;  

  int stack_position;
  
  /* width/height should be multiplied by resize inc and
   * added to base size; position should be interpreted in
   * light of gravity. This preserves semantics of the
   * window size/pos, even if fonts/themes change, etc.
   */
  int gravity;
  MetaRectangle rect;
  guint on_all_workspaces : 1;
  guint minimized : 1;
  guint maximized : 1;

  guint stack_position_set : 1;
  guint geometry_set : 1;
  guint on_all_workspaces_set : 1;
  guint minimized_set : 1;
  guint maximized_set : 1;
};

/* If lookup_saved_state returns something, it should be used,
 * and then released when you're done with it.
 */
const MetaWindowSessionInfo* meta_window_lookup_saved_state  (MetaWindow                  *window);
void                         meta_window_release_saved_state (const MetaWindowSessionInfo *info);

void meta_session_init (const char *client_id,
                        const char *save_file);


void meta_session_shutdown (void);

#endif




