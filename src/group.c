/* Metacity window groups */

/* 
 * Copyright (C) 2002 Red Hat Inc.
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

#include <config.h>
#include "util.h"
#include "group.h"
#include "window.h"

struct _MetaGroup
{
  MetaDisplay *display;
  GSList *windows;
  Window group_leader;
  int refcount;
};

static MetaGroup*
meta_group_new (MetaDisplay *display,
                Window       group_leader)
{
  MetaGroup *group;

  group = g_new0 (MetaGroup, 1);

  group->display = display;
  group->windows = NULL;
  group->group_leader = group_leader;
  group->refcount = 1; /* owned by caller, hash table has only weak ref */
  
  if (display->groups_by_leader == NULL)
    display->groups_by_leader = g_hash_table_new (meta_unsigned_long_hash,
                                                  meta_unsigned_long_equal);

  g_assert (g_hash_table_lookup (display->groups_by_leader, &group_leader) == NULL);

  g_hash_table_insert (display->groups_by_leader,
                       &group->group_leader,
                       group);
  
  return group;
}

static void
meta_group_unref (MetaGroup *group)
{
  g_return_if_fail (group->refcount > 0);

  group->refcount -= 1;
  if (group->refcount == 0)
    {
      g_assert (group->display->groups_by_leader != NULL);
      
      g_hash_table_remove (group->display->groups_by_leader,
                           &group->group_leader);

      /* mop up hash table, this is how it gets freed on display close */
      if (g_hash_table_size (group->display->groups_by_leader) == 0)
        {
          g_hash_table_destroy (group->display->groups_by_leader);
          group->display->groups_by_leader = NULL;
        }

      g_free (group);
    }
}

MetaGroup*
meta_window_get_group (MetaWindow *window)
{
  if (window->unmanaging)
    return NULL;
  
  if (window->cached_group == NULL &&
      window->xgroup_leader != None) /* some windows have no group */
    {
      MetaGroup *group;

      group = NULL;
      
      if (window->display->groups_by_leader)
        group = g_hash_table_lookup (window->display->groups_by_leader,
                                     &window->xgroup_leader);
      
      if (group != NULL)
        {
          window->cached_group = group;
          group->refcount += 1;
        }
      else
        {
          group = meta_group_new (window->display,
                                  window->xgroup_leader);

          window->cached_group = group;
        }

      window->cached_group->windows = g_slist_prepend (window->cached_group->windows,
                                                       window);
    }

  return window->cached_group;
}

void
meta_window_shutdown_group (MetaWindow *window)
{
  if (window->cached_group != NULL)
    {
      window->cached_group->windows =
        g_slist_remove (window->cached_group->windows,
                        window);
      meta_group_unref (window->cached_group);
      window->cached_group = NULL;
    }
}

MetaGroup*
meta_display_lookup_group (MetaDisplay *display,
                           Window       group_leader)
{
  MetaGroup *group;
  
  group = NULL;
  
  if (display->groups_by_leader)
    group = g_hash_table_lookup (display->groups_by_leader,
                                 &group_leader);

  return group;
}

GSList*
meta_group_list_windows (MetaGroup *group)
{
  return g_slist_copy (group->windows);
}

void
meta_group_update_layers (MetaGroup *group)
{
  GSList *tmp;
  GSList *frozen_stacks;
  
  if (group->windows == NULL)
    return;

  frozen_stacks = NULL;
  tmp = group->windows;
  while (tmp != NULL)
    {
      MetaWindow *window = tmp->data;

      /* we end up freezing the same stack a lot of times,
       * but doesn't hurt anything. have to handle
       * groups that span 2 screens.
       */
      meta_stack_freeze (window->screen->stack);
      frozen_stacks = g_slist_prepend (frozen_stacks, window->screen->stack);

      meta_stack_update_layer (window->screen->stack,
                               window);
      
      tmp = tmp->next;
    }

  tmp = frozen_stacks;
  while (tmp != NULL)
    {
      meta_stack_thaw (tmp->data);
      tmp = tmp->next;
    }

  g_slist_free (frozen_stacks);
}
