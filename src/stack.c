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
#include "frame.h"
#include "workspace.h"

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
  stack->n_added = 0;
  
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

  meta_verbose ("Adding window %s to the stack\n", window->desc);
  
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

  meta_verbose ("Removing window %s from the stack\n", window->desc);
  
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
  g_assert (g_list_find (stack->layers[window->layer], window) != NULL);
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
  op->update_transient = TRUE;
  
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
  op->update_transient = TRUE;
  
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
      window->layer = META_LAYER_DOCK;
      break;

    default:
      window->layer = META_LAYER_NORMAL;
      break;
    }

  meta_verbose ("Window %s on layer %d\n",
                window->desc, window->layer);
}

static GList*
ensure_before (GList *list,
               gconstpointer before,
               gconstpointer value)
{
  /* ensure before is before value */
  GList *b_link;
  GList *v_link;
  GList *tmp;

  b_link = NULL;
  v_link = NULL;
  tmp = list;
  while (tmp != NULL)
    {
      if (tmp->data == before)
        {
          if (v_link == NULL)
            return list; /* already before */

          b_link = tmp;
        }
      else if (tmp->data == value)
        {
          v_link = tmp;
        }

      tmp = tmp->next;
    }

  /* We weren't already before if we got here */
  list = g_list_remove_link (list, b_link);

  if (v_link == list)
    {
      b_link->next = v_link;
      v_link->prev = b_link;
      list = b_link;
    }
  else
    {
      b_link->prev = v_link->prev;
      b_link->prev->next = b_link;
      b_link->next = v_link;
      v_link->prev = b_link;
    }

  return list;
}

static GList*
sort_window_list (GList *list)
{
  GList *tmp;
  GList *copy;
  
  /* This algorithm could stand to be a bit less
   * quadratic
   */
  copy = g_list_copy (list);
  tmp = copy;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;

      if (w->xtransient_for != None)
        {
          MetaWindow *parent;
          
          parent =
            meta_display_lookup_x_window (w->display, w->xtransient_for);

          if (parent)
            {
              meta_verbose ("Stacking %s above %s due to transiency\n",
                            w->desc, parent->desc);
              list = ensure_before (list, w, parent);
            }
        }
      
      tmp = tmp->next;
    }
  g_list_free (copy);
  
  return list;
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
  GArray *root_children_stacked;
  
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

          if (op->add_order >= 0)
            {
              /* need to add to a layer */
              stack->layers[op->window->layer] =
                g_list_prepend (stack->layers[op->window->layer],
                                op->window);
            }

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

              needs_sort[op->window->layer] = TRUE;
            }
          else if (op->lowered)
            {
              stack->layers[op->window->layer] =
                g_list_remove (stack->layers[op->window->layer],
                               op->window);

              stack->layers[op->window->layer] =
                g_list_append (stack->layers[op->window->layer],
                               op->window);

              needs_sort[op->window->layer] = TRUE;
            }
        }

      if (op->window)
        op->window->stack_op = NULL;
      g_free (op);
      
      tmp = tmp->next;
    }

  g_list_free (stack->pending);
  stack->pending = NULL;
  stack->n_added = 0;

  /* Create stacked xwindow arrays.
   * Painfully, "stacked" is in bottom-to-top order for the
   * _NET hints, and "root_children_stacked" is in top-to-bottom
   * order for XRestackWindows()
   */
  stacked = g_array_new (FALSE, FALSE, sizeof (Window));
  root_children_stacked = g_array_new (FALSE, FALSE, sizeof (Window));
  i = META_LAYER_LAST; 
  do
    {
      --i;
      
      /* Sort each layer... */
      if (needs_sort[i])
        {
          meta_verbose ("Sorting layer %d\n", i);
          stack->layers[i] = sort_window_list (stack->layers[i]);
        }

      /* ... then append it */
      meta_verbose ("Layer %d: ", i);
      meta_push_no_msg_prefix ();
      tmp = stack->layers[i];
      while (tmp != NULL)
        {
          MetaWindow *w = tmp->data;

          /* remember, stacked is in reverse order (bottom to top) */
          g_array_prepend_val (stacked, w->xwindow);

          /* build XRestackWindows() array from top to bottom */
          if (w->frame)
            g_array_append_val (root_children_stacked, w->frame->xwindow);
          else
            g_array_append_val (root_children_stacked, w->xwindow);

          meta_verbose ("%s ", w->desc);
          
          tmp = tmp->next;
        }

      meta_verbose ("\n");
      meta_pop_no_msg_prefix ();
    }
  while (i > 0);

  /* All windows should be in some stacking order */
  if (stacked->len != stack->windows->len)
    meta_bug ("%d windows stacked, %d windows exist in stack\n",
              stacked->len, stack->windows->len);
  
  /* Sync to server */

  meta_verbose ("Restacking %d windows\n",
                root_children_stacked->len);
  
  meta_error_trap_push (stack->screen->display);
  XRestackWindows (stack->screen->display->xdisplay,
                   (Window *) root_children_stacked->data,
                   root_children_stacked->len);
  meta_error_trap_pop (stack->screen->display);
  /* on error, a window was destroyed; it should eventually
   * get removed from the stacking list when we unmanage it
   * and we'll fix stacking at that time.
   */
  
  /* Sync _NET_CLIENT_LIST and _NET_CLIENT_LIST_STACKING */

  XChangeProperty (stack->screen->display->xdisplay,
                   stack->screen->xroot,
                   stack->screen->display->atom_net_client_list,
                   XA_WINDOW,
                   32, PropModeReplace,
                   stack->windows->data,
                   stack->windows->len);
  XChangeProperty (stack->screen->display->xdisplay,
                   stack->screen->xroot,
                   stack->screen->display->atom_net_client_list_stacking,
                   XA_WINDOW,
                   32, PropModeReplace,
                   stacked->data,
                   stacked->len);

  g_array_free (stacked, TRUE);
  g_array_free (root_children_stacked, TRUE);

  /* That was scary... */
}

static MetaWindow*
find_next_above_layer (MetaStack *stack,
                       int        layer)
{
  ++layer;
  while (layer < META_LAYER_LAST)
    {
      GList *link;

      g_assert (layer >= 0 && layer < META_LAYER_LAST);

      /* bottom of this layer is at the end of the list */
      link = g_list_last (stack->layers[layer]);
      
      if (link)
        return link->data;
      
      ++layer;
    }

  return NULL;
}

static MetaWindow*
find_prev_below_layer (MetaStack     *stack,
                       int            layer)
{
  --layer;
  while (layer >= 0)
    {
      GList *link;

      g_assert (layer >= 0 && layer < META_LAYER_LAST);

      /* top of this layer is at the front of the list */
      link = stack->layers[layer];
      
      if (link)
        return link->data;
      
      --layer;
    }

  return NULL;
}

MetaWindow*
meta_stack_get_top (MetaStack *stack)
{
  /* FIXME if stack is frozen this is kind of broken. */
  
  return find_prev_below_layer (stack, META_LAYER_LAST);
}

MetaWindow*
meta_stack_get_bottom (MetaStack  *stack)
{
  /* FIXME if stack is frozen this is kind of broken. */
  
  return find_next_above_layer (stack, -1);
}

MetaWindow*
meta_stack_get_above (MetaStack      *stack,
                      MetaWindow     *window)
{
  GList *link;

  /* FIXME if stack is frozen this is kind of broken. */
  
  g_assert (window->layer >= 0 && window->layer < META_LAYER_LAST);
  link = g_list_find (stack->layers[window->layer], window);
  if (link == NULL)
    return NULL;

  if (link->prev)
    return link->prev->data;
  else
    return find_next_above_layer (stack, window->layer);
}

MetaWindow*
meta_stack_get_below (MetaStack      *stack,
                      MetaWindow     *window)
{
  GList *link;

  /* FIXME if stack is frozen this is kind of broken. */

  g_assert (window->layer >= 0 && window->layer < META_LAYER_LAST);
  link = g_list_find (stack->layers[window->layer], window);
  if (link == NULL)
    return NULL;

  if (link->next)
    return link->next->data;
  else
    return find_prev_below_layer (stack, window->layer);
}

#define IN_TAB_CHAIN(w) ((w)->layer != META_LAYER_DOCK && (w)->layer != META_LAYER_DESKTOP)
#define GET_XWINDOW(stack, i) (g_array_index ((stack)->windows,    \
                                              Window, (i)))

static MetaWindow*
find_tab_forward (MetaStack     *stack,
                  MetaWorkspace *workspace,
                  int            start)
{
  int i;

  /* start may be -1 to find any tab window at all */
  
  i = start + 1;
  while (i < stack->windows->len)
    {
      MetaWindow *window;
      
      window = meta_display_lookup_x_window (stack->screen->display,
                                             GET_XWINDOW (stack, i));

      if (window && IN_TAB_CHAIN (window) &&
          (workspace == NULL ||
           meta_workspace_contains_window (workspace, window)))
        return window;

      ++i;
    }

  i = 0;
  while (i < start)
    {
      MetaWindow *window;
      
      window = meta_display_lookup_x_window (stack->screen->display,
                                             GET_XWINDOW (stack, i));

      if (window && IN_TAB_CHAIN (window) &&
          (workspace == NULL ||
           meta_workspace_contains_window (workspace, window)))
        return window;

      ++i;
    }

  /* no window other than the start window is in the tab chain */
  return NULL;
}

static MetaWindow*
find_tab_backward (MetaStack     *stack,
                   MetaWorkspace *workspace,
                   int            start)
{
  int i;

  /* start may be stack->windows->len to find any tab window at all */
  
  i = start - 1;
  while (i >= 0)
    {
      MetaWindow *window;
      
      window = meta_display_lookup_x_window (stack->screen->display,
                                             GET_XWINDOW (stack, i));

      if (window && IN_TAB_CHAIN (window) &&
          (workspace == NULL ||
           meta_workspace_contains_window (workspace, window)))
        return window;

      --i;
    }

  i = stack->windows->len - 1;
  while (i > start)
    {
      MetaWindow *window;
      
      window = meta_display_lookup_x_window (stack->screen->display,
                                             GET_XWINDOW (stack, i));

      if (window && IN_TAB_CHAIN (window) &&
          (workspace == NULL ||
           meta_workspace_contains_window (workspace, window)))
        return window;

      --i;
    }

  /* no window other than the start window is in the tab chain */
  return NULL;
}

/* This ignores the dock/desktop layers */
MetaWindow*
meta_stack_get_tab_next (MetaStack  *stack,
                         MetaWindow *window,
                         gboolean    backward)
{
  int i;

  if (stack->windows->len == 0)
    return NULL;

  if (window != NULL)
    {
      i = 0;
      while (i < stack->windows->len)
        {
          Window w;
          
          w = g_array_index (stack->windows, Window, i);

          if (w == window->xwindow)
            {
              MetaWorkspace *workspace;

              workspace = window->screen->active_workspace;
              
              if (backward)
                return find_tab_backward (stack, workspace, i);
              else
                return find_tab_forward (stack, workspace, i);
            }

          ++i;
        }
    }
  
  /* window may be NULL, or maybe the origin window was already the last/first
   * window and we need to wrap around
   */
  if (backward)
    return find_tab_backward (stack, NULL, 
                              stack->windows->len);
  else
    return find_tab_forward (stack, NULL, -1);
}

int
meta_stack_windows_cmp  (MetaStack  *stack,
                         MetaWindow *window_a,
                         MetaWindow *window_b)
{
  g_return_val_if_fail (window_a->screen == window_b->screen, 0);

  /* -1 means a below b */
  
  if (window_a->layer < window_b->layer)
    return -1;
  else if (window_a->layer > window_b->layer)
    return 1;
  else
    {
      GList *tmp;

      g_assert (window_a->layer == window_b->layer);
      
      tmp = stack->layers[window_a->layer];
      while (tmp != NULL)
        {
          /* earlier in list is higher in stack */
          if (tmp->data == window_a)
            return 1;
          else if (tmp->data == window_b)
            return -1;

          tmp = tmp->next;
        }

      meta_bug ("Didn't find windows in layer in meta_stack_windows_cmp()\n");
    }

  /* not reached */
  return 0;
}
