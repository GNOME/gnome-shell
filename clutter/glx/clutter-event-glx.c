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

#include "clutter-stage-glx.h"
#include "clutter-backend-glx.h"
#include "clutter-glx.h"

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

static Atom Atom_XEMBED       = 0;
static Atom Atom_WM_PROTOCOLS = 0;

static Window ParentEmbedderWin = None;

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
check_xpending (ClutterBackend *backend)
{
  return XPending (CLUTTER_BACKEND_GLX (backend)->xdpy);
}

static gboolean
xembed_send_message (Display *xdisplay,
                     Window   window,
		     long     message,
		     long     detail,
		     long     data1, 
		     long     data2)
{
  XEvent ev;

  memset (&ev, 0, sizeof (ev));

  ev.xclient.type = ClientMessage;
  ev.xclient.window = window;
  ev.xclient.message_type = Atom_XEMBED;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = CurrentTime;
  ev.xclient.data.l[1] = message;
  ev.xclient.data.l[2] = detail;
  ev.xclient.data.l[3] = data1;
  ev.xclient.data.l[4] = data2;

  clutter_glx_trap_x_errors ();

  XSendEvent (xdisplay, window, False, NoEventMask, &ev);
  XSync (xdisplay, False);

  if (clutter_glx_untrap_x_errors ())
    return False;

  return True;
}

static void
xembed_set_info (Display *xdisplay,
                 Window   window,
                 gint     flags)
{
  gint32 list[2];
  Atom atom_XEMBED_INFO;
  
  atom_XEMBED_INFO = XInternAtom (xdisplay, "_XEMBED_INFO", False);

  list[0] = MAX_SUPPORTED_XEMBED_VERSION;
  list[1] = XEMBED_MAPPED;

  clutter_glx_trap_x_errors ();
  XChangeProperty (xdisplay, window,
                   atom_XEMBED_INFO,
                   atom_XEMBED_INFO, 32,
                   PropModeReplace, (unsigned char *) list, 2);
  clutter_glx_untrap_x_errors ();
}

void
_clutter_backend_glx_events_init (ClutterBackend *backend)
{
  GSource *source;
  ClutterEventSource *event_source;
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (backend);
  int connection_number;
  
  connection_number = ConnectionNumber (backend_glx->xdpy);
  CLUTTER_NOTE (EVENT, "Connection number: %d", connection_number);

  Atom_XEMBED = XInternAtom (backend_glx->xdpy, "_XEMBED", False);
  Atom_WM_PROTOCOLS = XInternAtom (backend_glx->xdpy, "WM_PROTOCOLS", False);

  source = backend_glx->event_source = clutter_event_source_new (backend);
  event_source = (ClutterEventSource *) source;
  g_source_set_priority (source, CLUTTER_PRIORITY_EVENTS);

  event_source->event_poll_fd.fd = connection_number;
  event_source->event_poll_fd.events = G_IO_IN;

  event_sources = g_list_prepend (event_sources, event_source);

  g_source_add_poll (source, &event_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  xembed_set_info (backend_glx->xdpy,
                   clutter_glx_get_stage_window (CLUTTER_STAGE (backend_glx->stage)),
                   0);
}

void
_clutter_backend_glx_events_uninit (ClutterBackend *backend)
{
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (backend);

  if (backend_glx->event_source)
    {
      CLUTTER_NOTE (EVENT, "Destroying the event source");

      event_sources = g_list_remove (event_sources,
                                     backend_glx->event_source);

      g_source_destroy (backend_glx->event_source);
      g_source_unref (backend_glx->event_source);
      backend_glx->event_source = NULL;
    }
}


static void
set_user_time (Display *display,
               Window  *xwindow,
               long     timestamp)
{
  if (timestamp != CLUTTER_CURRENT_TIME)
    {
      Atom atom_WM_USER_TIME;

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
  event->key.modifier_state = (ClutterModifierType) xevent->xkey.state;
  event->key.hardware_keycode = xevent->xkey.keycode;

  /* FIXME: We need to handle other modifiers rather than just shift */
  event->key.keyval 
    = XKeycodeToKeysym (xevent->xkey.display, 
			xevent->xkey.keycode,
			(event->key.modifier_state & CLUTTER_SHIFT_MASK) 
			   ? 1 : 0);
}

static gboolean
handle_wm_protocols_event (ClutterBackendGLX *backend_glx,
                           XEvent            *xevent)
{
  Atom atom = (Atom) xevent->xclient.data.l[0];
  Atom Atom_WM_DELETE_WINDOW;
  Atom Atom_NEW_WM_PING;

  ClutterStage *stage = CLUTTER_STAGE (backend_glx->stage);
  Window stage_xwindow = clutter_glx_get_stage_window (stage);

  Atom_WM_DELETE_WINDOW = XInternAtom (backend_glx->xdpy,
                                       "WM_DELETE_WINDOW",
                                        False);
  Atom_NEW_WM_PING = XInternAtom (backend_glx->xdpy, "_NET_WM_PING", False);

  if (atom == Atom_WM_DELETE_WINDOW &&
      xevent->xany.window == stage_xwindow)
    {
      /* the WM_DELETE_WINDOW is a request: we do not destroy
       * the window right away, as it might contain vital data;
       * we relay the event to the application and we let it
       * handle the request
       */
      CLUTTER_NOTE (EVENT, "delete window:\twindow: %ld",
                    xevent->xclient.window);

      set_user_time (backend_glx->xdpy,
                     &stage_xwindow,
                     xevent->xclient.data.l[1]);

      return TRUE;
    }
  else if (atom == Atom_NEW_WM_PING &&
           xevent->xany.window == stage_xwindow)
    {
      XClientMessageEvent xclient = xevent->xclient;

      xclient.window = backend_glx->xwin_root;
      XSendEvent (backend_glx->xdpy, xclient.window,
                  False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  (XEvent *) &xclient);

      return FALSE;
    }

  /* do not send any of the WM_PROTOCOLS events to the queue */
  return FALSE;
}

static gboolean
handle_xembed_event (ClutterBackendGLX *backend_glx,
                     XEvent            *xevent)
{
  ClutterActor *stage;

  stage = _clutter_backend_get_stage (CLUTTER_BACKEND (backend_glx));

  switch (xevent->xclient.data.l[1])
    {
    case XEMBED_EMBEDDED_NOTIFY:
      CLUTTER_NOTE (EVENT, "got XEMBED_EMBEDDED_NOTIFY from %lx",
                    xevent->xclient.data.l[3]);

      ParentEmbedderWin = xevent->xclient.data.l[3];

      clutter_actor_realize (stage);
      clutter_actor_show (stage);

      xembed_set_info (backend_glx->xdpy,
                       clutter_glx_get_stage_window (CLUTTER_STAGE (stage)),
                       XEMBED_MAPPED);
      break;
    case XEMBED_WINDOW_ACTIVATE:
      CLUTTER_NOTE (EVENT, "got XEMBED_WINDOW_ACTIVATE");
      break;
    case XEMBED_WINDOW_DEACTIVATE:
      CLUTTER_NOTE (EVENT, "got XEMBED_WINDOW_DEACTIVATE");
      break;
    case XEMBED_FOCUS_IN:
      CLUTTER_NOTE (EVENT, "got XEMBED_FOCUS_IN");
      if (ParentEmbedderWin)
        xembed_send_message (backend_glx->xdpy, ParentEmbedderWin,
                             XEMBED_FOCUS_NEXT,
                             0, 0, 0);
      break;
    default:
      CLUTTER_NOTE (EVENT, "got unknown XEMBED message");
      break;
    }

  /* do not propagate the XEMBED events to the stage */
  return FALSE;
}

static gboolean
event_translate (ClutterBackend *backend,
		 ClutterEvent   *event,
		 XEvent         *xevent)
{
  ClutterBackendGLX *backend_glx;
  ClutterStageGLX   *stage_glx;
  ClutterStage      *stage;
  gboolean           res;
  Window             xwindow, stage_xwindow;

  backend_glx    = CLUTTER_BACKEND_GLX (backend);
  stage          = CLUTTER_STAGE (_clutter_backend_get_stage (backend));
  stage_glx      = CLUTTER_STAGE_GLX (stage);
  stage_xwindow  = clutter_glx_get_stage_window (stage);

  xwindow = xevent->xany.window;
  if (xwindow == None)
    xwindow = stage_xwindow;

  if (backend_glx->event_filters)
    {
      GSList                *node;
      ClutterGLXEventFilter *filter;

      node = backend_glx->event_filters;

      while (node)
	{
	  filter = (ClutterGLXEventFilter *)node->data;

	  switch (filter->func(xevent, event, filter->data))
	    {
	    case CLUTTER_GLX_FILTER_CONTINUE:
	      break;
	    case CLUTTER_GLX_FILTER_TRANSLATE:
	      return TRUE;
	    case CLUTTER_GLX_FILTER_REMOVE:
	      return FALSE;
	    default:
	      break;
	    }
	  node = node->next;
	}
    }
  
  res = TRUE;

  switch (xevent->type)
    {
    case ConfigureNotify:
      if (xevent->xconfigure.width 
	  != clutter_actor_get_width (CLUTTER_ACTOR (stage))
	  ||
	  xevent->xconfigure.height 
	  != clutter_actor_get_height (CLUTTER_ACTOR (stage)))
	clutter_actor_set_size (CLUTTER_ACTOR (stage),
				xevent->xconfigure.width,
				xevent->xconfigure.height);
      res = FALSE;
      break;
    case PropertyNotify:
      {
	if (xevent->xproperty.atom == backend_glx->atom_WM_STATE)
	  {
	    Atom     type;
	    gint     format;
	    gulong   nitems, bytes_after;
	    guchar  *data = NULL;
	    Atom    *atoms = NULL;
	    gulong   i;
	    gboolean fullscreen_set = FALSE;

	    clutter_glx_trap_x_errors ();
	    XGetWindowProperty (backend_glx->xdpy, 
				stage_xwindow,
				backend_glx->atom_WM_STATE,
				0, G_MAXLONG, 
				False, XA_ATOM, 
				&type, &format, &nitems,
				&bytes_after, &data);
	    clutter_glx_untrap_x_errors ();

	    if (type != None && data != NULL)
	      {
		atoms = (Atom *)data;

		i = 0;
		while (i < nitems)
		  {
		    if (atoms[i] == backend_glx->atom_WM_STATE_FULLSCREEN)
		      fullscreen_set = TRUE;
		    i++;
		  }

		if (fullscreen_set 
		      != !!(stage_glx->state & CLUTTER_STAGE_STATE_FULLSCREEN))
		  {
		    if (fullscreen_set)
		      stage_glx->state |= CLUTTER_STAGE_STATE_FULLSCREEN;
		    else
		      stage_glx->state &= ~CLUTTER_STAGE_STATE_FULLSCREEN;

		    event->type = CLUTTER_STAGE_STATE;
		    event->stage_state.changed_mask 
		                = CLUTTER_STAGE_STATE_FULLSCREEN;
		    event->stage_state.new_state = stage_glx->state;
		  }
		else
		  res = FALSE;

		XFree (data);
	      }
	  }
	else
	  res = FALSE;
      }
      break;
    case FocusIn:
      if (!(stage_glx->state & CLUTTER_STAGE_STATE_ACTIVATED))
	{
	  /* TODO: check xevent->xfocus.detail ? */
	  stage_glx->state |= CLUTTER_STAGE_STATE_ACTIVATED;

	  event->type = CLUTTER_STAGE_STATE;
	  event->stage_state.changed_mask = CLUTTER_STAGE_STATE_ACTIVATED;
	  event->stage_state.new_state = stage_glx->state;
	}
      else
	res = FALSE;
      break;
    case FocusOut:
      if (stage_glx->state & CLUTTER_STAGE_STATE_ACTIVATED)
	{
	  stage_glx->state &= ~CLUTTER_STAGE_STATE_ACTIVATED;

	  event->type = CLUTTER_STAGE_STATE;
	  event->stage_state.changed_mask = CLUTTER_STAGE_STATE_ACTIVATED;
	  event->stage_state.new_state = stage_glx->state;
	}
      else
	res = FALSE;
      break;
    case Expose:
      {
        XEvent foo_xev;

        /* Cheap compress */
        while (XCheckTypedWindowEvent (backend_glx->xdpy, 
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
      set_user_time (backend_glx->xdpy, &xwindow, xevent->xkey.time);
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

          break;
        }

      set_user_time (backend_glx->xdpy, &xwindow, event->button.time);
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
    case ClientMessage:
      CLUTTER_NOTE (EVENT, "client message");
      
      event->type = event->any.type = CLUTTER_CLIENT_MESSAGE;
      
      if (xevent->xclient.message_type == Atom_XEMBED)
        res = handle_xembed_event (backend_glx, xevent);
      else if (xevent->xclient.message_type == Atom_WM_PROTOCOLS)
        {
          res = handle_wm_protocols_event (backend_glx, xevent);
          event->type = event->any.type = CLUTTER_DELETE;
        }
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
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (backend);
  ClutterEvent      *event;
  Display           *xdisplay = backend_glx->xdpy;
  XEvent             xevent;
  ClutterMainContext  *clutter_context;

  clutter_context = clutter_context_get_default ();

  while (!clutter_events_pending () && XPending (xdisplay))
    {
      XNextEvent (xdisplay, &xevent);

      event = clutter_event_new (CLUTTER_NOTHING);

      if (event_translate (backend, event, &xevent))
        {
	  /* push directly here to avoid copy of queue_put */
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

  clutter_threads_enter ();

  *timeout = -1;
  retval = (clutter_events_pending () || check_xpending (backend));

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

  if (event_source->event_poll_fd.revents & G_IO_IN)
    retval = (clutter_events_pending () || check_xpending (backend));
  else
    retval = FALSE;

  clutter_threads_leave ();

  return retval;
}

static gboolean
clutter_event_dispatch (GSource     *source,
                        GSourceFunc  callback,
                        gpointer     user_data)
{
  ClutterBackend *backend = ((ClutterEventSource *) source)->backend;
  ClutterEvent *event;

  clutter_threads_enter ();

  /*  Grab the event(s), translate and figure out double click.   
   *  The push onto queue (stack) if valid.
  */
  events_queue (backend);

  /* Pop an event off the queue if any */
  event = clutter_event_get ();

  if (event)
    {
      /* forward the event into clutter for emission etc. */
      clutter_do_event (event);
      clutter_event_free (event);
    }

  clutter_threads_leave ();

  return TRUE;
}
