/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 * Authored By Matthew Allum  <mallum@openedhand.com>
 * Copyright (C) 2006-2007 OpenedHand
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
 */

#include "config.h"

#include "clutter-backend-egl.h"
#include "clutter-egl.h"

#include "clutter-backend.h"
#include "clutter-event-private.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-main.h"

#include <string.h>

#include <glib.h>

#include <tslib.h>

typedef struct _ClutterEventSource  ClutterEventSource;

struct _ClutterEventSource
{
  GSource source;

  ClutterBackendEGL *backend;
  GPollFD event_poll_fd;

  struct tsdev *ts_device;
};

static gboolean clutter_event_prepare  (GSource     *source,
                                        gint        *timeout);
static gboolean clutter_event_check    (GSource     *source);
static gboolean clutter_event_dispatch (GSource     *source,
                                        GSourceFunc  callback,
                                        gpointer     user_data);

static GList *event_sources = NULL;

static GSourceFuncs event_funcs = {
  clutter_event_prepare,
  clutter_event_check,
  clutter_event_dispatch,
  NULL
};

static GSource *
clutter_event_source_new (ClutterBackendEGL *backend)
{
  GSource *source = g_source_new (&event_funcs, sizeof (ClutterEventSource));
  ClutterEventSource *event_source = (ClutterEventSource *) source;

  event_source->backend = backend;

  return source;
}

static guint32
get_backend_time (void)
{
  ClutterBackendEGL *backend_egl;

  backend_egl = CLUTTER_BACKEND_EGL (clutter_get_default_backend ());

  return g_timer_elapsed (backend_egl->event_timer, NULL) * 1000;
}

void
_clutter_events_tslib_init (ClutterBackend *backend)
{
  ClutterBackendEglNative *backend_egl;
  ClutterEventSource *event_source;
  const char *device_name;
  GSource *source;

  backend_egl = CLUTTER_BACKEND_EGL (backend);

  CLUTTER_NOTE (EVENT, "Starting timer");
  g_assert (backend_egl->event_timer != NULL);
  g_timer_start (backend_egl->event_timer);

  source = backend_egl->event_source = clutter_event_source_new (backend_egl);
  event_source = (ClutterEventSource *) source;

  device_name = g_getenv ("TSLIB_TSDEVICE");
  if (device_name == NULL || device_name[0] == '\0')
    {
      g_warning ("No device for TSLib has been defined; please set the "
                 "TSLIB_TSDEVICE environment variable to define a touch "
                 "screen device to be used with Clutter.");
      g_source_unref (source);
      return;
    }

  event_source->ts_device = ts_open (device_name, 0);
  if (event_source->ts_device)
    {
      CLUTTER_NOTE (EVENT, "Opened '%s'", device_name);

      if (ts_config (event_source->ts_device))
	{
	  g_warning ("Closing device '%s': ts_config() failed", device_name);
	  ts_close (event_source->ts_device);
          g_source_unref (source);
	  return;
	}

      g_source_set_priority (source, CLUTTER_PRIORITY_EVENTS);
      event_source->event_poll_fd.fd = ts_fd (event_source->ts_device);
      event_source->event_poll_fd.events = G_IO_IN;

      event_sources = g_list_prepend (event_sources, event_source);

      g_source_add_poll (source, &event_source->event_poll_fd);
      g_source_set_can_recurse (source, TRUE);
      g_source_attach (source, NULL);
    }
  else
    {
      g_warning ("Unable to open '%s'", device_name);
      g_source_unref (source);
    }
}

void
_clutter_events_egl_uninit (ClutterBackendEglNative *backend_egl)
{
  if (backend_egl->event_timer != NULL)
    {
      CLUTTER_NOTE (EVENT, "Stopping the timer");
      g_timer_stop (backend_egl->event_timer);
    }

  if (backend_egl->event_source != NULL)
    {
      CLUTTER_NOTE (EVENT, "Destroying the event source");

      ClutterEventSource *event_source =
                (ClutterEventSource *) backend_egl->event_source;

      ts_close (event_source->ts_device);
      event_sources = g_list_remove (event_sources, backend_egl->event_source);

      g_source_destroy (backend_egl->event_source);
      g_source_unref (backend_egl->event_source);
      backend_egl->event_source = NULL;
    }
}

static gboolean
clutter_event_prepare (GSource *source,
                       gint    *timeout)
{
  gboolean retval;

  _clutter_threads_acquire_lock ();

  *timeout = -1;
  retval = clutter_events_pending ();

  _clutter_threads_release_lock ();

  return retval;
}

static gboolean
clutter_event_check (GSource *source)
{
  ClutterEventSource *event_source = (ClutterEventSource *) source;
  gboolean retval;

  _clutter_threads_acquire_lock ();

  retval = ((event_source->event_poll_fd.revents & G_IO_IN) ||
            clutter_events_pending ());

  _clutter_threads_release_lock ();

  return retval;
}

static gboolean
clutter_event_dispatch (GSource     *source,
                        GSourceFunc  callback,
                        gpointer     user_data)
{
  ClutterEventSource *event_source = (ClutterEventSource *) source;
  struct ts_sample tsevent;
  ClutterEvent *event;

  _clutter_threads_acquire_lock ();

  /* FIXME while would be better here but need to deal with lockups */
  if ((!clutter_events_pending()) &&
      (ts_read(event_source->ts_device, &tsevent, 1) == 1))
    {
      static gint last_x = 0, last_y = 0;
      static gboolean clicked = FALSE;

      /* Avoid sending too many events which are just pressure changes.
       *
       * FIXME - We don't current handle pressure in events and thus
       * event_button_generate gets confused generating lots of double
       * and triple clicks.
      */
      if (tsevent.pressure && last_x == tsevent.x && last_y == tsevent.y)
        goto out;

      event = clutter_event_new (CLUTTER_NOTHING);

      event->any.stage = clutter_stage_get_default ();

      last_x = event->button.x = tsevent.x;
      last_y = event->button.y = tsevent.y;

      if (tsevent.pressure && !clicked)
        {
	  event->button.type = event->type = CLUTTER_BUTTON_PRESS;
          event->button.time = get_backend_time ();
          event->button.modifier_state = 0;
          event->button.button = 1;

          clicked = TRUE;
        }
      else if (tsevent.pressure && clicked)
        {
          event->motion.type = event->type = CLUTTER_MOTION;
          event->motion.time = get_backend_time ();
          event->motion.modifier_state = 0;
        }
      else
        {
	  event->button.type = event->type = CLUTTER_BUTTON_RELEASE;
          event->button.time = get_backend_time ();
          event->button.modifier_state = 0;
          event->button.button = 1;

          clicked = FALSE;
        }

      _clutter_event_push (event, FALSE);
    }

  /* Pop an event off the queue if any */
  event = clutter_event_get ();

  if (event)
    {
      /* forward the event into clutter for emission etc. */
      _clutter_stage_queue_event (event->any.stage, event, FALSE);
    }

out:
  _clutter_threads_release_lock ();

  return TRUE;
}
