/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * ClutterTimeoutPool: pool of timeout functions using the same slice of
 *                     the GLib main loop
 *
 * Author: Emmanuele Bassi <ebassi@openedhand.com>
 *
 * Based on similar code by Tristan van Berkom
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-debug.h"
#include "clutter-timeout-pool.h"
#include "clutter-timeout-interval.h"

typedef struct _ClutterTimeout  ClutterTimeout;
typedef enum {
  CLUTTER_TIMEOUT_NONE   = 0,
  CLUTTER_TIMEOUT_READY  = 1 << 1
} ClutterTimeoutFlags;

struct _ClutterTimeout
{
  guint id;
  ClutterTimeoutFlags flags;
  gint refcount;

  ClutterTimeoutInterval interval;

  GSourceFunc func;
  gpointer data;
  GDestroyNotify notify;
};

struct _ClutterTimeoutPool
{
  GSource source;

  guint next_id;

  GTimeVal start_time;
  GList *timeouts, *dispatched_timeouts;
  gint ready;

  guint id;
};

#define TIMEOUT_READY(timeout)   (timeout->flags & CLUTTER_TIMEOUT_READY)

static gboolean clutter_timeout_pool_prepare  (GSource     *source,
                                               gint        *next_timeout);
static gboolean clutter_timeout_pool_check    (GSource     *source);
static gboolean clutter_timeout_pool_dispatch (GSource     *source,
                                               GSourceFunc  callback,
                                               gpointer     data);
static void clutter_timeout_pool_finalize     (GSource     *source);

static GSourceFuncs clutter_timeout_pool_funcs =
{
  clutter_timeout_pool_prepare,
  clutter_timeout_pool_check,
  clutter_timeout_pool_dispatch,
  clutter_timeout_pool_finalize
};

static gint
clutter_timeout_sort (gconstpointer a,
                      gconstpointer b)
{
  const ClutterTimeout *t_a = a;
  const ClutterTimeout *t_b = b;

  /* Keep 'ready' timeouts at the front */
  if (TIMEOUT_READY (t_a))
    return -1;

  if (TIMEOUT_READY (t_b))
    return 1;

  return _clutter_timeout_interval_compare_expiration (&t_a->interval,
                                                       &t_b->interval);
}

static gint
clutter_timeout_find_by_id (gconstpointer a,
                            gconstpointer b)
{
  const ClutterTimeout *t_a = a;

  return t_a->id == GPOINTER_TO_UINT (b) ? 0 : 1;
}

static ClutterTimeout *
clutter_timeout_new (guint fps)
{
  ClutterTimeout *timeout;

  timeout = g_slice_new0 (ClutterTimeout);
  _clutter_timeout_interval_init (&timeout->interval, fps);
  timeout->flags = CLUTTER_TIMEOUT_NONE;
  timeout->refcount = 1;

  return timeout;
}

static gboolean
clutter_timeout_prepare (ClutterTimeoutPool *pool,
                         ClutterTimeout     *timeout,
                         gint               *next_timeout)
{
  GTimeVal now;

  g_source_get_current_time (&pool->source, &now);

  return _clutter_timeout_interval_prepare (&now, &timeout->interval,
                                            next_timeout);
}

/* ref and unref are always called under the main Clutter lock, so there
 * is not need for us to use g_atomic_int_* API.
 */

static ClutterTimeout *
clutter_timeout_ref (ClutterTimeout *timeout)
{
  g_return_val_if_fail (timeout != NULL, timeout);
  g_return_val_if_fail (timeout->refcount > 0, timeout);

  timeout->refcount += 1;

  return timeout;
}

static void
clutter_timeout_unref (ClutterTimeout *timeout)
{
  g_return_if_fail (timeout != NULL);
  g_return_if_fail (timeout->refcount > 0);

  timeout->refcount -= 1;

  if (timeout->refcount == 0)
    {
      if (timeout->notify)
        timeout->notify (timeout->data);

      g_slice_free (ClutterTimeout, timeout);
    }
}

static void
clutter_timeout_free (ClutterTimeout *timeout)
{
  if (G_LIKELY (timeout))
    {
      if (timeout->notify)
        timeout->notify (timeout->data);

      g_slice_free (ClutterTimeout, timeout);
    }
}

static gboolean
clutter_timeout_pool_prepare (GSource *source,
                              gint    *next_timeout)
{
  ClutterTimeoutPool *pool = (ClutterTimeoutPool *) source;
  GList *l = pool->timeouts;

  /* the pool is ready if the first timeout is ready */
  if (l && l->data)
    {
      ClutterTimeout *timeout = l->data;
      return clutter_timeout_prepare (pool, timeout, next_timeout);
    }
  else
    {
      *next_timeout = -1;
      return FALSE;
    }
}

static gboolean
clutter_timeout_pool_check (GSource *source)
{
  ClutterTimeoutPool *pool = (ClutterTimeoutPool *) source;
  GList *l = pool->timeouts;

  clutter_threads_enter ();

  for (l = pool->timeouts; l; l = l->next)
    {
      ClutterTimeout *timeout = l->data;

      /* since the timeouts are sorted by expiration, as soon
       * as we get a check returning FALSE we know that the
       * following timeouts are not expiring, so we break as
       * soon as possible
       */
      if (clutter_timeout_prepare (pool, timeout, NULL))
        {
          timeout->flags |= CLUTTER_TIMEOUT_READY;
          pool->ready += 1;
        }
      else
        break;
    }

  clutter_threads_leave ();

  return (pool->ready > 0);
}

static gboolean
clutter_timeout_pool_dispatch (GSource     *source,
                               GSourceFunc  func,
                               gpointer     data)
{
  ClutterTimeoutPool *pool = (ClutterTimeoutPool *) source;
  GList *dispatched_timeouts;

  /* the main loop might have predicted this, so we repeat the
   * check for ready timeouts.
   */
  if (!pool->ready)
    clutter_timeout_pool_check (source);

  clutter_threads_enter ();

  /* Iterate by moving the actual start of the list along so that it
   * can cope with adds and removes while a timeout is being dispatched
   */
  while (pool->timeouts && pool->timeouts->data && pool->ready-- > 0)
    {
      ClutterTimeout *timeout = pool->timeouts->data;
      GList *l;

      /* One of the ready timeouts may have been removed during dispatch,
       * in which case pool->ready will be wrong, but the ready timeouts
       * are always kept at the start of the list so we can stop once
       * we've reached the first non-ready timeout
       */
      if (!(TIMEOUT_READY (timeout)))
	break;

      /* Add a reference to the timeout so it can't disappear
       * while it's being dispatched
       */
      clutter_timeout_ref (timeout);

      timeout->flags &= ~CLUTTER_TIMEOUT_READY;

      /* Move the list node to a list of dispatched timeouts */
      l = pool->timeouts;
      if (l->next)
	l->next->prev = NULL;

      pool->timeouts = l->next;

      if (pool->dispatched_timeouts)
	pool->dispatched_timeouts->prev = l;

      l->prev = NULL;
      l->next = pool->dispatched_timeouts;
      pool->dispatched_timeouts = l;

      if (!_clutter_timeout_interval_dispatch (&timeout->interval,
                                               timeout->func, timeout->data))
	{
	  /* The timeout may have already been removed, but nothing
           * can be added to the dispatched_timeout list except in this
           * function so it will always either be at the head of the
           * dispatched list or have been removed
           */
          if (pool->dispatched_timeouts &&
              pool->dispatched_timeouts->data == timeout)
	    {
	      pool->dispatched_timeouts =
                g_list_delete_link (pool->dispatched_timeouts,
                                    pool->dispatched_timeouts);

	      /* Remove the reference that was held by it being in the list */
	      clutter_timeout_unref (timeout);
	    }
	}

      clutter_timeout_unref (timeout);
    }

  /* Re-insert the dispatched timeouts in sorted order */
  dispatched_timeouts = pool->dispatched_timeouts;
  while (dispatched_timeouts)
    {
      ClutterTimeout *timeout = dispatched_timeouts->data;
      GList *next = dispatched_timeouts->next;

      if (timeout)
        pool->timeouts = g_list_insert_sorted (pool->timeouts, timeout,
                                               clutter_timeout_sort);

      dispatched_timeouts = next;
    }

  g_list_free (pool->dispatched_timeouts);
  pool->dispatched_timeouts = NULL;

  pool->ready = 0;

  clutter_threads_leave ();

  return TRUE;
}

static void
clutter_timeout_pool_finalize (GSource *source)
{
  ClutterTimeoutPool *pool = (ClutterTimeoutPool *) source;

  /* force destruction */
  g_list_foreach (pool->timeouts, (GFunc) clutter_timeout_free, NULL);
  g_list_free (pool->timeouts);
}

/**
 * clutter_timeout_pool_new:
 * @priority: the priority of the timeout pool. Typically this will
 *   be #G_PRIORITY_DEFAULT
 *
 * Creates a new timeout pool source. A timeout pool should be used when
 * multiple timeout functions, running at the same priority, are needed and
 * the g_timeout_add() API might lead to starvation of the time slice of
 * the main loop. A timeout pool allocates a single time slice of the main
 * loop and runs every timeout function inside it. The timeout pool is
 * always sorted, so that the extraction of the next timeout function is
 * a constant time operation.
 *
 * Return value: the newly created #ClutterTimeoutPool. The created pool
 *   is owned by the GLib default context and will be automatically
 *   destroyed when the context is destroyed. It is possible to force
 *   the destruction of the timeout pool using g_source_destroy()
 *
 * Since: 0.4
 */
ClutterTimeoutPool *
clutter_timeout_pool_new (gint priority)
{
  ClutterTimeoutPool *pool;
  GSource *source;

  source = g_source_new (&clutter_timeout_pool_funcs,
                         sizeof (ClutterTimeoutPool));
  if (!source)
    return NULL;

  if (priority != G_PRIORITY_DEFAULT)
    g_source_set_priority (source, priority);

  pool = (ClutterTimeoutPool *) source;

  g_get_current_time (&pool->start_time);
  pool->next_id = 1;
  pool->id = g_source_attach (source, NULL);

  /* let the default GLib context manage the pool */
  g_source_unref (source);

  return pool;
}

/**
 * clutter_timeout_pool_add:
 * @pool: a #ClutterTimeoutPool
 * @fps: the time between calls to the function, in frames per second
 * @func: function to call
 * @data: data to pass to the function, or %NULL
 * @notify: function to call when the timeout is removed, or %NULL
 *
 * Sets a function to be called at regular intervals, and puts it inside
 * the @pool. The function is repeatedly called until it returns %FALSE,
 * at which point the timeout is automatically destroyed and the function
 * won't be called again. If @notify is not %NULL, the @notify function
 * will be called. The first call to @func will be at the end of @interval.
 *
 * Since Clutter 0.8 this will try to compensate for delays. For
 * example, if @func takes half the interval time to execute then the
 * function will be called again half the interval time after it
 * finished. Before version 0.8 it would not fire until a full
 * interval after the function completes so the delay between calls
 * would be @interval * 1.5. This function does not however try to
 * invoke the function multiple times to catch up missing frames if
 * @func takes more than @interval ms to execute.
 *
 * Return value: the ID (greater than 0) of the timeout inside the pool.
 *   Use clutter_timeout_pool_remove() to stop the timeout.
 *
 * Since: 0.4
 */
guint
clutter_timeout_pool_add (ClutterTimeoutPool *pool,
                          guint               fps,
                          GSourceFunc         func,
                          gpointer            data,
                          GDestroyNotify      notify)
{
  ClutterTimeout *timeout;
  guint retval = 0;

  timeout = clutter_timeout_new (fps);

  retval = timeout->id = pool->next_id++;

  timeout->func = func;
  timeout->data = data;
  timeout->notify = notify;

  pool->timeouts = g_list_insert_sorted (pool->timeouts, timeout,
                                         clutter_timeout_sort);

  return retval;
}

/**
 * clutter_timeout_pool_remove:
 * @pool: a #ClutterTimeoutPool
 * @id: the id of the timeout to remove
 *
 * Removes a timeout function with @id from the timeout pool. The id
 * is the same returned when adding a function to the timeout pool with
 * clutter_timeout_pool_add().
 *
 * Since: 0.4
 */
void
clutter_timeout_pool_remove (ClutterTimeoutPool *pool,
                             guint               id)
{
  GList *l;

  if ((l = g_list_find_custom (pool->timeouts, GUINT_TO_POINTER (id),
			       clutter_timeout_find_by_id)))
    {
      clutter_timeout_unref (l->data);
      pool->timeouts = g_list_delete_link (pool->timeouts, l);
    }
  else if ((l = g_list_find_custom (pool->dispatched_timeouts,
				    GUINT_TO_POINTER (id),
				    clutter_timeout_find_by_id)))
    {
      clutter_timeout_unref (l->data);
      pool->dispatched_timeouts
	= g_list_delete_link (pool->dispatched_timeouts, l);
    }
}
