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

/* Type of last-queued stacking operation, hung off of MetaWindow
 * but an opaque type used only in stack.c
 */
typedef struct _MetaStackOp MetaStackOp;

/* These MUST be in the order of stacking */
typedef enum
{
  META_LAYER_DESKTOP   = 0,
  META_LAYER_BOTTOM    = 1,
  META_LAYER_NORMAL    = 2,
  META_LAYER_TOP       = 3,
  META_LAYER_DOCK      = 4,
  META_LAYER_LAST      = 5
} MetaStackLayer;

struct _MetaStack
{
  MetaScreen *screen;

  /* All windows that we manage, in mapping order,
   * for _NET_CLIENT_LIST
   */
  GArray *windows;

  /* List of MetaWindow* in each layer */
  GList *layers[META_LAYER_LAST];

  /* List of MetaStackOp, most recent op
   * first in list.
   */
  GList *pending;
  
  int freeze_count;

  int n_added;
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
                                   MetaWindow *window);
MetaWindow* meta_stack_get_below  (MetaStack  *stack,
                                   MetaWindow *window);

MetaWindow* meta_stack_get_tab_next (MetaStack  *stack,
                                     MetaWindow *window,
                                     gboolean    backward);

#endif




