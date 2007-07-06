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

#include "config.h"

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

typedef struct _ClutterEventSource      ClutterEventSource;

struct _ClutterEventSource
{
  GSource source;

  ClutterBackend *backend;
  GPollFD event_poll_fd;
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

void
_clutter_events_init (ClutterBackend *backend)
{
  GSource *source;
  ClutterEventSource *event_source;
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);
#if 0
  int connection_number;

  connection_number = ConnectionNumber (backend_egl->xdpy);
  CLUTTER_NOTE (EVENT, "Connection number: %d", connection_number);

  source = backend_egl->event_source = clutter_event_source_new (backend);
  event_source = (ClutterEventSource *) source;
  g_source_set_priority (source, CLUTTER_PRIORITY_EVENTS);

  event_source->event_poll_fd.fd = connection_number;
  event_source->event_poll_fd.events = G_IO_IN;

  event_sources = g_list_prepend (event_sources, event_source);

  g_source_add_poll (source, &event_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);
#endif
}

void
_clutter_events_uninit (ClutterBackend *backend)
{
  ClutterBackendEGL *backend_egl = CLUTTER_BACKEND_EGL (backend);

  if (backend_egl->event_source)
    {
#if 0
      CLUTTER_NOTE (EVENT, "Destroying the event source");

      event_sources = g_list_remove (event_sources,
                                     backend_egl->event_source);

      g_source_destroy (backend_egl->event_source);
      g_source_unref (backend_egl->event_source);
#endif
      backend_egl->event_source = NULL;
    }
}

