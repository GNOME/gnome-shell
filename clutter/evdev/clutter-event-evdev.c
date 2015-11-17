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

#include "config.h"

#include "clutter/clutter-device-manager-private.h"
#include "clutter/clutter-event-private.h"
#include "clutter-input-device-evdev.h"
#include "clutter-evdev.h"

typedef struct _ClutterEventEvdev ClutterEventEvdev;

struct _ClutterEventEvdev
{
  guint32 evcode;
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
