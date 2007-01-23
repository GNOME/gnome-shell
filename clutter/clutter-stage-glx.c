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

/**
 * SECTION:clutter-stage
 * @short_description: Top level visual element to which actors are placed.
 * 
 * #ClutterStage is a top level 'window' on which child actors are placed
 * and manipulated.
 */

#include "config.h"

#include "clutter-stage.h"
#include "clutter-main.h"
#include "clutter-feature.h"
#include "clutter-color.h"
#include "clutter-util.h"
#include "clutter-marshal.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"
#include "clutter-debug.h"

#include "clutter-stage-glx.h"
#include "clutter-backend-glx.h"

#include <GL/glx.h>
#include <GL/gl.h>

#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>

struct _ClutterStageBackend
{
  XVisualInfo  *xvisinfo;
  Window        xwin;  
  Pixmap        xpixmap;
  gint          xwin_width, xwin_height; /* FIXME target_width / height */
  GLXPixmap     glxpixmap;
  GLXContext    gl_context;
  gboolean      is_foreign_xwin;
};

typedef struct 
{
  GSource  source;
  Display *display;
  GPollFD  event_poll_fd;
} 
ClutterXEventSource;

typedef void (*ClutterXEventFunc) (XEvent *xev, gpointer user_data);

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
  event->type = xevent->xany.type == KeyPress ? CLUTTER_KEY_PRESS
                                              : CLUTTER_KEY_RELEASE;
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
  CLUTTER_NOTE (EVENT, " button event at %ix%i",
		xevent->xbutton.x,
		xevent->xbutton.y);

  event->type = xevent->xany.type == ButtonPress ? CLUTTER_BUTTON_PRESS
                                                 : CLUTTER_BUTTON_RELEASE;
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

static void
clutter_dispatch_x_event (XEvent  *xevent,
			  gpointer data)
{
  ClutterMainContext *ctx = CLUTTER_CONTEXT ();
  ClutterEvent        event;
  ClutterStage       *stage = ctx->stage;
  gboolean            emit_input_event = FALSE;

  switch (xevent->type)
    {
    case Expose:
      {
	XEvent foo_xev;

	/* Cheap compress */
	while (XCheckTypedWindowEvent(clutter_glx_display(), 
				      xevent->xexpose.window,
				      Expose, 
				      &foo_xev));

	/* FIXME: need to make stage an 'actor' so can que
         * a paint direct from there rather than hack here...
	 */
	clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
      }
      break;
    case KeyPress:
      translate_key_event ((ClutterKeyEvent *) &event, xevent);
      g_signal_emit_by_name (stage, "key-press-event", &event);
      emit_input_event = TRUE;
      break;
    case KeyRelease:
      translate_key_event ((ClutterKeyEvent *) &event, xevent);
      g_signal_emit_by_name (stage, "key-release-event", &event);
      emit_input_event = TRUE;
      break;
    case ButtonPress:
      translate_button_event ((ClutterButtonEvent *) &event, xevent);
      g_signal_emit_by_name (stage, "button-press-event", &event);
      emit_input_event = TRUE;
      break;
    case ButtonRelease:
      translate_button_event ((ClutterButtonEvent *) &event, xevent);
      g_signal_emit_by_name (stage, "button-release-event", &event);
      emit_input_event = TRUE;
      break;
    case MotionNotify:
      translate_motion_event ((ClutterMotionEvent *) &event, xevent);
      g_signal_emit_by_name (stage, "motion-event", &event);
      emit_input_event = TRUE;
      break;
    }

  if (emit_input_event)
    g_signal_emit_by_name (stage, "input-event", &event);

}

static void
events_init()
{
  ClutterMainContext   *clutter_context;
  GMainContext         *gmain_context;
  int                   connection_number;
  GSource              *source;
  ClutterXEventSource  *display_source;

  clutter_context = clutter_context_get_default ();
  gmain_context = g_main_context_default ();

  g_main_context_ref (gmain_context);

  connection_number = ConnectionNumber (clutter_glx_display());
  
  source = g_source_new ((GSourceFuncs *)&x_event_funcs, 
			 sizeof (ClutterXEventSource));

  display_source = (ClutterXEventSource *)source;

  display_source->event_poll_fd.fd     = connection_number;
  display_source->event_poll_fd.events = G_IO_IN;
  display_source->display              = clutter_glx_display();
  
  g_source_add_poll (source, &display_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);

  g_source_set_callback (source, 
			 (GSourceFunc) clutter_dispatch_x_event, 
			 NULL  /* no userdata */, NULL);

  g_source_attach (source, gmain_context);
  g_source_unref (source);
}

static void
sync_fullscreen (ClutterStage *stage)
{
  Atom     atom_WINDOW_STATE, atom_WINDOW_STATE_FULLSCREEN;
  gboolean want_fullscreen;

  atom_WINDOW_STATE 
    = XInternAtom(clutter_glx_display(), "_NET_WM_STATE", False);
  atom_WINDOW_STATE_FULLSCREEN 
    = XInternAtom(clutter_glx_display(), "_NET_WM_STATE_FULLSCREEN",False);

  g_object_get (stage, "fullscreen", &want_fullscreen, NULL);

  if (want_fullscreen)
    {
      clutter_actor_set_size (CLUTTER_ACTOR(stage),
				DisplayWidth(clutter_glx_display(), 
					     clutter_glx_screen()),
				DisplayHeight(clutter_glx_display(), 
					      clutter_glx_screen()));

      if (stage->backend->xwin != None)
	XChangeProperty(clutter_glx_display(), stage->backend->xwin,
			atom_WINDOW_STATE, XA_ATOM, 32,
			PropModeReplace,
			(unsigned char *)&atom_WINDOW_STATE_FULLSCREEN, 1);
    }
  else
    {
      if (stage->backend->xwin != None)
	XDeleteProperty(clutter_glx_display(), 
			stage->backend->xwin, atom_WINDOW_STATE);
    }
}

static void
sync_cursor (ClutterStage *stage)
{
  gboolean hide_cursor;

  if (stage->backend->xwin == None)
    return;
  
  g_object_get (stage, "hide-cursor", &hide_cursor, NULL);

  /* FIXME: Use XFixesHideCursor */

  if (hide_cursor)
    {
      XColor col;
      Pixmap pix;
      Cursor curs;

      pix = XCreatePixmap (clutter_glx_display(), 
			   stage->backend->xwin, 1, 1, 1);
      memset (&col, 0, sizeof (col));
      curs = XCreatePixmapCursor (clutter_glx_display(), 
				  pix, pix, &col, &col, 1, 1);
      XFreePixmap (clutter_glx_display(), pix);
      XDefineCursor(clutter_glx_display(), stage->backend->xwin, curs);
    }
  else
    {
      XUndefineCursor(clutter_glx_display(), stage->backend->xwin);
    }
}

/* FIXME -> CGL */
static void
frustum (GLfloat left,
	 GLfloat right,
	 GLfloat bottom,
	 GLfloat top,
	 GLfloat nearval,
	 GLfloat farval)
{
  GLfloat x, y, a, b, c, d;
  GLfloat m[16];

  x = (2.0 * nearval) / (right - left);
  y = (2.0 * nearval) / (top - bottom);
  a = (right + left) / (right - left);
  b = (top + bottom) / (top - bottom);
  c = -(farval + nearval) / ( farval - nearval);
  d = -(2.0 * farval * nearval) / (farval - nearval);

#define M(row,col)  m[col*4+row]
  M(0,0) = x;     M(0,1) = 0.0F;  M(0,2) = a;      M(0,3) = 0.0F;
  M(1,0) = 0.0F;  M(1,1) = y;     M(1,2) = b;      M(1,3) = 0.0F;
  M(2,0) = 0.0F;  M(2,1) = 0.0F;  M(2,2) = c;      M(2,3) = d;
  M(3,0) = 0.0F;  M(3,1) = 0.0F;  M(3,2) = -1.0F;  M(3,3) = 0.0F;
#undef M

  glMultMatrixf (m);
}

static void
perspective (GLfloat fovy,
	     GLfloat aspect,
	     GLfloat zNear,
	     GLfloat zFar)
{
  GLfloat xmin, xmax, ymin, ymax;

  ymax = zNear * tan (fovy * M_PI / 360.0);
  ymin = -ymax;
  xmin = ymin * aspect;
  xmax = ymax * aspect;

  frustum (xmin, xmax, ymin, ymax, zNear, zFar);
}


static void
sync_viewport (ClutterStage *stage)
{
  glViewport (0, 0, stage->backend->xwin_width, stage->backend->xwin_height);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  perspective (60.0f, 1.0f, 0.1f, 100.0f);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  /* Then for 2D like transform */

  /* camera distance from screen, 0.5 * tan (FOV) */
#define DEFAULT_Z_CAMERA 0.866025404f

  glTranslatef (-0.5f, -0.5f, -DEFAULT_Z_CAMERA);
  glScalef (1.0f / stage->backend->xwin_width, 
	    -1.0f / stage->backend->xwin_height, 
	    1.0f / stage->backend->xwin_width);
  glTranslatef (0.0f, -stage->backend->xwin_height, 0.0f);
}

static void
clutter_stage_glx_show (ClutterActor *self)
{
  if (clutter_stage_glx_window (CLUTTER_STAGE(self)))
    XMapWindow (clutter_glx_display(), 
		clutter_stage_glx_window (CLUTTER_STAGE(self)));
}

static void
clutter_stage_glx_hide (ClutterActor *self)
{
  if (clutter_stage_glx_window (CLUTTER_STAGE(self)))
    XUnmapWindow (clutter_glx_display(), 
		  clutter_stage_glx_window (CLUTTER_STAGE(self)));
}

static void
clutter_stage_glx_unrealize (ClutterActor *actor)
{
  ClutterStage        *stage;
  ClutterStagePrivate *priv;
  ClutterStageBackend *backend;
  gboolean             want_offscreen;

  stage = CLUTTER_STAGE(actor);
  priv = stage->priv;
  backend = stage->backend;

  CLUTTER_MARK();

  g_object_get (stage, "offscreen", &want_offscreen, NULL);

  if (want_offscreen)
    {
      if (backend->glxpixmap)
	{
	  glXDestroyGLXPixmap (clutter_glx_display(), backend->glxpixmap);
	  backend->glxpixmap = None;
	}

      if (backend->xpixmap)
	{
	  XFreePixmap (clutter_glx_display(), backend->xpixmap);
	  backend->xpixmap = None;
	}
    }
  else
    {
      if (!backend->is_foreign_xwin && backend->xwin != None)
	{
	  XDestroyWindow (clutter_glx_display(), backend->xwin);
	  backend->xwin = None;
	}
      else
	backend->xwin = None;
    }

  glXMakeCurrent(clutter_glx_display(), None, NULL);
  if (backend->gl_context != None)
    {
      glXDestroyContext (clutter_glx_display(), backend->gl_context);
      backend->gl_context = None;
    }
}

static void
clutter_stage_glx_realize (ClutterActor *actor)
{
  ClutterStage        *stage;
  ClutterStagePrivate *priv;
  ClutterStageBackend *backend;
  gboolean             want_offscreen;

  stage = CLUTTER_STAGE(actor);

  priv = stage->priv;
  backend = stage->backend;

  CLUTTER_MARK();

  g_object_get (stage, "offscreen", &want_offscreen, NULL);

  if (want_offscreen)
    {
      int gl_attributes[] = {
	GLX_RGBA, 
	GLX_RED_SIZE, 1,
	GLX_GREEN_SIZE, 1,
	GLX_BLUE_SIZE, 1,
	0
      };

      if (backend->xvisinfo)
	XFree(backend->xvisinfo);

      backend->xvisinfo = glXChooseVisual (clutter_glx_display(),
					   clutter_glx_screen(),
					   gl_attributes);
      if (!backend->xvisinfo)
	{
	  g_critical ("Unable to find suitable GL visual.");
	  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
	  return;
	}

      if (backend->gl_context)
	glXDestroyContext (clutter_glx_display(), backend->gl_context);

      backend->xpixmap = XCreatePixmap (clutter_glx_display(),
				     clutter_glx_root_window(),
				     backend->xwin_width, 
				     backend->xwin_height,
				     backend->xvisinfo->depth);

      backend->glxpixmap = glXCreateGLXPixmap(clutter_glx_display(),
					      backend->xvisinfo,
					      backend->xpixmap);
      sync_fullscreen (stage);  

      /* indirect */
      backend->gl_context = glXCreateContext (clutter_glx_display(), 
					      backend->xvisinfo, 
					      0, 
					      False);
      
      glXMakeCurrent(clutter_glx_display(), 
		     backend->glxpixmap, backend->gl_context);

#if 0
      /* Debug code for monitoring a off screen pixmap via window */
      {
	Colormap cmap;
	XSetWindowAttributes swa;

	cmap = XCreateColormap(clutter_glx_display(),
			       clutter_glx_root_window(), 
			       backend->xvisinfo->visual, AllocNone);

	/* create a window */
	swa.colormap = cmap; 

	foo_win = XCreateWindow(clutter_glx_display(),
				clutter_glx_root_window(), 
				0, 0, 
				backend->xwin_width, backend->xwin_height,
				0, 
				backend->xvisinfo->depth, 
				InputOutput, 
				backend->xvisinfo->visual,
				CWColormap, &swa);

	XMapWindow(clutter_glx_display(), foo_win);
      }
#endif
    }
  else
    {
      int gl_attributes[] = 
	{
	  GLX_RGBA, 
	  GLX_DOUBLEBUFFER,
	  GLX_RED_SIZE, 1,
	  GLX_GREEN_SIZE, 1,
	  GLX_BLUE_SIZE, 1,
	  GLX_STENCIL_SIZE, 1,
	  0
	};

      if (backend->xvisinfo)
	XFree(backend->xvisinfo);

      if (backend->xvisinfo == None)
	backend->xvisinfo = glXChooseVisual (clutter_glx_display(),
					     clutter_glx_screen(),
					     gl_attributes);
      if (!backend->xvisinfo)
	{
	  g_critical ("Unable to find suitable GL visual.");
	  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
	  return;
	}

      if (backend->xwin == None)
	backend->xwin = XCreateSimpleWindow(clutter_glx_display(),
					    clutter_glx_root_window(),
					    0, 0,
					    backend->xwin_width, backend->xwin_height,
					    0, 0, 
					    WhitePixel(clutter_glx_display(), 
						       clutter_glx_screen()));
      XSelectInput(clutter_glx_display(), 
		   backend->xwin, 
		   StructureNotifyMask
		   |ExposureMask
		   /* FIXME: we may want to eplicity enable MotionMask */
		   |PointerMotionMask
		   |KeyPressMask
		   |KeyReleaseMask
		   |ButtonPressMask
		   |ButtonReleaseMask
		   |PropertyChangeMask);

      sync_fullscreen (stage);  
      sync_cursor (stage);  

      if (backend->gl_context)
	glXDestroyContext (clutter_glx_display(), backend->gl_context);

      backend->gl_context = glXCreateContext (clutter_glx_display(), 
					      backend->xvisinfo, 
					      0, 
					      True);
      
      if (backend->gl_context == None)
	{
	  g_critical ("Unable to create suitable GL context.");
	  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
	  return;
	}

      glXMakeCurrent(clutter_glx_display(), backend->xwin, backend->gl_context);
    }

  CLUTTER_NOTE (GL,
                "\n"
		"===========================================\n"
		"GL_VENDOR: %s\n"
		"GL_RENDERER: %s\n"
		"GL_VERSION: %s\n"
		"GL_EXTENSIONS: %s\n"
		"Is direct: %s\n"
		"===========================================\n",
		glGetString (GL_VENDOR),
		glGetString (GL_RENDERER),
		glGetString (GL_VERSION),
		glGetString (GL_EXTENSIONS),
		glXIsDirect(clutter_glx_display(), backend->gl_context) ? "yes" : "no"
		);

  sync_viewport (stage);
}

static void
clutter_stage_glx_paint (ClutterActor *self)
{
  ClutterStage  *stage = CLUTTER_STAGE(self);
  ClutterColor   stage_color;
  static GTimer *timer = NULL; 
  static guint   timer_n_frames = 0;

  static ClutterActorClass *parent_class = NULL;

  CLUTTER_NOTE (PAINT, " Redraw enter");

  if (parent_class == NULL)
    parent_class = g_type_class_peek_parent (CLUTTER_STAGE_GET_CLASS(stage));

  if (clutter_want_fps ())
    {
      if (!timer)
	timer = g_timer_new ();
    }

  clutter_stage_get_color (stage, &stage_color);

  glClearColor(((float) stage_color.red / 0xff * 1.0),
	       ((float) stage_color.green / 0xff * 1.0),
	       ((float) stage_color.blue / 0xff * 1.0),
	       0.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glDisable(GL_LIGHTING); 
  glDisable(GL_DEPTH_TEST);

  parent_class->paint (self);

  if (clutter_stage_glx_window (stage))
    {
      clutter_feature_wait_for_vblank ();
      glXSwapBuffers(clutter_glx_display(), clutter_stage_glx_window (stage));  
    }
  else
    {
      glXWaitGL();
      CLUTTER_GLERR();
    }

  if (clutter_want_fps ())
    {
      timer_n_frames++;

      if (g_timer_elapsed (timer, NULL) >= 1.0)
	{
	  g_print ("*** FPS: %i ***\n", timer_n_frames);
	  timer_n_frames = 0;
	  g_timer_start (timer);
	}
    }

  CLUTTER_NOTE (PAINT, " Redraw leave");
}

static void
clutter_stage_glx_allocate_coords (ClutterActor    *self,
			       ClutterActorBox *box)
{
  /* Do nothing, just stop group_allocate getting called */

  /* TODO: sync up with any configure events from WM ??  */
  return;
}

static void
clutter_stage_glx_request_coords (ClutterActor    *self,
				  ClutterActorBox *box)
{
  ClutterStage        *stage;
  ClutterStageBackend *backend;
  gint                 new_width, new_height;

  stage = CLUTTER_STAGE (self);
  backend = stage->backend;

  /* FIXME: some how have X configure_notfiys call this ?  
  */

  new_width  = ABS(box->x2 - box->x1);
  new_height = ABS(box->y2 - box->y1); 

  if (new_width != backend->xwin_width || new_height != backend->xwin_height)
    {
      backend->xwin_width  = new_width;
      backend->xwin_height = new_height;

      if (backend->xwin != None)
	XResizeWindow (clutter_glx_display(), 
		       backend->xwin, 
		       backend->xwin_width, 
		       backend->xwin_height);

      if (backend->xpixmap != None)
	{
	  /* Need to recreate to resize */
	  clutter_actor_unrealize(self);
	  clutter_actor_realize(self);
	}

      sync_viewport (stage);
    }

  if (backend->xwin != None) /* Do we want to bother ? */
    XMoveWindow (clutter_glx_display(), 
		 backend->xwin,
		 box->x1,
		 box->y1);
}

static void 
clutter_stage_glx_dispose (GObject *object)
{
#if 0
  ClutterStage *self = CLUTTER_STAGE (object);

  if (self->backend->xwin)
    clutter_actor_unrealize (CLUTTER_ACTOR (self));

  G_OBJECT_CLASS (clutter_stage_parent_class)->dispose (object);
#endif
}

static void 
clutter_stage_glx_finalize (GObject *object)
{
#if 0
  G_OBJECT_CLASS (clutter_stage_parent_class)->finalize (object);
#endif
}

void
clutter_stage_backend_init_vtable (ClutterStageVTable *vtable)
{
  vtable->show            = clutter_stage_glx_show; 
  vtable->hide            = clutter_stage_glx_hide;
  vtable->realize         = clutter_stage_glx_realize;
  vtable->unrealize       = clutter_stage_glx_unrealize;
  vtable->paint           = clutter_stage_glx_paint;
  vtable->request_coords  = clutter_stage_glx_request_coords;
  vtable->allocate_coords = clutter_stage_glx_allocate_coords;

  vtable->sync_fullscreen = sync_fullscreen;
  vtable->sync_cursor     = sync_cursor;
  vtable->sync_viewport   = sync_viewport;

}

ClutterStageBackend*
clutter_stage_backend_init (ClutterStage *stage)
{
  ClutterStageBackend *backend;

  backend = g_new0(ClutterStageBackend, 1);

  backend->xwin_width  = 100;
  backend->xwin_height = 100;

  /* Maybe better somewhere else */
  events_init ();

  return backend;
}

/**
 * clutter_stage_glx_get_xwindow
 * @stage: A #ClutterStage
 *
 * Get the stage's underlying x window ID.
 *
 * Return Value: Stage X Window XID
 *
 * Since: 0.3
 **/
Window
clutter_stage_glx_window (ClutterStage *stage)
{
  return stage->backend->xwin;
}

/**
 * clutter_stage_set_xwindow_foreign
 * @stage: A #ClutterStage
 * @xid: A preexisting X Window ID
 *
 * Target the #ClutterStage to use an existing external X Window.
 *
 * Return Value: TRUE if foreign window valid, FALSE otherwise 
 *
 * Since: 0.3
 **/
gboolean
clutter_stage_glx_set_window_foreign (ClutterStage *stage,
				      Window        xid)
{
  /* For screensavers via XSCREENSAVER_WINDOW env var.
   * Also for toolkit binding.
  */
  gint x,y;
  guint width, height, border, depth;
  Window root_return;
  Status status;
  ClutterGeometry geom;

  clutter_glx_trap_x_errors();

  status = XGetGeometry (clutter_glx_display(),
			 xid,
			 &root_return,
			 &x,
			 &y,
			 &width,
			 &height,
			 &border,
			 &depth);
	       
  if (clutter_glx_untrap_x_errors() || !status 
      || width == 0 || height == 0 || depth != stage->backend->xvisinfo->depth)
    return FALSE;

  clutter_actor_unrealize (CLUTTER_ACTOR(stage));

  stage->backend->xwin = xid;

  geom.x = x;
  geom.y = y;

  geom.width  = stage->backend->xwin_width  = width;
  geom.height = stage->backend->xwin_height = height;

  clutter_actor_set_geometry (CLUTTER_ACTOR(stage), &geom);

  clutter_actor_realize (CLUTTER_ACTOR(stage));

  return TRUE;
}

/**
 * clutter_stage_glx_get_xvisual
 * @stage: A #ClutterStage
 *
 * Get the stage's XVisualInfo.
 *
 * Return Value: The stage's XVisualInfo
 *
 * Since: 0.3
 **/
const XVisualInfo*
clutter_stage_glx_get_visual (ClutterStage *stage)
{
  return stage->backend->xvisinfo;
}
