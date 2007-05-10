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

#include "clutter-stage-sdl.h"
#include "clutter-backend-sdl.h"
#include "clutter-sdl.h"

#include "../clutter-backend.h"
#include "../clutter-event.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-main.h"

#include <string.h>
#include <glib.h>

typedef struct _ClutterEventSource ClutterEventSource;

struct _ClutterEventSource
{
  GSource source;

  ClutterBackend *backend;
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
  ClutterBackendSDL *backend_sdl = CLUTTER_BACKEND_SDL (backend);
  
  source = backend_sdl->event_source = clutter_event_source_new (backend);
  event_source = (ClutterEventSource *) source;
  g_source_set_priority (source, CLUTTER_PRIORITY_EVENTS);

  event_sources = g_list_prepend (event_sources, event_source);

  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);
}

void
_clutter_events_uninit (ClutterBackend *backend)
{
  ClutterBackendSDL *backend_sdl = CLUTTER_BACKEND_SDL (backend);

  if (backend_sdl->event_source)
    {
      CLUTTER_NOTE (EVENT, "Destroying the event source");

      event_sources = g_list_remove (event_sources,
                                     backend_sdl->event_source);

      g_source_destroy (backend_sdl->event_source);
      g_source_unref (backend_sdl->event_source);
      backend_sdl->event_source = NULL;
    }
}

static gboolean
clutter_event_prepare (GSource *source,
                       gint    *timeout)
{
  return FALSE;
}

static gboolean
clutter_event_check (GSource *source)
{
  SDL_Event           events;

  /* Pump SDL */
  SDL_PumpEvents();

  return SDL_PeepEvents(&events, 1, SDL_PEEKEVENT, SDL_ALLEVENTS);
}

static gboolean
clutter_event_dispatch (GSource     *source,
                        GSourceFunc  callback,
                        gpointer     user_data)
{
  SDL_Event       sdl_event;
  ClutterEvent   *event = NULL;

  while (SDL_PollEvent(&sdl_event))
    {
      /* FIXME: essentially translate events and push them onto the queue
       *        below will then pop them out via _clutter_events_queue.
       */ 
      if (sdl_event.type == SDL_QUIT)
	{
	  SDL_Quit();
	  exit(0);
	}

    }

  return TRUE;

  event = clutter_event_get ();

  if (event)
    {
      clutter_do_event(event);
      clutter_event_free (event);
    }

  return TRUE;
}
