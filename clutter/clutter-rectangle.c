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
 * SECTION:clutter-rectangle
 * @short_description: An actor that displays simple rectangles.
 *
 * #ClutterRectangle is an Actor which draws simple filled rectangles.
 */

#include "clutter-rectangle.h"
#include "clutter-main.h"
#include "clutter-private.h" 	/* for DBG */

#include <GL/glx.h>
#include <GL/gl.h>

G_DEFINE_TYPE (ClutterRectangle, clutter_rectangle, CLUTTER_TYPE_ACTOR);

enum
{
  PROP_0,
  PROP_COLOR

  /* FIXME: Add gradient, rounded corner props etc */
};

#define CLUTTER_RECTANGLE_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_RECTANGLE, ClutterRectanglePrivate))

struct _ClutterRectanglePrivate
{
  ClutterColor color;
};

static void
clutter_rectangle_paint (ClutterActor *self)
{
  ClutterRectangle        *rectangle = CLUTTER_RECTANGLE(self);
  ClutterRectanglePrivate *priv;
  ClutterGeometry          geom;

  rectangle = CLUTTER_RECTANGLE(self);
  priv = rectangle->priv;

  glPushMatrix();

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  clutter_actor_get_geometry (self, &geom);

  glColor4ub(priv->color.red,
	     priv->color.green,
	     priv->color.blue, 
	     clutter_actor_get_opacity (self));

  glRecti (geom.x,
	   geom.y,
	   geom.x + geom.width,
	   geom.y + geom.height);

  glDisable(GL_BLEND);

  glPopMatrix();
}

static void
clutter_rectangle_set_property (GObject      *object, 
				guint         prop_id,
				const GValue *value, 
				GParamSpec   *pspec)
{
  ClutterRectangle *rectangle = CLUTTER_RECTANGLE(object);

  switch (prop_id) 
    {
    case PROP_COLOR:
      clutter_rectangle_set_color (rectangle, g_value_get_boxed (value)); 
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_rectangle_get_property (GObject    *object, 
				guint       prop_id,
				GValue     *value, 
				GParamSpec *pspec)
{
  ClutterRectangle *rectangle = CLUTTER_RECTANGLE(object);
  ClutterColor      color;

  switch (prop_id) 
    {
    case PROP_COLOR:
      clutter_rectangle_get_color (rectangle, &color);
      g_value_set_boxed (value, &color);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    } 
}


static void 
clutter_rectangle_finalize (GObject *object)
{
  G_OBJECT_CLASS (clutter_rectangle_parent_class)->finalize (object);
}

static void 
clutter_rectangle_dispose (GObject *object)
{
  G_OBJECT_CLASS (clutter_rectangle_parent_class)->dispose (object);
}


static void
clutter_rectangle_class_init (ClutterRectangleClass *klass)
{
  GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint        = clutter_rectangle_paint;

  gobject_class->finalize     = clutter_rectangle_finalize;
  gobject_class->dispose      = clutter_rectangle_dispose;
  gobject_class->set_property = clutter_rectangle_set_property;
  gobject_class->get_property = clutter_rectangle_get_property;

  g_object_class_install_property
    (gobject_class, PROP_COLOR,
     g_param_spec_boxed ("color",
		         "Color",
		         "The color of the rectangle",
			 CLUTTER_TYPE_COLOR,
			 G_PARAM_READWRITE));

  g_type_class_add_private (gobject_class, sizeof (ClutterRectanglePrivate));
}

static void
clutter_rectangle_init (ClutterRectangle *self)
{
  self->priv = CLUTTER_RECTANGLE_GET_PRIVATE (self);

  self->priv->color.red = 0xff;
  self->priv->color.green = 0xff;
  self->priv->color.blue = 0xff;
  self->priv->color.alpha = 0xff;
}

/**
 * clutter_rectangle_new:
 *
 * Creates a new #ClutterActor with a rectangular shape.
 *
 * Return value: a new #ClutterActor
 */
ClutterActor*
clutter_rectangle_new (void)
{
  return g_object_new (CLUTTER_TYPE_RECTANGLE, NULL);
}

/**
 * clutter_rectangle_new_with_color:
 * @color: a #ClutterColor
 *
 * Creates a new #ClutterActor with a rectangular shape
 * and with @color.
 *
 * Return value: a new #ClutterActor
 */
ClutterActor *
clutter_rectangle_new_with_color (const ClutterColor *color)
{
  return g_object_new (CLUTTER_TYPE_RECTANGLE,
		       "color", color,
		       NULL);
}

/**
 * clutter_rectangle_get_color:
 * @rectangle: a #ClutterRectangle
 * @color: return location for a #ClutterColor
 *
 * Retrieves the color of @rectangle.
 */
void
clutter_rectangle_get_color (ClutterRectangle *rectangle,
			     ClutterColor     *color)
{
  ClutterRectanglePrivate *priv;
  
  g_return_if_fail (CLUTTER_IS_RECTANGLE (rectangle));
  g_return_if_fail (color != NULL);

  priv = rectangle->priv;

  color->red = priv->color.red;
  color->green = priv->color.green;
  color->blue = priv->color.blue;
  color->alpha = priv->color.alpha;
}

/**
 * clutter_rectangle_set_color:
 * @rectangle: a #ClutterRectangle
 * @color: a #ClutterColor
 *
 * Sets the color of @rectangle.
 */
void
clutter_rectangle_set_color (ClutterRectangle   *rectangle,
			     const ClutterColor *color)
{
  ClutterRectanglePrivate *priv;
  
  g_return_if_fail (CLUTTER_IS_RECTANGLE (rectangle));
  g_return_if_fail (color != NULL);

  priv = rectangle->priv;

  priv->color.red = color->red;
  priv->color.green = color->green;
  priv->color.blue = color->blue;
  priv->color.alpha = color->alpha;

  clutter_actor_set_opacity (CLUTTER_ACTOR (rectangle),
		  	       priv->color.alpha);

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (rectangle)))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (rectangle));

  g_object_notify (G_OBJECT (rectangle), "color");
}
