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
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-version.h" 	/* For flavour */

#ifdef CLUTTER_FLAVOUR_GLX
#include <clutter/clutter-stage-glx.h>
#endif

#ifdef CLUTTER_FLAVOUR_EGL
#include <clutter/clutter-stage-egl.h>
#endif

#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>

/* the stage is a singleton instance */
static ClutterStage *stage_singleton = NULL;

/* Backend hooks */
static ClutterStageVTable _vtable;

G_DEFINE_TYPE (ClutterStage, clutter_stage, CLUTTER_TYPE_GROUP);

#define CLUTTER_STAGE_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_STAGE, ClutterStagePrivate))

struct _ClutterStagePrivate
{
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
	  _vtable.sync_fullscreen (stage);
	}
      break;
    case PROP_HIDE_CURSOR:
      if (priv->hide_cursor != g_value_get_boolean (value))
	{
	  priv->hide_cursor = g_value_get_boolean (value);
	  _vtable.sync_cursor (stage);
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

  clutter_stage_backend_init_vtable (&_vtable);

  actor_class->realize    = _vtable.realize;
  actor_class->unrealize  = _vtable.unrealize;
  actor_class->show       = _vtable.show;
  actor_class->hide       = _vtable.hide;
  actor_class->paint      = _vtable.paint;

  actor_class->request_coords  = _vtable.request_coords;
  actor_class->allocate_coords = _vtable.allocate_coords;

  /*
  gobject_class->dispose      = _vtable.stage_dispose;
  gobject_class->finalize     = _vtable.stage_finalize;
  */

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
			   G_PARAM_CONSTRUCT | CLUTTER_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_OFFSCREEN,
     g_param_spec_boolean ("offscreen",
			   "Offscreen",
			   "Make Clutter stage offscreen",
			   FALSE,
			   G_PARAM_CONSTRUCT | CLUTTER_PARAM_READWRITE));


  g_object_class_install_property
    (gobject_class, PROP_HIDE_CURSOR,
     g_param_spec_boolean ("hide-cursor",
			   "Hide Cursor",
			   "Make Clutter stage cursor-less",
			   FALSE,
			   G_PARAM_CONSTRUCT | CLUTTER_PARAM_READWRITE));

  g_object_class_install_property
    (gobject_class, PROP_COLOR,
     g_param_spec_boxed ("color",
			 "Color",
			 "The color of the stage",
			 CLUTTER_TYPE_COLOR,
			 CLUTTER_PARAM_READWRITE));

  /**
   * ClutterStage::input-event:
   * @stage: the actor which received the event
   * @event: the event received
   *
   * The ::input-event is a signal emitted when any input event is
   * received.  Valid input events are mouse button press and release
   * events, and key press and release events.
   */
  stage_signals[INPUT_EVENT] =
    g_signal_new ("input-event",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, input_event),
		  NULL, NULL,
		  clutter_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_EVENT);
  /**
   * ClutterStage::button-press-event:
   * @stage: the actor which received the event
   * @event: a #ClutterButtonEvent
   *
   * The ::button-press-event is emitted each time a mouse button
   * is pressed on @stage.
   */
  stage_signals[BUTTON_PRESS_EVENT] =
    g_signal_new ("button-press-event",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, button_press_event),
		  NULL, NULL,
		  clutter_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_EVENT);
  /**
   * ClutterStage::button-release-event:
   * @stage: the actor which received the event
   * @event: a #ClutterButtonEvent
   *
   * The ::button-release-event is emitted each time a mouse button
   * is released on @stage.
   */
  stage_signals[BUTTON_RELEASE_EVENT] =
    g_signal_new ("button-release-event",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, button_release_event),
		  NULL, NULL,
		  clutter_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_EVENT);
  /**
   * ClutterStage::key-press-event:
   * @stage: the actor which received the event
   * @event: a #ClutterKeyEvent
   *
   * The ::key-press-event is emitted each time a keyboard button
   * is pressed on @stage.
   */
  stage_signals[KEY_PRESS_EVENT] =
    g_signal_new ("key-press-event",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, key_press_event),
		  NULL, NULL,
		  clutter_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_EVENT);
  /**
   * ClutterStage::key-release-event:
   * @stage: the actor which received the event
   * @event: a #ClutterKeyEvent
   *
   * The ::key-release-event is emitted each time a keyboard button
   * is released on @stage.
   */
  stage_signals[KEY_RELEASE_EVENT] =
    g_signal_new ("key-release-event",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ClutterStageClass, key_release_event),
		  NULL, NULL,
		  clutter_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  CLUTTER_TYPE_EVENT);
  /**
   * ClutterStage::motion-event:
   * @stage: the actor which received the event
   * @event: a #ClutterMotionEvent
   *
   * The ::motion-event is emitted each time the mouse pointer is
   * moved on @stage.
   */
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

  /* a stage is a top-level object */
  CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_IS_TOPLEVEL);
  
  self->priv = priv = CLUTTER_STAGE_GET_PRIVATE (self);

  self->backend = clutter_stage_backend_init (self);

  priv->want_offscreen  = FALSE;
  priv->want_fullscreen = FALSE;
  priv->hide_cursor     = FALSE;

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
 * Return value: the main #ClutterStage.  You should never
 *   destroy the returned actor.
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

#if 0
static void
snapshot_pixbuf_free (guchar   *pixels,
		      gpointer  data)
{
  g_free(pixels);
}
#endif

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
#if 0
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
#endif
  return 0;
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
  
  _vtable.sync_viewport (stage); 

  return found;
}
