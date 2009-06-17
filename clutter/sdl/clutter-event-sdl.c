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

#include "clutter-stage-sdl.h"
#include "clutter-backend-sdl.h"
#include "clutter-sdl.h"

#include "../clutter-backend.h"
#include "../clutter-event.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-main.h"
#include "../clutter-keysyms.h"

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

static guint32
get_backend_time (void)
{
  ClutterBackendSDL *backend_sdl;
  gdouble elapsed;

  backend_sdl = CLUTTER_BACKEND_SDL (clutter_get_default_backend ());

  elapsed = g_timer_elapsed (backend_sdl->timer, NULL);

  return (elapsed * 1000.0);
}

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

  CLUTTER_NOTE (EVENT, "Starting timer");
  g_assert (backend_sdl->timer != NULL);
  g_timer_start (backend_sdl->timer);

  CLUTTER_NOTE (EVENT, "Creating event source");
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
      CLUTTER_NOTE (EVENT, "Stopping the timer");
      g_timer_stop (backend_sdl->timer);

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
  SDL_Event events;
  int       num_events;
  gboolean retval;

  clutter_threads_enter ();

  num_events = SDL_PeepEvents(&events, 1, SDL_PEEKEVENT, SDL_ALLEVENTS);

  if (num_events == 1)
    {
      clutter_threads_leave ();

      *timeout = 0;
      return TRUE;
    }

  if (num_events == -1)
    g_warning("Error polling SDL: %s", SDL_GetError());

  *timeout = 50;

  retval = clutter_events_pending ();

  clutter_threads_leave ();

  return retval;
}

static gboolean
clutter_event_check (GSource *source)
{
  SDL_Event events;
  int       num_events;
  gboolean  retval;

  clutter_threads_enter ();

  /* Pump SDL */
  SDL_PumpEvents();

  num_events = SDL_PeepEvents(&events, 1, SDL_PEEKEVENT, SDL_ALLEVENTS);

  if (num_events == -1)
    g_warning("Error polling SDL: %s", SDL_GetError());

  retval = (num_events == 1 || clutter_events_pending ());

  clutter_threads_leave ();

  return retval;
}

static void
key_event_translate (ClutterEvent   *event,
		     SDL_Event      *sdl_event)
{
  event->key.time = get_backend_time ();

  /* FIXME: This is just a quick hack to make SDL keys roughly work.
   * Fixing it properly is left as a exercise to someone who enjoys
   * battleing the SDL API.
   *
   * We probably need to use sdl_event->key.keysym.unicode to do lookups
   * and I have no idea how to get shifted keysyms. It looks quite easy
   * if you drop into xlib but that then avoids the whole point of using
   * SDL in the first place (More portability than just GLX)
  */

  switch (sdl_event->key.keysym.sym)
    {
    case SDLK_UP:        event->key.keyval = CLUTTER_Up; break;
    case SDLK_DOWN:      event->key.keyval = CLUTTER_Down; break;
    case SDLK_LEFT:      event->key.keyval = CLUTTER_Left; break;
    case SDLK_RIGHT:     event->key.keyval = CLUTTER_Right; break;
    case SDLK_HOME:      event->key.keyval = CLUTTER_Home; break;
    case SDLK_END:       event->key.keyval = CLUTTER_End; break;
    case SDLK_PAGEUP:    event->key.keyval = CLUTTER_Page_Up; break;
    case SDLK_PAGEDOWN:  event->key.keyval = CLUTTER_Page_Down; break;
    case SDLK_BACKSPACE: event->key.keyval = CLUTTER_BackSpace; break;
    case SDLK_DELETE:    event->key.keyval = CLUTTER_Delete; break;
    default:
      event->key.keyval = sdl_event->key.keysym.sym;
  }

  event->key.hardware_keycode = sdl_event->key.keysym.scancode;

  if (sdl_event->key.keysym.mod & KMOD_CTRL)
    event->key.modifier_state
      = event->key.modifier_state & CLUTTER_CONTROL_MASK;

  if (sdl_event->key.keysym.mod & KMOD_SHIFT)
    event->key.modifier_state
      = event->key.modifier_state & CLUTTER_SHIFT_MASK;
}

static gboolean
event_translate (ClutterBackend *backend,
		 ClutterEvent   *event,
		 SDL_Event      *sdl_event)
{
  ClutterBackendSDL *backend_sdl;
  gboolean res;

  backend_sdl = CLUTTER_BACKEND_SDL (clutter_get_default_backend ());

  res = TRUE;

  switch (sdl_event->type)
    {
    case SDL_KEYDOWN:
      event->type = CLUTTER_KEY_PRESS;
      key_event_translate (event, sdl_event);
      break;

    case SDL_KEYUP:
      event->type = CLUTTER_KEY_RELEASE;
      key_event_translate (event, sdl_event);
      break;

    case SDL_MOUSEBUTTONDOWN:
      switch (sdl_event->button.button)
        {
        case 4: /* up */
        case 5: /* down */
        case 6: /* left */
        case 7: /* right */
          event->scroll.type = event->type = CLUTTER_SCROLL;

          if (sdl_event->button.button == 4)
            event->scroll.direction = CLUTTER_SCROLL_UP;
          else if (sdl_event->button.button == 5)
            event->scroll.direction = CLUTTER_SCROLL_DOWN;
          else if (sdl_event->button.button == 6)
            event->scroll.direction = CLUTTER_SCROLL_LEFT;
          else
            event->scroll.direction = CLUTTER_SCROLL_RIGHT;

          event->scroll.time = get_backend_time ();
          event->scroll.x = sdl_event->button.x;
          event->scroll.y = sdl_event->button.y;
          event->scroll.modifier_state = sdl_event->button.state;

          break;

        default:
          event->button.type = event->type = CLUTTER_BUTTON_PRESS;
          event->button.time = get_backend_time (); 
          event->button.x = sdl_event->button.x;
          event->button.y = sdl_event->button.y;
          event->button.modifier_state = sdl_event->button.state;
          event->button.button = sdl_event->button.button;
          break;
        }
      break;

    case SDL_MOUSEBUTTONUP:
      /* scroll events don't have a corresponding release */
      if (sdl_event->button.button == 4 ||
          sdl_event->button.button == 5 ||
          sdl_event->button.button == 6 ||
          sdl_event->button.button == 7)
        {
          res = FALSE;
          break;
        }

      event->button.type = event->type = CLUTTER_BUTTON_RELEASE;
      event->button.time = get_backend_time ();
      event->button.x = sdl_event->button.x;
      event->button.y = sdl_event->button.y;
      event->button.modifier_state = sdl_event->button.state;
      event->button.button = sdl_event->button.button;
      break;

    case SDL_MOUSEMOTION:
      event->motion.type = event->type = CLUTTER_MOTION;
      event->motion.time = get_backend_time ();
      event->motion.x = sdl_event->motion.x;
      event->motion.y = sdl_event->motion.y;
      event->motion.modifier_state = sdl_event->motion.state;
      break;

    default:
      res = FALSE;
      break;
    }

  return res;
}


static gboolean
clutter_event_dispatch (GSource     *source,
                        GSourceFunc  callback,
                        gpointer     user_data)
{
  SDL_Event            sdl_event;
  ClutterEvent        *event = NULL;
  ClutterBackend      *backend = ((ClutterEventSource *) source)->backend;
  ClutterMainContext  *clutter_context;

  clutter_threads_enter ();

  clutter_context = _clutter_context_get_default ();

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
      else
	{
	  event = clutter_event_new (CLUTTER_NOTHING);

          event->any.stage = CLUTTER_STAGE (clutter_stage_get_default ());

	  if (event_translate (backend, event, &sdl_event))
	    {
	      /* push directly here to avoid copy of queue_put */
	      g_queue_push_head (clutter_context->events_queue, event);
	    }
	  else
	    clutter_event_free (event);
	}
    }

  event = clutter_event_get ();

  if (event)
    {
      clutter_do_event(event);
      clutter_event_free (event);
    }

  clutter_threads_leave ();

  return TRUE;
}
