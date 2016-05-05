/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2015 Red Hat
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
 *
 * Authored by:
 *      Carlos Garnacho <carlosg@gnome.org>
 */

#include "clutter-build-config.h"

#include "clutter/clutter-device-manager-private.h"
#include "clutter/clutter-event-private.h"
#include "clutter-input-device-evdev.h"
#include "clutter-evdev.h"

typedef struct _ClutterEventEvdev ClutterEventEvdev;

struct _ClutterEventEvdev
{
  guint32 evcode;

  guint64 time_usec;

  gboolean has_relative_motion;
  double dx;
  double dy;
  double dx_unaccel;
  double dy_unaccel;
};

static ClutterEventEvdev *
_clutter_event_evdev_new (void)
{
  return g_slice_new0 (ClutterEventEvdev);
}

ClutterEventEvdev *
_clutter_event_evdev_copy (ClutterEventEvdev *event_evdev)
{
  if (event_evdev != NULL)
    return g_slice_dup (ClutterEventEvdev, event_evdev);

  return NULL;
}

void
_clutter_event_evdev_free (ClutterEventEvdev *event_evdev)
{
  if (event_evdev != NULL)
    g_slice_free (ClutterEventEvdev, event_evdev);
}

static ClutterEventEvdev *
clutter_evdev_event_ensure_platform_data (ClutterEvent *event)
{
  ClutterEventEvdev *event_evdev = _clutter_event_get_platform_data (event);

  if (!event_evdev)
    {
      event_evdev = _clutter_event_evdev_new ();
      _clutter_event_set_platform_data (event, event_evdev);
    }

  return event_evdev;
}

void
_clutter_evdev_event_set_event_code (ClutterEvent *event,
                                     guint32       evcode)
{
  ClutterEventEvdev *event_evdev;

  event_evdev = clutter_evdev_event_ensure_platform_data (event);
  event_evdev->evcode = evcode;
}

void
_clutter_evdev_event_set_time_usec       (ClutterEvent *event,
                                          guint64       time_usec)
{
  ClutterEventEvdev *event_evdev;

  event_evdev = clutter_evdev_event_ensure_platform_data (event);
  event_evdev->time_usec = time_usec;
}

void
_clutter_evdev_event_set_relative_motion (ClutterEvent *event,
                                          double        dx,
                                          double        dy,
                                          double        dx_unaccel,
                                          double        dy_unaccel)
{
  ClutterEventEvdev *event_evdev;

  event_evdev = clutter_evdev_event_ensure_platform_data (event);
  event_evdev->dx = dx;
  event_evdev->dy = dy;
  event_evdev->dx_unaccel = dx_unaccel;
  event_evdev->dy_unaccel = dy_unaccel;
  event_evdev->has_relative_motion = TRUE;
}

/**
 * clutter_evdev_event_get_event_code:
 * @event: a #ClutterEvent
 *
 * Returns the event code of the original event. See linux/input.h for more
 * information.
 *
 * Returns: The event code.
 **/
guint32
clutter_evdev_event_get_event_code (const ClutterEvent *event)
{
  ClutterEventEvdev *event_evdev = _clutter_event_get_platform_data (event);

  if (event_evdev)
    return event_evdev->evcode;

  return 0;
}

/**
 * clutter_evdev_event_get_time_usec:
 * @event: a #ClutterEvent
 *
 * Returns the time in microsecond granularity, or 0 if unavailable.
 *
 * Returns: The time in microsecond granularity, or 0 if unavailable.
 */
guint64
clutter_evdev_event_get_time_usec (const ClutterEvent *event)
{
  ClutterEventEvdev *event_evdev = _clutter_event_get_platform_data (event);

  if (event_evdev)
    return event_evdev->time_usec;

  return 0;
}

/**
 * clutter_evdev_event_get_pointer_motion
 * @event: a #ClutterEvent
 *
 * If available, the normal and unaccelerated motion deltas are written
 * to the dx, dy, dx_unaccel and dy_unaccel and TRUE is returned.
 *
 * If unavailable, FALSE is returned.
 *
 * Returns: TRUE on success, otherwise FALSE.
 **/
gboolean
clutter_evdev_event_get_relative_motion (const ClutterEvent *event,
                                         double             *dx,
                                         double             *dy,
                                         double             *dx_unaccel,
                                         double             *dy_unaccel)
{
  ClutterEventEvdev *event_evdev = _clutter_event_get_platform_data (event);

  if (event_evdev && event_evdev->has_relative_motion)
    {
      if (dx)
        *dx = event_evdev->dx;
      if (dy)
        *dy = event_evdev->dy;
      if (dx_unaccel)
        *dx_unaccel = event_evdev->dx_unaccel;
      if (dy_unaccel)
        *dy_unaccel = event_evdev->dy_unaccel;
      return TRUE;
    }
  else
    return FALSE;
}
