/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2006, 2007, 2008  OpenedHand Ltd
 * Copyright (C) 2009, 2010  Intel Corp.
 *               2011 Giovanni Campagna <scampa.giovanni@gmail.com>
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
 *
 *
 * Authored by:
 *      Matthew Allum <mallum@openedhand.com>
 *      Emmanuele Bassi <ebassi@linux.intel.com>
 */

#include "config.h"

#include "clutter-gdk.h"
#include "clutter-backend-gdk.h"
#include "clutter-device-manager-gdk.h"

#include "clutter-actor-private.h"
#include "clutter-backend-private.h"
#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-event-private.h"
#include "clutter-main.h"
#include "clutter-paint-volume-private.h"
#include "clutter-stage-private.h"

#include <string.h>

#include <glib.h>

/**
 * clutter_gdk_handle_event:
 * @event: a #GdkEvent
 *
 * This function processes a single GDK event; it can be used to hook
 * into external event processing
 *
 * Return value: #GdkFilterReturn. %GDK_FILTER_REMOVE indicates that
 *  Clutter has internally handled the event and the caller should do
 *  no further processing. %GDK_FILTER_CONTINUE indicates that Clutter
 *  is either not interested in the event, or has used the event to
 *  update internal state without taking any exclusive action.
 *  %GDK_FILTER_TRANSLATE will not occur.
 *
 */
GdkFilterReturn
clutter_gdk_handle_event (GdkEvent *gdk_event)
{
  ClutterDeviceManager *device_manager;
  ClutterBackendGdk *backend_gdk;
  ClutterBackend *backend;
  ClutterStage *stage = NULL;
  ClutterEvent *event = NULL;
  gint spin = 0;
  GdkFilterReturn result = GDK_FILTER_CONTINUE;
  ClutterInputDevice *device, *source_device;
  GdkDevice *gdk_device;

  backend = clutter_get_default_backend ();
  if (!CLUTTER_IS_BACKEND_GDK (backend))
    return GDK_FILTER_CONTINUE;

  if (gdk_event->any.window == NULL)
    return GDK_FILTER_CONTINUE;

  device_manager = clutter_device_manager_get_default ();
  if (G_UNLIKELY (device_manager == NULL))
    return GDK_FILTER_CONTINUE;

  backend_gdk = CLUTTER_BACKEND_GDK (backend);
  stage = clutter_gdk_get_stage_from_window (gdk_event->any.window);

  gdk_device = gdk_event_get_device (gdk_event);
  if (gdk_device != NULL)
    device = _clutter_device_manager_gdk_lookup_device (device_manager,
                                                        gdk_device);
  else
    device = NULL;

  gdk_device = gdk_event_get_source_device (gdk_event);
  if (gdk_device != NULL)
    source_device = _clutter_device_manager_gdk_lookup_device (device_manager,
                                                               gdk_device);
  else
    source_device = NULL;

  if (stage == NULL)
    return GDK_FILTER_CONTINUE;

  _clutter_threads_acquire_lock ();

  switch (gdk_event->type)
    {
    case GDK_DELETE:
      event = clutter_event_new (CLUTTER_DELETE);
      break;

    case GDK_DESTROY:
      event = clutter_event_new (CLUTTER_DESTROY_NOTIFY);
      break;

    case GDK_EXPOSE:
      {
        ClutterPaintVolume clip;
        ClutterVertex origin;

        CLUTTER_NOTE (EVENT, "Expose for stage '%s' [%p] { %d, %d - %d x %d }",
                      _clutter_actor_get_debug_name (CLUTTER_ACTOR (stage)),
                      stage,
                      gdk_event->expose.area.x,
                      gdk_event->expose.area.y,
                      gdk_event->expose.area.width,
                      gdk_event->expose.area.height);

        origin.x = gdk_event->expose.area.x;
        origin.y = gdk_event->expose.area.y;
        origin.z = 0;

        _clutter_paint_volume_init_static (&clip, CLUTTER_ACTOR (stage));

        clutter_paint_volume_set_origin (&clip, &origin);
        clutter_paint_volume_set_width (&clip, gdk_event->expose.area.width);
        clutter_paint_volume_set_height (&clip, gdk_event->expose.area.height);

        _clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (stage), 0, &clip);

        clutter_paint_volume_free (&clip);
      }
      break;

    case GDK_DAMAGE:
      /* This is handled by cogl */
      break;

    case GDK_MOTION_NOTIFY:
      event = clutter_event_new (CLUTTER_MOTION);
      event->motion.time = gdk_event->motion.time;
      event->motion.x = gdk_event->motion.x;
      event->motion.y = gdk_event->motion.y;
      event->motion.axes = NULL;
      /* It's all X in the end, right? */
      event->motion.modifier_state = gdk_event->motion.state;
      clutter_event_set_device (event, device);
      clutter_event_set_source_device (event, source_device);
      CLUTTER_NOTE (EVENT, "Motion notifiy [%.2f, %.2f]",
                    event->motion.x,
                    event->motion.y);
      break;

    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      event = clutter_event_new (gdk_event->type == GDK_BUTTON_PRESS ?
                                 CLUTTER_BUTTON_PRESS :
                                 CLUTTER_BUTTON_RELEASE);
      event->button.time = gdk_event->button.time;
      event->button.x = gdk_event->button.x;
      event->button.y = gdk_event->button.y;
      event->button.axes = NULL;
      event->button.modifier_state = gdk_event->button.state;
      event->button.button = gdk_event->button.button;
      event->button.click_count = 1;
      clutter_event_set_device (event, device);
      clutter_event_set_source_device (event, source_device);
      CLUTTER_NOTE (EVENT, "Button %d %s [%.2f, %.2f]",
                    event->button.button,
                    event->type == CLUTTER_BUTTON_PRESS ? "press" : "release",
                    event->button.x,
                    event->button.y);
      break;

    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
      /* these are handled by clutter-main.c updating click_count */
      break;

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      event = clutter_event_new (gdk_event->type == GDK_KEY_PRESS ?
                                 CLUTTER_KEY_PRESS :
                                 CLUTTER_KEY_RELEASE);
      event->key.time = gdk_event->key.time;
      event->key.modifier_state = gdk_event->key.state;
      event->key.keyval = gdk_event->key.keyval;
      event->key.hardware_keycode = gdk_event->key.hardware_keycode;
      event->key.unicode_value = g_utf8_get_char (gdk_event->key.string);
      clutter_event_set_device (event, device);
      clutter_event_set_source_device (event, source_device);
      CLUTTER_NOTE (EVENT, "Key %d %s",
                    event->key.keyval,
                    event->type == CLUTTER_KEY_PRESS ? "press" : "release");
      break;

    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      event = clutter_event_new (gdk_event->type == GDK_ENTER_NOTIFY ?
                                 CLUTTER_ENTER :
                                 CLUTTER_LEAVE);
      event->crossing.source = CLUTTER_ACTOR (stage);
      event->crossing.time = gdk_event_get_time (gdk_event);
      event->crossing.x = gdk_event->crossing.x;
      event->crossing.y = gdk_event->crossing.y;

      /* XXX: no better fallback here? */
      clutter_event_set_device (event, device);
      clutter_event_set_source_device (event, source_device);
      if (gdk_event->type == GDK_ENTER_NOTIFY)
        _clutter_input_device_set_stage (clutter_event_get_device (event), stage);
      else
        _clutter_input_device_set_stage (clutter_event_get_device (event), NULL);
      CLUTTER_NOTE (EVENT, "Crossing %s [%.2f, %.2f]",
                    event->type == CLUTTER_ENTER ? "enter" : "leave",
                    event->crossing.x,
                    event->crossing.y);
      break;

    case GDK_FOCUS_CHANGE:
      if (gdk_event->focus_change.in)
        _clutter_stage_update_state (stage, 0, CLUTTER_STAGE_STATE_ACTIVATED);
      else
        _clutter_stage_update_state (stage, CLUTTER_STAGE_STATE_ACTIVATED, 0);
      break;

    case GDK_CONFIGURE:
      {
        gfloat w, h;

        clutter_actor_get_size (CLUTTER_ACTOR (stage), &w, &h);

        if (w != gdk_event->configure.width ||
            h != gdk_event->configure.height)
          {
            clutter_actor_set_size (CLUTTER_ACTOR (stage),
                                    gdk_event->configure.width,
                                    gdk_event->configure.height);
          }
      }
      break;

    case GDK_SCROLL:
      event = clutter_event_new (CLUTTER_SCROLL);
      event->scroll.time = gdk_event->scroll.time;
      event->scroll.x = gdk_event->scroll.x;
      event->scroll.y = gdk_event->scroll.y;
      event->scroll.modifier_state = gdk_event->scroll.state;
      event->scroll.axes = NULL;
      /* XXX: must keep ClutterScrollDirection compatible with GdkScrollDirection */
      event->scroll.direction = (ClutterScrollDirection) gdk_event->scroll.direction;
      clutter_event_set_device (event, device);
      clutter_event_set_source_device (event, source_device);
      clutter_event_set_scroll_delta (event,
                                      gdk_event->scroll.delta_x,
                                      gdk_event->scroll.delta_y);
      break;

    case GDK_WINDOW_STATE:
      if (gdk_event->window_state.changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
        {
          gboolean is_fullscreen;

          is_fullscreen = (gdk_event->window_state.new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
          if (is_fullscreen)
            _clutter_stage_update_state (stage, 0, CLUTTER_STAGE_STATE_FULLSCREEN);
          else
            _clutter_stage_update_state (stage, CLUTTER_STAGE_STATE_FULLSCREEN, 0);
        }
      break;

    case GDK_SETTING:
      _clutter_backend_gdk_update_setting (backend_gdk, gdk_event->setting.name);
      break;

    default:
      break;
    }

  if (event != NULL)
    {
      event->any.stage = stage;

      if (gdk_event->any.send_event)
	event->any.flags = CLUTTER_EVENT_FLAG_SYNTHETIC;

      _clutter_event_push (event, FALSE);

      spin = 1;

      CLUTTER_NOTE (EVENT, "Translated one event from Gdk");

      /* handle also synthetic enter/leave events */
      if (event->type == CLUTTER_MOTION)
	spin += 2;

      while (spin > 0 && (event = clutter_event_get ()))
	{
	  /* forward the event into clutter for emission etc. */
          _clutter_stage_queue_event (event->any.stage, event, FALSE);
	  --spin;
	}

      result = GDK_FILTER_REMOVE;
    }

  _clutter_threads_release_lock ();

  return result;
}
