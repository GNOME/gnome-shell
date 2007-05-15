/* -*- mode:C; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Tomas Frydrych  <tf@openedhand.com>
 *
 * Copyright (C) 2007 OpenedHand
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
 * SECTION:clutter-behaviour-ellipse
 * @short_description: elliptic path behaviour.
 *
 * #ClutterBehaviourEllipse interpolates actors along a path defined by
 *  en ellipse.
 *
 * Each time the behaviour reaches a point on the path, the "knot-reached"
 * signal is emitted.
 *
 * Since: 0.4
 */

#include "clutter-fixed.h"
#include "clutter-marshal.h"
#include "clutter-behaviour-ellipse.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"

#include <stdlib.h>
#include <memory.h>

/****************************************************************************
 *                                                                          *
 * ClutterBehaviourEllipse                                                  *
 *                                                                          *
 ****************************************************************************/

G_DEFINE_TYPE (ClutterBehaviourEllipse, 
               clutter_behaviour_ellipse,
	       CLUTTER_TYPE_BEHAVIOUR);

#define CLUTTER_BEHAVIOUR_ELLIPSE_GET_PRIVATE(obj)    \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj),    \
               CLUTTER_TYPE_BEHAVIOUR_ELLIPSE,        \
               ClutterBehaviourEllipsePrivate))

enum
  {
    KNOT_REACHED,

    LAST_SIGNAL
  };

static guint ellipse_signals[LAST_SIGNAL] = { 0, };

enum
{
  PROP_0,

  PROP_CENTER,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_ANGLE_BEGIN,
  PROP_ANGLE_END,
  PROP_ANGLE_TILT,
};

struct _ClutterBehaviourEllipsePrivate
{
  /*
   * Ellipse center
   */
  ClutterKnot  center;
  gint         a;
  gint         b;
  ClutterAngle angle_begin;
  ClutterAngle angle_end;
  ClutterAngle angle_tilt;
};

static void 
clutter_behaviour_ellipse_finalize (GObject *object)
{
  G_OBJECT_CLASS (clutter_behaviour_ellipse_parent_class)->finalize (object);
}

static void
clutter_behaviour_ellipse_advance (ClutterBehaviourEllipse * e,
                                   ClutterAngle              angle,
                                   ClutterKnot             * knot)
{
  gint x = CFX_INT (e->priv->a * clutter_cosi (angle));
  gint y = CFX_INT (e->priv->b * clutter_sini (angle));

  if (e->priv->angle_tilt)
    {
      /* Problem: sqrti is not giving us sufficient precission here
       * and ClutterFixed range is too small to hold x^2+y^2 even for
       * reasonable x, y values.
       *
       * We take advantage of sqrt (a * b^2) == sqrt (a) * b,
       * and divide repeatedly by 4 until we get it into range, and then
       * multiply the result by 2 the same number of times.
       */
      ClutterFixed r;
      guint q = x*x + y*y;
      gint shift = 0;

      while (q > G_MAXINT16)
        {
          ++shift;
          
          q >>= 2;
        }

      r = clutter_sqrtx (CLUTTER_INT_TO_FIXED (q)) << shift;

      x = CFX_INT (CFX_MUL (r, clutter_cosi (angle + e->priv->angle_tilt)));
      y = CFX_INT (CFX_MUL (r, clutter_sini (angle + e->priv->angle_tilt)));
    }

  knot->x = x;
  knot->y = y;
  
#if 0
   g_debug ("advancing to angle %d [%d, %d] (a: %d, b: %d)", 
            angle, knot->x, knot->y, e->priv->a, e->priv->b);
#endif
}


static void
actor_apply_knot_foreach (ClutterBehaviour *behave,
                          ClutterActor     *actor,
                          gpointer          data)
{
  ClutterKnot *knot = data;
  clutter_actor_set_position (actor, knot->x, knot->y);
}

static void
clutter_behaviour_ellipse_alpha_notify (ClutterBehaviour * behave,
                                        guint32            alpha)
{
  ClutterKnot knot;
  ClutterBehaviourEllipse * self = CLUTTER_BEHAVIOUR_ELLIPSE (behave);
  ClutterAngle angle;

  if (self->priv->angle_end >= self->priv->angle_begin)
    {
      angle = self->priv->angle_end - self->priv->angle_begin;
      angle =
        (angle * alpha) / CLUTTER_ALPHA_MAX_ALPHA + self->priv->angle_begin;
    }
  else
    {
      angle = self->priv->angle_begin - self->priv->angle_end;
      angle =
        self->priv->angle_begin - (angle * alpha) / CLUTTER_ALPHA_MAX_ALPHA;
    }
    
  clutter_behaviour_ellipse_advance (self, angle, &knot);

  knot.x += self->priv->center.x;
  knot.y += self->priv->center.y;
  
  clutter_behaviour_actors_foreach (behave, 
                                    actor_apply_knot_foreach,
                                    &knot);

  g_signal_emit (behave, ellipse_signals[KNOT_REACHED], 0, &knot);
}

static void
clutter_behaviour_ellipse_set_property (GObject      *gobject,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ClutterBehaviourEllipse * el = CLUTTER_BEHAVIOUR_ELLIPSE (gobject);
  ClutterBehaviourEllipsePrivate *priv;

  priv = CLUTTER_BEHAVIOUR_ELLIPSE (gobject)->priv;

  switch (prop_id)
    {
    case PROP_ANGLE_BEGIN:
      priv->angle_begin = g_value_get_int (value);
      break;
    case PROP_ANGLE_END:
      priv->angle_end = g_value_get_int (value);
      break;
    case PROP_ANGLE_TILT:
      priv->angle_tilt = g_value_get_int (value);
      break;
    case PROP_WIDTH:
      priv->a = g_value_get_int (value) >> 1;
      break;
    case PROP_HEIGHT:
      priv->b = g_value_get_int (value) >> 1;
      break;
    case PROP_CENTER:
      clutter_behaviour_ellipse_set_center (el, g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_ellipse_get_property (GObject    *gobject,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ClutterBehaviourEllipsePrivate *priv;

  priv = CLUTTER_BEHAVIOUR_ELLIPSE (gobject)->priv;

  switch (prop_id)
    {
    case PROP_ANGLE_BEGIN:
      g_value_set_int (value, priv->angle_begin);
      break;
    case PROP_ANGLE_END:
      g_value_set_int (value, priv->angle_end);
      break;
    case PROP_ANGLE_TILT:
      g_value_set_int (value, priv->angle_tilt);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, 2 * priv->a);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, 2 * priv->b);
      break;
    case PROP_CENTER:
      g_value_set_boxed (value, &priv->center);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_ellipse_class_init (ClutterBehaviourEllipseClass *klass)
{
  GObjectClass          * object_class = G_OBJECT_CLASS (klass);
  ClutterBehaviourClass * behave_class = CLUTTER_BEHAVIOUR_CLASS (klass);

  object_class->finalize = clutter_behaviour_ellipse_finalize;
  object_class->set_property = clutter_behaviour_ellipse_set_property;
  object_class->get_property = clutter_behaviour_ellipse_get_property;

  behave_class->alpha_notify = clutter_behaviour_ellipse_alpha_notify;

  /**
   * ClutterBehaviourEllipse:angle-begin:
   *
   * The initial angle from where the rotation should begin.
   *
   * Since: 0.4
   */
  g_object_class_install_property (object_class,
                                   PROP_ANGLE_BEGIN,
                                   g_param_spec_int ("angle-begin",
                                                     "Angle Begin",
                                                     "Initial angle",
                                                     0, G_MAXINT, 0,
                                                     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterBehaviourEllipse:angle-end:
   *
   * The final angle to where the rotation should end.
   * 
   * Since: 0.4
   */
  g_object_class_install_property (object_class,
                                   PROP_ANGLE_END,
                                   g_param_spec_int ("angle-end",
                                                     "Angle End",
                                                     "Final angle",
                                                     0, G_MAXINT, 1024,
                                                     CLUTTER_PARAM_READWRITE));

  /**
   * ClutterBehaviourEllipse:angle-end:
   *
   * The final angle to where the rotation should end.
   * 
   * Since: 0.4
   */
  g_object_class_install_property (object_class,
                                   PROP_ANGLE_TILT,
                                   g_param_spec_int ("angle-tilt",
                                                     "Angle Tilt",
                                                     "Tilt of the ellipse",
                                                     0, G_MAXINT, 1024,
                                                     CLUTTER_PARAM_READWRITE));
  
  /**
   * ClutterBehaviourEllipse:width:
   *
   * Width of the ellipse.
   * 
   * Since: 0.4
   */
  g_object_class_install_property (object_class,
                                   PROP_WIDTH,
                                   g_param_spec_int ("width",
                                                     "Width",
                                                     "Width of ellipse",
                                                     0, G_MAXINT, 100,
                                                     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterBehaviourEllipse:height:
   *
   * Height of the ellipse.
   * 
   * Since: 0.4
   */
  g_object_class_install_property (object_class,
                                   PROP_HEIGHT,
                                   g_param_spec_int ("height",
                                                     "Height",
                                                     "Height of ellipse",
                                                     0, G_MAXINT, 50,
                                                     CLUTTER_PARAM_READWRITE));
  /**
   * ClutterBehaviourEllipse:center:
   *
   * The center of the ellipse.
   *
   * Since: 0.4
   */
  g_object_class_install_property (object_class,
                                   PROP_CENTER,
                                   g_param_spec_boxed ("center",
                                                       "Center",
                                                       "Center of ellipse",
                                                       CLUTTER_TYPE_KNOT,
                                                       CLUTTER_PARAM_READWRITE));
  
  /**
   * ClutterBehaviourEllipse::knot-reached:
   * @ellipse: the object which received the signal
   * @knot: the #ClutterKnot reached
   *
   * This signal is emitted at the end of each frame.
   *
   * Since: 0.4
   */
  ellipse_signals[KNOT_REACHED] =
    g_signal_new ("knot-reached",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterBehaviourEllipseClass, knot_reached),
                  NULL, NULL,
                  clutter_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_KNOT);
  
  g_type_class_add_private (klass, sizeof (ClutterBehaviourEllipsePrivate));
}

static void
clutter_behaviour_ellipse_init (ClutterBehaviourEllipse * self)
{
  ClutterBehaviourEllipsePrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_ELLIPSE_GET_PRIVATE (self);
}

/**
 * clutter_behaviour_ellipse_new:
 * @alpha: a #ClutterAlpha, or %NULL
 * @center: center of the ellipse as #ClutterKnot
 * @width: width of the ellipse
 * @height: height of the ellipse
 * @begin: #ClutterAngle at which movement begins
 * @end: #ClutterAngle at which movement ends
 * @tilt: #ClutterAngle with which the ellipse should be tilted around its
 * center
 *
 * Creates a behaviour that drives actors along an elliptical path with
 * given center, width and height; the movement begins at angle_begin and
 * ends at angle_end; if angle_end > angle_begin, the movement is in
 * counter-clockwise direction, clockwise other wise.
 *
 * Return value: a #ClutterBehaviour
 *
 * Since: 0.4
 */
ClutterBehaviour *
clutter_behaviour_ellipse_new (ClutterAlpha          * alpha,
                               ClutterKnot           * center,
                               gint                    width,
                               gint                    height,
                               ClutterAngle            begin,
                               ClutterAngle            end,
                               ClutterAngle            tilt)
{
  ClutterBehaviourEllipse *bc;

  g_return_val_if_fail (alpha == NULL || CLUTTER_IS_ALPHA (alpha), NULL);
     
  bc = g_object_new (CLUTTER_TYPE_BEHAVIOUR_ELLIPSE, 
                     "alpha", alpha,
                     "center", center,
                     "width", width,
                     "height", height,
                     "angle-begin", begin,
                     "angle-end", end,
                     "angle-tilt", tilt,
                     NULL);

  return CLUTTER_BEHAVIOUR (bc);
}

/**
 * clutter_behaviour_ellipse_set_center
 * @self: a #ClutterBehaviourEllipse
 * @knot: a #ClutterKnot center for the ellipse
 *
 * Sets the center of the elliptical path to the point represented by knot.
 * 
 * Since: 0.4
 */
void
clutter_behaviour_ellipse_set_center (ClutterBehaviourEllipse * self,
                                      ClutterKnot             * knot)
{
  if (self->priv->center.x != knot->x || self->priv->center.y != knot->y)
    {
      g_object_ref (self);
      self->priv->center = *knot;
      g_object_notify (G_OBJECT (self), "center");
      g_object_unref (self);
    }
}

/**
 * clutter_behaviour_ellipse_get_center
 * @self: a #ClutterBehaviourEllipse
 * @knot: a #ClutterKnot where to store the center of the ellipse
 *
 * Gets the center of the elliptical path path.
 * 
 * Since: 0.4
 */
void
clutter_behaviour_ellipse_get_center (ClutterBehaviourEllipse  * self,
                                      ClutterKnot              * knot)
{
  *knot = self->priv->center;
}


/**
 * clutter_behaviour_ellipse_set_width
 * @self: a #ClutterBehaviourEllipse
 * @width: width of the ellipse
 *
 * Sets the width of the elliptical path.
 * 
 * Since: 0.4
 */
void
clutter_behaviour_ellipse_set_width (ClutterBehaviourEllipse * self,
                                     gint                      width)
{
  if (self->priv->a != width >> 1)
    {
      g_object_ref (self);
      self->priv->a = width >> 1;
      g_object_notify (G_OBJECT (self), "width");
      g_object_unref (self);
    }
}

/**
 * clutter_behaviour_ellipse_get_width
 * @self: a #ClutterBehaviourEllipse
 *
 * Gets the width of the elliptical path.
 * 
 * Since: 0.4
 */
gint
clutter_behaviour_ellipse_get_width (ClutterBehaviourEllipse  * self)
{
  return self->priv->a << 1;
}

/**
 * clutter_behaviour_ellipse_set_height
 * @self: a #ClutterBehaviourEllipse
 * @height: height of the ellipse
 *
 * Sets the height of the elliptical path.
 * 
 * Since: 0.4
 */
void
clutter_behaviour_ellipse_set_height (ClutterBehaviourEllipse * self,
                                      gint                      height)
{
  if (self->priv->b != height >> 1)
    {
      g_object_ref (self);
      self->priv->b = height >> 1;
      g_object_notify (G_OBJECT (self), "height");
      g_object_unref (self);
    }
}

/**
 * clutter_behaviour_ellipse_get_height
 * @self: a #ClutterBehaviourEllipse
 *
 * Gets the height of the elliptical path.
 * 
 * Since: 0.4
 */
gint
clutter_behaviour_ellipse_get_height (ClutterBehaviourEllipse  * self)
{
  return self->priv->b << 1;
}


/**
 * clutter_behaviour_ellipse_set_angle_begin
 * @self: a #ClutterBehaviourEllipse
 * @angle_begin: #ClutterAngle at which movement begins
 *
 * Sets the angle at which movement begins.
 * 
 * Since: 0.4
 */
void
clutter_behaviour_ellipse_set_angle_begin (ClutterBehaviourEllipse * self,
                                           ClutterAngle            angle_begin)
{
  if (self->priv->angle_begin != angle_begin)
    {
      g_object_ref (self);
      self->priv->angle_begin = angle_begin;
      g_object_notify (G_OBJECT (self), "angle-begin");
      g_object_unref (self);
    }
}

/**
 * clutter_behaviour_ellipse_get_angle_begin
 * @self: a #ClutterBehaviourEllipse
 *
 * Gets the at which movements begins.
 * 
 * Since: 0.4
 */
ClutterAngle
clutter_behaviour_ellipse_get_angle_begin (ClutterBehaviourEllipse  * self)
{
  return self->priv->angle_begin;
}


/**
 * clutter_behaviour_ellipse_set_angle_end
 * @self: a #ClutterBehaviourEllipse
 * @angle_end: #ClutterAngle at which movement ends
 *
 * Sets the angle at which movement ends.
 * 
 * Since: 0.4
 */
void
clutter_behaviour_ellipse_set_angle_end (ClutterBehaviourEllipse * self,
                                         ClutterAngle              angle_end)
{
  if (self->priv->angle_end != angle_end)
    {
      g_object_ref (self);
      self->priv->angle_end = angle_end;
      g_object_notify (G_OBJECT (self), "angle-end");
      g_object_unref (self);
    }
}

/**
 * clutter_behaviour_ellipse_get_angle_end
 * @self: a #ClutterBehaviourEllipse
 *
 * Gets the at which movements ends.
 * 
 * Since: 0.4
 */
ClutterAngle
clutter_behaviour_ellipse_get_angle_end (ClutterBehaviourEllipse  * self)
{
  return self->priv->angle_end;
}


/**
 * clutter_behaviour_ellipse_set_angle_tilt
 * @self: a #ClutterBehaviourEllipse
 * @angle_end: #ClutterAngle tilt of the elipse around the center
 *
 * Sets the angle at which the ellipse should be tilted around it's center.
 * 
 * Since: 0.4
 */
void
clutter_behaviour_ellipse_set_angle_tilt (ClutterBehaviourEllipse * self,
                                          ClutterAngle              angle_tilt)
{
  if (self->priv->angle_tilt != angle_tilt)
    {
      g_object_ref (self);
      self->priv->angle_tilt = angle_tilt;
      g_object_notify (G_OBJECT (self), "angle-tilt");
      g_object_unref (self);
    }
}

/**
 * clutter_behaviour_ellipse_get_angle_tilt
 * @self: a #ClutterBehaviourEllipse
 *
 * Gets the tilt of the ellipse around the center.
 * 
 * Since: 0.4
 */
ClutterAngle
clutter_behaviour_ellipse_get_angle_tilt (ClutterBehaviourEllipse  * self)
{
  return self->priv->angle_tilt;
}

