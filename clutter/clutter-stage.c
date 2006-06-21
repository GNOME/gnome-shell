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
#include "clutter-color.h"
#include "clutter-util.h"
#include "clutter-marshal.h"
#include "clutter-enum-types.h"
#include "clutter-private.h" 	/* for DBG */

#include <GL/glx.h>
#include <GL/gl.h>

#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>

/* the stage is a singleton instance */
static ClutterStage *stage_singleton = NULL;

G_DEFINE_TYPE (ClutterStage, clutter_stage, CLUTTER_TYPE_GROUP);

#define CLUTTER_STAGE_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_STAGE, ClutterStagePrivate))

struct _ClutterStagePrivate
{
  XVisualInfo  *xvisinfo;
  Window        xwin;  
  Pixmap        xpixmap;
  gint          xwin_width, xwin_height; /* FIXME target_width / height */
  
  GLXPixmap     glxpixmap;
  GLXContext    gl_context;

  
  ClutterColor  color;
  
  guint         want_fullscreen : 1;
  guint         want_offscreen  : 1;
  guint         hide_cursor     : 1;
};

enum
{
  PROP_0,
  
  PROP_COLOR,
  PROP_FULLSCREEN,
  PROP_OFFSCREEN,
  PROP_HIDE_CURSOR
};

enum
{
  INPUT_EVENT,
  BUTTON_PRESS_EVENT,
  BUTTON_RELEASE_EVENT,
  KEY_PRESS_EVENT,
  KEY_RELEASE_EVENT,
  MOTION_EVENT,
  
  LAST_SIGNAL
};

static guint stage_signals[LAST_SIGNAL] = { 0 };

static ClutterActorClass *parent_class = NULL;

static void
sync_fullscreen (ClutterStage *stage)
{
  Atom                 atom_WINDOW_STATE, atom_WINDOW_STATE_FULLSCREEN;

  atom_WINDOW_STATE 
    = XInternAtom(clutter_xdisplay(), "_NET_WM_STATE", False);
  atom_WINDOW_STATE_FULLSCREEN 
    = XInternAtom(clutter_xdisplay(), "_NET_WM_STATE_FULLSCREEN",False);

  if (stage->priv->want_fullscreen)
    {
      clutter_actor_set_size (CLUTTER_ACTOR(stage),
				DisplayWidth(clutter_xdisplay(), 
					     clutter_xscreen()),
				DisplayHeight(clutter_xdisplay(), 
					      clutter_xscreen()));

      if (stage->priv->xwin != None)
	XChangeProperty(clutter_xdisplay(), stage->priv->xwin,
			atom_WINDOW_STATE, XA_ATOM, 32,
			PropModeReplace,
			(unsigned char *)&atom_WINDOW_STATE_FULLSCREEN, 1);
    }
  else
    {
      if (stage->priv->xwin != None)
	XDeleteProperty(clutter_xdisplay(), 
			stage->priv->xwin, atom_WINDOW_STATE);
    }
}

static void
sync_cursor_visible (ClutterStage *stage)
{
  if (stage->priv->xwin == None)
    return;
  
  if (stage->priv->hide_cursor)
    {
      XColor col;
      Pixmap pix;
      Cursor curs;

      pix = XCreatePixmap (clutter_xdisplay(), 
			   stage->priv->xwin, 1, 1, 1);
      memset (&col, 0, sizeof (col));
      curs = XCreatePixmapCursor (clutter_xdisplay(), 
				  pix, pix, &col, &col, 1, 1);
      XFreePixmap (clutter_xdisplay(), pix);
      XDefineCursor(clutter_xdisplay(), stage->priv->xwin, curs);
    }
  else
    {
      XUndefineCursor(clutter_xdisplay(), stage->priv->xwin);
    }
}

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
sync_gl_viewport (ClutterStage *stage)
{
  /* Set For 2D */
#if 0
  glViewport (0, 0, stage->priv->xwin_width, stage->priv->xwin_height);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (0, stage->priv->xwin_width, stage->priv->xwin_height, 0, -1, 1);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

#endif
  /* For 3D */

  glViewport (0, 0, stage->priv->xwin_width, stage->priv->xwin_height);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  perspective (60.0f, 1.0f, 0.1f, 100.0f);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  /* Then for 2D like transform */

  /* camera distance from screen, 0.5 * tan (FOV) */
#define DEFAULT_Z_CAMERA 0.866025404f

  glTranslatef (-0.5f, -0.5f, -DEFAULT_Z_CAMERA);
  glScalef (1.0f / stage->priv->xwin_width, 
	    -1.0f / stage->priv->xwin_height, 1.0f / stage->priv->xwin_width);
  glTranslatef (0.0f, -stage->priv->xwin_height, 0.0f);
}

static void
clutter_stage_show (ClutterActor *self)
{
  if (clutter_stage_get_xwindow (CLUTTER_STAGE(self)))
    XMapWindow (clutter_xdisplay(), 
		clutter_stage_get_xwindow (CLUTTER_STAGE(self)));
}

static void
clutter_stage_hide (ClutterActor *self)
{
  if (clutter_stage_get_xwindow (CLUTTER_STAGE(self)))
    XUnmapWindow (clutter_xdisplay(), 
		  clutter_stage_get_xwindow (CLUTTER_STAGE(self)));
}

static void
clutter_stage_unrealize (ClutterActor *actor)
{
  ClutterStage        *stage;
  ClutterStagePrivate *priv;

  stage = CLUTTER_STAGE(actor);
  priv = stage->priv;

  CLUTTER_MARK();

  if (priv->want_offscreen)
    {
      

      if (priv->glxpixmap)
	{
	  glXDestroyGLXPixmap (clutter_xdisplay(), priv->glxpixmap);
	  priv->glxpixmap = None;
	}

      if (priv->xpixmap)
	{
	  XFreePixmap (clutter_xdisplay(), priv->xpixmap);
	  priv->xpixmap = None;
	}
    }
  else
    {
      if (clutter_stage_get_xwindow (CLUTTER_STAGE(actor)))
	{
	  XDestroyWindow (clutter_xdisplay(), priv->xwin);
	  priv->xwin = None;
	}
    }

  glXMakeCurrent(clutter_xdisplay(), None, NULL);
  glXDestroyContext (clutter_xdisplay(), priv->gl_context);
  priv->gl_context = None;
}

static void
clutter_stage_realize (ClutterActor *actor)
{
  ClutterStage        *stage;
  ClutterStagePrivate *priv;

  stage = CLUTTER_STAGE(actor);

  priv = stage->priv;

  CLUTTER_MARK();

  if (priv->want_offscreen)
    {
      int gl_attributes[] = {
	GLX_RGBA, 
	GLX_RED_SIZE, 1,
	GLX_GREEN_SIZE, 1,
	GLX_BLUE_SIZE, 1,
	0
      };

      if (priv->xvisinfo)
	XFree(priv->xvisinfo);

      priv->xvisinfo = glXChooseVisual (clutter_xdisplay(),
					clutter_xscreen(),
					gl_attributes);
      if (!priv->xvisinfo)
	{
	  g_critical ("Unable to find suitable GL visual.");
	  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
	  return;
	}

      if (priv->gl_context)
	glXDestroyContext (clutter_xdisplay(), priv->gl_context);

      priv->xpixmap = XCreatePixmap (clutter_xdisplay(),
				     clutter_root_xwindow(),
				     priv->xwin_width, 
				     priv->xwin_height,
				     priv->xvisinfo->depth);

      priv->glxpixmap = glXCreateGLXPixmap(clutter_xdisplay(),
					   priv->xvisinfo,
					   priv->xpixmap);
      sync_fullscreen (stage);  

      /* indirect */
      priv->gl_context = glXCreateContext (clutter_xdisplay(), 
					   priv->xvisinfo, 
					   0, 
					   False);
      
      glXMakeCurrent(clutter_xdisplay(), priv->glxpixmap, priv->gl_context);

#if 0
      /* Debug code for monitoring a off screen pixmap via window */
      {
	Colormap cmap;
	XSetWindowAttributes swa;

	cmap = XCreateColormap(clutter_xdisplay(),
			       clutter_root_xwindow(), 
			       priv->xvisinfo->visual, AllocNone);

	/* create a window */
	swa.colormap = cmap; 

	foo_win = XCreateWindow(clutter_xdisplay(),
				clutter_root_xwindow(), 
				0, 0, 
				priv->xwin_width, priv->xwin_height,
				0, 
				priv->xvisinfo->depth, 
				InputOutput, 
				priv->xvisinfo->visual,
				CWColormap, &swa);

	XMapWindow(clutter_xdisplay(), foo_win);
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
	  0
	};

      if (priv->xvisinfo)
	XFree(priv->xvisinfo);

      priv->xvisinfo = glXChooseVisual (clutter_xdisplay(),
					clutter_xscreen(),
					gl_attributes);
      if (!priv->xvisinfo)
	{
	  g_critical ("Unable to find suitable GL visual.");
	  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
	  return;
	}

      if (priv->xwin == None)
	priv->xwin = XCreateSimpleWindow(clutter_xdisplay(),
					 clutter_root_xwindow(),
					 0, 0,
					 priv->xwin_width, priv->xwin_height,
					 0, 0, 
					 WhitePixel(clutter_xdisplay(), 
						    clutter_xscreen()));
      XSelectInput(clutter_xdisplay(), 
		   priv->xwin, 
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
      sync_cursor_visible (stage);  

      if (priv->gl_context)
	glXDestroyContext (clutter_xdisplay(), priv->gl_context);

      priv->gl_context = glXCreateContext (clutter_xdisplay(), 
					   priv->xvisinfo, 
					   0, 
					   True);
      
      glXMakeCurrent(clutter_xdisplay(), priv->xwin, priv->gl_context);
    }

  CLUTTER_DBG("===========================================")
  CLUTTER_DBG("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
  CLUTTER_DBG("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
  CLUTTER_DBG("GL_VERSION: %s\n", glGetString(GL_VERSION));
  CLUTTER_DBG("GL_EXTENSIONS: %s\n", glGetString(GL_EXTENSIONS));
  CLUTTER_DBG("===========================================")

  sync_gl_viewport (stage);
}

static void
clutter_stage_paint (ClutterActor *self)
{
  parent_class->paint (self);
}

static void
clutter_stage_allocate_coords (ClutterActor    *self,
			       ClutterActorBox *box)
{
  /* Do nothing, just stop group_allocate getting called */

  /* TODO: sync up with any configure events from WM ??  */
  return;
}

static void
clutter_stage_request_coords (ClutterActor    *self,
			      ClutterActorBox *box)
{
  ClutterStage        *stage;
  ClutterStagePrivate *priv;
  gint                 new_width, new_height;

  stage = CLUTTER_STAGE (self);
  priv = stage->priv;

  /* FIXME: some how have X configure_notfiys call this ?  
  */

  new_width  = ABS(box->x2 - box->x1);
  new_height = ABS(box->y2 - box->y1); 

  if (new_width != priv->xwin_width || new_height != priv->xwin_height)
    {
      priv->xwin_width  = new_width;
      priv->xwin_height = new_height;

      if (priv->xwin)
	XResizeWindow (clutter_xdisplay(), 
		       priv->xwin, 
		       priv->xwin_width, 
		       priv->xwin_height);

      if (priv->xpixmap)
	{
	  /* Need to recreate to resize */
	  clutter_actor_unrealize(self);
	  clutter_actor_realize(self);
	}

      sync_gl_viewport (stage);
    }

  if (priv->xwin) /* Do we want to bother ? */
    XMoveWindow (clutter_xdisplay(), 
		 priv->xwin,
		 box->x1,
		 box->y1);
}

static void 
clutter_stage_dispose (GObject *object)
{
  ClutterStage *self = CLUTTER_STAGE (object);

  if (self->priv->xwin)
    clutter_stage_unrealize (CLUTTER_ACTOR (self));

  G_OBJECT_CLASS (clutter_stage_parent_class)->dispose (object);
}

static void 
clutter_stage_finalize (GObject *object)
{
  G_OBJECT_CLASS (clutter_stage_parent_class)->finalize (object);
}


static void
clutter_stage_set_property (GObject      *object, 
			    guint         prop_id,
			    const GValue *value, 
			    GParamSpec   *pspec)
{
  ClutterStage        *stage;
  ClutterStagePrivate *priv;

  stage = CLUTTER_STAGE(object);
  priv = stage->priv;

  switch (prop_id) 
    {
    case PROP_COLOR:
      clutter_stage_set_color (stage, g_value_get_boxed (value));
      break;
    case PROP_OFFSCREEN:
      if (priv->want_offscreen != g_value_get_boolean (value))
	{
	  clutter_actor_unrealize (CLUTTER_ACTOR(stage));
	  /* NOTE: as we are changing GL contexts here. so  
	   * all textures will need unreleasing as they will
           * likely have set up ( i.e labels ) in the old
           * context. We should probably somehow do this
           * automatically
	  */
	  priv->want_offscreen = g_value_get_boolean (value);
	  clutter_actor_realize (CLUTTER_ACTOR(stage));
	}
      break;
    case PROP_FULLSCREEN:
      if (priv->want_fullscreen != g_value_get_boolean (value))
	{
	  priv->want_fullscreen = g_value_get_boolean (value);
	  sync_fullscreen (stage);
	}
      break;
    case PROP_HIDE_CURSOR:
      if (priv->hide_cursor != g_value_get_boolean (value))
	{
	  priv->hide_cursor = g_value_get_boolean (value);
	  sync_cursor_visible (stage);
	}
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_stage_get_property (GObject    *object, 
			      guint       prop_id,
			      GValue     *value, 
			      GParamSpec *pspec)
{
  ClutterStage        *stage;
  ClutterStagePrivate *priv;
  ClutterColor         color;

  stage = CLUTTER_STAGE(object);
  priv = stage->priv;

  switch (prop_id) 
    {
    case PROP_COLOR:
      clutter_stage_get_color (stage, &color);
      g_value_set_boxed (value, &color);
      break;
    case PROP_OFFSCREEN:
      g_value_set_boolean (value, priv->want_offscreen);
      break;
    case PROP_FULLSCREEN:
      g_value_set_boolean (value, priv->want_fullscreen);
      break;
    case PROP_HIDE_CURSOR:
      g_value_set_boolean (value, priv->hide_cursor);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    } 
}

static void
clutter_stage_class_init (ClutterStageClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  actor_class->realize    = clutter_stage_realize;
  actor_class->unrealize  = clutter_stage_unrealize;
  actor_class->show       = clutter_stage_show;
  actor_class->hide       = clutter_stage_hide;
  actor_class->paint      = clutter_stage_paint;

  actor_class->request_coords  = clutter_stage_request_coords;
  actor_class->allocate_coords = clutter_stage_allocate_coords;

  gobject_class->dispose      = clutter_stage_dispose;
  gobject_class->finalize     = clutter_stage_finalize;
  gobject_class->set_property = clutter_stage_set_property;
  gobject_class->get_property = clutter_stage_get_property;

  /**
   * ClutterStage:fullscreen
   *
   * Whether the stage should be fullscreen or not.
   */
  g_object_class_install_property
    (gobject_class, PROP_FULLSCREEN,
     g_param_spec_boolean ("fullscreen",
			   "Fullscreen",
			   "Make Clutter stage fullscreen",
			   FALSE,
			   G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_OFFSCREEN,
     g_param_spec_boolean ("offscreen",
			   "Offscreen",
			   "Make Clutter stage offscreen",
			   FALSE,
			   G_PARAM_CONSTRUCT | G_PARAM_READWRITE));


  g_object_class_install_property
    (gobject_class, PROP_HIDE_CURSOR,
     g_param_spec_boolean ("hide-cursor",
			   "Hide Cursor",
			   "Make Clutter stage cursor-less",
			   FALSE,
			   G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_COLOR,
     g_param_spec_boxed ("color",
			 "Color",
			 "The color of the stage",
			 CLUTTER_TYPE_COLOR,
			 G_PARAM_READWRITE));

  stage_signals[INPUT_EVENT] =
    g_signal_new ("input-event",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, input_event),
		  NULL, NULL,
		  clutter_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_EVENT);

  stage_signals[BUTTON_PRESS_EVENT] =
    g_signal_new ("button-press-event",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, button_press_event),
		  NULL, NULL,
		  clutter_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_EVENT);
  stage_signals[BUTTON_RELEASE_EVENT] =
    g_signal_new ("button-release-event",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, button_release_event),
		  NULL, NULL,
		  clutter_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_EVENT);
  stage_signals[BUTTON_PRESS_EVENT] =
    g_signal_new ("key-press-event",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, key_press_event),
		  NULL, NULL,
		  clutter_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_EVENT);
  stage_signals[BUTTON_RELEASE_EVENT] =
    g_signal_new ("key-release-event",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, key_release_event),
		  NULL, NULL,
		  clutter_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_EVENT);
  stage_signals[MOTION_EVENT] =
    g_signal_new ("motion-event",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, motion_event),
		  NULL, NULL,
		  clutter_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_EVENT);
  
  g_type_class_add_private (gobject_class, sizeof (ClutterStagePrivate));
}

static void
clutter_stage_init (ClutterStage *self)
{
  ClutterStagePrivate *priv;
  
  self->priv = priv = CLUTTER_STAGE_GET_PRIVATE (self);

  priv->want_offscreen  = FALSE;
  priv->want_fullscreen = FALSE;
  priv->hide_cursor     = FALSE;

  priv->xwin_width  = 100;
  priv->xwin_height = 100;

  priv->color.red   = 0xff;
  priv->color.green = 0xff;
  priv->color.blue  = 0xff;
  priv->color.alpha = 0xff;
  
  clutter_actor_set_size (CLUTTER_ACTOR (self), 640, 480);
}

/**
 * clutter_stage_get_default:
 *
 * Returns the main stage.  #ClutterStage is a singleton, so
 * the stage will be created the first time this function is
 * called (typically, inside clutter_init()); all the subsequent
 * calls to clutter_stage_get_default() will return the same
 * instance, with its reference count increased.
 *
 * Return value: the main #ClutterStage.  Use g_object_unref()
 *   when finished using it.
 */
ClutterActor *
clutter_stage_get_default (void)
{
  ClutterActor *retval = NULL;
  
  if (!stage_singleton)
    {
      stage_singleton = g_object_new (CLUTTER_TYPE_STAGE, NULL);
      retval = CLUTTER_ACTOR (stage_singleton);
    }
  else
    {
      retval = CLUTTER_ACTOR (stage_singleton);

      /* We dont ref for now as its assumed there will always be
       * a stage and no real support for multiple stages. Non
       * reffing makes API slightly simpler and allows for things
       * like CLUTTER_STAGE_WIDTH() work nicely.
       *
       * In future if multiple stage support is added probably
       * add a clutter-stage-manager class that would manage 
       * multiple instances.
       *  g_object_ref (retval);
      */
    }

  return retval;
}

/**
 * clutter_stage_get_xwindow
 * @stage: A #ClutterStage
 *
 * Get the stage's underlying x window ID.
 *
 * Return Value: Stage X Window XID
 **/
Window
clutter_stage_get_xwindow (ClutterStage *stage)
{
  return stage->priv->xwin;
}

/**
 * clutter_stage_set_xwindow_foreign
 * @stage: A #ClutterStage
 * @xid: A preexisting X Window ID
 *
 * Target the #ClutterStage to use an existing external X Window.
 *
 * Return Value: TRUE if foreign window valid, FALSE otherwise 
 **/
gboolean
clutter_stage_set_xwindow_foreign (ClutterStage *stage,
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

  clutter_util_trap_x_errors();

  status = XGetGeometry (clutter_xdisplay(),
			 xid,
			 &root_return,
			 &x,
			 &y,
			 &width,
			 &height,
			 &border,
			 &depth);
	       
  if (clutter_util_untrap_x_errors() || !status 
      || width == 0 || height == 0 || depth != stage->priv->xvisinfo->depth)
    return FALSE;

  clutter_actor_unrealize (CLUTTER_ACTOR(stage));

  stage->priv->xwin = xid;

  geom.x = x;
  geom.y = y;

  geom.width  = stage->priv->xwin_width  = width;
  geom.height = stage->priv->xwin_height = height;

  clutter_actor_set_geometry (CLUTTER_ACTOR(stage), &geom);

  clutter_actor_realize (CLUTTER_ACTOR(stage));

  return TRUE;
}

/**
 * clutter_stage_get_xvisual
 * @stage: A #ClutterStage
 *
 * Get the stage's XVisualInfo.
 *
 * Return Value: The stage's XVisualInfo
 **/
const XVisualInfo*
clutter_stage_get_xvisual (ClutterStage *stage)
{
  return stage->priv->xvisinfo;
}

/**
 * clutter_stage_set_color
 * @stage: A #ClutterStage
 * @color: A #ClutterColor
 * 
 * Set the stage color.
 **/
void
clutter_stage_set_color (ClutterStage       *stage,
			 const ClutterColor *color)
{
  ClutterStagePrivate *priv;

  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (color != NULL);
  
  priv = stage->priv;
  priv->color.red = color->red;
  priv->color.green = color->green;
  priv->color.blue = color->blue;
  priv->color.alpha = color->alpha;

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (stage)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));
  
  g_object_notify (G_OBJECT (stage), "color");
}

/**
 * clutter_stage_get_color
 * @stage: A #ClutterStage
 * @color: return location for a #ClutterColor
 * 
 * Retrieves the stage color.
 */
void
clutter_stage_get_color (ClutterStage *stage,
			 ClutterColor *color)
{
  ClutterStagePrivate *priv;
  
  g_return_if_fail (CLUTTER_IS_STAGE (stage));
  g_return_if_fail (color != NULL);

  priv = stage->priv;
  
  color->red = priv->color.red;
  color->green = priv->color.green;
  color->blue = priv->color.blue;
  color->alpha = priv->color.alpha;
}

static void
snapshot_pixbuf_free (guchar   *pixels,
		      gpointer  data)
{
  g_free(pixels);
}

/**
 * clutter_stage_snapshot
 * @stage: A #ClutterStage
 * @x: x coordinate of the first pixel that is read from stage
 * @y: y coordinate of the first pixel that is read from stage
 * @width: Width dimention of pixels to be read, or -1 for the
 *   entire stage width
 * @height: Height dimention of pixels to be read, or -1 for the
 *   entire stage height
 *
 * Gets a pixel based representation of the current rendered stage.
 *
 * Return value: pixel representation as a  #GdkPixbuf
 **/
GdkPixbuf*
clutter_stage_snapshot (ClutterStage *stage,
			gint          x,
			gint          y,
			gint          width,
			gint          height)
{
  guchar              *data;
  GdkPixbuf           *pixb, *fpixb;
  ClutterActor      *actor;
  ClutterStagePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);
  g_return_val_if_fail (x >= 0 && y >= 0, NULL);

  priv = stage->priv;

  actor = CLUTTER_ACTOR (stage);

  if (width < 0)
    width = clutter_actor_get_width (actor);

  if (height < 0)
    height = clutter_actor_get_height (actor);


  if (priv->want_offscreen)
    {
      gdk_pixbuf_xlib_init (clutter_xdisplay(), clutter_xscreen());

      pixb = gdk_pixbuf_xlib_get_from_drawable 
                        	(NULL,
				 (Drawable)priv->xpixmap,
				 DefaultColormap(clutter_xdisplay(),
						 clutter_xscreen()),
				 priv->xvisinfo->visual,
				 x, y, 0, 0, width, height);
      return pixb;
    }
  else
    {
      data = g_malloc0 (sizeof (guchar) * width * height * 4);

      glReadPixels (x, 
		    clutter_actor_get_height (actor) 
		    - y - height,
		    width, 
		    height, GL_RGBA, GL_UNSIGNED_BYTE, data);
      
      pixb = gdk_pixbuf_new_from_data (data,
				       GDK_COLORSPACE_RGB, 
				       TRUE, 
				       8, 
				       width, 
				       height,
				       width * 4,
				       snapshot_pixbuf_free,
				       NULL);
      
      fpixb = gdk_pixbuf_flip (pixb, TRUE); 

      g_object_unref (pixb);

      return fpixb;
    }
}

/**
 * clutter_stage_get_actor_at_pos:
 * @stage: a #ClutterStage
 * @x: the x coordinate
 * @y: the y coordinate
 *
 * If found, retrieves the actor that the (x, y) coordinates.
 *
 * Return value: the #ClutterActor at the desired coordinates,
 *   or %NULL if no actor was found.
 */
ClutterActor*
clutter_stage_get_actor_at_pos (ClutterStage *stage,
				  gint          x,
				  gint          y)
{
  ClutterActor *found = NULL;
  GLuint          buff[64] = {0};
  GLint           hits, view[4];
 
  glSelectBuffer(64, buff);
  glGetIntegerv(GL_VIEWPORT, view);
  glRenderMode(GL_SELECT);

  glInitNames();

  glPushName(0);
 
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();

  /* This is gluPickMatrix(x, y, 1.0, 1.0, view); */
  glTranslatef((view[2] - 2 * (x - view[0])),
	       (view[3] - 2 * (y - view[1])), 0);
  glScalef(view[2], -view[3], 1.0);

  perspective (60.0f, 1.0f, 0.1f, 100.0f); 

  glMatrixMode(GL_MODELVIEW);

  clutter_actor_paint (CLUTTER_ACTOR (stage));

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();

  hits = glRenderMode(GL_RENDER);

  if (hits != 0)
    {
#if 0
      gint i
      for (i = 0; i < hits; i++)
	g_print ("Hit at %i\n", buff[i * 4 + 3]);
#endif
  
      found = clutter_group_find_child_by_id (CLUTTER_GROUP (stage), 
					      buff[(hits-1) * 4 + 3]);
    }
  
  sync_gl_viewport (stage);

  return found;
}

