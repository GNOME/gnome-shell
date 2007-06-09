v/*
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

#include "clutter-timeout-pool.h"

#include "clutter-main.h"
#include "clutter-debug.h"
#include "clutter-private.h"

typedef struct _ClutterTimeout  ClutterTimeout;

struct _ClutterTimeout
{
  guint id;
  
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

  if (l)
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

      if (clutter_timeout_check (source, timeout))
        pool->ready += 1;
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

  if (!pool->ready)
    clutter_timeout_pool_check (source);

  while (l && l->data && (pool->ready-- > 0))
    {
      ClutterTimeout *timeout = l->data;
      GList *next = l->next;

      if (clutter_timeout_dispatch (source, timeout))
        {
          sort_needed = TRUE;
        }
      else
        {
          pool->timeouts = g_list_delete_link (pool->timeouts, l);
          clutter_timeout_free (timeout);
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
 * @priority:
 *
 * FIXME
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
 * @interval: FIXME
 * @func: FIXME
 * @data: FIXME
 * @notify: FIXME
 *
 * FIXME
 *
 * Return value: FIXME
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
 * @id: FIXME
 *
 * FIXME
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
      clutter_timeout_free (l->data);
      pool->timeouts = g_list_delete_link (pool->timeouts, l);
    }
}
