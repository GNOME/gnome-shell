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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-stage-egl.h"
#include "clutter-backend-egl.h"
#include "clutter-egl.h"

#include "../clutter-backend.h"
#include "../clutter-event.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-main.h"

#include <string.h>

#include <glib.h>

#ifdef HAVE_TSLIB
#include <tslib.h>
#endif

typedef struct _ClutterEventSource  ClutterEventSource;

struct _ClutterEventSource
{
  GSource source;

  ClutterBackend *backend;
  GPollFD         event_poll_fd;
#ifdef HAVE_TSLIB
  struct tsdev   *ts_device;
#endif
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
clutter_event_source_new (ClutterBackend *backend)
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
_clutter_events_init (ClutterBackend *backend)
{
  ClutterBackendEGL  *backend_egl = CLUTTER_BACKEND_EGL (backend);
  GSource            *source;
  ClutterEventSource *event_source;

  CLUTTER_NOTE (EVENT, "Starting timer");
  g_assert (backend_egl->event_timer != NULL);
  g_timer_start (backend_egl->event_timer);

#ifdef HAVE_TSLIB
  /* FIXME LEAK on error paths */
  source = backend_egl->event_source = clutter_event_source_new (backend);
  event_source = (ClutterEventSource *) source;

  event_source->ts_device = ts_open (g_getenv ("TSLIB_TSDEVICE"), 0);

  if (event_source->ts_device)
    {
      CLUTTER_NOTE (EVENT, "Opened '%s'", g_getenv ("TSLIB_TSDEVICE"));

      if (ts_config (event_source->ts_device))
	{
	  g_warning ("ts_config() failed");
	  ts_close (event_source->ts_device);
	  return;
	}

      g_source_set_priority (source, CLUTTER_PRIORITY_EVENTS);
      event_source->event_poll_fd.fd = ts_fd(event_source->ts_device);
      event_source->event_poll_fd.events = G_IO_IN;

      event_sources = g_list_prepend (event_sources, event_source);

      g_source_add_poll (source, &event_source->event_poll_fd);
      g_source_set_can_recurse (source, TRUE);
      g_source_attach (source, NULL);
    }
  else
    g_warning ("ts_open() failed opening %s'",
	       g_getenv("TSLIB_TSDEVICE") ?
  	         g_getenv("TSLIB_TSDEVICE") : "None, TSLIB_TSDEVICE not set");
#endif
}

void
_clutter_events_uninit (ClutterBackend *backend)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);

  if (backend_egl->event_timer)
    {
      CLUTTER_NOTE (EVENT, "Stopping the timer");
      g_timer_stop (backend_egl->event_timer);
    }

  if (backend_egl->event_source)
    {
      CLUTTER_NOTE (EVENT, "Destroying the event source");

      ClutterEventSource *event_source =
                (ClutterEventSource *) backend_egl->event_source;

#ifdef HAVE_TSLIB
      ts_close (event_source->ts_device);
      event_sources = g_list_remove (event_sources,
                                     backend_egl->event_source);
#endif
      g_source_destroy (backend_egl->event_source);
      g_source_unref (backend_egl->event_source);
      backend_egl->event_source = NULL;
    }
}

static gboolean
clutter_event_prepare (GSource *source,
                       gint    *timeout)
{
  ClutterBackend *backend = ((ClutterEventSource *) source)->backend;
  gboolean retval;

  clutter_threads_enter ();

  *timeout = -1;
  retval = clutter_events_pending ();

  clutter_threads_leave ();

  return retval;
}

static gboolean
clutter_event_check (GSource *source)
{
  ClutterEventSource *event_source = (ClutterEventSource *) source;
  ClutterBackend *backend = event_source->backend;
  gboolean retval;

  clutter_threads_enter ();

  retval = ((event_source->event_poll_fd.revents & G_IO_IN) ||
            clutter_events_pending ());

  clutter_threads_leave ();

  return retval;
}

static gboolean
clutter_event_dispatch (GSource     *source,
                        GSourceFunc  callback,
                        gpointer     user_data)
{
  ClutterBackend     *backend = ((ClutterEventSource *) source)->backend;
  ClutterEventSource *event_source = (ClutterEventSource *) source;
  ClutterEvent       *event;
#ifdef HAVE_TSLIB
  struct ts_sample    tsevent;
#endif
  ClutterMainContext *clutter_context;
  static gint         last_x = 0, last_y = 0;
  static gboolean     clicked = FALSE;

  clutter_threads_enter ();

  clutter_context = _clutter_context_get_default ();

#ifdef HAVE_TSLIB
  /* FIXME while would be better here but need to deal with lockups */
  if ((!clutter_events_pending()) &&
        (ts_read(event_source->ts_device, &tsevent, 1) == 1))
    {
      /* Avoid sending too many events which are just pressure changes.
       * We dont current handle pressure in events (FIXME) and thus
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

      g_queue_push_head (clutter_context->events_queue, event);
    }
#endif

  /* Pop an event off the queue if any */
  event = clutter_event_get ();

  if (event)
    {
      /* forward the event into clutter for emission etc. */
      clutter_do_event (event);
      clutter_event_free (event);
    }

out:
  clutter_threads_leave ();

  return TRUE;
}
