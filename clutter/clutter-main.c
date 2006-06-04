/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
#include "clutter-main.h"
#include "clutter-element.h"
#include "clutter-stage.h"
#include "clutter-private.h"

#include <gst/gst.h> 		/* for gst_init() */

typedef struct 
{
  GSource  source;
  Display *display;
  GPollFD  event_poll_fd;
} 
ClutterXEventSource;

ClutterMainContext ClutterCntx;

#define GLX_SAMPLE_BUFFERS_ARB             100000
#define GLX_SAMPLES_ARB                    100001

typedef void (*ClutterXEventFunc) (XEvent *xev, gpointer user_data);

static gboolean __clutter_has_debug = FALSE;
static gboolean __clutter_has_fps   = FALSE;

static gboolean  
x_event_prepare (GSource  *source,
		 gint     *timeout)
{
  Display *display = ((ClutterXEventSource*)source)->display;

  *timeout = -1;

  return XPending (display);
}

static gboolean  
x_event_check (GSource *source) 
{
  ClutterXEventSource *display_source = (ClutterXEventSource*)source;
  gboolean         retval;

  if (display_source->event_poll_fd.revents & G_IO_IN)
    retval = XPending (display_source->display);
  else
    retval = FALSE;

  return retval;
}

static gboolean  
x_event_dispatch (GSource    *source,
		  GSourceFunc callback,
		  gpointer    user_data)
{
  Display *display = ((ClutterXEventSource*)source)->display;
  ClutterXEventFunc event_func = (ClutterXEventFunc) callback;
  
  XEvent xev;

  if (XPending (display))
    {
      XNextEvent (display, &xev);

      if (event_func)
	(*event_func) (&xev, user_data);
    }

  return TRUE;
}

static const GSourceFuncs x_event_funcs = {
  x_event_prepare,
  x_event_check,
  x_event_dispatch,
  NULL
};

static void
translate_key_event (ClutterKeyEvent   *event,
		     XEvent            *xevent)
{
  event->type = xevent->xany.type 
    == KeyPress ? CLUTTER_KEY_PRESS : CLUTTER_KEY_RELEASE;
  event->time = xevent->xkey.time;
  event->modifier_state = xevent->xkey.state; /* FIXME: handle modifiers */
  event->hardware_keycode = xevent->xkey.keycode;
  event->keyval = XKeycodeToKeysym(xevent->xkey.display, 
				   xevent->xkey.keycode,
				   0 );	/* FIXME: index with modifiers */
}

static void
translate_button_event (ClutterButtonEvent   *event,
			XEvent               *xevent)
{
  /* FIXME: catch double click */
 CLUTTER_DBG("button event at %ix%i", xevent->xbutton.x, xevent->xbutton.y);

  event->type = xevent->xany.type 
    == ButtonPress ? CLUTTER_BUTTON_PRESS : CLUTTER_BUTTON_RELEASE;
  event->time = xevent->xbutton.time;
  event->x = xevent->xbutton.x;
  event->y = xevent->xbutton.y;
  event->modifier_state = xevent->xbutton.state; /* includes button masks */
  event->button = xevent->xbutton.button;
}

static void
translate_motion_event (ClutterMotionEvent   *event,
			XEvent               *xevent)
{
  event->type = CLUTTER_MOTION;
  event->time = xevent->xbutton.time;
  event->x = xevent->xmotion.x;
  event->y = xevent->xmotion.y;
  event->modifier_state = xevent->xmotion.state;
}

void
clutter_dispatch_x_event (XEvent  *xevent,
			  gpointer data)
{
  ClutterMainContext *ctx = CLUTTER_CONTEXT();
  ClutterEvent        event;

  switch (xevent->type)
    {
    case Expose:
      {
	XEvent foo_xev;

	/* Cheap compress */
	while (XCheckTypedWindowEvent(ctx->xdpy, 
				      xevent->xexpose.window,
				      Expose, 
				      &foo_xev));

	/* FIXME: need to make stage an 'element' so can que
         * a paint direct from there rather than hack here...
	*/
	clutter_element_queue_redraw (CLUTTER_ELEMENT(clutter_stage()));
      }
      break;
    case KeyPress:
    case KeyRelease:
      translate_key_event ((ClutterKeyEvent*)&event, xevent);
      g_signal_emit_by_name (clutter_stage(), "input-event", &event);
      break;
    case ButtonPress:
    case ButtonRelease:
      translate_button_event ((ClutterButtonEvent*)&event, xevent);
      g_signal_emit_by_name (clutter_stage(), "input-event", &event);
      break;
    case MotionNotify:
      translate_motion_event ((ClutterMotionEvent*)&event, xevent);
      g_signal_emit_by_name (clutter_stage(), "input-event", &event);
      break;
    }
}

static void
events_init()
{
  GMainContext         *gmain_context;
  int                   connection_number;
  GSource              *source;
  ClutterXEventSource  *display_source;

  gmain_context = g_main_context_default ();

  g_main_context_ref (gmain_context);

  connection_number = ConnectionNumber (ClutterCntx.xdpy);
  
  source = g_source_new ((GSourceFuncs *)&x_event_funcs, 
			 sizeof (ClutterXEventSource));

  display_source = (ClutterXEventSource *)source;

  display_source->event_poll_fd.fd     = connection_number;
  display_source->event_poll_fd.events = G_IO_IN;
  display_source->display              = ClutterCntx.xdpy;
  
  g_source_add_poll (source, &display_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);

  g_source_set_callback (source, 
			 (GSourceFunc) clutter_dispatch_x_event, 
			 NULL  /* no userdata */, NULL);

  g_source_attach (source, gmain_context);
  g_source_unref (source);
}

static gboolean
clutter_want_fps(void)
{
  return __clutter_has_fps;
}

void
clutter_redraw ()
{
  ClutterMainContext *ctx = CLUTTER_CONTEXT();
  ClutterStage       *stage = CLUTTER_STAGE(clutter_stage());
  ClutterColor        stage_color;
  
  static GTimer      *timer = NULL; 
  static guint        timer_n_frames = 0;

  /* FIXME: Should move all this into stage...
  */

  CLUTTER_DBG("@@@ Redraw enter @@@");

  clutter_threads_enter();

  if (clutter_want_fps())
    {
      if (!timer)
	timer = g_timer_new();
    }

  stage_color = clutter_stage_get_color (stage);

  glClearColor( ( (float)clutter_color_r(stage_color) / 0xff ) * 1.0,
		( (float)clutter_color_g(stage_color) / 0xff ) * 1.0,
		( (float)clutter_color_b(stage_color) / 0xff ) * 1.0,
		0.0 );
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_LIGHTING); 
  glDisable(GL_DEPTH_TEST);

  clutter_element_paint(CLUTTER_ELEMENT(stage));

  if (clutter_stage_get_xwindow (stage))
    {
#if 0
      unsigned int retraceCount;

      // Wait for vertical retrace
      // glXGetVideoSyncSGI(&retraceCount);
      // glXWaitVideoSyncSGI(2, (retraceCount+1)%2, &retraceCount);
      glXWaitVideoSyncSGI(1, 0, &retraceCount);
#endif
      glXSwapBuffers(ctx->xdpy, clutter_stage_get_xwindow (stage));  
    }
  else
    {
      glFlush();
    }

  if (clutter_want_fps())
    {
      timer_n_frames++;

      if (g_timer_elapsed (timer, NULL) >= 1.0)
	{
	  g_print ("*** FPS: %i ***\n", timer_n_frames);
	  timer_n_frames = 0;
	  g_timer_start (timer);
	}
    }

  clutter_threads_leave();

  CLUTTER_DBG("@@@ Redraw leave @@@");
}


void
clutter_main()
{
  GMainLoop *loop;

  loop = g_main_loop_new (g_main_context_default (), TRUE);

  g_main_loop_run (loop);
}

void
clutter_threads_enter(void)
{
  g_mutex_lock(ClutterCntx.gl_lock);
}

void
clutter_threads_leave(void)
{
  g_mutex_unlock(ClutterCntx.gl_lock);
}

ClutterGroup*
clutter_stage(void)
{
  return CLUTTER_GROUP(ClutterCntx.stage);
}

Display*
clutter_xdisplay(void)
{
  return ClutterCntx.xdpy;
}

int
clutter_xscreen(void)
{
  return ClutterCntx.xscreen;
}

Window
clutter_root_xwindow(void)
{
  return ClutterCntx.xwin_root;
}

XVisualInfo*
clutter_xvisual(void)
{
  return ClutterCntx.xvinfo;
}

gboolean
clutter_want_debug(void)
{
  return __clutter_has_debug;
}

void
clutter_gl_context_set_indirect (gboolean indirect)
{

}

int
clutter_init(int *argc, char ***argv)
{
  int  gl_attributes[] =
    {
      GLX_RGBA, 
      GLX_DOUBLEBUFFER, 
      GLX_RED_SIZE, 1,
      GLX_GREEN_SIZE, 1,
      GLX_BLUE_SIZE, 1,
      /* GLX_DEPTH_SIZE, 1, */
      /* GLX_DEPTH_SIZE, 32, */
      0
    };

  if (getenv("CLUTTER_DEBUG"))
    __clutter_has_debug = TRUE;

  if (getenv("CLUTTER_SHOW_FPS"))
    __clutter_has_fps = TRUE;

  g_type_init();

  if (!g_thread_supported ())
    g_thread_init (NULL);

  XInitThreads();

  gst_init (argc, argv);

  if ((ClutterCntx.xdpy = XOpenDisplay(getenv("DISPLAY"))) == NULL)
    {
      g_warning("Unable to connect to X DISPLAY.");
      return -1;
    }

  ClutterCntx.xscreen   = DefaultScreen(ClutterCntx.xdpy);
  ClutterCntx.xwin_root = RootWindow(ClutterCntx.xdpy, ClutterCntx.xscreen);

  if ((ClutterCntx.xvinfo = glXChooseVisual(ClutterCntx.xdpy, 
					    ClutterCntx.xscreen,
					    gl_attributes)) == NULL)
    {
      g_warning("Unable to find suitable GL visual.");
      return -2;
    }

  ClutterCntx.font_map = PANGO_FT2_FONT_MAP (pango_ft2_font_map_new ());

  pango_ft2_font_map_set_resolution (ClutterCntx.font_map, 96.0, 96.0);

  ClutterCntx.gl_lock = g_mutex_new ();

  ClutterCntx.stage = g_object_new (CLUTTER_TYPE_STAGE, NULL);

  g_return_val_if_fail (ClutterCntx.stage != NULL, -3);

  clutter_element_realize(CLUTTER_ELEMENT(ClutterCntx.stage));

  events_init();

  return 1;
}

