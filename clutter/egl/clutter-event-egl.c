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

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#include <X11/Xatom.h>

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

static gboolean
clutter_check_xpending (ClutterBackend *backend)
{
  return XPending (CLUTTER_BACKEND_EGL (backend)->xdpy);
}


void
_clutter_events_init (ClutterBackend *backend)
{
  GSource *source;
  ClutterEventSource *event_source;
  ClutterBackendEgl *backend_egl = CLUTTER_BACKEND_EGL (backend);
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
}

void
_clutter_events_uninit (ClutterBackend *backend)
{
  ClutterBackendEgl *backend_egl = CLUTTER_BACKEND_EGL (backend);

  if (backend_egl->event_source)
    {
      CLUTTER_NOTE (EVENT, "Destroying the event source");

      event_sources = g_list_remove (event_sources,
                                     backend_egl->event_source);

      g_source_destroy (backend_egl->event_source);
      g_source_unref (backend_egl->event_source);
      backend_egl->event_source = NULL;
    }
}

static void
set_user_time (Display      *display,
               Window       *xwindow,
               ClutterEvent *event)
{
  if (clutter_event_get_time (event) != CLUTTER_CURRENT_TIME)
    {
      Atom atom_WM_USER_TIME;
      long timestamp = clutter_event_get_time (event);

      atom_WM_USER_TIME = XInternAtom (display, "_NET_WM_USER_TIME", False);

      XChangeProperty (display, *xwindow,
                       atom_WM_USER_TIME,
                       XA_CARDINAL, 32, PropModeReplace,
                       (unsigned char *) &timestamp, 1);
    }
}


static void
translate_key_event (ClutterBackend *backend,
                     ClutterEvent   *event,
                     XEvent         *xevent)
{
  CLUTTER_NOTE (EVENT, "Translating key %s event",
                xevent->xany.type == KeyPress ? "press" : "release");

  event->key.type = (xevent->xany.type == KeyPress) ? CLUTTER_KEY_PRESS
                                                    : CLUTTER_KEY_RELEASE;
  event->key.time = xevent->xkey.time;
  event->key.modifier_state = xevent->xkey.state; /* FIXME: handle modifiers */
  event->key.hardware_keycode = xevent->xkey.keycode;
  event->key.keyval = XKeycodeToKeysym (xevent->xkey.display, 
                                        xevent->xkey.keycode,
                                        0); /* FIXME: index with modifiers */
}

static gboolean
clutter_event_translate (ClutterBackend *backend,
                         ClutterEvent   *event,
                         XEvent         *xevent)
{
  ClutterBackendEgl *backend_egl;
  ClutterStage *stage;
  gboolean res;
  Window xwindow, stage_xwindow;

  backend_egl = CLUTTER_BACKEND_EGL (backend);
  stage = CLUTTER_STAGE (_clutter_backend_get_stage (backend));
  stage_xwindow = clutter_egl_get_stage_window (stage);

  xwindow = xevent->xany.window;
  if (xwindow == None)
    xwindow = stage_xwindow;

  res = TRUE;

  switch (xevent->type)
    {
    case Expose:
      {
        XEvent foo_xev;

        /* Cheap compress */
        while (XCheckTypedWindowEvent (backend_egl->xdpy, 
                                       xevent->xexpose.window,
                                       Expose, 
                                       &foo_xev));

        /* FIXME: need to make stage an 'actor' so can que
         * a paint direct from there rather than hack here...
         */
        clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
        res = FALSE;
      }
      break;
    case KeyPress:
      event->type = CLUTTER_KEY_PRESS;
      translate_key_event (backend, event, xevent);
      set_user_time (backend_egl->xdpy, &xwindow, event);
      break;
    case KeyRelease:
      event->type = CLUTTER_KEY_RELEASE;
      translate_key_event (backend, event, xevent);
      break;
    case ButtonPress:
      switch (xevent->xbutton.button)
        {
        case 4: /* up */
        case 5: /* down */
        case 6: /* left */
        case 7: /* right */
          event->scroll.type = event->type = CLUTTER_SCROLL;

          if (xevent->xbutton.button == 4)
            event->scroll.direction = CLUTTER_SCROLL_UP;
          else if (xevent->xbutton.button == 5)
            event->scroll.direction = CLUTTER_SCROLL_DOWN;
          else if (xevent->xbutton.button == 6)
            event->scroll.direction = CLUTTER_SCROLL_LEFT;
          else
            event->scroll.direction = CLUTTER_SCROLL_RIGHT;
          
          event->scroll.time = xevent->xbutton.time;
          event->scroll.x = xevent->xbutton.x;
          event->scroll.y = xevent->xbutton.y;
          event->scroll.modifier_state = xevent->xbutton.state;

          break;
        default:
          event->button.type = event->type = CLUTTER_BUTTON_PRESS;
          event->button.time = xevent->xbutton.time;
          event->button.x = xevent->xbutton.x;
          event->button.y = xevent->xbutton.y;
          event->button.modifier_state = xevent->xbutton.state;
          event->button.button = xevent->xbutton.button;

          _clutter_event_button_generate (backend, event);
          break;
        }

      set_user_time (backend_egl->xdpy, &xwindow, event);
      break;
    case ButtonRelease:
      /* scroll events don't have a corresponding release */
      if (xevent->xbutton.button == 4 ||
          xevent->xbutton.button == 5 ||
          xevent->xbutton.button == 6 ||
          xevent->xbutton.button == 7)
        {
          res = FALSE;
          break;
        }

      event->button.type = event->type = CLUTTER_BUTTON_RELEASE;
      event->button.time = xevent->xbutton.time;
      event->button.x = xevent->xbutton.x;
      event->button.y = xevent->xbutton.y;
      event->button.modifier_state = xevent->xbutton.state;
      event->button.button = xevent->xbutton.button;
      break;
    case MotionNotify:
      event->motion.type = event->type = CLUTTER_MOTION;
      event->motion.time = xevent->xmotion.time;
      event->motion.x = xevent->xmotion.x;
      event->motion.y = xevent->xmotion.y;
      event->motion.modifier_state = xevent->xmotion.state;
      break;
    case DestroyNotify:
      CLUTTER_NOTE (EVENT, "destroy notify:\twindow: %ld",
                    xevent->xdestroywindow.window);
      event->type = event->any.type = CLUTTER_DESTROY_NOTIFY;
      break;
    default:
      /* ignore every other event */
      res = FALSE;
      break;
    }

  return res;
}

static void
events_queue (ClutterBackend *backend)
{
  ClutterBackendEgl   *backend_egl = CLUTTER_BACKEND_EGL (backend);
  ClutterEvent        *event;
  XEvent               xevent;
  ClutterMainContext  *clutter_context;

  clutter_context = clutter_context_get_default ();

  Display *xdisplay = backend_egl->xdpy;

  while (!clutter_events_pending () && XPending (xdisplay))
    {
      XNextEvent (xdisplay, &xevent);

      event = clutter_event_new (CLUTTER_NOTHING);
      if (clutter_event_translate (backend, event, &xevent))
        {
	  g_queue_push_head (clutter_context->events_queue, event);
        }
      else
        {
          clutter_event_free (event);
        }
    }
}

static gboolean
clutter_event_prepare (GSource *source,
                       gint    *timeout)
{
  ClutterBackend *backend = ((ClutterEventSource *) source)->backend;
  gboolean retval;

  *timeout = -1;
  retval = (clutter_events_pending () || clutter_check_xpending (backend));

  return retval;
}

static gboolean
clutter_event_check (GSource *source)
{
  ClutterEventSource *event_source = (ClutterEventSource *) source;
  ClutterBackend *backend = event_source->backend;
  gboolean retval;

  if (event_source->event_poll_fd.revents & G_IO_IN)
    retval = (clutter_events_pending () || clutter_check_xpending (backend));
  else
    retval = FALSE;

  return retval;
}

static gboolean
clutter_event_dispatch (GSource     *source,
                        GSourceFunc  callback,
                        gpointer     user_data)
{
  ClutterBackend *backend = ((ClutterEventSource *) source)->backend;
  ClutterEvent *event;

  events_queue (backend);

  event = clutter_event_get ();

  if (event)
    {
      clutter_do_event (event);
      clutter_event_free (event);
    }

  return TRUE;
}
