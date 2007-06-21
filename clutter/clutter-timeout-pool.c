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

#include "config.h"

#include "clutter-debug.h"
#include "clutter-timeout-pool.h"

typedef struct _ClutterTimeout  ClutterTimeout;
typedef enum {
  CLUTTER_TIMEOUT_NONE   = 0,
  CLUTTER_TIMEOUT_ACTIVE = 1 << 0,
  CLUTTER_TIMEOUT_READY  = 1 << 1
} ClutterTimeoutFlags;

struct _ClutterTimeout
{
  guint id;
  ClutterTimeoutFlags flags;
  
  guint interval;

  GSourceFunc func;
  gpointer data;
  GDestroyNotify notify;

  GTimeVal expiration;
};

struct _ClutterTimeoutPool
{
  GSource source;

  guint next_id;

  GList *timeouts;
  gint ready;

  guint id;
};

#define TIMEOUT_REMOVED(timeout) ((timeout->flags & CLUTTER_TIMEOUT_ACTIVE) == 0)
#define TIMEOUT_READY(timeout)   ((timeout->flags & CLUTTER_TIMEOUT_READY) == 1)

static gboolean clutter_timeout_pool_prepare  (GSource     *source,
                                               gint        *next_timeout);
static gboolean clutter_timeout_pool_check    (GSource     *source);
static gboolean clutter_timeout_pool_dispatch (GSource     *source,
                                               GSourceFunc  callback,
                                               gpointer     data);
static void clutter_timeout_pool_finalize (GSource     *source);

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

  return ((t_a->expiration.tv_sec < t_b->expiration.tv_sec) ||
          ((t_a->expiration.tv_sec == t_b->expiration.tv_sec) &&
           (t_a->expiration.tv_usec <= t_b->expiration.tv_usec)));
}

static gint
clutter_timeout_find_by_id (gconstpointer a,
                            gconstpointer b)
{
  const ClutterTimeout *t_a = a;

  return t_a->id == GPOINTER_TO_UINT (b) ? 0 : 1;
}

static void
clutter_timeout_set_expiration (ClutterTimeout *timeout,
                                GTimeVal       *current_time)
{
  guint seconds = timeout->interval / 1000;
  guint msecs = timeout->interval - seconds * 1000;

  timeout->expiration.tv_sec  = current_time->tv_sec + seconds;
  timeout->expiration.tv_usec = current_time->tv_usec + msecs * 1000;

  if (timeout->expiration.tv_usec >= 1000000)
    {
      timeout->expiration.tv_usec -= 1000000;
      timeout->expiration.tv_sec += 1;
    }
}

static gboolean
clutter_timeout_prepare (GSource        *source,
                         ClutterTimeout *timeout,
                         gint           *next_timeout)
{
  glong sec;
  glong msec;
  GTimeVal current_time;

  g_source_get_current_time (source, &current_time);

  sec = timeout->expiration.tv_sec - current_time.tv_sec;
  msec = (timeout->expiration.tv_usec - current_time.tv_usec) / 1000;

  if (sec < 0 || (sec == 0 && msec < 0))
    msec = 0;
  else
    {
      glong interval_sec = timeout->interval / 1000;
      glong interval_msec = timeout->interval % 1000;

      if (msec < 0)
        {
          msec += 1000;
          sec -= 1;
        }

      if (sec > interval_sec ||
          (sec == interval_sec && msec > interval_msec))
        {
          clutter_timeout_set_expiration (timeout, &current_time);
          msec = MIN (G_MAXINT, timeout->interval);
        }
      else
        msec = MIN (G_MAXINT, (guint) msec + 1000 * (guint) sec);
    }

  *next_timeout = (gint) msec;
  return (msec == 0);
}

static gboolean
clutter_timeout_check (GSource        *source,
                       ClutterTimeout *timeout)
{
  GTimeVal current_time;

  g_source_get_current_time (source, &current_time);

  return ((timeout->expiration.tv_sec < current_time.tv_sec) ||
          ((timeout->expiration.tv_sec == current_time.tv_sec) &&
           (timeout->expiration.tv_usec <= current_time.tv_usec)));
}

static gboolean
clutter_timeout_dispatch (GSource        *source,
                          ClutterTimeout *timeout)
{
  if (G_UNLIKELY (!timeout->func))
    {
      g_warning ("Timeout dispatched without a callback.");
      return FALSE;
    }

  if (timeout->func (timeout->data))
    {
      GTimeVal current_time;

      g_source_get_current_time (source, &current_time);
      clutter_timeout_set_expiration (timeout, &current_time);

      return TRUE;
    }
  else
    return FALSE;
}

static ClutterTimeout *
clutter_timeout_new (guint interval)
{
  ClutterTimeout *timeout;
  GTimeVal current_time;

  timeout = g_slice_new0 (ClutterTimeout);
  timeout->interval = interval;
  timeout->flags = CLUTTER_TIMEOUT_NONE;

  g_get_current_time (&current_time);
  clutter_timeout_set_expiration (timeout, &current_time);

  return timeout;
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
      return clutter_timeout_prepare (source, timeout, next_timeout);
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

  for (l = pool->timeouts; l; l = l->next)
    {
      ClutterTimeout *timeout = l->data;

      /* since the timeouts are sorted by expiration, as soon
       * as we get a check returning FALSE we know that the
       * following timeouts are not expiring, so we break as
       * soon as possible
       */
      if (clutter_timeout_check (source, timeout))
        {
          timeout->flags |= CLUTTER_TIMEOUT_READY;
          pool->ready += 1;
        }
      else
        break;
    }

  return (pool->ready > 0);
}

static gboolean
clutter_timeout_pool_dispatch (GSource     *source,
                               GSourceFunc  func,
                               gpointer     data)
{
  ClutterTimeoutPool *pool = (ClutterTimeoutPool *) source;
  GList *l = pool->timeouts;
  gboolean sort_needed = FALSE;

  /* the main loop might have predicted this, so we repeat the
   * check for ready timeouts.
   */
  if (!pool->ready)
    clutter_timeout_pool_check (source);

  while (l && l->data && (pool->ready-- > 0))
    {
      ClutterTimeout *timeout = l->data;
      GList *next = l->next;
      gboolean needs_removing = FALSE;

      timeout->flags &= ~CLUTTER_TIMEOUT_READY;

      if (!TIMEOUT_REMOVED (timeout))
        {
          needs_removing = !clutter_timeout_dispatch (source, timeout);
          sort_needed = needs_removing;
        }
      else
        needs_removing = TRUE;

      if (needs_removing)
        {
          pool->timeouts = g_list_remove_link (pool->timeouts, l);

          clutter_timeout_free (timeout);
          g_list_free1 (l);
        }

      l = next;
    }

  if (sort_needed)
    pool->timeouts = g_list_sort (pool->timeouts, clutter_timeout_sort);

  pool->ready = 0;

  return TRUE;
}

static void
clutter_timeout_pool_finalize (GSource *source)
{
  ClutterTimeoutPool *pool = (ClutterTimeoutPool *) source;

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
 * Inside Clutter, every #ClutterTimeline share the same timeout pool, unless
 * the CLUTTER_TIMELINE=no-pool environment variable is set.
 *
 * Return value: the newly created #ClutterTimeoutPool
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
  pool->next_id = 1;
  pool->id = g_source_attach (source, NULL);
  g_source_unref (source);

  return pool;
}

/**
 * clutter_timeout_pool_add:
 * @pool: a #ClutterTimeoutPool
 * @interval: the time between calls to the function, in milliseconds
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
 * Note that timeout functions may be delayed, due to the processing of other
 * event sources. Thus they should not be relied on for precise timing.
 * After each call to the timeout function, the time of the next
 * timeout is recalculated based on the current time and the given interval
 * (it does not try to 'catch up' time lost in delays).
 * 
 * Return value: the ID (greater than 0) of the timeout inside the pool.
 *   Use clutter_timeout_pool_remove() to stop the timeout.
 *
 * Since: 0.4
 */
guint
clutter_timeout_pool_add (ClutterTimeoutPool *pool,
                          guint               interval,
                          GSourceFunc         func,
                          gpointer            data,
                          GDestroyNotify      notify)
{
  ClutterTimeout *timeout;
  guint retval = 0;

  timeout = clutter_timeout_new (interval);
  timeout->flags |= CLUTTER_TIMEOUT_ACTIVE;

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

  l = g_list_find_custom (pool->timeouts, GUINT_TO_POINTER (id),
                          clutter_timeout_find_by_id);
  if (l)
    {
      ClutterTimeout *timeout = l->data;

      timeout->flags &= ~CLUTTER_TIMEOUT_ACTIVE;
    }
}
