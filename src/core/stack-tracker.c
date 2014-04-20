/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * SECTION:stack-tracker
 * @short_description: Track stacking order for compositor
 *
 * #MetaStackTracker maintains the most accurate view we have at a
 * given point of time of the ordering of the children of the root
 * window (including override-redirect windows.) This is used to order
 * the windows when the compositor draws them.
 *
 * By contrast, #MetaStack is responsible for keeping track of how we
 * think that windows *should* be ordered.  For windows we manage
 * (non-override-redirect windows), the two stacking orders will be
 * the same.
 */

/*
 * Copyright (C) 2009 Red Hat, Inc.
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

#include <string.h>

#include "frame.h"
#include "screen-private.h"
#include "stack-tracker.h"
#include <meta/util.h>

#include <meta/compositor.h>

/* The complexity here comes from resolving two competing factors:
 *
 *  - We need to have a view of the stacking order that takes into
 *    account everything we have done without waiting for events
 *    back from the X server; we don't want to draw intermediate
 *    partially-stacked stack states just because we haven't received
 *    some notification yet.
 *
 *  - Only the X server has an accurate view of the complete stacking;
 *    when we make a request to restack windows, we don't know how
 *    it will affect override-redirect windows, because at any point
 *    applications may restack these windows without our involvement.
 *
 * The technique we use is that we keep three sets of information:
 *
 *  - The stacking order on the server as known from the last
 *    event we received.
 *  - A queue of stacking requests that *we* made subsequent to
 *    that last event.
 *  - A predicted stacking order, derived from applying the queued
 *    requests to the last state from the server.
 *
 * When we receive a new event: a) we compare the serial in the event to
 * the serial of the queued requests and remove any that are now
 * no longer pending b) if necessary, drop the predicted stacking
 * order to recompute it at the next opportunity.
 *
 * Possible optimizations:
 *  Keep the stacks as an array + reverse-mapping hash table to avoid
 *    linear lookups.
 *  Keep the stacks as a GList + reverse-mapping hash table to avoid
 *    linear lookups and to make restacking constant-time.
 */

typedef union _MetaStackOp MetaStackOp;

typedef enum {
  STACK_OP_ADD,
  STACK_OP_REMOVE,
  STACK_OP_RAISE_ABOVE,
  STACK_OP_LOWER_BELOW
} MetaStackOpType;

/* MetaStackOp represents a "stacking operation" - a change to
 * apply to a window stack. Depending on the context, it could
 * either reflect a request we have sent to the server, or a
 * notification event we received from the X server.
 */
union _MetaStackOp
{
  struct {
    MetaStackOpType type;
    gulong serial;
    MetaStackWindow window;
  } any;
  struct {
    MetaStackOpType type;
    gulong serial;
    MetaStackWindow window;
  } add;
  struct {
    MetaStackOpType type;
    gulong serial;
    MetaStackWindow window;
  } remove;
  struct {
    MetaStackOpType type;
    gulong serial;
    MetaStackWindow window;
    MetaStackWindow sibling;
  } raise_above;
  struct {
    MetaStackOpType type;
    gulong serial;
    MetaStackWindow window;
    MetaStackWindow sibling;
  } lower_below;
};

struct _MetaStackTracker
{
  MetaScreen *screen;

  /* This is the last state of the stack as based on events received
   * from the X server.
   */
  GArray *xserver_stack;

  /* This is the serial of the last request we made that was reflected
   * in xserver_stack
   */
  gulong xserver_serial;

  /* A combined stack containing X and Wayland windows but without
   * any unverified operations applied. */
  GArray *verified_stack;

  /* This is a queue of requests we've made to change the stacking order,
   * where we haven't yet gotten a reply back from the server.
   */
  GQueue *unverified_predictions;

  /* This is how we think the stack is, based on verified_stack, and
   * on the unverified_predictions we've made subsequent to
   * verified_stack.
   */
  GArray *predicted_stack;

  /* Idle function used to sync the compositor's view of the window
   * stack up with our best guess before a frame is drawn.
   */
  guint sync_stack_later;
};

static gboolean
meta_stack_window_is_set (const MetaStackWindow *window)
{
  if (window->any.type == META_WINDOW_CLIENT_TYPE_X11)
    return window->x11.xwindow == None ? FALSE : TRUE;
  else
    return window->wayland.meta_window ? TRUE : FALSE;
}

gboolean
meta_stack_window_equal (const MetaStackWindow *a,
                         const MetaStackWindow *b)
{
  if (a->any.type == b->any.type)
    {
      if (a->any.type == META_WINDOW_CLIENT_TYPE_X11)
        return a->x11.xwindow == b->x11.xwindow;
      else
        return a->wayland.meta_window == b->wayland.meta_window;
    }
  else
    return FALSE;
}

static char *
get_window_id (MetaStackWindow *window)
{
  if (window->any.type == META_WINDOW_CLIENT_TYPE_X11)
    return g_strdup_printf ("X11:%lx", window->x11.xwindow);
  else
    return g_strdup_printf ("Wayland:%p", window->wayland.meta_window);
}

static void
meta_stack_op_dump (MetaStackOp *op,
		    const char  *prefix,
		    const char  *suffix)
{
  char *window_id = get_window_id (&op->any.window);

  switch (op->any.type)
    {
    case STACK_OP_ADD:
      meta_topic (META_DEBUG_STACK, "%sADD(%s; %ld)%s",
		  prefix, window_id, op->any.serial, suffix);
      break;
    case STACK_OP_REMOVE:
      meta_topic (META_DEBUG_STACK, "%sREMOVE(%s; %ld)%s",
		  prefix, window_id, op->any.serial, suffix);
      break;
    case STACK_OP_RAISE_ABOVE:
      {
        char *sibling_id = get_window_id (&op->raise_above.sibling);
        meta_topic (META_DEBUG_STACK, "%sRAISE_ABOVE(%s, %s; %ld)%s",
                    prefix,
                    window_id, sibling_id,
                    op->any.serial,
                    suffix);
        g_free (sibling_id);
        break;
      }
    case STACK_OP_LOWER_BELOW:
      {
        char *sibling_id = get_window_id (&op->lower_below.sibling);
        meta_topic (META_DEBUG_STACK, "%sLOWER_BELOW(%s, %s; %ld)%s",
                    prefix,
                    window_id, sibling_id,
                    op->any.serial,
                    suffix);
        g_free (sibling_id);
        break;
      }
    }

  g_free (window_id);
}

static void
meta_stack_tracker_dump (MetaStackTracker *tracker)
{
  guint i;
  GList *l;

  meta_topic (META_DEBUG_STACK, "MetaStackTracker state (screen=%d)\n", tracker->screen->number);
  meta_push_no_msg_prefix ();
  meta_topic (META_DEBUG_STACK, "  xserver_serial: %ld\n", tracker->xserver_serial);
  meta_topic (META_DEBUG_STACK, "  xserver_stack: ");
  for (i = 0; i < tracker->xserver_stack->len; i++)
    {
      MetaStackWindow *window = &g_array_index (tracker->xserver_stack, MetaStackWindow, i);
      char *window_id = get_window_id (window);
      meta_topic (META_DEBUG_STACK, "  %s", window_id);
      g_free (window_id);
    }
  meta_topic (META_DEBUG_STACK, "\n  verfied_stack: ");
  for (i = 0; i < tracker->verified_stack->len; i++)
    {
      MetaStackWindow *window = &g_array_index (tracker->verified_stack, MetaStackWindow, i);
      char *window_id = get_window_id (window);
      meta_topic (META_DEBUG_STACK, "  %s", window_id);
      g_free (window_id);
    }
  meta_topic (META_DEBUG_STACK, "\n  unverified_predictions: [");
  for (l = tracker->unverified_predictions->head; l; l = l->next)
    {
      MetaStackOp *op = l->data;
      meta_stack_op_dump (op, "", l->next ? ", " : "");
    }
  meta_topic (META_DEBUG_STACK, "]\n");
  if (tracker->predicted_stack)
    {
      meta_topic (META_DEBUG_STACK, "\n  predicted_stack: ");
      for (i = 0; i < tracker->predicted_stack->len; i++)
        {
          MetaStackWindow *window = &g_array_index (tracker->predicted_stack, MetaStackWindow, i);
          char *window_id = get_window_id (window);
          meta_topic (META_DEBUG_STACK, "  %s", window_id);
          g_free (window_id);
        }
    }
  meta_topic (META_DEBUG_STACK, "\n");
  meta_pop_no_msg_prefix ();
}

static void
meta_stack_op_free (MetaStackOp *op)
{
  g_slice_free (MetaStackOp, op);
}

static int
find_window (GArray *window_stack,
             MetaStackWindow *window)
{
  guint i;

  for (i = 0; i < window_stack->len; i++)
    {
      MetaStackWindow *current = &g_array_index (window_stack, MetaStackWindow, i);
      if (current->any.type == window->any.type)
        {
          if (current->any.type == META_WINDOW_CLIENT_TYPE_X11 &&
              current->x11.xwindow == window->x11.xwindow)
            return i;
          else
            if (current->wayland.meta_window == window->wayland.meta_window)
              return i;
        }
    }

  return -1;
}

/* Returns TRUE if stack was changed */
static gboolean
move_window_above (GArray          *stack,
                   MetaStackWindow *window,
                   int              old_pos,
                   int              above_pos)
{
  /* Copy the window by-value before we start shifting things around
   * in the stack in case window points into the stack itself. */
  MetaStackWindow window_val = *window;
  int i;

  if (old_pos < above_pos)
    {
      for (i = old_pos; i < above_pos; i++)
	g_array_index (stack, MetaStackWindow, i) =
          g_array_index (stack, MetaStackWindow, i + 1);

      g_array_index (stack, MetaStackWindow, above_pos) = window_val;

      return TRUE;
    }
  else if (old_pos > above_pos + 1)
    {
      for (i = old_pos; i > above_pos + 1; i--)
	g_array_index (stack, MetaStackWindow, i) =
          g_array_index (stack, MetaStackWindow, i - 1);

      g_array_index (stack, MetaStackWindow, above_pos + 1) = window_val;

      return TRUE;
    }
  else
    return FALSE;
}

/* Returns TRUE if stack was changed */
static gboolean
meta_stack_op_apply (MetaStackOp *op,
		     GArray      *stack)
{
  switch (op->any.type)
    {
    case STACK_OP_ADD:
      {
	int old_pos = find_window (stack, &op->add.window);
	if (old_pos >= 0)
	  {
            char *window_id = get_window_id (&op->add.window);
	    g_warning ("STACK_OP_ADD: window %s already in stack",
		       window_id);
            g_free (window_id);
	    return FALSE;
	  }

	g_array_append_val (stack, op->add.window);
	return TRUE;
      }
    case STACK_OP_REMOVE:
      {
	int old_pos = find_window (stack, &op->remove.window);
	if (old_pos < 0)
	  {
            char *window_id = get_window_id (&op->remove.window);
	    g_warning ("STACK_OP_REMOVE: window %s not in stack",
		       window_id);
            g_free (window_id);
	    return FALSE;
	  }

	g_array_remove_index (stack, old_pos);
	return TRUE;
      }
    case STACK_OP_RAISE_ABOVE:
      {
	int old_pos = find_window (stack, &op->raise_above.window);
	int above_pos;
	if (old_pos < 0)
	  {
            char *window_id = get_window_id (&op->raise_above.window);
	    g_warning ("STACK_OP_RAISE_ABOVE: window %s not in stack",
		       window_id);
            g_free (window_id);
	    return FALSE;
	  }

        if (meta_stack_window_is_set (&op->raise_above.sibling))
	  {
	    above_pos = find_window (stack, &op->raise_above.sibling);
	    if (above_pos < 0)
	      {
                char *sibling_id = get_window_id (&op->raise_above.sibling);
		g_warning ("STACK_OP_RAISE_ABOVE: sibling window %s not in stack",
			   sibling_id);
                g_free (sibling_id);
		return FALSE;
	      }
	  }
	else
	  {
	    above_pos = -1;
	  }

	return move_window_above (stack, &op->raise_above.window, old_pos, above_pos);
      }
    case STACK_OP_LOWER_BELOW:
      {
	int old_pos = find_window (stack, &op->lower_below.window);
	int above_pos;
	if (old_pos < 0)
	  {
            char *window_id = get_window_id (&op->lower_below.window);
	    g_warning ("STACK_OP_LOWER_BELOW: window %s not in stack",
		       window_id);
            g_free (window_id);
	    return FALSE;
	  }

        if (meta_stack_window_is_set (&op->lower_below.sibling))
	  {
	    int below_pos = find_window (stack, &op->lower_below.sibling);
	    if (below_pos < 0)
	      {
                char *sibling_id = get_window_id (&op->lower_below.sibling);
		g_warning ("STACK_OP_LOWER_BELOW: sibling window %s not in stack",
			   sibling_id);
                g_free (sibling_id);
		return FALSE;
	      }

	    above_pos = below_pos - 1;
	  }
	else
	  {
	    above_pos = stack->len - 1;
	  }

	return move_window_above (stack, &op->lower_below.window, old_pos, above_pos);
      }
    }

  g_assert_not_reached ();
  return FALSE;
}

static GArray *
copy_stack (GArray *stack)
{
  GArray *copy = g_array_sized_new (FALSE, FALSE, sizeof (MetaStackWindow), stack->len);

  g_array_set_size (copy, stack->len);

  memcpy (copy->data, stack->data, sizeof (MetaStackWindow) * stack->len);

  return copy;
}

static void
requery_xserver_stack (MetaStackTracker *tracker)
{
  MetaScreen *screen = tracker->screen;
  Window ignored1, ignored2;
  Window *children;
  guint n_children;
  guint i;

  if (tracker->xserver_stack)
    g_array_free (tracker->xserver_stack, TRUE);

  tracker->xserver_serial = XNextRequest (screen->display->xdisplay);

  XQueryTree (screen->display->xdisplay,
              screen->xroot,
              &ignored1, &ignored2, &children, &n_children);

  tracker->xserver_stack =
    g_array_sized_new (FALSE, FALSE, sizeof (MetaStackWindow), n_children);
  g_array_set_size (tracker->xserver_stack, n_children);

  for (i = 0; i < n_children; i++)
    {
      MetaStackWindow *window =
        &g_array_index (tracker->xserver_stack, MetaStackWindow, i);
      window->any.type = META_WINDOW_CLIENT_TYPE_X11;
      window->x11.xwindow = children[i];
    }

  XFree (children);
}

MetaStackTracker *
meta_stack_tracker_new (MetaScreen *screen)
{
  MetaStackTracker *tracker;

  tracker = g_new0 (MetaStackTracker, 1);
  tracker->screen = screen;

  requery_xserver_stack (tracker);

  tracker->verified_stack = copy_stack (tracker->xserver_stack);

  tracker->unverified_predictions = g_queue_new ();

  meta_stack_tracker_dump (tracker);

  return tracker;
}

void
meta_stack_tracker_free (MetaStackTracker *tracker)
{
  if (tracker->sync_stack_later)
    meta_later_remove (tracker->sync_stack_later);

  g_array_free (tracker->xserver_stack, TRUE);
  g_array_free (tracker->verified_stack, TRUE);
  if (tracker->predicted_stack)
    g_array_free (tracker->predicted_stack, TRUE);

  g_queue_foreach (tracker->unverified_predictions, (GFunc)meta_stack_op_free, NULL);
  g_queue_free (tracker->unverified_predictions);
  tracker->unverified_predictions = NULL;

  g_free (tracker);
}

static void
stack_tracker_apply_prediction (MetaStackTracker *tracker,
			        MetaStackOp      *op)
{
  /* If this is a wayland operation then it's implicitly verified so
   * we can apply it immediately so long as it doesn't depend on any
   * unverified X operations...
   */
  if (op->any.window.any.type == META_WINDOW_CLIENT_TYPE_WAYLAND &&
      tracker->unverified_predictions->length == 0)
    {
      if (meta_stack_op_apply (op, tracker->verified_stack))
        meta_stack_tracker_queue_sync_stack (tracker);
    }
  else
    {
      meta_stack_op_dump (op, "Predicting: ", "\n");
      g_queue_push_tail (tracker->unverified_predictions, op);
    }

  if (!tracker->predicted_stack ||
      meta_stack_op_apply (op, tracker->predicted_stack))
    meta_stack_tracker_queue_sync_stack (tracker);

  meta_stack_tracker_dump (tracker);
}

void
meta_stack_tracker_record_add (MetaStackTracker       *tracker,
                               const MetaStackWindow  *window,
			       gulong                  serial)
{
  MetaStackOp *op = g_slice_new (MetaStackOp);

  op->any.type = STACK_OP_ADD;
  op->any.serial = serial;
  op->any.window = *window;

  stack_tracker_apply_prediction (tracker, op);
}

void
meta_stack_tracker_record_remove (MetaStackTracker      *tracker,
                                  const MetaStackWindow *window,
				  gulong                 serial)
{
  MetaStackOp *op = g_slice_new (MetaStackOp);

  op->any.type = STACK_OP_REMOVE;
  op->any.serial = serial;
  op->any.window = *window;

  stack_tracker_apply_prediction (tracker, op);
}

void
meta_stack_tracker_record_restack_windows (MetaStackTracker       *tracker,
                                           const MetaStackWindow  *windows,
					   int                     n_windows,
					   gulong                  serial)
{
  int i;
  int n_x_windows = 0;

  /* XRestackWindows() isn't actually a X requests - it's broken down
   * by XLib into a series of XConfigureWindow(StackMode=below); we
   * mirror that here.
   *
   * Since there may be a mixture of X and wayland windows in the
   * stack it's ambiguous which operations we should associate with an
   * X serial number. One thing we do know though is that there will
   * be (n_x_window - 1) X requests made.
   *
   * Aside: Having a separate StackOp for this would be possible to
   * get some extra efficiency in memory allocation and in applying
   * the op, at the expense of a code complexity. Implementation hint
   * for that - keep op->restack_window.n_complete, and when receiving
   * events with intermediate serials, set n_complete rather than
   * removing the op from the queue.
   */
  if (n_windows && windows[0].any.type == META_WINDOW_CLIENT_TYPE_X11)
    n_x_windows++;
  for (i = 0; i < n_windows - 1; i++)
    {
      const MetaStackWindow *lower = &windows[i + 1];
      gboolean involves_x = FALSE;

      if (lower->any.type == META_WINDOW_CLIENT_TYPE_X11)
        {
          n_x_windows++;

          /* Since the first X window is a reference point we only
           * assoicate a serial number with the operations involving
           * later X windows. */
          if (n_x_windows > 1)
            involves_x = TRUE;
        }

      meta_stack_tracker_record_lower_below (tracker, lower, &windows[i],
                                             involves_x ? serial++ : 0);
    }
}

void
meta_stack_tracker_record_raise_above (MetaStackTracker       *tracker,
                                       const MetaStackWindow  *window,
                                       const MetaStackWindow  *sibling,
				       gulong                  serial)
{
  MetaStackOp *op = g_slice_new (MetaStackOp);

  op->any.type = STACK_OP_RAISE_ABOVE;
  op->any.serial = serial;
  op->any.window = *window;
  if (sibling)
    op->raise_above.sibling = *sibling;
  else
    {
      op->raise_above.sibling.any.type = META_WINDOW_CLIENT_TYPE_X11;
      op->raise_above.sibling.x11.xwindow = None;
    }

  stack_tracker_apply_prediction (tracker, op);
}

void
meta_stack_tracker_record_lower_below (MetaStackTracker       *tracker,
                                       const MetaStackWindow  *window,
                                       const MetaStackWindow  *sibling,
				       gulong                  serial)
{
  MetaStackOp *op = g_slice_new (MetaStackOp);

  op->any.type = STACK_OP_LOWER_BELOW;
  op->any.serial = serial;
  op->any.window = *window;
  if (sibling)
    op->lower_below.sibling = *sibling;
  else
    {
      op->lower_below.sibling.any.type = META_WINDOW_CLIENT_TYPE_X11;
      op->lower_below.sibling.x11.xwindow = None;
    }

  stack_tracker_apply_prediction (tracker, op);
}

void
meta_stack_tracker_record_lower (MetaStackTracker       *tracker,
                                 const MetaStackWindow  *window,
				 gulong                  serial)
{
  meta_stack_tracker_record_raise_above (tracker, window, NULL, serial);
}

/* @op is an operation derived from an X event from the server and we
 * want to verify that our predicted operations are consistent with
 * what's being reported by the X server.
 *
 * NB: Since our stack may actually be a mixture of X and Wayland
 * clients we can't simply apply these operations derived from X
 * events onto our stack and discard old predictions because these
 * operations aren't aware of wayland windows.
 *
 * This function applies all the unverified predicted operations up to
 * the given @serial onto the verified_stack so that we can check the
 * stack for consistency with the given X operation.
 *
 * Return value: %TRUE if the predicted state is consistent with
 * receiving the given @op from X, else %FALSE.
 *
 * @modified will be set to %TRUE if tracker->verified_stack is
 * changed by applying any newly validated operations, else %FALSE.
 */
static gboolean
stack_tracker_verify_predictions (MetaStackTracker *tracker,
                                  MetaStackOp      *op,
                                  gboolean         *modified)
{
  GArray *tmp_predicted_stack = NULL;
  GArray *predicted_stack;
  gboolean modified_stack = FALSE;

  /* Wayland operations don't need to be verified and shouldn't end up
   * passed to this api. */
  g_return_val_if_fail (op->any.window.any.type == META_WINDOW_CLIENT_TYPE_X11, FALSE);

  if (tracker->unverified_predictions->length)
    {
      GList *l;

      tmp_predicted_stack = predicted_stack = copy_stack (tracker->verified_stack);

      for (l = tracker->unverified_predictions->head; l; l = l->next)
        {
          MetaStackOp *current_op = l->data;

          if (current_op->any.serial > op->any.serial)
            break;

          modified_stack |= meta_stack_op_apply (current_op, predicted_stack);
        }
    }
  else
    predicted_stack = tracker->verified_stack;

  switch (op->any.type)
    {
    case STACK_OP_ADD:
      if (!find_window (predicted_stack, &op->any.window))
        {
          char *window_id = get_window_id (&op->any.window);
          meta_topic (META_DEBUG_STACK, "Verify STACK_OP_ADD: window %s not found\n",
                      window_id);
          g_free (window_id);
          goto not_verified;
        }
      break;
    case STACK_OP_REMOVE:
      if (find_window (predicted_stack, &op->any.window))
        {
          char *window_id = get_window_id (&op->any.window);
          meta_topic (META_DEBUG_STACK, "Verify STACK_OP_REMOVE: window %s was unexpectedly found\n",
                      window_id);
          g_free (window_id);
          goto not_verified;
        }
      break;
    case STACK_OP_RAISE_ABOVE:
      {
        Window last_xwindow = None;
        char *window_id;
        unsigned int i;

        /* This code is only intended for verifying operations based
         * on XEvents where we can assume the sibling refers to
         * another X window...  */
        g_return_val_if_fail (op->raise_above.sibling.any.type ==
                              META_WINDOW_CLIENT_TYPE_X11, FALSE);

        for (i = 0; i < predicted_stack->len; i++)
          {
            MetaStackWindow *window = &g_array_index (predicted_stack, MetaStackWindow, i);

            if (meta_stack_window_equal (window, &op->any.window))
              {
                if (last_xwindow == op->raise_above.sibling.x11.xwindow)
                  goto verified;
                else
                  goto not_verified;
              }

            if (window->any.type == META_WINDOW_CLIENT_TYPE_X11)
              last_xwindow = window->x11.xwindow;
          }

        window_id = get_window_id (&op->any.window);
        meta_topic (META_DEBUG_STACK, "Verify STACK_OP_RAISE_ABOVE: window %s not found\n",
                    window_id);
        g_free (window_id);
        goto not_verified;
      }
    case STACK_OP_LOWER_BELOW:
      g_warn_if_reached (); /* No X events currently lead to this path */
      goto not_verified;
    }

verified:

  /* We can free the operations which we have now verified... */
  while (tracker->unverified_predictions->head)
    {
      MetaStackOp *queued_op = tracker->unverified_predictions->head->data;

      if (queued_op->any.serial > op->any.serial)
	break;

      g_queue_pop_head (tracker->unverified_predictions);
      meta_stack_op_free (queued_op);
    }

  *modified = modified_stack;
  if (modified_stack)
    {
      g_array_free (tracker->verified_stack, TRUE);
      tracker->verified_stack = predicted_stack;
    }
  else if (tmp_predicted_stack)
    g_array_free (tmp_predicted_stack, TRUE);

  return TRUE;

not_verified:

  if (tmp_predicted_stack)
    g_array_free (tmp_predicted_stack, TRUE);

  if (tracker->predicted_stack)
    {
      g_array_free (tracker->predicted_stack, TRUE);
      tracker->predicted_stack = NULL;
    }

  *modified = FALSE;

  return FALSE;
}

/* If we find that our predicted state is not consistent with what the
 * X server is reporting to us then this function can re-query and
 * re-synchronize verified_stack with the X server stack while
 * hopefully not disrupting the relative stacking of Wayland windows.
 *
 * Return value: %TRUE if the verified stack was modified with respect
 * to the predicted stack else %FALSE.
 *
 * Note: ->predicted_stack will be cleared by this function if
 * ->verified_stack had to be modified when re-synchronizing.
 */
static gboolean
resync_verified_stack_with_xserver_stack (MetaStackTracker *tracker)
{
  GList *l;
  unsigned int i, j;
  MetaStackWindow *expected_xwindow;
  gboolean modified_stack = FALSE;

  /* Overview of the algorithm:
   *
   * - Re-query the complete X window stack from the X server via
   *   XQueryTree() and update xserver_stack.
   *
   * - Apply all operations in unverified_predictions to
   *   verified_stack so we have a predicted stack including Wayland
   *   windows and free the queue of unverified_predictions.
   *
   * - Iterate through the x windows listed in verified_stack at the
   *   same time as iterating the windows in xserver_list. (Stop
   *   when we reach the end of the xserver_list)
   *     - If the window found doesn't match the window expected
   *     according to the order of xserver_list then:
   *       - Look ahead for the window we were expecting and restack
   *       that above the previous X window. If we fail to find the
   *       expected window then create a new entry for it and stack
   *       that.
   *
   * - Continue to iterate through verified_stack for any remaining
   *   X windows that we now know aren't in the xserver_list and
   *   remove them.
   *
   * - Free ->predicted_stack if any.
   */

  meta_topic (META_DEBUG_STACK, "Fully re-synchronizing X stack with verified stack\n");

  requery_xserver_stack (tracker);

  for (l = tracker->unverified_predictions->head; l; l = l->next)
    meta_stack_op_apply (l->data, tracker->verified_stack);
  g_queue_clear (tracker->unverified_predictions);

  j = 0;
  expected_xwindow =
    &g_array_index (tracker->xserver_stack, MetaStackWindow, j);

  for (i = 0;
       i < tracker->verified_stack->len;
       )
    {
      MetaStackWindow *current =
        &g_array_index (tracker->verified_stack, MetaStackWindow, i);

      if (current->any.type != META_WINDOW_CLIENT_TYPE_X11)
        {
          /* Progress i but not j */
          i++;
          continue;
        }

      if (current->x11.xwindow != expected_xwindow->x11.xwindow)
        {
          MetaStackWindow new;
          MetaStackWindow *expected;
          int expected_index;
          
          /* If the current window corresponds to a window that's not
           * in xserver_stack any more then the least disruptive thing
           * we can do is to simply remove it and take another look at
           * the same index.
           *
           * Note: we didn't used to do this and instead relied on
           * removed windows getting pushed to the end of the list so
           * they could all be removed together but this also resulted
           * in pushing Wayland windows to the end too, disrupting
           * their positioning relative to X windows too much.
           *
           * Technically we only need to look forward from j if we
           * wanted to optimize this a bit...
           */
          if (find_window (tracker->xserver_stack, current) < 0)
            {
              g_array_remove_index (tracker->verified_stack, i);
              continue;
            }

          /* Technically we only need to look forward from i if we
           * wanted to optimize this a bit... */
          expected_index =
            find_window (tracker->verified_stack, expected_xwindow);

          if (expected_index >= 0)
            {
              expected = &g_array_index (tracker->verified_stack,
                                         MetaStackWindow, expected_index);
            }
          else
            {
              new.any.type = META_WINDOW_CLIENT_TYPE_X11;
              new.x11.xwindow = expected_xwindow->x11.xwindow;

              g_array_append_val (tracker->verified_stack, new);

              expected = &new;
              expected_index = tracker->verified_stack->len - 1;
            }

          /* Note: that this move will effectively bump the index of
           * the current window.
           *
           * We want to continue by re-checking this window against
           * the next expected window though so we don't have to
           * update i to compensate here.
           */
          move_window_above (tracker->verified_stack, expected,
                             expected_index, /* current index */
                             i - 1); /* above */
          modified_stack = TRUE;
        }

      /* NB: we want to make sure that if we break the loop because j
       * reaches the end of xserver_stack that i has also been
       * incremented already so that we can run a final loop to remove
       * remaining windows based on the i index. */
      i++;

      j++;
      expected_xwindow =
        &g_array_index (tracker->xserver_stack, MetaStackWindow, j);

      if (j >= tracker->xserver_stack->len)
        break;
    }

  /* We now know that any remaining X windows aren't listed in the
   * xserver_stack and so we can remove them. */
  while (i < tracker->verified_stack->len)
    {
      MetaStackWindow *current =
        &g_array_index (tracker->verified_stack, MetaStackWindow, i);

      if (current->any.type == META_WINDOW_CLIENT_TYPE_X11)
        g_array_remove_index (tracker->verified_stack, i);
      else
        i++;

      modified_stack = TRUE;
    }

  /* If we get to the end of verified_list and there are any remaining
   * entries in xserver_list then append them all to the end */
  for (; j < tracker->xserver_stack->len; j++)
    {
      MetaStackWindow *current =
        &g_array_index (tracker->xserver_stack, MetaStackWindow, j);
      g_array_append_val (tracker->verified_stack, *current);

      modified_stack = TRUE;
    }

  if (modified_stack)
    {
      if (tracker->predicted_stack)
        {
          g_array_free (tracker->predicted_stack, TRUE);
          tracker->predicted_stack = NULL;
        }

      meta_stack_tracker_queue_sync_stack (tracker);
    }

  return modified_stack;
}

static void
stack_tracker_event_received (MetaStackTracker *tracker,
			      MetaStackOp      *op)
{
  gboolean need_sync = FALSE;
  gboolean verified;

  meta_stack_op_dump (op, "Stack op event received: ", "\n");

  if (op->any.serial < tracker->xserver_serial)
    {
      /* g_warning ("Spurious X event received affecting stack; doing full re-query"); */
      resync_verified_stack_with_xserver_stack (tracker);
      meta_stack_tracker_dump (tracker);
      return;
    }

  tracker->xserver_serial = op->any.serial;

  /* XXX: With the design we have ended up with it looks like we've
   * ended up making it unnecessary to maintain tracker->xserver_stack
   * since we only need an xserver_stack during the
   * resync_verified_stack_with_xserver_stack() at which point we are
   * going to query the full stack from the X server using
   * XQueryTree() anyway.
   *
   * TODO: remove tracker->xserver_stack.
   */
  meta_stack_op_apply (op, tracker->xserver_stack);

  verified = stack_tracker_verify_predictions (tracker, op, &need_sync);
  if (!verified)
    {
      resync_verified_stack_with_xserver_stack (tracker);
      meta_stack_tracker_dump (tracker);
      return;
    }

  meta_stack_tracker_dump (tracker);
}

void
meta_stack_tracker_create_event (MetaStackTracker    *tracker,
				 XCreateWindowEvent  *event)
{
  MetaStackOp op;

  op.any.type = STACK_OP_ADD;
  op.any.serial = event->serial;
  op.add.window.any.type = META_WINDOW_CLIENT_TYPE_X11;
  op.add.window.x11.xwindow = event->window;

  stack_tracker_event_received (tracker, &op);
}

void
meta_stack_tracker_destroy_event (MetaStackTracker    *tracker,
				  XDestroyWindowEvent *event)
{
  MetaStackOp op;

  op.any.type = STACK_OP_REMOVE;
  op.any.serial = event->serial;
  op.remove.window.any.type = META_WINDOW_CLIENT_TYPE_X11;
  op.remove.window.x11.xwindow = event->window;

  stack_tracker_event_received (tracker, &op);
}

void
meta_stack_tracker_reparent_event (MetaStackTracker    *tracker,
				   XReparentEvent      *event)
{
  if (event->parent == event->event)
    {
      MetaStackOp op;

      op.any.type = STACK_OP_ADD;
      op.any.serial = event->serial;
      op.add.window.any.type = META_WINDOW_CLIENT_TYPE_X11;
      op.add.window.x11.xwindow = event->window;

      stack_tracker_event_received (tracker, &op);
    }
  else
    {
      MetaStackOp op;

      op.any.type = STACK_OP_REMOVE;
      op.any.serial = event->serial;
      op.remove.window.any.type = META_WINDOW_CLIENT_TYPE_X11;
      op.remove.window.x11.xwindow = event->window;

      stack_tracker_event_received (tracker, &op);
    }
}

void
meta_stack_tracker_configure_event (MetaStackTracker    *tracker,
				    XConfigureEvent     *event)
{
  MetaStackOp op;

  op.any.type = STACK_OP_RAISE_ABOVE;
  op.any.serial = event->serial;
  op.raise_above.window.any.type = META_WINDOW_CLIENT_TYPE_X11;
  op.raise_above.window.x11.xwindow = event->window;
  op.raise_above.sibling.any.type = META_WINDOW_CLIENT_TYPE_X11;
  op.raise_above.sibling.x11.xwindow = event->above;

  stack_tracker_event_received (tracker, &op);
}

/**
 * meta_stack_tracker_get_stack:
 * @tracker: a #MetaStackTracker
 * @windows: location to store list of windows, or %NULL
 * @n_windows: location to store count of windows, or %NULL
 *
 * @windows will contain the most current view we have of the stacking order
 * of the children of the root window. The returned array contains
 * everything: InputOnly windows, override-redirect windows,
 * hidden windows, etc. Some of these will correspond to MetaWindow
 * objects, others won't.
 *
 * Assuming that no other clients have made requests that change
 * the stacking order since we last received a notification, the
 * returned list of windows is exactly that you'd get as the
 * children when calling XQueryTree() on the root window.
 */
void
meta_stack_tracker_get_stack (MetaStackTracker *tracker,
                              MetaStackWindow **windows,
			      int              *n_windows)
{
  GArray *stack;

  if (tracker->unverified_predictions->length == 0)
    {
      stack = tracker->verified_stack;
    }
  else
    {
      if (tracker->predicted_stack == NULL)
        {
          GList *l;

          tracker->predicted_stack = copy_stack (tracker->verified_stack);
          for (l = tracker->unverified_predictions->head; l; l = l->next)
            {
              MetaStackOp *op = l->data;
              meta_stack_op_apply (op, tracker->predicted_stack);
            }
        }

      stack = tracker->predicted_stack;
    }

  meta_topic (META_DEBUG_STACK, "Get Stack\n");
  meta_stack_tracker_dump (tracker);

  if (windows)
    *windows = (MetaStackWindow *)stack->data;
  if (n_windows)
    *n_windows = stack->len;
}

/**
 * meta_stack_tracker_sync_stack:
 * @tracker: a #MetaStackTracker
 *
 * Informs the compositor of the current stacking order of windows,
 * based on the predicted view maintained by the #MetaStackTracker.
 */
void
meta_stack_tracker_sync_stack (MetaStackTracker *tracker)
{
  MetaStackWindow *windows;
  GList *meta_windows;
  int n_windows;
  int i;

  if (tracker->sync_stack_later)
    {
      meta_later_remove (tracker->sync_stack_later);
      tracker->sync_stack_later = 0;
    }

  meta_stack_tracker_get_stack (tracker, &windows, &n_windows);

  meta_windows = NULL;
  for (i = 0; i < n_windows; i++)
    {
      MetaStackWindow *window = &windows[i];

      if (window->any.type == META_WINDOW_CLIENT_TYPE_X11)
        {
          MetaWindow *meta_window =
            meta_display_lookup_x_window (tracker->screen->display, windows[i].x11.xwindow);

          /* When mapping back from xwindow to MetaWindow we have to be a bit careful;
           * children of the root could include unmapped windows created by toolkits
           * for internal purposes, including ones that we have registered in our
           * XID => window table. (Wine uses a toplevel for _NET_WM_USER_TIME_WINDOW;
           * see window-prop.c:reload_net_wm_user_time_window() for registration.)
           */
          if (meta_window &&
              (windows[i].x11.xwindow == meta_window->xwindow ||
               (meta_window->frame && windows[i].x11.xwindow == meta_window->frame->xwindow)))
            meta_windows = g_list_prepend (meta_windows, meta_window);
        }
      else
        meta_windows = g_list_prepend (meta_windows, window->wayland.meta_window);
    }

  meta_compositor_sync_stack (tracker->screen->display->compositor,
                              meta_windows);
  g_list_free (meta_windows);

  meta_screen_restacked (tracker->screen);
}

static gboolean
stack_tracker_sync_stack_later (gpointer data)
{
  meta_stack_tracker_sync_stack (data);

  return FALSE;
}

/**
 * meta_stack_tracker_queue_sync_stack:
 * @tracker: a #MetaStackTracker
 *
 * Queue informing the compositor of the new stacking order before the
 * next redraw. (See meta_stack_tracker_sync_stack()). This is called
 * internally when the stack of X windows changes, but also needs be
 * called directly when we an undecorated window is first shown or
 * withdrawn since the compositor's stacking order (which contains only
 * the windows that have a corresponding MetaWindow) will change without
 * any change to the stacking order of the X windows, if we are creating
 * or destroying MetaWindows.
 */
void
meta_stack_tracker_queue_sync_stack (MetaStackTracker *tracker)
{
  if (tracker->sync_stack_later == 0)
    {
      tracker->sync_stack_later = meta_later_add (META_LATER_SYNC_STACK,
                                                  stack_tracker_sync_stack_later,
                                                  tracker, NULL);
    }
}

