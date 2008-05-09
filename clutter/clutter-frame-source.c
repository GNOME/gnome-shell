/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2008 OpenedHand
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-frame-source.h"

typedef struct _ClutterFrameSource ClutterFrameSource;

struct _ClutterFrameSource
{
  GSource source;

  GTimeVal start_time;
  guint last_time, frame_time;
};

static gboolean clutter_frame_source_prepare  (GSource *source, gint *timeout);
static gboolean clutter_frame_source_check    (GSource *source);
static gboolean clutter_frame_source_dispatch (GSource    *source,
					       GSourceFunc callback,
					       gpointer    user_data);

static GSourceFuncs clutter_frame_source_funcs = 
  {
    clutter_frame_source_prepare,
    clutter_frame_source_check,
    clutter_frame_source_dispatch,
    NULL
  };

/**
 * clutter_frame_source_add_full:
 * @priority: the priority of the frame source. Typically this will be in the
 *            range between #G_PRIORITY_DEFAULT and #G_PRIORITY_HIGH.
 * @interval: the time between calls to the function, in milliseconds
 * @func: function to call
 * @data: data to pass to the function
 * @notify: function to call when the timeout source is removed
 *
 * Sets a function to be called at regular intervals with the given
 * priority.  The function is called repeatedly until it returns
 * %FALSE, at which point the timeout is automatically destroyed and
 * the function will not be called again.  The @notify function is
 * called when the timeout is destroyed.  The first call to the
 * function will be at the end of the first @interval.
 *
 * This function is similar to g_timeout_add_full() except that it
 * will try to compensate for delays. For example, if @func takes half
 * the interval time to execute then the function will be called again
 * half the interval time after it finished. In contrast
 * g_timeout_add_full() would not fire until a full interval after the
 * function completes so the delay between calls would be @interval *
 * 1.5. This function does not however try to invoke the function
 * multiple times to catch up missing frames if @func takes more than
 * @interval ms to execute.
 *
 * Return value: the ID (greater than 0) of the event source.
 *
 * Since: 0.8
 */
guint
clutter_frame_source_add_full (gint           priority,
			       guint          interval,
			       GSourceFunc    func,
			       gpointer       data,
			       GDestroyNotify notify)
{
  guint ret;
  GSource *source = g_source_new (&clutter_frame_source_funcs,
				  sizeof (ClutterFrameSource));
  ClutterFrameSource *frame_source = (ClutterFrameSource *) source;

  frame_source->last_time = 0;
  frame_source->frame_time = interval;
  g_get_current_time (&frame_source->start_time);

  if (priority != G_PRIORITY_DEFAULT)
    g_source_set_priority (source, priority);

  g_source_set_callback (source, func, data, notify);

  ret = g_source_attach (source, NULL);

  g_source_unref (source);

  return ret;
}

/**
 * clutter_frame_source_add:
 * @interval: the time between calls to the function, in milliseconds
 * @func: function to call
 * @data: data to pass to the function
 *
 * Simple wrapper around clutter_frame_source_add_full().
 *
 * Return value: the ID (greater than 0) of the event source.
 *
 * Since: 0.8
 */
guint
clutter_frame_source_add (guint          interval,
			  GSourceFunc    func,
			  gpointer       data)
{
  return clutter_frame_source_add_full (G_PRIORITY_DEFAULT,
					interval, func, data, NULL);
}

static guint
clutter_frame_source_get_ticks (ClutterFrameSource *frame_source)
{
  GTimeVal time_now;

  g_source_get_current_time ((GSource *) frame_source, &time_now);
  
  return (time_now.tv_sec - frame_source->start_time.tv_sec) * 1000
         + (time_now.tv_usec - frame_source->start_time.tv_usec) / 1000;
}

static gboolean
clutter_frame_source_prepare (GSource *source, gint *timeout)
{
  ClutterFrameSource *frame_source = (ClutterFrameSource *) source;

  guint now = clutter_frame_source_get_ticks (frame_source);

  /* If time has gone backwards or the time since the last frame is
     greater than the two frames worth then reset the time and do a
     frame now */
  if (frame_source->last_time > now ||
      (now - frame_source->last_time) > frame_source->frame_time * 2)
    {
      frame_source->last_time = now - frame_source->frame_time;
      if (timeout)
	*timeout = 0;
      return TRUE;
    }
  else if (now - frame_source->last_time >= frame_source->frame_time)
    {
      if (timeout)
	*timeout = 0;
      return TRUE;
    }
  else
    {
      if (timeout)
	*timeout = frame_source->frame_time + frame_source->last_time - now;
      return FALSE;
    }
}

static gboolean
clutter_frame_source_check (GSource *source)
{
  return clutter_frame_source_prepare (source, NULL);
}

static gboolean
clutter_frame_source_dispatch (GSource     *source,
			       GSourceFunc callback,
			       gpointer    user_data)
{
  ClutterFrameSource *frame_source = (ClutterFrameSource *) source;

  if ((* callback) (user_data))
    {
      frame_source->last_time += frame_source->frame_time;
      return TRUE;
    }
  else
    return FALSE;
}
