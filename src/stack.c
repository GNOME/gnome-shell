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
#include "group.h"
#include "prefs.h"
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

  stack->last_root_children_stacked = NULL;
  
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

  if (stack->last_root_children_stacked)
    g_array_free (stack->last_root_children_stacked, TRUE);
  
  g_free (stack);
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

  meta_topic (META_DEBUG_STACK, "Adding window %s to the stack\n", window->desc);
  
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

  meta_topic (META_DEBUG_STACK, "Removing window %s from the stack\n", window->desc);
  
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

/* Get layer ignoring any transient or group relationships */
static MetaStackLayer
get_standalone_layer (MetaWindow *window)
{
  MetaStackLayer layer;
  
  switch (window->type)
    {
    case META_WINDOW_DESKTOP:
      layer = META_LAYER_DESKTOP;
      break;

    case META_WINDOW_DOCK:
      /* still experimenting here */
      layer = META_LAYER_DOCK;
      break;

    case META_WINDOW_SPLASHSCREEN:
      layer = META_LAYER_SPLASH;
      break;
      
    default:
      if (window->has_focus &&
          meta_prefs_get_focus_mode () == META_FOCUS_MODE_CLICK)
        layer = META_LAYER_FOCUSED_WINDOW;
      else if (window->fullscreen)
        layer = META_LAYER_FULLSCREEN;
      else
        layer = META_LAYER_NORMAL;
      break;
    }

  return layer;
}

static MetaStackLayer
get_maximum_layer_of_ancestor (MetaWindow *window)
{
  MetaWindow *w;
  MetaStackLayer max;
  MetaStackLayer layer;
  
  max = get_standalone_layer (window);
  
  w = window;
  while (w != NULL)
    {
      if (w->xtransient_for == None ||
          w->transient_parent_is_root_window)
        break;
      
      w = meta_display_lookup_x_window (w->display, w->xtransient_for);
      
      if (w == window)
        break; /* Cute, someone thought they'd make a transient_for cycle */
      
      /* w may be null... */
      if (w != NULL)
        {
          layer = get_standalone_layer (w);
          if (layer > max)
            max = layer;
        }
    }

  return max;
}

/* Note that this function can never use window->layer only
 * get_standalone_layer, or we'd have issues.
 */
static MetaStackLayer
get_maximum_layer_in_group_or_ancestor (MetaWindow *window)
{
  GSList *members;
  MetaGroup *group;
  GSList *tmp;
  MetaStackLayer max;
  MetaStackLayer layer;
  
  max = META_LAYER_DESKTOP;
  
  group = meta_window_get_group (window);

  if (group != NULL)
    members = meta_group_list_windows (group);
  else
    members = NULL;
  
  tmp = members;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;

      layer = get_standalone_layer (w);
      if (layer > max)
        max = layer;
      
      tmp = tmp->next;
    }

  g_slist_free (members);
  
  layer = get_maximum_layer_of_ancestor (window);
  if (layer > max)
    max = layer;
  
  return max;
}

static void
compute_layer (MetaWindow *window)
{
  MetaStackLayer group_max;

  window->layer = get_standalone_layer (window);
  group_max = get_maximum_layer_in_group_or_ancestor (window); 

  if (group_max > window->layer)
    {
      meta_topic (META_DEBUG_STACK,
                  "Promoting window %s from layer %d to %d due to group or transiency\n",
                  window->desc, window->layer, group_max);
      window->layer = group_max;
    }
  
  meta_topic (META_DEBUG_STACK, "Window %s on layer %d type = %d has_focus = %d\n",
              window->desc, window->layer,
              window->type, window->has_focus);
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
      
      if ((w->xtransient_for == None ||
           w->transient_parent_is_root_window) &&
          (w->type == META_WINDOW_DIALOG ||
	   w->type == META_WINDOW_MODAL_DIALOG))
        {
          GSList *group_windows;
          GSList *tmp;
          MetaGroup *group;

          group = meta_window_get_group (w);

          if (group != NULL)
            group_windows = meta_group_list_windows (group);
          else
            group_windows = NULL;
          
          tmp = group_windows;
          
          while (tmp != NULL)
            {
              MetaWindow *group_window = tmp->data;
              
              if (!(meta_window_is_ancestor_of_transient (w, group_window)))
                list = ensure_before (list, w, group_window);
              
              tmp = tmp->next;
            }
        }
      else if (w->xtransient_for != None &&
               !w->transient_parent_is_root_window)
        {
          MetaWindow *parent;
          
          parent =
            meta_display_lookup_x_window (w->display, w->xtransient_for);

          if (parent)
            {
              meta_topic (META_DEBUG_STACK, "Stacking %s above %s due to transiency\n",
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
raise_window_relative_to_managed_windows (MetaScreen *screen,
                                          Window      xwindow)
{
  /* This function is used to avoid raising a window above popup
   * menus and other such things.
   *
   * FIXME This is sort of an expensive function, should probably
   * do something to avoid it. One approach would be to reverse
   * the stacking algorithm to work by placing each window above
   * the others, and start by lowering a window to the bottom
   * (instead of the current way, which works by placing each
   * window below another and starting with a raise)
   */

  Window ignored1, ignored2;
  Window *children;
  int n_children;
  int i;

  /* Normally XQueryTree() means "must grab server" but here
   * we don't, since we know we won't manage any new windows
   * or restack any windows before using the XQueryTree results.
   */
  
  meta_error_trap_push (screen->display);
  
  XQueryTree (screen->display->xdisplay,
              screen->xroot,
              &ignored1, &ignored2, &children, &n_children);

  if (meta_error_trap_pop (screen->display))
    {
      meta_topic (META_DEBUG_STACK,
                  "Error querying root children to raise window 0x%lx\n",
                  xwindow);
      return;
    }

  /* Children are in order from bottom to top. We want to
   * find the topmost managed child, then configure
   * our window to be above it.
   */
  i = n_children - 1;
  while (i >= 0)
    {
      if (children[i] == xwindow)
        {
          /* Do nothing. This means we're already the topmost managed
           * window, but it DOES NOT mean we are already just above
           * the topmost managed window. This is important because if
           * an override redirect window is up, and we map a new
           * managed window, the new window is probably above the old
           * popup by default, and we want to push it below that
           * popup. So keep looking for a sibling managed window
           * to be moved below.
           */
        }
      else if (meta_display_lookup_x_window (screen->display,
                                             children[i]) != NULL)
        {
          XWindowChanges changes;
          
          /* children[i] is the topmost managed child */
          meta_topic (META_DEBUG_STACK,
                      "Moving 0x%lx above topmost managed child window 0x%lx\n",
                      xwindow, children[i]);

          changes.sibling = children[i];
          changes.stack_mode = Above;

          meta_error_trap_push (screen->display);
          XConfigureWindow (screen->display->xdisplay,
                            xwindow,
                            CWSibling | CWStackMode,
                            &changes);
          meta_error_trap_pop (screen->display);

          break;
        }

      --i;
    }

  if (i < 0)
    {
      /* No sibling to use, just lower ourselves to the bottom
       * to be sure we're below any override redirect windows.
       */
      meta_error_trap_push (screen->display);
      XLowerWindow (screen->display->xdisplay,
                    xwindow);
      meta_error_trap_pop (screen->display);
    }
  
  if (children)
    XFree (children);
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
  
  meta_topic (META_DEBUG_STACK, "Syncing window stack to server\n");
  
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
              /* FIXME when we move > 1 window into a new layer
               * within a single stack freeze/thaw bracket,
               * perhaps due to moving a whole window group,
               * the ordering of the newly-added windows in the
               * layer is not defined. So if you raise a whole group
               * from the normal layer to the fullscreen layer, the
               * windows in that group may get randomly reordered.
               */
              
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
           * raise inside the new layer.
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
          meta_topic (META_DEBUG_STACK, "Sorting layer %d\n", i);
          stack->layers[i] = sort_window_list (stack->layers[i]);
        }

      /* ... then append it */
      meta_topic (META_DEBUG_STACK, "Layer %d: ", i);
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

          meta_topic (META_DEBUG_STACK, "%s ", w->desc);
          
          tmp = tmp->next;
        }

      meta_topic (META_DEBUG_STACK, "\n");
      meta_pop_no_msg_prefix ();
    }
  while (i > 0);

  /* All windows should be in some stacking order */
  if (stacked->len != stack->windows->len)
    meta_bug ("%d windows stacked, %d windows exist in stack\n",
              stacked->len, stack->windows->len);
  
  /* Sync to server */

  meta_topic (META_DEBUG_STACK, "Restacking %d windows\n",
              root_children_stacked->len);
  
  meta_error_trap_push (stack->screen->display);

  if (stack->last_root_children_stacked == NULL)
    {
      /* Just impose our stack, we don't know the previous state.
       * This involves a ton of circulate requests and may flicker.
       */
      meta_topic (META_DEBUG_STACK, "Don't know last stack state, restacking everything\n");

      if (root_children_stacked->len > 0)
        XRestackWindows (stack->screen->display->xdisplay,
                         (Window *) root_children_stacked->data,
                         root_children_stacked->len);
    }
  else if (root_children_stacked->len > 0)
    {
      /* Try to do minimal window moves to get the stack in order */
      /* A point of note: these arrays include frames not client windows,
       * so if a client window has changed frame since last_root_children_stacked
       * was saved, then we may have inefficiency, but I don't think things
       * break...
       */
      const Window *old_stack = (Window *) stack->last_root_children_stacked->data;
      const Window *new_stack = (Window *) root_children_stacked->data;
      const int old_len = stack->last_root_children_stacked->len;
      const int new_len = root_children_stacked->len;
      const Window *oldp = old_stack;
      const Window *newp = new_stack;
      const Window *old_end = old_stack + old_len;
      const Window *new_end = new_stack + new_len;
      Window last_window = None;
      
      while (oldp != old_end &&
             newp != new_end)
        {
          if (*oldp == *newp)
            {
              /* Stacks are the same here, move on */
              ++oldp;
              last_window = *newp;
              ++newp;
            }
          else if (meta_display_lookup_x_window (stack->screen->display,
                                                 *oldp) == NULL)
            {
              /* *oldp is no longer known to us (probably destroyed),
               * so we can just skip it
               */
              ++oldp;
            }
          else
            {
              /* Move *newp below last_window */
              if (last_window == None)
                {
                  meta_topic (META_DEBUG_STACK, "Using window 0x%lx as topmost (but leaving it in-place)\n", *newp);

                  raise_window_relative_to_managed_windows (stack->screen,
                                                            *newp);
                }
              else
                {
                  /* This means that if last_window is dead, but not
                   * *newp, then we fail to restack *newp; but on
                   * unmanaging last_window, we'll fix it up.
                   */
                  
                  XWindowChanges changes;

                  changes.sibling = last_window;
                  changes.stack_mode = Below;

                  meta_topic (META_DEBUG_STACK, "Placing window 0x%lx below 0x%lx\n",
                              *newp, last_window);
                  
                  XConfigureWindow (stack->screen->display->xdisplay,
                                    *newp,
                                    CWSibling | CWStackMode,
                                    &changes);
                }

              last_window = *newp;
              ++newp;
            }
        }

      if (newp != new_end)
        {
          /* Restack remaining windows */
          meta_topic (META_DEBUG_STACK, "Restacking remaining %d windows\n",
                        (int) (new_end - newp));
          /* We need to include an already-stacked window
           * in the restack call, so we get in the proper position
           * with respect to it.
           */
          if (newp != new_stack)
            --newp;
          XRestackWindows (stack->screen->display->xdisplay,
                           (Window *) newp, new_end - newp);
        }
    }

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

  if (stack->last_root_children_stacked)
    g_array_free (stack->last_root_children_stacked, TRUE);
  stack->last_root_children_stacked = root_children_stacked;
  
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
                      MetaWindow     *window,
                      gboolean        only_within_layer)
{
  GList *link;

  /* FIXME if stack is frozen this is kind of broken. */
  
  g_assert (window->layer >= 0 && window->layer < META_LAYER_LAST);
  link = g_list_find (stack->layers[window->layer], window);
  if (link == NULL)
    return NULL;

  if (link->prev)
    return link->prev->data;
  else if (only_within_layer)
    return NULL;
  else
    return find_next_above_layer (stack, window->layer);
}

MetaWindow*
meta_stack_get_below (MetaStack      *stack,
                      MetaWindow     *window,
                      gboolean        only_within_layer)
{
  GList *link;

  /* FIXME if stack is frozen this is kind of broken. */

  g_assert (window->layer >= 0 && window->layer < META_LAYER_LAST);
  link = g_list_find (stack->layers[window->layer], window);
  if (link == NULL)
    return NULL;

  if (link->next)
    return link->next->data;
  else if (only_within_layer)
    return NULL;
  else
    return find_prev_below_layer (stack, window->layer);
}                               

MetaWindow*
meta_stack_get_default_focus_window (MetaStack     *stack,
                                     MetaWorkspace *workspace,
                                     MetaWindow    *not_this_one)
{
  /* FIXME if stack is frozen this is kind of broken. */

  /* Find the topmost, focusable, mapped, window. */

  MetaWindow *topmost_dock;
  int layer = META_LAYER_LAST;  

  topmost_dock = NULL;
  
  --layer;
  while (layer >= 0)
    {
      GList *link;

      g_assert (layer >= 0 && layer < META_LAYER_LAST);

      /* top of this layer is at the front of the list */
      link = stack->layers[layer];
      
      while (link)
        {
          MetaWindow *window = link->data;

          if (window &&
              window != not_this_one &&
              (window->unmaps_pending == 0) &&
              !window->minimized &&
              (workspace == NULL ||
               meta_window_visible_on_workspace (window, workspace)))
            {
              if (topmost_dock == NULL &&
                  window->type == META_WINDOW_DOCK)
                topmost_dock = window;
              else if (window->type != META_WINDOW_DOCK)
                return window;
            }

          link = link->next;
        }
      
      --layer;
    }

  /* If we didn't find a window to focus, we use the topmost dock.
   * Note that we already tried the desktop - so we prefer focusing
   * desktop to focusing the dock.
   */
  return topmost_dock;
}

GList*
meta_stack_list_windows (MetaStack     *stack,
                         MetaWorkspace *workspace)
{
  GList *workspace_windows = NULL;
  int layer = META_LAYER_LAST;  

  --layer;
  while (layer >= 0)
    {
      GList *link;

      g_assert (layer >= 0 && layer < META_LAYER_LAST);

      /* top of this layer is at the front of the list */
      link = stack->layers[layer];
      
      while (link)
        {
          MetaWindow *window = link->data;

          if (window && meta_window_visible_on_workspace (window, workspace))
            {
              workspace_windows = g_list_prepend (workspace_windows,
                                                  window);
            }

          link = link->next;
        }
      
      --layer;
    }

  return workspace_windows;
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
