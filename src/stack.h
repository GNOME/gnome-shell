/* Metacity Window Stack */

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

#ifndef META_STACK_H
#define META_STACK_H

#include "screen.h"

/* Layers vs. stack positions
 * ==========================
 *
 * There are two factors that determine window position.
 * 
 * One is window->stack_position, which is a unique integer
 * indicating how windows are ordered with respect to one
 * another. The ordering here transcends layers; it isn't changed
 * as the window is moved among layers. This allows us to move several
 * windows from one layer to another, while preserving the relative
 * order of the moved windows. Also, it allows us to restore
 * the stacking order from a saved session.
 * 
 * However when actually stacking windows on the screen, the
 * layer overrides the stack_position; windows are first sorted
 * by layer, then by stack_position within each layer.
 *
 */

/* These MUST be in the order of stacking */
typedef enum
{
  META_LAYER_DESKTOP        = 0,
  META_LAYER_BOTTOM         = 1,
  META_LAYER_NORMAL         = 2,
  META_LAYER_TOP            = 3,
  META_LAYER_DOCK           = 4,
  META_LAYER_FULLSCREEN     = 5,
  META_LAYER_SPLASH         = 6,
  META_LAYER_FOCUSED_WINDOW = 7,
  META_LAYER_LAST           = 8
} MetaStackLayer;

struct _MetaStack
{
  MetaScreen *screen;

  /* All X windows that we manage, in mapping order,
   * for _NET_CLIENT_LIST
   */
  GArray *windows;

  /* Currently-stacked MetaWindow */
  GList *sorted;
  /* MetaWindow to be added to the sorted list */
  GList *added;
  /* Window IDs to be removed from the stack */
  GList *removed;
  
  int freeze_count;

  /* The last-known stack */
  GArray *last_root_children_stacked;

  /* number of stack positions */
  int n_positions;

  /* What needs doing */
  unsigned int need_resort : 1;
  unsigned int need_relayer : 1;
  unsigned int need_constrain : 1;
};

MetaStack *meta_stack_new       (MetaScreen     *screen);
void       meta_stack_free      (MetaStack      *stack);
void       meta_stack_add       (MetaStack      *stack,
                                 MetaWindow     *window);
void       meta_stack_remove    (MetaStack      *stack,
                                 MetaWindow     *window);
/* re-read layer-related hints */
void       meta_stack_update_layer    (MetaStack      *stack,
                                       MetaWindow     *window);
/* reconsider transient_for */
void       meta_stack_update_transient (MetaStack     *stack,
                                        MetaWindow    *window);

/* raise/lower within a layer */
void       meta_stack_raise     (MetaStack      *stack,
                                 MetaWindow     *window);
void       meta_stack_lower     (MetaStack      *stack,
                                 MetaWindow     *window);

/* On thaw, process pending and sync to server */
void       meta_stack_freeze    (MetaStack      *stack);
void       meta_stack_thaw      (MetaStack      *stack);

MetaWindow* meta_stack_get_top    (MetaStack  *stack);
MetaWindow* meta_stack_get_bottom (MetaStack  *stack);
MetaWindow* meta_stack_get_above  (MetaStack  *stack,
                                   MetaWindow *window,
                                   gboolean    only_within_layer);
MetaWindow* meta_stack_get_below  (MetaStack  *stack,
                                   MetaWindow *window,
                                   gboolean    only_within_layer);
MetaWindow* meta_stack_get_default_focus_window (MetaStack *stack,
                                                 MetaWorkspace *workspace,
                                                 MetaWindow    *not_this_one);
GList*      meta_stack_list_windows (MetaStack *stack,
                                     MetaWorkspace *workspace);
				       

/* -1 if a < b, etc. */
int         meta_stack_windows_cmp  (MetaStack  *stack,
                                     MetaWindow *window_a,
                                     MetaWindow *window_b);

void meta_window_set_stack_position (MetaWindow *window,
                                     int         position);

#endif




