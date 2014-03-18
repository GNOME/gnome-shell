/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * SECTION:stack
 * @short_description: Which windows cover which other windows
 */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2004 Rob Adams
 * Copyright (C) 2004, 2005 Elijah Newren
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "stack.h"
#include "window-private.h"
#include <meta/errors.h>
#include "frame.h"
#include <meta/group.h>
#include <meta/prefs.h>
#include <meta/workspace.h>

#include <X11/Xatom.h>

#define WINDOW_HAS_TRANSIENT_TYPE(w)                    \
          (w->type == META_WINDOW_DIALOG ||             \
	   w->type == META_WINDOW_MODAL_DIALOG ||       \
           w->type == META_WINDOW_TOOLBAR ||            \
           w->type == META_WINDOW_MENU ||               \
           w->type == META_WINDOW_UTILITY)

#define WINDOW_TRANSIENT_FOR_WHOLE_GROUP(w)             \
         ((w->xtransient_for == None ||                 \
           w->transient_parent_is_root_window) &&       \
          WINDOW_HAS_TRANSIENT_TYPE (w))

#define WINDOW_IN_STACK(w) (w->stack_position >= 0)

static void stack_sync_to_xserver (MetaStack *stack);
static void meta_window_set_stack_position_no_sync (MetaWindow *window,
                                                    int         position);
static void stack_do_window_deletions (MetaStack *stack);
static void stack_do_window_additions (MetaStack *stack);
static void stack_do_relayer          (MetaStack *stack);
static void stack_do_constrain        (MetaStack *stack);
static void stack_do_resort           (MetaStack *stack);

static void stack_ensure_sorted (MetaStack *stack);

MetaStack*
meta_stack_new (MetaScreen *screen)
{
  MetaStack *stack;
  
  stack = g_new (MetaStack, 1);

  stack->screen = screen;
  stack->xwindows = g_array_new (FALSE, FALSE, sizeof (Window));

  stack->sorted = NULL;
  stack->added = NULL;
  stack->removed = NULL;

  stack->freeze_count = 0;
  stack->last_all_root_children_stacked = NULL;

  stack->n_positions = 0;

  stack->need_resort = FALSE;
  stack->need_relayer = FALSE;
  stack->need_constrain = FALSE;
  
  return stack;
}

static void
free_last_all_root_children_stacked_cache (MetaStack *stack)
{
  g_array_free (stack->last_all_root_children_stacked, TRUE);
  stack->last_all_root_children_stacked = NULL;
}

void
meta_stack_free (MetaStack *stack)
{
  g_array_free (stack->xwindows, TRUE);

  g_list_free (stack->sorted);
  g_list_free (stack->added);
  g_list_free (stack->removed);

  if (stack->last_all_root_children_stacked)
    free_last_all_root_children_stacked_cache (stack);
  
  g_free (stack);
}

void
meta_stack_add (MetaStack  *stack,
                MetaWindow *window)
{
  meta_topic (META_DEBUG_STACK, "Adding window %s to the stack\n", window->desc);

  if (window->stack_position >= 0)
    meta_bug ("Window %s had stack position already\n", window->desc);
  
  stack->added = g_list_prepend (stack->added, window);

  window->stack_position = stack->n_positions;
  stack->n_positions += 1;
  meta_topic (META_DEBUG_STACK,
              "Window %s has stack_position initialized to %d\n",
              window->desc, window->stack_position);
  
  stack_sync_to_xserver (stack);
  meta_stack_update_window_tile_matches (stack, window->screen->active_workspace);
}

void
meta_stack_remove (MetaStack  *stack,
                   MetaWindow *window)
{
  meta_topic (META_DEBUG_STACK, "Removing window %s from the stack\n", window->desc);

  if (window->stack_position < 0)
    meta_bug ("Window %s removed from stack but had no stack position\n",
              window->desc);

  /* Set window to top position, so removing it will not leave gaps
   * in the set of positions
   */
  meta_window_set_stack_position_no_sync (window,
                                          stack->n_positions - 1);
  window->stack_position = -1;
  stack->n_positions -= 1;  

  /* We don't know if it's been moved from "added" to "stack" yet */
  stack->added = g_list_remove (stack->added, window);
  stack->sorted = g_list_remove (stack->sorted, window);

  /* Remember the window ID to remove it from the stack array.
   * The macro is safe to use: Window is guaranteed to be 32 bits, and
   * GUINT_TO_POINTER says it only works on 32 bits.
   */
  stack->removed = g_list_prepend (stack->removed,
                                   GUINT_TO_POINTER (window->xwindow));
  if (window->frame)
    stack->removed = g_list_prepend (stack->removed,
                                     GUINT_TO_POINTER (window->frame->xwindow));
  
  stack_sync_to_xserver (stack);
  meta_stack_update_window_tile_matches (stack, window->screen->active_workspace);
}

void
meta_stack_update_layer (MetaStack  *stack,
                         MetaWindow *window)
{
  stack->need_relayer = TRUE;
  
  stack_sync_to_xserver (stack);
  meta_stack_update_window_tile_matches (stack, window->screen->active_workspace);
}

void
meta_stack_update_transient (MetaStack  *stack,
                             MetaWindow *window)
{
  stack->need_constrain = TRUE;
  
  stack_sync_to_xserver (stack);
  meta_stack_update_window_tile_matches (stack, window->screen->active_workspace);
}

/* raise/lower within a layer */
void
meta_stack_raise (MetaStack  *stack,
                  MetaWindow *window)
{
  GList *l;
  int max_stack_position = window->stack_position;
  MetaWorkspace *workspace;

  stack_ensure_sorted (stack);

  workspace = meta_window_get_workspace (window);
  for (l = stack->sorted; l; l = l->next)
    {
      MetaWindow *w = (MetaWindow *) l->data;
      if (meta_window_located_on_workspace (w, workspace) &&
          w->stack_position > max_stack_position)
        max_stack_position = w->stack_position;
    }

  if (max_stack_position == window->stack_position)
    return;

  meta_window_set_stack_position_no_sync (window, max_stack_position);

  stack_sync_to_xserver (stack);
  meta_stack_update_window_tile_matches (stack, window->screen->active_workspace);
}

void
meta_stack_lower (MetaStack  *stack,
                  MetaWindow *window)
{
  GList *l;
  int min_stack_position = window->stack_position;
  MetaWorkspace *workspace;

  stack_ensure_sorted (stack);

  workspace = meta_window_get_workspace (window);
  for (l = stack->sorted; l; l = l->next)
    {
      MetaWindow *w = (MetaWindow *) l->data;
      if (meta_window_located_on_workspace (w, workspace) &&
          w->stack_position < min_stack_position)
        min_stack_position = w->stack_position;
    }

  if (min_stack_position == window->stack_position)
    return;

  meta_window_set_stack_position_no_sync (window, min_stack_position);
  
  stack_sync_to_xserver (stack);
  meta_stack_update_window_tile_matches (stack, window->screen->active_workspace);
}

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
  stack_sync_to_xserver (stack);
  meta_stack_update_window_tile_matches (stack, NULL);
}

void
meta_stack_update_window_tile_matches (MetaStack     *stack,
                                       MetaWorkspace *workspace)
{
  GList *windows, *tmp;

  if (stack->freeze_count > 0)
    return;

  windows = meta_stack_list_windows (stack, workspace);
  tmp = windows;
  while (tmp)
    {
      meta_window_compute_tile_match ((MetaWindow *) tmp->data);
      tmp = tmp->next;
    }

  g_list_free (windows);
}

static gboolean
is_focused_foreach (MetaWindow *window,
                    void       *data)
{
  if (window->has_focus)
    {
      *((gboolean*) data) = TRUE;
      return FALSE;
    }
  return TRUE;
}

static gboolean
windows_on_different_monitor (MetaWindow *a,
                              MetaWindow *b)
{
  if (a->screen != b->screen)
    return TRUE;

  return meta_screen_get_monitor_for_window (a->screen, a) !=
    meta_screen_get_monitor_for_window (b->screen, b);
}

/* Get layer ignoring any transient or group relationships */
static MetaStackLayer
get_standalone_layer (MetaWindow *window)
{
  MetaStackLayer layer;
  gboolean focused_transient = FALSE;

  switch (window->type)
    {
    case META_WINDOW_DESKTOP:
      layer = META_LAYER_DESKTOP;
      break;

    case META_WINDOW_DOCK:
      /* still experimenting here */
      if (window->wm_state_below)
        layer = META_LAYER_BOTTOM;
      else
        layer = META_LAYER_DOCK;
      break;

    case META_WINDOW_DROPDOWN_MENU:
    case META_WINDOW_POPUP_MENU:
    case META_WINDOW_TOOLTIP:
    case META_WINDOW_NOTIFICATION:
    case META_WINDOW_COMBO:
    case META_WINDOW_OVERRIDE_OTHER:
      layer = META_LAYER_OVERRIDE_REDIRECT;
      break;
    default:       
      meta_window_foreach_transient (window,
                                     is_focused_foreach,
                                     &focused_transient);

      if (window->wm_state_below)
        layer = META_LAYER_BOTTOM;
      else if (window->fullscreen &&
               (focused_transient ||
                window == window->display->focus_window ||
                window->display->focus_window == NULL ||
                (window->display->focus_window != NULL &&
                 windows_on_different_monitor (window,
                                               window->display->focus_window))))
        layer = META_LAYER_FULLSCREEN;
      else if (window->wm_state_above && !META_WINDOW_MAXIMIZED (window))
        layer = META_LAYER_TOP;
      else
        layer = META_LAYER_NORMAL;
      break;
    }

  return layer;
}

/* Note that this function can never use window->layer only
 * get_standalone_layer, or we'd have issues.
 */
static MetaStackLayer
get_maximum_layer_in_group (MetaWindow *window)
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

      if (!w->override_redirect)
        {
          layer = get_standalone_layer (w);
          if (layer > max)
            max = layer;
        }
      
      tmp = tmp->next;
    }

  g_slist_free (members);
  
  return max;
}

static void
compute_layer (MetaWindow *window)
{
  MetaStackLayer old_layer = window->layer;

  window->layer = get_standalone_layer (window);
  
  /* We can only do promotion-due-to-group for dialogs and other
   * transients, or weird stuff happens like the desktop window and
   * nautilus windows getting in the same layer, or all gnome-terminal
   * windows getting in fullscreen layer if any terminal is
   * fullscreen.
   */
  if (window->layer != META_LAYER_DESKTOP &&
      WINDOW_HAS_TRANSIENT_TYPE(window) &&
      (window->xtransient_for == None ||
       window->transient_parent_is_root_window))
    {
      /* We only do the group thing if the dialog is NOT transient for
       * a particular window. Imagine a group with a normal window, a dock,
       * and a dialog transient for the normal window; you don't want the dialog
       * above the dock if it wouldn't normally be.
       */
      
      MetaStackLayer group_max;
      
      group_max = get_maximum_layer_in_group (window);
      
      if (group_max > window->layer)
        {
          meta_topic (META_DEBUG_STACK,
                      "Promoting window %s from layer %u to %u due to group membership\n",
                      window->desc, window->layer, group_max);
          window->layer = group_max;
        }
    }

  meta_topic (META_DEBUG_STACK, "Window %s on layer %u type = %u has_focus = %d\n",
              window->desc, window->layer,
              window->type, window->has_focus);

  if (window->layer != old_layer &&
      (old_layer == META_LAYER_FULLSCREEN || window->layer == META_LAYER_FULLSCREEN))
    meta_screen_queue_check_fullscreen (window->screen);
}

/* Front of the layer list is the topmost window,
 * so the lower stack position is later in the list
 */
static int
compare_window_position (void *a,
                         void *b)
{
  MetaWindow *window_a = a;
  MetaWindow *window_b = b;

  /* Go by layer, then stack_position */
  if (window_a->layer < window_b->layer)
    return 1; /* move window_a later in list */
  else if (window_a->layer > window_b->layer)
    return -1;
  else if (window_a->stack_position < window_b->stack_position)
    return 1; /* move window_a later in list */
  else if (window_a->stack_position > window_b->stack_position)
    return -1;
  else
    return 0; /* not reached */
}
  
/*
 * Stacking constraints
 * 
 * Assume constraints of the form "AB" meaning "window A must be
 * below window B"
 *
 * If we have windows stacked from bottom to top
 * "ABC" then raise A we get "BCA". Say C is
 * transient for B is transient for A. So
 * we have constraints AB and BC.
 *
 * After raising A, we need to reapply the constraints.
 * If we do this by raising one window at a time -
 *
 *  start:    BCA
 *  apply AB: CAB
 *  apply BC: ABC
 *
 * but apply constraints in the wrong order and it breaks:
 * 
 *  start:    BCA
 *  apply BC: BCA
 *  apply AB: CAB
 *
 * We make a directed graph of the constraints by linking
 * from "above windows" to "below windows as follows:
 * 
 *   AB -> BC -> CD
 *          \
 *           CE
 *
 * If we then walk that graph and apply the constraints in the order
 * that they appear, we will apply them correctly. Note that the
 * graph MAY have cycles, so we have to guard against that.
 *
 */

typedef struct Constraint Constraint;

struct Constraint
{
  MetaWindow *above;
  MetaWindow *below;

  /* used to keep the constraint in the
   * list of constraints for window "below"
   */
  Constraint *next;

  /* used to create the graph. */
  GSList *next_nodes;
  
  /* constraint has been applied, used
   * to detect cycles.
   */
  unsigned int applied : 1;

  /* constraint has a previous node in the graph,
   * used to find places to start in the graph.
   * (I think this also has the side effect
   * of preventing cycles, since cycles will
   * have no starting point - so maybe
   * the "applied" flag isn't needed.)
   */
  unsigned int has_prev : 1;
};

/* We index the array of constraints by window
 * stack positions, just because the stack
 * positions are a convenient index.
 */
static void
add_constraint (Constraint **constraints,
                MetaWindow  *above,
                MetaWindow  *below)
{
  Constraint *c;

  g_assert (above->screen == below->screen);
  
  /* check if constraint is a duplicate */
  c = constraints[below->stack_position];
  while (c != NULL)
    {
      if (c->above == above)
        return;
      c = c->next;
    }

  /* if not, add the constraint */
  c = g_new (Constraint, 1);
  c->above = above;
  c->below = below;
  c->next = constraints[below->stack_position];
  c->next_nodes = NULL;
  c->applied = FALSE;
  c->has_prev = FALSE;

  constraints[below->stack_position] = c;
}

static void
create_constraints (Constraint **constraints,
                    GList       *windows)
{
  GList *tmp;
  
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;

      if (!WINDOW_IN_STACK (w))
        {
          meta_topic (META_DEBUG_STACK, "Window %s not in the stack, not constraining it\n",
                      w->desc);
          tmp = tmp->next;
          continue;
        }
      
      if (WINDOW_TRANSIENT_FOR_WHOLE_GROUP (w))
        {
          GSList *group_windows;
          GSList *tmp2;
          MetaGroup *group;

          group = meta_window_get_group (w);

          if (group != NULL)
            group_windows = meta_group_list_windows (group);
          else
            group_windows = NULL;
          
          tmp2 = group_windows;
          
          while (tmp2 != NULL)
            {
              MetaWindow *group_window = tmp2->data;

              if (!WINDOW_IN_STACK (group_window) ||
                  w->screen != group_window->screen ||
                  group_window->override_redirect)
                {
                  tmp2 = tmp2->next;
                  continue;
                }
              
#if 0
              /* old way of doing it */
              if (!(meta_window_is_ancestor_of_transient (w, group_window)) &&
                  !WINDOW_TRANSIENT_FOR_WHOLE_GROUP (group_window))  /* note */;/*note*/
#else
              /* better way I think, so transient-for-group are constrained
               * only above non-transient-type windows in their group
               */
              if (!WINDOW_HAS_TRANSIENT_TYPE (group_window))
#endif
                {
                  meta_topic (META_DEBUG_STACK, "Constraining %s above %s as it's transient for its group\n",
                              w->desc, group_window->desc);
                  add_constraint (constraints, w, group_window);
                }
              
              tmp2 = tmp2->next;
            }

          g_slist_free (group_windows);
        }
      else if (w->xtransient_for != None &&
               !w->transient_parent_is_root_window)
        {
          MetaWindow *parent;
          
          parent =
            meta_display_lookup_x_window (w->display, w->xtransient_for);

          if (parent && WINDOW_IN_STACK (parent) &&
              parent->screen == w->screen)
            {
              meta_topic (META_DEBUG_STACK, "Constraining %s above %s due to transiency\n",
                          w->desc, parent->desc);
              add_constraint (constraints, w, parent);
            }
        }
      
      tmp = tmp->next;
    }
}

static void
graph_constraints (Constraint **constraints,
                   int          n_constraints)
{
  int i;

  i = 0;
  while (i < n_constraints)
    {
      Constraint *c;

      /* If we have "A below B" and "B below C" then AB -> BC so we
       * add BC to next_nodes in AB.
       */
      
      c = constraints[i];
      while (c != NULL)
        {
          Constraint *n;
            
          g_assert (c->below->stack_position == i);

          /* Constraints where ->above is below are our
           * next_nodes and we are their previous
           */
          n = constraints[c->above->stack_position];
          while (n != NULL)
            {
              c->next_nodes = g_slist_prepend (c->next_nodes,
                                               n);
              /* c is a previous node of n */
              n->has_prev = TRUE;
              
              n = n->next;
            }
          
          c = c->next;
        }

      ++i;
    }
}

static void
free_constraints (Constraint **constraints,
                  int          n_constraints)
{
  int i;

  i = 0;
  while (i < n_constraints)
    {
      Constraint *c;
      
      c = constraints[i];
      while (c != NULL)
        {
          Constraint *next = c->next;
          
          g_slist_free (c->next_nodes);

          g_free (c);
          
          c = next;
        }

      ++i;
    }
}

static void
ensure_above (MetaWindow *above,
              MetaWindow *below)
{  
  if (WINDOW_HAS_TRANSIENT_TYPE(above) &&
      above->layer < below->layer)
    {
      meta_topic (META_DEBUG_STACK,
		  "Promoting window %s from layer %u to %u due to contraint\n",
		  above->desc, above->layer, below->layer);
      above->layer = below->layer;
    }

  if (above->stack_position < below->stack_position)
    {
      /* move above to below->stack_position bumping below down the stack */
      meta_window_set_stack_position_no_sync (above, below->stack_position);
      g_assert (below->stack_position + 1 == above->stack_position);
    }
  meta_topic (META_DEBUG_STACK, "%s above at %d > %s below at %d\n",
              above->desc, above->stack_position,
              below->desc, below->stack_position);
}

static void
traverse_constraint (Constraint *c)
{
  GSList *tmp;

  if (c->applied)
    return;
  
  ensure_above (c->above, c->below);
  c->applied = TRUE;
  
  tmp = c->next_nodes;
  while (tmp != NULL)
    {
      traverse_constraint (tmp->data);

      tmp = tmp->next;
    }
}

static void
apply_constraints (Constraint **constraints,
                   int          n_constraints)
{
  GSList *heads;
  GSList *tmp;
  int i;

  /* List all heads in an ordered constraint chain */
  heads = NULL;
  i = 0;
  while (i < n_constraints)
    {
      Constraint *c;
      
      c = constraints[i];
      while (c != NULL)
        {
          if (!c->has_prev)
            heads = g_slist_prepend (heads, c);
          
          c = c->next;
        }

      ++i;
    }

  /* Now traverse the chain and apply constraints */
  tmp = heads;
  while (tmp != NULL)
    {
      Constraint *c = tmp->data;

      traverse_constraint (c);
      
      tmp = tmp->next;
    }

  g_slist_free (heads);
}

/**
 * stack_do_window_deletions:
 *
 * Go through "deleted" and take the matching windows
 * out of "windows".
 */
static void
stack_do_window_deletions (MetaStack *stack)
{
  /* Do removals before adds, with paranoid idea that we might re-add
   * the same window IDs.
   */
  GList *tmp;
  int i;
    
  tmp = stack->removed;
  while (tmp != NULL)
    {
      Window xwindow;
      xwindow = GPOINTER_TO_UINT (tmp->data);

      /* We go from the end figuring removals are more
       * likely to be recent.
       */
      i = stack->xwindows->len;
      while (i > 0)
        {
          --i;
          
          /* there's no guarantee we'll actually find windows to
           * remove, e.g. the same xwindow could have been
           * added/removed before we ever synced, and we put
           * both the window->xwindow and window->frame->xwindow
           * in the removal list.
           */
          if (xwindow == g_array_index (stack->xwindows, Window, i))
            {
              g_array_remove_index (stack->xwindows, i);
              goto next;
            }
        }

    next:
      tmp = tmp->next;
    }

  g_list_free (stack->removed);
  stack->removed = NULL;
}

static void
stack_do_window_additions (MetaStack *stack)
{
  GList *tmp;
  gint i, n_added;

  n_added = g_list_length (stack->added);
  if (n_added > 0)
    {
      Window *end;
      int old_size;

      meta_topic (META_DEBUG_STACK,
                  "Adding %d windows to sorted list\n",
                  n_added);
      
      old_size = stack->xwindows->len;
      g_array_set_size (stack->xwindows, old_size + n_added);
      
      end = &g_array_index (stack->xwindows, Window, old_size);

      /* stack->added has the most recent additions at the
       * front of the list, so we need to reverse it
       */
      stack->added = g_list_reverse (stack->added);
      
      i = 0;
      tmp = stack->added;
      while (tmp != NULL)
        {
          MetaWindow *w;
          
          w = tmp->data;
          
          end[i] = w->xwindow;

          /* add to the main list */
          stack->sorted = g_list_prepend (stack->sorted, w);
          
          ++i;
          tmp = tmp->next;
        }
      
      stack->need_resort = TRUE; /* may not be needed as we add to top */
      stack->need_constrain = TRUE;
      stack->need_relayer = TRUE;
    }

  g_list_free (stack->added);
  stack->added = NULL;
}

/**
 * stack_do_relayer:
 *
 * Update the layers that windows are in
 */
static void
stack_do_relayer (MetaStack *stack)
{
  GList *tmp;
    
  if (!stack->need_relayer)
      return;
    
  meta_topic (META_DEBUG_STACK,
              "Recomputing layers\n");
      
  tmp = stack->sorted;

  while (tmp != NULL)
    {
      MetaWindow *w;
      MetaStackLayer old_layer;

      w = tmp->data;
      old_layer = w->layer;

      compute_layer (w);

      if (w->layer != old_layer)
        {
          meta_topic (META_DEBUG_STACK,
                      "Window %s moved from layer %u to %u\n",
                      w->desc, old_layer, w->layer);
          stack->need_resort = TRUE;
          stack->need_constrain = TRUE;
          /* don't need to constrain as constraining
           * purely operates in terms of stack_position
           * not layer
           */
        }
          
      tmp = tmp->next;
    }

  stack->need_relayer = FALSE;
}

/**
 * stack_do_constrain:
 *
 * Update stack_position and layer to reflect transiency
 * constraints
 */
static void
stack_do_constrain (MetaStack *stack)
{
  Constraint **constraints;

  /* It'd be nice if this were all faster, probably */
  
  if (!stack->need_constrain)
    return;

  meta_topic (META_DEBUG_STACK,
              "Reapplying constraints\n");

  constraints = g_new0 (Constraint*,
                        stack->n_positions);

  create_constraints (constraints, stack->sorted);

  graph_constraints (constraints, stack->n_positions);

  apply_constraints (constraints, stack->n_positions);
  
  free_constraints (constraints, stack->n_positions);
  g_free (constraints);
  
  stack->need_constrain = FALSE;
}

/**
 * stack_do_resort:
 *
 * Sort stack->sorted with layers having priority over stack_position.
 */
static void
stack_do_resort (MetaStack *stack)
{
  if (!stack->need_resort)
    return;
  
  meta_topic (META_DEBUG_STACK,
              "Sorting stack list\n");
      
  stack->sorted = g_list_sort (stack->sorted,
                               (GCompareFunc) compare_window_position);

  stack->need_resort = FALSE;
}

/**
 * stack_ensure_sorted:
 *
 * Puts the stack into canonical form.
 *
 * Honour the removed and added lists of the stack, and then recalculate
 * all the layers (if the flag is set), re-run all the constraint calculations
 * (if the flag is set), and finally re-sort the stack (if the flag is set,
 * and if it wasn't already it might have become so during all the previous
 * activity).
 */
static void
stack_ensure_sorted (MetaStack *stack)
{
  stack_do_window_deletions (stack);
  stack_do_window_additions (stack);
  stack_do_relayer (stack);
  stack_do_constrain (stack);
  stack_do_resort (stack);
}

static MetaStackWindow *
find_top_most_managed_window (MetaScreen *screen,
                              const MetaStackWindow *ignore)
{
  MetaStackTracker *stack_tracker = screen->stack_tracker;
  MetaStackWindow *windows;
  int n_windows;
  int i;

  meta_stack_tracker_get_stack (stack_tracker,
                                &windows, &n_windows);

  /* Children are in order from bottom to top. We want to
   * find the topmost managed child, then configure
   * our window to be above it.
   */
  for (i = n_windows -1; i >= 0; i--)
    {
      MetaStackWindow *other_window = &windows[i];

      if (other_window->any.type == ignore->any.type &&
          ((other_window->any.type == META_WINDOW_CLIENT_TYPE_X11 &&
            other_window->x11.xwindow == ignore->x11.xwindow) ||
           other_window->wayland.meta_window == ignore->wayland.meta_window))
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
      else
        {
          if (other_window->any.type == META_WINDOW_CLIENT_TYPE_X11)
            {
              MetaWindow *other = meta_display_lookup_x_window (screen->display,
                                                                other_window->x11.xwindow);

              if (other != NULL && !other->override_redirect)
                return other_window;
            }
          else
            {
              /* All wayland windows are currently considered "managed"
               * TODO: consider wayland pop-up windows like override
               * redirect windows here. */
              return other_window;
            }
        }
    }

  return NULL;
}

/* When moving an X window we sometimes need an X based sibling.
 *
 * If the given sibling is X based this function returns it back
 * otherwise it searches downwards looking for the nearest X window.
 *
 * If no X based sibling could be found return NULL. */
static MetaStackWindow *
find_x11_sibling_downwards (MetaScreen *screen,
                            MetaStackWindow *sibling)
{
  MetaStackTracker *stack_tracker = screen->stack_tracker;
  MetaStackWindow *windows;
  int n_windows;
  int i;

  if (sibling->any.type == META_WINDOW_CLIENT_TYPE_X11)
    return sibling;

  meta_stack_tracker_get_stack (stack_tracker,
                                &windows, &n_windows);

  /* NB: Children are in order from bottom to top and we
   * want to search downwards for the nearest X window.
   */

  for (i = n_windows - 1; i >= 0; i--)
    if (meta_stack_window_equal (&windows[i], sibling))
      break;

  for (; i >= 0; i--)
    {
      if (windows[i].any.type == META_WINDOW_CLIENT_TYPE_X11)
        return &windows[i];
    }

  return NULL;
}

/**
 * raise_window_relative_to_managed_windows:
 *
 * This function is used to avoid raising a window above popup
 * menus and other such things.
 *
 * The key to the operation of this function is that we are expecting
 * at most one window to be added at a time. If xwindow is newly added,
 * then its own stack position will be too high (the frame window
 * is created at the top of the stack), but if we ignore xwindow,
 * then the *next* managed window in the stack will be a window that
 * we've already stacked.
 *
 * We could generalize this and remove the assumption that windows
 * are added one at a time by keeping an explicit ->stacked flag in
 * MetaWindow.
 *
 * An alternate approach would be to reverse the stacking algorithm to
 * work by placing each window above the others, and start by lowering
 * a window to the bottom (instead of the current way, which works by
 * placing each window below another and starting with a raise)
 */
static void
raise_window_relative_to_managed_windows (MetaScreen *screen,
                                          const MetaStackWindow *window)
{
  gulong serial = 0;
  MetaStackWindow *sibling;

  sibling = find_top_most_managed_window (screen, window);
  if (!sibling)
    {
      if (window->any.type == META_WINDOW_CLIENT_TYPE_X11)
        {
          serial = XNextRequest (screen->display->xdisplay);
          meta_error_trap_push (screen->display);
          XLowerWindow (screen->display->xdisplay,
                        window->x11.xwindow);
          meta_error_trap_pop (screen->display);
        }

      /* No sibling to use, just lower ourselves to the bottom
       * to be sure we're below any override redirect windows.
           */
      meta_stack_tracker_record_lower (screen->stack_tracker,
                                       window,
                                       serial);
      return;
        }

  /* window is the topmost managed child */
              meta_topic (META_DEBUG_STACK,
                          "Moving 0x%lx above topmost managed child window 0x%lx\n",
              window->any.type == META_WINDOW_CLIENT_TYPE_X11 ? window->x11.xwindow: 0,
              sibling->any.type == META_WINDOW_CLIENT_TYPE_X11 ? sibling->x11.xwindow: 0);

  if (window->any.type == META_WINDOW_CLIENT_TYPE_X11)
    {
      XWindowChanges changes;
      MetaStackWindow *x11_sibling = find_x11_sibling_downwards (screen, sibling);
      serial = XNextRequest (screen->display->xdisplay);

      if (x11_sibling)
        {
          changes.sibling = x11_sibling->x11.xwindow;
              changes.stack_mode = Above;

              meta_error_trap_push (screen->display);
              XConfigureWindow (screen->display->xdisplay,
                            window->x11.xwindow,
                                CWSibling | CWStackMode,
                                &changes);
              meta_error_trap_pop (screen->display);
        }
      else
    {
      /* No sibling to use, just lower ourselves to the bottom
       * to be sure we're below any override redirect windows.
       */
      meta_error_trap_push (screen->display);
      XLowerWindow (screen->display->xdisplay,
                        window->x11.xwindow);
      meta_error_trap_pop (screen->display);
    }
}

  meta_stack_tracker_record_raise_above (screen->stack_tracker,
                                         window,
                                         sibling,
                                         serial);
}

/**
 * stack_sync_to_server:
 *
 * Order the windows on the X server to be the same as in our structure.
 * We do this using XRestackWindows if we don't know the previous order,
 * or XConfigureWindow on a few particular windows if we do and can figure
 * out the minimum set of changes.  After that, we set __NET_CLIENT_LIST
 * and __NET_CLIENT_LIST_STACKING.
 *
 * FIXME: Now that we have a good view of the stacking order on the server
 * with MetaStackTracker it should be possible to do a simpler and better
 * job of computing the minimal set of stacking requests needed.
 */
static void
stack_sync_to_xserver (MetaStack *stack)
{
  GArray *x11_stacked;
  GArray *x11_root_children_stacked;
  GArray *all_root_children_stacked; /* wayland OR x11 */
  GList *tmp;
  GArray *x11_hidden;
  GArray *x11_hidden_stack_windows;
  int n_override_redirect = 0;
  MetaStackWindow guard_stack_window;
  
  /* Bail out if frozen */
  if (stack->freeze_count > 0)
    return;
  
  meta_topic (META_DEBUG_STACK, "Syncing window stack to server\n");  

  stack_ensure_sorted (stack);

  /* Create stacked xwindow arrays.
   * Painfully, "stacked" is in bottom-to-top order for the
   * _NET hints, and "root_children_stacked" is in top-to-bottom
   * order for XRestackWindows()
   */
  x11_stacked = g_array_new (FALSE, FALSE, sizeof (Window));

  all_root_children_stacked = g_array_new (FALSE, FALSE, sizeof (MetaStackWindow));
  x11_root_children_stacked = g_array_new (FALSE, FALSE, sizeof (Window));

  x11_hidden_stack_windows = g_array_new (FALSE, FALSE, sizeof (MetaStackWindow));
  x11_hidden = g_array_new (FALSE, FALSE, sizeof (Window));

  /* The screen guard window sits above all hidden windows and acts as
   * a barrier to input reaching these windows. */
  g_array_append_val (x11_hidden, stack->screen->guard_window);

  meta_topic (META_DEBUG_STACK, "Top to bottom: ");
  meta_push_no_msg_prefix ();

  for (tmp = stack->sorted; tmp != NULL; tmp = tmp->next)
    {
      MetaWindow *w = tmp->data;
      Window top_level_window;
      MetaStackWindow stack_window;

      stack_window.any.type = w->client_type;
      
      meta_topic (META_DEBUG_STACK, "%u:%d - %s ",
		  w->layer, w->stack_position, w->desc);

      /* remember, stacked is in reverse order (bottom to top) */
      if (w->override_redirect)
	n_override_redirect++;
      else
	g_array_prepend_val (x11_stacked, w->xwindow);
      
      if (w->frame)
	top_level_window = w->frame->xwindow;
      else
	top_level_window = w->xwindow;

      if (w->client_type == META_WINDOW_CLIENT_TYPE_X11)
        stack_window.x11.xwindow = top_level_window;
      else
        stack_window.wayland.meta_window = w;

      /* We don't restack hidden windows along with the rest, though they are
       * reflected in the _NET hints. Hidden windows all get pushed below
       * the screens fullscreen guard_window. */
      if (w->hidden)
	{
          if (w->client_type == META_WINDOW_CLIENT_TYPE_X11)
            {
              MetaStackWindow stack_window;

              stack_window.any.type = META_WINDOW_CLIENT_TYPE_X11;
              stack_window.x11.xwindow = top_level_window;

              g_array_append_val (x11_hidden_stack_windows, stack_window);
              g_array_append_val (x11_hidden, top_level_window);
            }
	  continue;
	}

      g_array_append_val (all_root_children_stacked, stack_window);

      /* build XRestackWindows() array from top to bottom */
      if (w->client_type == META_WINDOW_CLIENT_TYPE_X11)
        g_array_append_val (x11_root_children_stacked, top_level_window);
    }

  meta_topic (META_DEBUG_STACK, "\n");
  meta_pop_no_msg_prefix ();

  /* All X windows should be in some stacking order */
  if (x11_stacked->len != stack->xwindows->len - n_override_redirect)
    meta_bug ("%u windows stacked, %u windows exist in stack\n",
              x11_stacked->len, stack->xwindows->len);
  
  /* Sync to server */

  meta_topic (META_DEBUG_STACK, "Restacking %u windows\n",
              all_root_children_stacked->len);
  
  meta_error_trap_push (stack->screen->display);

  if (stack->last_all_root_children_stacked == NULL)
    {
      /* Just impose our stack, we don't know the previous state.
       * This involves a ton of circulate requests and may flicker.
       */
      meta_topic (META_DEBUG_STACK, "Don't know last stack state, restacking everything\n");

      if (all_root_children_stacked->len > 1)
        {
          gulong serial = 0;
          if (x11_root_children_stacked->len > 1)
            {
              serial = XNextRequest (stack->screen->display->xdisplay);
          XRestackWindows (stack->screen->display->xdisplay,
                               (Window *) x11_root_children_stacked->data,
                               x11_root_children_stacked->len);
            }
          meta_stack_tracker_record_restack_windows (stack->screen->stack_tracker,
                                                     (MetaStackWindow *) all_root_children_stacked->data,
                                                     all_root_children_stacked->len,
                                                     serial);
        }
    }
  else if (all_root_children_stacked->len > 0)
    {
      /* Try to do minimal window moves to get the stack in order */
      /* A point of note: these arrays include frames not client windows,
       * so if a client window has changed frame since last_root_children_stacked
       * was saved, then we may have inefficiency, but I don't think things
       * break...
       */
      const MetaStackWindow *old_stack = (MetaStackWindow *) stack->last_all_root_children_stacked->data;
      const MetaStackWindow *new_stack = (MetaStackWindow *) all_root_children_stacked->data;
      const int old_len = stack->last_all_root_children_stacked->len;
      const int new_len = all_root_children_stacked->len;
      const MetaStackWindow *oldp = old_stack;
      const MetaStackWindow *newp = new_stack;
      const MetaStackWindow *old_end = old_stack + old_len;
      const MetaStackWindow *new_end = new_stack + new_len;
      Window last_xwindow = None;
      const MetaStackWindow *last_window = NULL;

      while (oldp != old_end &&
             newp != new_end)
        {
          if (meta_stack_window_equal (oldp, newp))
            {
              /* Stacks are the same here, move on */
              ++oldp;
              if (newp->any.type == META_WINDOW_CLIENT_TYPE_X11)
                last_xwindow = newp->x11.xwindow;
              last_window = newp;
              ++newp;
            }
          else if ((oldp->any.type == META_WINDOW_CLIENT_TYPE_X11 &&
                    meta_display_lookup_x_window (stack->screen->display,
                                                  oldp->x11.xwindow) == NULL) ||
                   (oldp->any.type == META_WINDOW_CLIENT_TYPE_WAYLAND &&
                    oldp->wayland.meta_window == NULL))
            {
              /* *oldp is no longer known to us (probably destroyed),
               * so we can just skip it
               */
              ++oldp;
            }
          else
            {
              /* Move *newp below the last_window */
              if (!last_window)
                {
                  meta_topic (META_DEBUG_STACK, "Using window 0x%lx as topmost (but leaving it in-place)\n",
                              newp->x11.xwindow);

                  raise_window_relative_to_managed_windows (stack->screen, newp);
                }
              else if (newp->any.type == META_WINDOW_CLIENT_TYPE_X11 &&
                       last_xwindow == None)
                {
                  /* In this case we have an X window that we need to
                   * put below a wayland window and this is the
                   * topmost X window. */
                  
                  /* In X terms (because this is the topmost X window)
                   * we want to
                   * raise_window_relative_to_managed_windows() to
                   * ensure the X window is below override-redirect
                   * pop-up windows.
                   *
                   * In Wayland terms we just want to ensure
                   * newp is lowered below last_window (which
                   * notably doesn't require an X request because we
                   * know last_window isn't an X window).
                   */

                  raise_window_relative_to_managed_windows (stack->screen, newp);

                  meta_stack_tracker_record_lower_below (stack->screen->stack_tracker,
                                                         newp, last_window,
                                                         0); /* no x request serial */
                }
              else
                {
                  gulong serial = 0;

                  /* This means that if last_xwindow is dead, but not
                   * *newp, then we fail to restack *newp; but on
                   * unmanaging last_xwindow, we'll fix it up.
                   */
                  
                  meta_topic (META_DEBUG_STACK, "Placing window 0x%lx below 0x%lx\n",
                              newp->any.type == META_WINDOW_CLIENT_TYPE_X11 ? newp->x11.xwindow : 0,
                              last_xwindow);

                  if (newp->any.type == META_WINDOW_CLIENT_TYPE_X11)
                    {
                  XWindowChanges changes;
                      serial = XNextRequest (stack->screen->display->xdisplay);

                      changes.sibling = last_xwindow;
                  changes.stack_mode = Below;

                  XConfigureWindow (stack->screen->display->xdisplay,
                                        newp->x11.xwindow,
                                    CWSibling | CWStackMode,
                                    &changes);
                }

                  meta_stack_tracker_record_lower_below (stack->screen->stack_tracker,
                                                         newp, last_window,
                                                         serial);
                }

              if (newp->any.type == META_WINDOW_CLIENT_TYPE_X11)
                last_xwindow = newp->x11.xwindow;
              last_window = newp;
              ++newp;
            }
        }

      if (newp != new_end)
        {
          const MetaStackWindow *x_ref;
          unsigned long serial = 0;

          /* Restack remaining windows */
          meta_topic (META_DEBUG_STACK, "Restacking remaining %d windows\n",
                        (int) (new_end - newp));

          /* rewind until we find the last stacked X window that we can use
           * as a reference point for re-stacking remaining X windows */
          if (newp != new_stack)
            for (x_ref = newp - 1;
                 x_ref->any.type != META_WINDOW_CLIENT_TYPE_X11 && x_ref > new_stack;
                 x_ref--)
              ;
          else
            x_ref = new_stack;

          /* If we didn't find an X window looking backwards then walk forwards
           * through the remaining windows to find the first remaining X window
           * instead. */
          if (x_ref->any.type != META_WINDOW_CLIENT_TYPE_X11)
            {
              for (x_ref = newp;
                   x_ref->any.type != META_WINDOW_CLIENT_TYPE_X11 && x_ref > new_stack;
                   x_ref++)
                ;
            }

          /* If there are any X windows remaining unstacked then restack them */
          if (x_ref->any.type == META_WINDOW_CLIENT_TYPE_X11)
            {
              int i;

              for (i = x11_root_children_stacked->len - 1; i; i--)
                {
                  Window *reference = &g_array_index (x11_root_children_stacked, Window, i);

                  if (*reference == x_ref->x11.xwindow)
                    {
                      int n = x11_root_children_stacked->len - i;

                      /* There's no point restacking if there's only one X window */
                      if (n == 1)
                        break;

                      serial = XNextRequest (stack->screen->display->xdisplay);
                      XRestackWindows (stack->screen->display->xdisplay,
                                       reference, n);
                      break;
                    }
                }
            }

          /* We need to include an already-stacked window
           * in the restack call, so we get in the proper position
           * with respect to it.
           */
          if (newp != new_stack)
            newp = MIN (newp - 1, x_ref);
          meta_stack_tracker_record_restack_windows (stack->screen->stack_tracker,
                                                     newp, new_end - newp,
                                                     serial);
        }
    }

  /* Push hidden X windows to the bottom of the stack under the guard window */
  guard_stack_window.any.type = META_WINDOW_CLIENT_TYPE_X11;
  guard_stack_window.x11.xwindow = stack->screen->guard_window;
  meta_stack_tracker_record_lower (stack->screen->stack_tracker,
                                   &guard_stack_window,
                                   XNextRequest (stack->screen->display->xdisplay));
  XLowerWindow (stack->screen->display->xdisplay, stack->screen->guard_window);
  meta_stack_tracker_record_restack_windows (stack->screen->stack_tracker,
                                             (MetaStackWindow *)x11_hidden_stack_windows->data,
                                             x11_hidden_stack_windows->len,
                                             XNextRequest (stack->screen->display->xdisplay));
  XRestackWindows (stack->screen->display->xdisplay,
		   (Window *)x11_hidden->data,
		   x11_hidden->len);
  g_array_free (x11_hidden, TRUE);
  g_array_free (x11_hidden_stack_windows, TRUE);

  meta_error_trap_pop (stack->screen->display);
  /* on error, a window was destroyed; it should eventually
   * get removed from the stacking list when we unmanage it
   * and we'll fix stacking at that time.
   */
  
  /* Sync _NET_CLIENT_LIST and _NET_CLIENT_LIST_STACKING */

  XChangeProperty (stack->screen->display->xdisplay,
                   stack->screen->xroot,
                   stack->screen->display->atom__NET_CLIENT_LIST,
                   XA_WINDOW,
                   32, PropModeReplace,
                   (unsigned char *)stack->xwindows->data,
                   stack->xwindows->len);
  XChangeProperty (stack->screen->display->xdisplay,
                   stack->screen->xroot,
                   stack->screen->display->atom__NET_CLIENT_LIST_STACKING,
                   XA_WINDOW,
                   32, PropModeReplace,
                   (unsigned char *)x11_stacked->data,
                   x11_stacked->len);

  g_array_free (x11_stacked, TRUE);

  if (stack->last_all_root_children_stacked)
    free_last_all_root_children_stacked_cache (stack);
  stack->last_all_root_children_stacked = all_root_children_stacked;

  g_array_free (x11_root_children_stacked, TRUE);

  /* That was scary... */
}

MetaWindow*
meta_stack_get_top (MetaStack *stack)
{
  stack_ensure_sorted (stack);

  if (stack->sorted)
    return stack->sorted->data;
  else
    return NULL;
}

MetaWindow*
meta_stack_get_bottom (MetaStack  *stack)
{
  GList *link;

  stack_ensure_sorted (stack);

  link = g_list_last (stack->sorted);
  if (link != NULL)
    return link->data;
  else
    return NULL;
}

MetaWindow*
meta_stack_get_above (MetaStack      *stack,
                      MetaWindow     *window,
                      gboolean        only_within_layer)
{
  GList *link;
  MetaWindow *above;
  
  stack_ensure_sorted (stack);
  
  link = g_list_find (stack->sorted, window);
  if (link == NULL)
    return NULL;
  if (link->prev == NULL)
    return NULL;

  above = link->prev->data;

  if (only_within_layer &&
      above->layer != window->layer)
    return NULL;
  else
    return above;
}

MetaWindow*
meta_stack_get_below (MetaStack      *stack,
                      MetaWindow     *window,
                      gboolean        only_within_layer)
{
  GList *link;
  MetaWindow *below;
  
  stack_ensure_sorted (stack);

  link = g_list_find (stack->sorted, window);

  if (link == NULL)
    return NULL;
  if (link->next == NULL)
    return NULL;
  
  below = link->next->data;

  if (only_within_layer &&
      below->layer != window->layer)
    return NULL;
  else
    return below;
}

static gboolean
window_contains_point (MetaWindow *window,
                       int         root_x,
                       int         root_y)
{
  MetaRectangle rect;

  meta_window_get_frame_rect (window, &rect);

  return POINT_IN_RECT (root_x, root_y, rect);
}

static MetaWindow*
get_default_focus_window (MetaStack     *stack,
                          MetaWorkspace *workspace,
                          MetaWindow    *not_this_one,
                          gboolean       must_be_at_point,
                          int            root_x,
                          int            root_y)
{
  /* Find the topmost, focusable, mapped, window.
   * not_this_one is being unfocused or going away, so exclude it.
   * Also, prefer to focus transient parent of not_this_one,
   * or top window in same group as not_this_one.
   */

  MetaWindow *transient_parent;
  MetaWindow *topmost_in_group;
  MetaWindow *topmost_overall;
  MetaGroup *not_this_one_group;
  GList *l;

  transient_parent = NULL;
  topmost_in_group = NULL;
  topmost_overall = NULL;
  if (not_this_one)
    not_this_one_group = meta_window_get_group (not_this_one);
  else
    not_this_one_group = NULL;

  stack_ensure_sorted (stack);

  /* top of this layer is at the front of the list */
  for (l = stack->sorted; l != NULL; l = l->next)
    {
      MetaWindow *window = l->data;

      if (!window)
        continue;

      if (window == not_this_one)
        continue;

      if (window->unmaps_pending > 0)
        continue;

      if (window->minimized)
        continue;

      if (!(window->input || window->take_focus))
        continue;

      if (workspace != NULL && !meta_window_located_on_workspace (window, workspace))
        continue;

      if (must_be_at_point && !window_contains_point (window, root_x, root_y))
        continue;

      if (not_this_one != NULL)
        {
          if (transient_parent == NULL &&
              meta_window_get_transient_for (not_this_one) == window)
            transient_parent = window;

          if (topmost_in_group == NULL &&
              not_this_one_group != NULL &&
              not_this_one_group == meta_window_get_group (window))
            topmost_in_group = window;
        }

      if (topmost_overall == NULL && window->type != META_WINDOW_DOCK)
        topmost_overall = window;

      /* We could try to bail out early here for efficiency in
       * some cases, but it's just not worth the code.
       */
    }

  if (transient_parent)
    return transient_parent;
  else if (topmost_in_group)
    return topmost_in_group;
  else if (topmost_overall)
    return topmost_overall;
  else
    return NULL;
}

MetaWindow*
meta_stack_get_default_focus_window_at_point (MetaStack     *stack,
                                              MetaWorkspace *workspace,
                                              MetaWindow    *not_this_one,
                                              int            root_x,
                                              int            root_y)
{
  return get_default_focus_window (stack, workspace, not_this_one,
                                   TRUE, root_x, root_y);
}

MetaWindow*
meta_stack_get_default_focus_window (MetaStack     *stack,
                                     MetaWorkspace *workspace,
                                     MetaWindow    *not_this_one)
{
  return get_default_focus_window (stack, workspace, not_this_one,
                                   FALSE, 0, 0);
}

GList*
meta_stack_list_windows (MetaStack     *stack,
                         MetaWorkspace *workspace)
{
  GList *workspace_windows = NULL;
  GList *link;
  
  stack_ensure_sorted (stack); /* do adds/removes */
  
  link = stack->sorted;
  
  while (link)
    {
      MetaWindow *window = link->data;
      
      if (window &&
          (workspace == NULL || meta_window_located_on_workspace (window, workspace)))
        {
          workspace_windows = g_list_prepend (workspace_windows,
                                              window);
        }
      
      link = link->next;
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

  stack_ensure_sorted (stack); /* update constraints, layers */
  
  if (window_a->layer < window_b->layer)
    return -1;
  else if (window_a->layer > window_b->layer)
    return 1;
  else if (window_a->stack_position < window_b->stack_position)
    return -1;
  else if (window_a->stack_position > window_b->stack_position)
    return 1;
  else
    return 0; /* not reached */
}

static int
compare_just_window_stack_position (void *a,
                                    void *b)
{
  MetaWindow *window_a = a;
  MetaWindow *window_b = b;

  if (window_a->stack_position < window_b->stack_position)
    return -1; /* move window_a earlier in list */
  else if (window_a->stack_position > window_b->stack_position)
    return 1;
  else
    return 0; /* not reached */
}

GList*
meta_stack_get_positions (MetaStack *stack)
{
  GList *tmp;

  /* Make sure to handle any adds or removes */
  stack_ensure_sorted (stack);

  tmp = g_list_copy (stack->sorted);
  tmp = g_list_sort (tmp, (GCompareFunc) compare_just_window_stack_position);

  return tmp;
}

static gint
compare_pointers (gconstpointer a,
                  gconstpointer b)
{
  if (a > b)
    return 1;
  else if (a < b)
    return -1;
  else 
    return 0;
}

static gboolean
lists_contain_same_windows (GList *a,
                            GList *b)
{
  GList *copy1, *copy2;
  GList *tmp1, *tmp2;

  if (g_list_length (a) != g_list_length (b))
    return FALSE;

  tmp1 = copy1 = g_list_sort (g_list_copy (a), compare_pointers);
  tmp2 = copy2 = g_list_sort (g_list_copy (b), compare_pointers);

  while (tmp1 && tmp1->data == tmp2->data)   /* tmp2 is non-NULL if tmp1 is */
    {
      tmp1 = tmp1->next;
      tmp2 = tmp2->next;
    }

  g_list_free (copy1);
  g_list_free (copy2);

  return (tmp1 == NULL);    /* tmp2 is non-NULL if tmp1 is */
}

void
meta_stack_set_positions (MetaStack *stack,
                          GList     *windows)
{
  int i;
  GList *tmp;

  /* Make sure any adds or removes aren't in limbo -- is this needed? */
  stack_ensure_sorted (stack);
  
  if (!lists_contain_same_windows (windows, stack->sorted))
    {
      meta_warning ("This list of windows has somehow changed; not resetting "
                    "positions of the windows.\n");
      return;
    }

  g_list_free (stack->sorted);
  stack->sorted = g_list_copy (windows);

  stack->need_resort = TRUE;
  stack->need_constrain = TRUE;
   
  i = 0;
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;
      w->stack_position = i++;
      tmp = tmp->next;
    }
  
  meta_topic (META_DEBUG_STACK,
              "Reset the stack positions of (nearly) all windows\n");

  stack_sync_to_xserver (stack);
  meta_stack_update_window_tile_matches (stack, NULL);
}

void
meta_window_set_stack_position_no_sync (MetaWindow *window,
                                        int         position)
{
  int low, high, delta;
  GList *tmp;
  
  g_return_if_fail (window->screen->stack != NULL);
  g_return_if_fail (window->stack_position >= 0);
  g_return_if_fail (position >= 0);
  g_return_if_fail (position < window->screen->stack->n_positions);

  if (position == window->stack_position)
    {
      meta_topic (META_DEBUG_STACK, "Window %s already has position %d\n",
                  window->desc, position);
      return;
    }

  window->screen->stack->need_resort = TRUE;
  window->screen->stack->need_constrain = TRUE;
  
  if (position < window->stack_position)
    {
      low = position;
      high = window->stack_position - 1;
      delta = 1;
    }
  else
    {
      low = window->stack_position + 1;
      high = position;
      delta = -1;
    }

  tmp = window->screen->stack->sorted;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;

      if (w->stack_position >= low &&
          w->stack_position <= high)
        w->stack_position += delta;

      tmp = tmp->next;
    }
  
  window->stack_position = position;

  meta_topic (META_DEBUG_STACK,
              "Window %s had stack_position set to %d\n",
              window->desc, window->stack_position);
}

void
meta_window_set_stack_position (MetaWindow *window,
                                int         position)
{
  meta_window_set_stack_position_no_sync (window, position);
  stack_sync_to_xserver (window->screen->stack);
  meta_stack_update_window_tile_matches (window->screen->stack,
                                         window->screen->active_workspace);
}
