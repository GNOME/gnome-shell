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

#include "stack.h"

struct _MetaStackOp
{
  guint raised : 1;
  guint lowered : 1;
  guint update_transient : 1;
  guint update_layer : 1;
  guint removed : 1;
  MetaWindow *window;
  Window xwindow; /* needed for remove, since window will be NULL */
  int add_order;  /* sequence number of add since last sync */
};

static void meta_stack_sync_to_server (MetaStack *stack);


MetaStack*
meta_stack_new (MetaScreen *screen)
{
  MetaStack *stack;
  int i;
  
  stack = g_new (MetaStack, 1);

  stack->screen = screen;
  stack->windows = g_array_new (FALSE, FALSE, sizeof (Window));

  i = 0;
  while (i < META_LAYER_LAST)
    {
      stack->layers[i] = NULL;
      ++i;
    }
  stack->pending = NULL;
  stack->freeze_count = 0;

  return stack;
}

void
meta_stack_free (MetaStack *stack)
{
  GList *tmp;
  
  g_array_free (stack->windows, TRUE);

  i = 0;
  while (i < META_LAYER_LAST)
    {
      g_list_free (stack->layers[i]);
      ++i;
    }
  
  tmp = stack->pending;
  while (tmp != NULL)
    {
      MetaStackOp *op;

      op = tmp->data;
      if (op->window)
        op->window->stack_op = NULL;
      g_free (op);
      
      tmp = tmp->next;
    }

  g_list_free (stack->pending);
}

static MetaStackOp*
ensure_op (MetaStack  *stack,
           MetaWindow *window)
{
  if (window->stack_op == NULL)
    {
      /* init all flags to 0 */
      window->stack_op = g_new0 (MetaStackOp, 1);
      window->stack_op->window = window;
      window->stack_op->xwindow = window->xwindow;
      window->stack_op->add_order = -1; /* indicates never added */
    }
  else
    {
      /* Need to move to front of list */
      stack->pending = g_list_remove (stack->pending, window->stack_op);
    }

  stack->pending = g_list_prepend (stack->pending, window->stack_op);
  
  return window->stack_op;
}

void
meta_stack_add (MetaStack  *stack,
                MetaWindow *window)
{
  MetaStackOp *op;

  op = ensure_op (stack, window);

  if (op->add_order >= 0)
    meta_bug ("Window %s added to stack twice\n", window->desc);
  
  op->add_order = stack->n_added;
  stack->n_added += 1;

  if (op->removed)
    meta_bug ("Remove op was left associated with window %s\n",
              window->desc);
  
  /* We automatically need to update all new windows */
  op->update_layer = TRUE;
  op->update_transient = TRUE;
  
  meta_stack_sync_to_server (stack);
}

void
meta_stack_remove (MetaStack  *stack,
                   MetaWindow *window)
{
  MetaStackOp *op;
  
  op = ensure_op (stack, window);

  if (op->add_order >= 0)
    {
      /* All we have to do is cancel the add */
      stack->pending = g_list_remove (stack->pending, op);
      window->stack_op = NULL;
      g_free (op);
      return;
    }

  /* op was other than an add, save a remove op */
  
  op->window = NULL;       /* can't touch this anymore. */
  op->removed = TRUE;
  op->add_order = -1;

  /* Need to immediately remove from layer lists */
  stack->layers[window->layer] =
    g_list_remove (stack->layers[window->layer], window);
  
  meta_stack_sync_to_server (stack);
}

void
meta_stack_update_layer (MetaStack  *stack,
                         MetaWindow *window)
{
  MetaStackOp *op;

  op = ensure_op (stack, window);
  op->update_layer = TRUE;

  meta_stack_sync_to_server (stack);
}

void
meta_stack_update_transient (MetaStack  *stack,
                             MetaWindow *window)
{
  MetaStackOp *op;

  op = ensure_op (stack, window);
  op->update_transient = TRUE;

  meta_stack_sync_to_server (stack);
}

/* raise/lower within a layer */
void
meta_stack_raise (MetaStack  *stack,
                  MetaWindow *window)
{
  MetaStackOp *op;

  op = ensure_op (stack, window);
  op->raised = TRUE;
  op->lowered = FALSE;
  
  meta_stack_sync_to_server (stack);
}

void
meta_stack_lower (MetaStack  *stack,
                  MetaWindow *window)
{
  MetaStackOp *op;

  op = ensure_op (stack, window);
  op->raised = FALSE;
  op->lowered = TRUE;
  
  meta_stack_sync_to_server (stack);
}

/* Prevent syncing to server until thaw */
void
meta_stack_freeze (MetaStack *stack)
{
  stack->freeze_count += 1;
}

void
meta_stack_thaw (MetaStack *stack)
{
  g_return_if_fail (stack->freeze_count > 0);
  
  stack->freeze_count -= 1;
  meta_stack_sync_to_server (stack);
}

static void
compute_layer (MetaWindow *window)
{
  switch (window->type)
    {
    case META_WINDOW_DESKTOP:
      window->layer = META_LAYER_DESKTOP;
      break;

    case META_WINDOW_DOCK:
      window->layer = META_WINDOW_DOCK;
      break;

    default:
      window->layer = META_LAYER_NORMAL;
      break;
    }
}

static void
meta_stack_sync_to_server (MetaStack *stack)
{
  gboolean needs_sort[META_LAYER_LAST] = {
    FALSE, FALSE, FALSE, FALSE, FALSE
  };
  GList *tmp;
  Window *added;
  Window *scratch;
  int n_actually_added;
  int i, j;
  int old_size;
  
  /* Bail out if frozen */
  if (stack->freeze_count > 0)
    return;

  /* Here comes the fun - figure out all the stacking.
   * We make no pretense of efficiency.
   *   a) replay all the pending operations
   *   b) do a stable sort within each layer with a comparison
   *      function that uses our constraints such as TRANSIENT_FOR
   *   c) sync to server
   */

  /* n_added is the number of meta_stack_add() calls, not the number
   * of windows that eventually get added here. meta_stack_remove()
   * may have cancelled some add operations.
   */
  if (stack->n_added > 0)
    added = g_new0 (Window, stack->n_added);
  else
    added = NULL;

  n_actually_added = 0;
  tmp = stack->pending;
  while (tmp != NULL)
    {
      MetaStackOp *op;

      op = tmp->data;

      if (op->add_order >= 0)
        {
          added[op->add_order] = op->window->xwindow;
          ++n_actually_added;
        }

      tmp = tmp->next;
    }

  old_size = stack->window->len;
  g_array_set_size (stack->windows,
                    old_size + n_actually_added);
  
  scratch = &g_array_index (stack->windows, Window, old_size);

  i = 0;
  j = 0;
  while (i < stack->n_added)
    {
      if (added[i] != None)
        {
          scratch[j] = added[i];
          ++j;
        }
      
      ++i;
    }
  
  g_assert (j == n_actually_added);
  g_assert (i == stack->n_added);

  g_free (added);

  /* Now remove windows that need removing;
   * they were already removed from the layer lists
   * in meta_stack_remove()
   */
  tmp = stack->pending;
  while (tmp != NULL)
    {
      MetaStackOp *op;

      op = tmp->data;

      if (op->removed)
        {
          /* Go from the end, on the principle that more recent
           * windows are more likely to be removed, and also that we
           * can remove without changing what we're iterating over.
           */
          i = stack->windows->len;
          while (i > 0)
            {
              --i;

              /* there's no guarantee we'll actually find windows to
               * remove, e.g. the same xwindow could have been
               * added/removed before we ever synced, or even
               * added/removed/added/removed/added again, etc.
               */
              if (op->xwindow == g_array_index (stack->windows, Window, i))
                {
                  g_array_remove_index (stack->windows, i);
                  goto next;
                }
            }
        }

    next:
      tmp = tmp->next;
    }
  
  /* With all the adding/removing sorted out, actually do our
   * operations
   */
  
  tmp = stack->pending;
  while (tmp != NULL)
    {
      MetaStackOp *op;

      op = tmp->data;

      if (!op->removed)
        {
          MetaStackLayer old_layer;

          old_layer = op->window->layer;

          if (op->update_layer)
            {
              compute_layer (op->window);

              if (op->window->layer != old_layer)
                {
                  /* don't resort old layer, it's
                   * assumed that removing a window
                   * makes no difference.
                   */
                  needs_sort[op->window->layer] = TRUE;

                  stack->layers[old_layer] =
                    g_list_remove (stack->layers[old_layer], op->window);
                  stack->layers[op->window->layer] =
                    g_list_prepend (stack->layers[op->window->layer], op->window);
                }
            }
                    
          if (op->update_transient)
            {
              /* need to resort our layer */
              needs_sort[op->window->layer] = TRUE;
            }

          /* We assume that ordering between changing layers
           * and raise/lower is irrelevant; if you raise, then
           * the layer turns out to be different, you still
           * raise inside the new layer
           */
          if (op->raised)
            {
              /* "top" is the front of the list */

              stack->layers[op->window->layer] =
                g_list_remove (stack->layers[op->window->layer],
                               op->window);

              stack->layers[op->window->layer] =
                g_list_prepend (stack->layers[op->window->layer],
                                op->window);
            }
          else if (op->lowered)
            {
              stack->layers[op->window->layer] =
                g_list_remove (stack->layers[op->window->layer],
                               op->window);

              stack->layers[op->window->layer] =
                g_list_append (stack->layers[op->window->layer],
                               op->window);
            }
        }

      g_free (op);
      
      tmp = tmp->next;
    }  

  g_list_free (stack->pending);
  stack->pending = NULL;
  stack->n_added = 0;


  /* Sort the layer lists */
  i = 0;  
  while (i < META_LAYER_LAST)
    {
      if (needs_sort[i])
        {
          

        }

      ++i;
    }

  /* Create stacked xwindow array */
  
  /* Sync to server */
  
  /* Sync _NET_CLIENT_LIST and _NET_CLIENT_LIST_STACKING */
  
}

