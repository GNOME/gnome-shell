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
  } any;
  struct {
    MetaStackOpType type;
    gulong serial;
    Window window;
  } add;
  struct {
    MetaStackOpType type;
    gulong serial;
    Window window;
  } remove;
  struct {
    MetaStackOpType type;
    gulong serial;
    Window window;
    Window sibling;
  } raise_above;
  struct {
    MetaStackOpType type;
    gulong serial;
    Window window;
    Window sibling;
  } lower_below;
};

struct _MetaStackTracker
{
  MetaScreen *screen;

  /* This is the last state of the stack as based on events received
   * from the X server.
   */
  GArray *server_stack;

  /* This is the serial of the last request we made that was reflected
   * in server_stack
   */
  gulong server_serial;

  /* This is a queue of requests we've made to change the stacking order,
   * where we haven't yet gotten a reply back from the server.
   */
  GQueue *queued_requests;

  /* This is how we think the stack is, based on server_stack, and
   * on requests we've made subsequent to server_stack
   */
  GArray *predicted_stack;

  /* Idle function used to sync the compositor's view of the window
   * stack up with our best guess before a frame is drawn.
   */
  guint sync_stack_later;
};

static void
meta_stack_op_dump (MetaStackOp *op,
		    const char  *prefix,
		    const char  *suffix)
{
  switch (op->any.type)
    {
    case STACK_OP_ADD:
      meta_topic (META_DEBUG_STACK, "%sADD(%#lx; %ld)%s",
		  prefix, op->add.window, op->any.serial, suffix);
      break;
    case STACK_OP_REMOVE:
      meta_topic (META_DEBUG_STACK, "%sREMOVE(%#lx; %ld)%s",
		  prefix, op->add.window, op->any.serial, suffix);
      break;
    case STACK_OP_RAISE_ABOVE:
      meta_topic (META_DEBUG_STACK, "%sRAISE_ABOVE(%#lx, %#lx; %ld)%s",
		  prefix,
                  op->raise_above.window, op->raise_above.sibling,
                  op->any.serial,
                  suffix);
      break;
    case STACK_OP_LOWER_BELOW:
      meta_topic (META_DEBUG_STACK, "%sLOWER_BELOW(%#lx, %#lx; %ld)%s",
		  prefix,
                  op->lower_below.window, op->lower_below.sibling,
                  op->any.serial,
                  suffix);
      break;
    }
}

static void
meta_stack_tracker_dump (MetaStackTracker *tracker)
{
  guint i;
  GList *l;

  meta_topic (META_DEBUG_STACK, "MetaStackTracker state (screen=%d)\n", tracker->screen->number);
  meta_push_no_msg_prefix ();
  meta_topic (META_DEBUG_STACK, "  server_serial: %ld\n", tracker->server_serial);
  meta_topic (META_DEBUG_STACK, "  server_stack: ");
  for (i = 0; i < tracker->server_stack->len; i++)
    meta_topic (META_DEBUG_STACK, "  %#lx", g_array_index (tracker->server_stack, Window, i));
  if (tracker->predicted_stack)
    {
      meta_topic (META_DEBUG_STACK, "\n  predicted_stack: ");
      for (i = 0; i < tracker->predicted_stack->len; i++)
	meta_topic (META_DEBUG_STACK, "  %#lx", g_array_index (tracker->predicted_stack, Window, i));
    }
  meta_topic (META_DEBUG_STACK, "\n  queued_requests: [");
  for (l = tracker->queued_requests->head; l; l = l->next)
    {
      MetaStackOp *op = l->data;
      meta_stack_op_dump (op, "", l->next ? ", " : "");
    }
  meta_topic (META_DEBUG_STACK, "]\n");
  meta_pop_no_msg_prefix ();
}

static void
meta_stack_op_free (MetaStackOp *op)
{
  g_slice_free (MetaStackOp, op);
}

static int
find_window (GArray *stack,
	     Window  window)
{
  guint i;

  for (i = 0; i < stack->len; i++)
    if (g_array_index (stack, Window, i) == window)
      return i;

  return -1;
}

/* Returns TRUE if stack was changed */
static gboolean
move_window_above (GArray *stack,
                   Window  window,
                   int     old_pos,
                   int     above_pos)
{
  int i;

  if (old_pos < above_pos)
    {
      for (i = old_pos; i < above_pos; i++)
	g_array_index (stack, Window, i) = g_array_index (stack, Window, i + 1);

      g_array_index (stack, Window, above_pos) = window;

      return TRUE;
    }
  else if (old_pos > above_pos + 1)
    {
      for (i = old_pos; i > above_pos + 1; i--)
	g_array_index (stack, Window, i) = g_array_index (stack, Window, i - 1);

      g_array_index (stack, Window, above_pos + 1) = window;

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
	int old_pos = find_window (stack, op->add.window);
	if (old_pos >= 0)
	  {
	    g_warning ("STACK_OP_ADD: window %#lx already in stack",
		       op->add.window);
	    return FALSE;
	  }

	g_array_append_val (stack, op->add.window);
	return TRUE;
      }
    case STACK_OP_REMOVE:
      {
	int old_pos = find_window (stack, op->remove.window);
	if (old_pos < 0)
	  {
	    g_warning ("STACK_OP_REMOVE: window %#lx not in stack",
		       op->remove.window);
	    return FALSE;
	  }

	g_array_remove_index (stack, old_pos);
	return TRUE;
      }
    case STACK_OP_RAISE_ABOVE:
      {
	int old_pos = find_window (stack, op->raise_above.window);
	int above_pos;
	if (old_pos < 0)
	  {
	    g_warning ("STACK_OP_RAISE_ABOVE: window %#lx not in stack",
		       op->raise_above.window);
	    return FALSE;
	  }

	if (op->raise_above.sibling != None)
	  {
	    above_pos = find_window (stack, op->raise_above.sibling);
	    if (above_pos < 0)
	      {
		g_warning ("STACK_OP_RAISE_ABOVE: sibling window %#lx not in stack",
			   op->raise_above.sibling);
		return FALSE;
	      }
	  }
	else
	  {
	    above_pos = -1;
	  }

	return move_window_above (stack, op->raise_above.window, old_pos, above_pos);
      }
    case STACK_OP_LOWER_BELOW:
      {
	int old_pos = find_window (stack, op->lower_below.window);
	int above_pos;
	if (old_pos < 0)
	  {
	    g_warning ("STACK_OP_LOWER_BELOW: window %#lx not in stack",
		       op->lower_below.window);
	    return FALSE;
	  }

	if (op->lower_below.sibling != None)
	  {
	    int below_pos = find_window (stack, op->lower_below.sibling);
	    if (below_pos < 0)
	      {
		g_warning ("STACK_OP_LOWER_BELOW: sibling window %#lx not in stack",
			   op->lower_below.sibling);
		return FALSE;
	      }

	    above_pos = below_pos - 1;
	  }
	else
	  {
	    above_pos = stack->len - 1;
	  }

	return move_window_above (stack, op->lower_below.window, old_pos, above_pos);
      }
    }

  g_assert_not_reached ();
  return FALSE;
}

static GArray *
copy_stack (Window *windows,
	    guint   n_windows)
{
  GArray *stack = g_array_new (FALSE, FALSE, sizeof (Window));

  g_array_set_size (stack, n_windows);
  memcpy (stack->data, windows, sizeof (Window) * n_windows);

  return stack;
}

MetaStackTracker *
meta_stack_tracker_new (MetaScreen *screen)
{
  MetaStackTracker *tracker;
  Window ignored1, ignored2;
  Window *children;
  guint n_children;

  tracker = g_new0 (MetaStackTracker, 1);
  tracker->screen = screen;

  tracker->server_serial = XNextRequest (screen->display->xdisplay);

  XQueryTree (screen->display->xdisplay,
              screen->xroot,
              &ignored1, &ignored2, &children, &n_children);
  tracker->server_stack = copy_stack (children, n_children);
  XFree (children);

  tracker->queued_requests = g_queue_new ();

  return tracker;
}

void
meta_stack_tracker_free (MetaStackTracker *tracker)
{
  if (tracker->sync_stack_later)
    meta_later_remove (tracker->sync_stack_later);

  g_array_free (tracker->server_stack, TRUE);
  if (tracker->predicted_stack)
    g_array_free (tracker->predicted_stack, TRUE);

  g_queue_foreach (tracker->queued_requests, (GFunc)meta_stack_op_free, NULL);
  g_queue_free (tracker->queued_requests);
  tracker->queued_requests = NULL;

  g_free (tracker);
}

static void
stack_tracker_queue_request (MetaStackTracker *tracker,
			     MetaStackOp      *op)
{
  meta_stack_op_dump (op, "Queueing: ", "\n");
  g_queue_push_tail (tracker->queued_requests, op);
  if (!tracker->predicted_stack ||
      meta_stack_op_apply (op, tracker->predicted_stack))
    meta_stack_tracker_queue_sync_stack (tracker);

  meta_stack_tracker_dump (tracker);
}

void
meta_stack_tracker_record_add (MetaStackTracker *tracker,
			       Window            window,
			       gulong            serial)
{
  MetaStackOp *op = g_slice_new (MetaStackOp);

  op->any.type = STACK_OP_ADD;
  op->any.serial = serial;
  op->add.window = window;

  stack_tracker_queue_request (tracker, op);
}

void
meta_stack_tracker_record_remove (MetaStackTracker *tracker,
				  Window            window,
				  gulong            serial)
{
  MetaStackOp *op = g_slice_new (MetaStackOp);

  op->any.type = STACK_OP_REMOVE;
  op->any.serial = serial;
  op->remove.window = window;

  stack_tracker_queue_request (tracker, op);
}

void
meta_stack_tracker_record_restack_windows (MetaStackTracker *tracker,
					   Window           *windows,
					   int               n_windows,
					   gulong            serial)
{
  int i;

  /* XRestackWindows() isn't actually a X requests - it's broken down
   * by XLib into a series of XConfigureWindow(StackMode=below); we
   * mirror that exactly here.
   *
   * Aside: Having a separate StackOp for this would be possible to
   * get some extra efficiency in memory allocation and in applying
   * the op, at the expense of a code complexity. Implementation hint
   * for that - keep op->restack_window.n_complete, and when receiving
   * events with intermediate serials, set n_complete rather than
   * removing the op from the queue.
   */
  for (i = 0; i < n_windows - 1; i++)
    meta_stack_tracker_record_lower_below (tracker, windows[i + 1], windows[i],
					   serial + i);
}

void
meta_stack_tracker_record_raise_above (MetaStackTracker *tracker,
				       Window            window,
				       Window            sibling,
				       gulong            serial)
{
  MetaStackOp *op = g_slice_new (MetaStackOp);

  op->any.type = STACK_OP_RAISE_ABOVE;
  op->any.serial = serial;
  op->raise_above.window = window;
  op->raise_above.sibling = sibling;

  stack_tracker_queue_request (tracker, op);
}

void
meta_stack_tracker_record_lower_below (MetaStackTracker *tracker,
				       Window            window,
				       Window            sibling,
				       gulong            serial)
{
  MetaStackOp *op = g_slice_new (MetaStackOp);

  op->any.type = STACK_OP_LOWER_BELOW;
  op->any.serial = serial;
  op->lower_below.window = window;
  op->lower_below.sibling = sibling;

  stack_tracker_queue_request (tracker, op);
}

void
meta_stack_tracker_record_lower (MetaStackTracker *tracker,
				 Window            window,
				 gulong            serial)
{
  meta_stack_tracker_record_raise_above (tracker, window, None, serial);
}

static void
stack_tracker_event_received (MetaStackTracker *tracker,
			      MetaStackOp      *op)
{
  gboolean need_sync = FALSE;

  meta_stack_op_dump (op, "Stack op event received: ", "\n");

  if (op->any.serial < tracker->server_serial)
    return;

  tracker->server_serial = op->any.serial;

  if (meta_stack_op_apply (op, tracker->server_stack))
    need_sync = TRUE;

  while (tracker->queued_requests->head)
    {
      MetaStackOp *queued_op = tracker->queued_requests->head->data;
      if (queued_op->any.serial > op->any.serial)
	break;

      g_queue_pop_head (tracker->queued_requests);
      meta_stack_op_free (queued_op);
      need_sync = TRUE;
    }

  if (need_sync)
    {
      if (tracker->predicted_stack)
        {
          g_array_free (tracker->predicted_stack, TRUE);
          tracker->predicted_stack = NULL;
        }

      meta_stack_tracker_queue_sync_stack (tracker);
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
  op.add.window = event->window;

  stack_tracker_event_received (tracker, &op);
}

void
meta_stack_tracker_destroy_event (MetaStackTracker    *tracker,
				  XDestroyWindowEvent *event)
{
  MetaStackOp op;

  op.any.type = STACK_OP_REMOVE;
  op.any.serial = event->serial;
  op.remove.window = event->window;

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
      op.add.window = event->window;

      stack_tracker_event_received (tracker, &op);
    }
  else
    {
      MetaStackOp op;

      op.any.type = STACK_OP_REMOVE;
      op.any.serial = event->serial;
      op.remove.window = event->window;

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
  op.raise_above.window = event->window;
  op.raise_above.sibling = event->above;

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
			      Window          **windows,
			      int              *n_windows)
{
  GArray *stack;

  if (tracker->queued_requests->length == 0)
    {
      stack = tracker->server_stack;
    }
  else
    {
      if (tracker->predicted_stack == NULL)
        {
          GList *l;

          tracker->predicted_stack = copy_stack ((Window *)tracker->server_stack->data,
                                                 tracker->server_stack->len);
          for (l = tracker->queued_requests->head; l; l = l->next)
            {
              MetaStackOp *op = l->data;
              meta_stack_op_apply (op, tracker->predicted_stack);
            }
        }

      stack = tracker->predicted_stack;
    }

  if (windows)
    *windows = (Window *)stack->data;
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
  GList *meta_windows;
  Window *windows;
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
      MetaWindow *meta_window;

      meta_window = meta_display_lookup_x_window (tracker->screen->display,
                                                  windows[i]);
      /* When mapping back from xwindow to MetaWindow we have to be a bit careful;
       * children of the root could include unmapped windows created by toolkits
       * for internal purposes, including ones that we have registered in our
       * XID => window table. (Wine uses a toplevel for _NET_WM_USER_TIME_WINDOW;
       * see window-prop.c:reload_net_wm_user_time_window() for registration.)
       */
      if (meta_window &&
          (windows[i] == meta_window->xwindow ||
           (meta_window->frame && windows[i] == meta_window->frame->xwindow)))
        meta_windows = g_list_prepend (meta_windows, meta_window);
    }

  if (tracker->screen->display->compositor)
    meta_compositor_sync_stack (tracker->screen->display->compositor,
                                tracker->screen,
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

