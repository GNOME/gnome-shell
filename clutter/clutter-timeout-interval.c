/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Neil Roberts  <neil@linux.intel.com>
 *
 * Copyright (C) 2009  Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* This file contains the common code to check whether an interval has
   expired used in clutter-frame-source and clutter-timeout-pool. */

#include "clutter-timeout-interval.h"

void
_clutter_timeout_interval_init (ClutterTimeoutInterval *interval,
                                guint                   fps)
{
  g_get_current_time (&interval->start_time);
  interval->fps = fps;
  interval->frame_count = 0;
}

static guint
_clutter_timeout_interval_get_ticks (const GTimeVal         *current_time,
                                     ClutterTimeoutInterval *interval)
{
  return ((current_time->tv_sec - interval->start_time.tv_sec) * 1000
        + (current_time->tv_usec - interval->start_time.tv_usec) / 1000);
}

gboolean
_clutter_timeout_interval_prepare (const GTimeVal         *current_time,
                                   ClutterTimeoutInterval *interval,
                                   gint                   *delay)
{
  guint elapsed_time, new_frame_num;

  elapsed_time = _clutter_timeout_interval_get_ticks (current_time,
                                                      interval);
  new_frame_num = elapsed_time * interval->fps
                / 1000;

  /* If time has gone backwards or the time since the last frame is
     greater than the two frames worth then reset the time and do a
     frame now */
  if (new_frame_num < interval->frame_count ||
      new_frame_num - interval->frame_count > 2)
    {
      /* Get the frame time rounded up to the nearest ms */
      guint frame_time = (1000 + interval->fps - 1) / interval->fps;

      /* Reset the start time */
      interval->start_time = *current_time;

      /* Move the start time as if one whole frame has elapsed */
      g_time_val_add (&interval->start_time, -(gint) frame_time * 1000);

      interval->frame_count = 0;

      if (delay)
	*delay = 0;

      return TRUE;
    }
  else if (new_frame_num > interval->frame_count)
    {
      if (delay)
	*delay = 0;

      return TRUE;
    }
  else
    {
      if (delay)
	*delay = ((interval->frame_count + 1) * 1000 / interval->fps
               - elapsed_time);

      return FALSE;
    }
}

gboolean
_clutter_timeout_interval_dispatch (ClutterTimeoutInterval *interval,
                                    GSourceFunc             callback,
                                    gpointer                user_data)
{
  if ((* callback) (user_data))
    {
      interval->frame_count++;

      return TRUE;
    }

  return FALSE;
}

gint
_clutter_timeout_interval_compare_expiration (const ClutterTimeoutInterval *a,
                                              const ClutterTimeoutInterval *b)
{
  guint a_delay = 1000 / a->fps;
  guint b_delay = 1000 / b->fps;
  glong b_difference;
  gint comparison;

  b_difference = ((a->start_time.tv_sec - b->start_time.tv_sec) * 1000
               + (a->start_time.tv_usec - b->start_time.tv_usec) / 1000);

  comparison = ((gint) ((a->frame_count + 1) * a_delay)
             - (gint) ((b->frame_count + 1) * b_delay + b_difference));

  return (comparison < 0 ? -1
                         : comparison > 0 ? 1
                                          : 0);
}
