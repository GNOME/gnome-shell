/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter X event source for main loop */

/* 
 * Copyright (C) 2001 Havoc Pennington (based on GDK code (C) Owen
 * Taylor, Red Hat Inc.)
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
 * 02111-1307, USA.  */

#include "eventqueue.h"
#include <X11/Xlib.h>

static gboolean eq_prepare  (GSource     *source,
                             gint        *timeout);
static gboolean eq_check    (GSource     *source);
static gboolean eq_dispatch (GSource     *source,
                             GSourceFunc  callback,
                             gpointer     user_data);
static void     eq_destroy  (GSource     *source);

static GSourceFuncs eq_funcs = {
  eq_prepare,
  eq_check,
  eq_dispatch,
  eq_destroy
};

struct _MetaEventQueue
{
  GSource source;

  Display *display;
  GPollFD poll_fd;
  int connection_fd;
  GQueue *events;
};

MetaEventQueue*
meta_event_queue_new (Display *display, MetaEventQueueFunc func, gpointer data)
{
  GSource *source;
  MetaEventQueue *eq;

  source = g_source_new (&eq_funcs, sizeof (MetaEventQueue));
  eq = (MetaEventQueue*) source;
  
  eq->connection_fd = ConnectionNumber (display);
  eq->poll_fd.fd = eq->connection_fd;
  eq->poll_fd.events = G_IO_IN;

  eq->events = g_queue_new ();

  eq->display = display;
  
  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_add_poll (source, &eq->poll_fd);
  g_source_set_can_recurse (source, TRUE);

  g_source_set_callback (source, (GSourceFunc) func, data, NULL);
  
  g_source_attach (source, NULL);
  g_source_unref (source);

  return eq;
}

void
meta_event_queue_free (MetaEventQueue *eq)
{
  GSource *source;

  source = (GSource*) eq;
  
  g_source_destroy (source);
}

static gboolean
eq_events_pending (MetaEventQueue *eq)
{
  return eq->events->length > 0 || XPending (eq->display);
}

static void
eq_queue_events (MetaEventQueue *eq)
{
  XEvent xevent;

  while (XPending (eq->display))
    {
      XEvent *copy;
      
      XNextEvent (eq->display, &xevent);

      copy = g_new (XEvent, 1);
      *copy = xevent;

      g_queue_push_tail (eq->events, copy);
    }
}

static gboolean  
eq_prepare (GSource *source, gint *timeout)
{
  MetaEventQueue *eq;

  eq = (MetaEventQueue*) source;
  
  *timeout = -1;

  return eq_events_pending (eq);
}

static gboolean  
eq_check (GSource  *source) 
{
  MetaEventQueue *eq;

  eq = (MetaEventQueue*) source;

  if (eq->poll_fd.revents & G_IO_IN)
    return eq_events_pending (eq);
  else
    return FALSE;
}

static gboolean  
eq_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
  MetaEventQueue *eq;

  eq = (MetaEventQueue*) source;
  
  eq_queue_events (eq);

  if (eq->events->length > 0)
    {
      XEvent *event;
      MetaEventQueueFunc func;

      event = g_queue_pop_head (eq->events);
      func = (MetaEventQueueFunc) callback;
      
      (* func) (event, user_data);

      g_free (event);
    }
  
  return TRUE;
}

static void
eq_destroy (GSource *source)
{
  MetaEventQueue *eq;

  eq = (MetaEventQueue*) source;

  while (eq->events->length > 0)
    {
      XEvent *event;
      
      event = g_queue_pop_head (eq->events);

      g_free (event);
    }

  g_queue_free (eq->events);

  /* source itself is freed by glib */
}
