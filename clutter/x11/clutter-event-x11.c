/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2006, 2007, 2008  OpenedHand Ltd
 * Copyright (C) 2009, 2010  Intel Corp.
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

#include "clutter-backend-x11.h"
#include "clutter-x11.h"

#include "clutter-backend-private.h"
#include "clutter-debug.h"
#include "clutter-event-private.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#include <string.h>

#include <glib.h>

#if 0
/* XEMBED protocol support for toolkit embedding */
#define XEMBED_MAPPED                   (1 << 0)
#define MAX_SUPPORTED_XEMBED_VERSION    1

#define XEMBED_EMBEDDED_NOTIFY          0
#define XEMBED_WINDOW_ACTIVATE          1
#define XEMBED_WINDOW_DEACTIVATE        2
#define XEMBED_REQUEST_FOCUS            3
#define XEMBED_FOCUS_IN                 4
#define XEMBED_FOCUS_OUT                5
#define XEMBED_FOCUS_NEXT               6
#define XEMBED_FOCUS_PREV               7
/* 8-9 were used for XEMBED_GRAB_KEY/XEMBED_UNGRAB_KEY */
#define XEMBED_MODALITY_ON              10
#define XEMBED_MODALITY_OFF             11
#define XEMBED_REGISTER_ACCELERATOR     12
#define XEMBED_UNREGISTER_ACCELERATOR   13
#define XEMBED_ACTIVATE_ACCELERATOR     14

static Window ParentEmbedderWin = None;
#endif

typedef struct _ClutterEventSource      ClutterEventSource;

struct _ClutterEventSource
{
  GSource source;

  ClutterBackendX11 *backend;

  GPollFD event_poll_fd;
};

ClutterEventX11 *
_clutter_event_x11_new (void)
{
  return g_slice_new0 (ClutterEventX11);
}

ClutterEventX11 *
_clutter_event_x11_copy (ClutterEventX11 *event_x11)
{
  if (event_x11 != NULL)
    return g_slice_dup (ClutterEventX11, event_x11);

  return NULL;
}

void
_clutter_event_x11_free (ClutterEventX11 *event_x11)
{
  if (event_x11 != NULL)
    g_slice_free (ClutterEventX11, event_x11);
}

static gboolean clutter_event_prepare  (GSource     *source,
                                        gint        *timeout);
static gboolean clutter_event_check    (GSource     *source);
static gboolean clutter_event_dispatch (GSource     *source,
                                        GSourceFunc  callback,
                                        gpointer     user_data);

static GSourceFuncs event_funcs = {
  clutter_event_prepare,
  clutter_event_check,
  clutter_event_dispatch,
  NULL
};

GSource *
_clutter_x11_event_source_new (ClutterBackendX11 *backend_x11)
{
  ClutterEventSource *event_source;
  int connection_number;
  GSource *source;
  gchar *name;

  connection_number = ConnectionNumber (backend_x11->xdpy);
  CLUTTER_NOTE (EVENT, "Connection number: %d", connection_number);

  source = g_source_new (&event_funcs, sizeof (ClutterEventSource));
  event_source = (ClutterEventSource *) source;

  name = g_strdup_printf ("Clutter X11 Event (connection: %d)",
                          connection_number);
  g_source_set_name (source, name);
  g_free (name);

  event_source->backend = backend_x11;
  event_source->event_poll_fd.fd = connection_number;
  event_source->event_poll_fd.events = G_IO_IN;

  g_source_add_poll (source, &event_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);

  return source;
}

/**
 * clutter_x11_handle_event:
 * @xevent: pointer to XEvent structure
 *
 * This function processes a single X event; it can be used to hook
 * into external X11 event processing (for example, a GDK filter
 * function).
 *
 * If clutter_x11_disable_event_retrieval() has been called, you must
 * let this function process events to update Clutter's internal state.
 *
 * Return value: #ClutterX11FilterReturn. %CLUTTER_X11_FILTER_REMOVE
 *  indicates that Clutter has internally handled the event and the
 *  caller should do no further processing. %CLUTTER_X11_FILTER_CONTINUE
 *  indicates that Clutter is either not interested in the event,
 *  or has used the event to update internal state without taking
 *  any exclusive action. %CLUTTER_X11_FILTER_TRANSLATE will not
 *  occur.
 *
 * Since: 0.8
 */
ClutterX11FilterReturn
clutter_x11_handle_event (XEvent *xevent)
{
  ClutterX11FilterReturn result;
  ClutterBackend *backend;
  ClutterEvent *event;
  gint spin = 1;
#ifdef HAVE_XGE
  ClutterBackendX11 *backend_x11;
  Display *xdisplay;
  gboolean allocated_event;
#endif

  /* The return values here are someone approximate; we return
   * CLUTTER_X11_FILTER_REMOVE if a clutter event is
   * generated for the event. This mostly, but not entirely,
   * corresponds to whether other event processing should be
   * excluded. As long as the stage window is not shared with another
   * toolkit it should be safe, and never return
   * %CLUTTER_X11_FILTER_REMOVE when more processing is needed.
   */

  result = CLUTTER_X11_FILTER_CONTINUE;

  _clutter_threads_acquire_lock ();

  backend = clutter_get_default_backend ();

  event = clutter_event_new (CLUTTER_NOTHING);

#ifdef HAVE_XGE
  backend_x11 = CLUTTER_BACKEND_X11 (backend);
  xdisplay = backend_x11->xdpy;

  allocated_event = XGetEventData (xdisplay, &xevent->xcookie);
#endif

  if (_clutter_backend_translate_event (backend, xevent, event))
    {
      _clutter_event_push (event, FALSE);

      result = CLUTTER_X11_FILTER_REMOVE;
    }
  else
    {
      clutter_event_free (event);
      goto out;
    }

  /*
   * Motion events can generate synthetic enter and leave events, so if we
   * are processing a motion event, we need to spin the event loop at least
   * two extra times to pump the enter/leave events through (otherwise they
   * just get pushed down the queue and never processed).
   */
  if (event->type == CLUTTER_MOTION)
    spin += 2;

  while (spin > 0 && (event = clutter_event_get ()))
    {
      /* forward the event into clutter for emission etc. */
      _clutter_stage_queue_event (event->any.stage, event, FALSE);
      --spin;
    }

out:
#ifdef HAVE_XGE
  if (allocated_event)
    XFreeEventData (xdisplay, &xevent->xcookie);
#endif

  _clutter_threads_release_lock ();

  return result;
}

static gboolean
clutter_event_prepare (GSource *source,
                       gint    *timeout)
{
  ClutterBackendX11 *backend = ((ClutterEventSource *) source)->backend;
  gboolean retval;

  _clutter_threads_acquire_lock ();

  *timeout = -1;
  retval = (clutter_events_pending () || XPending (backend->xdpy));

  _clutter_threads_release_lock ();

  return retval;
}

static gboolean
clutter_event_check (GSource *source)
{
  ClutterEventSource *event_source = (ClutterEventSource *) source;
  ClutterBackendX11 *backend = event_source->backend;
  gboolean retval;

  _clutter_threads_acquire_lock ();

  if (event_source->event_poll_fd.revents & G_IO_IN)
    retval = (clutter_events_pending () || XPending (backend->xdpy));
  else
    retval = FALSE;

  _clutter_threads_release_lock ();

  return retval;
}

static void
events_queue (ClutterBackendX11 *backend_x11)
{
  ClutterBackend *backend = CLUTTER_BACKEND (backend_x11);
  Display *xdisplay = backend_x11->xdpy;
  ClutterEvent *event;
  XEvent xevent;

  while (!clutter_events_pending () && XPending (xdisplay))
    {
      XNextEvent (xdisplay, &xevent);

      event = clutter_event_new (CLUTTER_NOTHING);

#ifdef HAVE_XGE
      XGetEventData (xdisplay, &xevent.xcookie);
#endif

      if (_clutter_backend_translate_event (backend, &xevent, event))
        _clutter_event_push (event, FALSE);
      else
        clutter_event_free (event);

#ifdef HAVE_XGE
      XFreeEventData (xdisplay, &xevent.xcookie);
#endif
    }
}

static gboolean
clutter_event_dispatch (GSource     *source,
                        GSourceFunc  callback,
                        gpointer     user_data)
{
  ClutterBackendX11 *backend = ((ClutterEventSource *) source)->backend;
  ClutterEvent *event;

  _clutter_threads_acquire_lock ();

  /*  Grab the event(s), translate and figure out double click.
   *  The push onto queue (stack) if valid.
  */
  events_queue (backend);

  /* Pop an event off the queue if any */
  event = clutter_event_get ();
  if (event != NULL)
    {
      /* forward the event into clutter for emission etc. */
      _clutter_stage_queue_event (event->any.stage, event, FALSE);
    }

  _clutter_threads_release_lock ();

  return TRUE;
}

/**
 * clutter_x11_get_current_event_time: (skip)
 *
 * Retrieves the timestamp of the last X11 event processed by
 * Clutter. This might be different from the timestamp returned
 * by clutter_get_current_event_time(), as Clutter may synthesize
 * or throttle events.
 *
 * Return value: a timestamp, in milliseconds
 *
 * Since: 1.0
 */
Time
clutter_x11_get_current_event_time (void)
{
  ClutterBackend *backend = clutter_get_default_backend ();

  return CLUTTER_BACKEND_X11 (backend)->last_event_time;
}

/**
 * clutter_x11_event_get_key_group:
 * @event: a #ClutterEvent of type %CLUTTER_KEY_PRESS or %CLUTTER_KEY_RELEASE
 *
 * Retrieves the group for the modifiers set in @event
 *
 * Return value: the group id
 *
 * Since: 1.4
 */
gint
clutter_x11_event_get_key_group (const ClutterEvent *event)
{
  ClutterEventX11 *event_x11;

  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (event->type == CLUTTER_KEY_PRESS ||
                        event->type == CLUTTER_KEY_RELEASE, 0);

  event_x11 = _clutter_event_get_platform_data (event);
  if (event_x11 == NULL)
    return 0;

  return event_x11->key_group;
}

/**
 * clutter_x11_event_sequence_get_touch_detail:
 * @sequence: a #ClutterEventSequence
 *
 * Retrieves the touch detail froma #ClutterEventSequence.
 *
 * Return value: the touch detail
 *
 * Since: 1.12
 */
guint
clutter_x11_event_sequence_get_touch_detail (const ClutterEventSequence *sequence)
{
  g_return_val_if_fail (sequence != NULL, 0);

  return GPOINTER_TO_UINT (sequence);
}
