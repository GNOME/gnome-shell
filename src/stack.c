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
#include "window.h"
#include "errors.h"

#include <X11/Xatom.h>

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
  int i;
  
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

  meta_verbose ("Window %s on layer %d\n",
                window->desc, window->layer);
}

static gboolean
is_transient_for (MetaWindow *transient,
                  MetaWindow *parent)
{
  MetaWindow *w;

  w = transient;
  while (w != NULL)
    {
      if (w->xtransient_for == None)
        return FALSE;
      
      w = meta_display_lookup_x_window (w->display, w->xtransient_for);

      if (w == transient)
        return FALSE; /* Cycle detected */
      else if (w == parent)
        return TRUE;
    }

  return FALSE;
}

static int
window_stack_cmp (MetaWindow *a,
                  MetaWindow *b)
{
  /* Less than means higher in stacking, i.e. at the
   * front of the list.
   */

  if (a->xtransient_for != None &&
      is_transient_for (a, b))
    {
      /* a is higher than b due to transient_for */
      return -1;
    }
  else if (b->xtransient_for != None &&
           is_transient_for (b, a))
    {
      /* b is higher than a due to transient_for */
      return 1;
    }
  else
    return 0;  /* leave things as-is */
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
  GArray *stacked;
  
  /* Bail out if frozen */
  if (stack->freeze_count > 0)
    return;

  if (stack->pending == NULL)
    return;
  
  meta_verbose ("Syncing window stack to server\n");
  
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

  old_size = stack->windows->len;
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
          stack->layers[i] =
            g_list_sort (stack->layers[i],
                         (GCompareFunc) window_stack_cmp);
        }

      ++i;
    }

  /* Create stacked xwindow array */
  stacked = g_array_new (FALSE, FALSE, sizeof (Window));
  i = 0;  
  while (i < META_LAYER_LAST)
    {      
      if (needs_sort[i])
        {
          stack->layers[i] =
            g_list_sort (stack->layers[i],
                         (GCompareFunc) window_stack_cmp);
        }

      tmp = stack->layers[i];
      while (tmp != NULL)
        {
          MetaWindow *w = tmp->data;

          g_array_append_val (stacked, w->xwindow);

          tmp = tmp->next;
        }
      
      ++i;
    }

  /* All windows should be in some stacking order */
  if (stacked->len != stack->windows->len)
    meta_bug ("%d windows stacked, %d windows exist in stack\n",
              stacked->len, stack->windows->len);
  
  /* Sync to server */

  meta_error_trap_push (stack->screen->display);
  XRestackWindows (stack->screen->display->xdisplay,
                   (Window *) stacked->data,
                   stacked->len);
  meta_error_trap_pop (stack->screen->display);
  /* on error, a window was destroyed; it should eventually
   * get removed from the stacking list when we unmanage it
   * and we'll fix stacking at that time.
   */
  
  /* Sync _NET_CLIENT_LIST and _NET_CLIENT_LIST_STACKING */

  XChangeProperty (stack->screen->display->xdisplay,
                   stack->screen->xroot,
                   stack->screen->display->atom_net_client_list,
                   XA_ATOM,
                   32, PropModeReplace,
                   stack->windows->data,
                   stack->windows->len);
  XChangeProperty (stack->screen->display->xdisplay,
                   stack->screen->xroot,
                   stack->screen->display->atom_net_client_list_stacking,
                   XA_ATOM,
                   32, PropModeReplace,
                   stacked->data,
                   stacked->len);

  g_array_free (stacked, TRUE);

  /* That was scary... */
}

