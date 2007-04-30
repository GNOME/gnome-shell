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
  PROP_DIRECTION,
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
  ClutterRotateDirection direction;
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
  knot->x = CFX_INT (e->priv->a * clutter_cosi (angle));
  knot->y = CFX_INT (e->priv->b * clutter_sini (angle));

/*   g_debug ("advancing to angle %d [%d, %d] (a: %d, b: %d)", */
/*            angle, knot->x, knot->y, e->priv->a, e->priv->b); */
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

  if (self->priv->direction == CLUTTER_ROTATE_CW)
    {
      angle = self->priv->angle_end - self->priv->angle_begin;
      angle = (angle * alpha) / CLUTTER_ALPHA_MAX_ALPHA +
        self->priv->angle_begin;
    }
  else
    {
      angle = 1024 - (self->priv->angle_end - self->priv->angle_begin);
      angle = self->priv->angle_begin - (angle * alpha) / CLUTTER_ALPHA_MAX_ALPHA;
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
    case PROP_WIDTH:
      priv->a = g_value_get_int (value) / 2;
      break;
    case PROP_HEIGHT:
      priv->b = g_value_get_int (value) / 2;
      break;
    case PROP_DIRECTION:
      priv->direction = g_value_get_enum (value);
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
    case PROP_WIDTH:
      g_value_set_int (value, 2 * priv->a);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, 2 * priv->b);
      break;
    case PROP_DIRECTION:
      g_value_set_enum (value, priv->direction);
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
   * The initial angle from whence the rotation should begin.
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
   * ClutterBehaviourEllipse:direction:
   *
   * The direction of the rotation.
   *
   * Since: 0.4
   */
  g_object_class_install_property (object_class,
                                   PROP_DIRECTION,
                                   g_param_spec_enum ("direction",
                                                      "Direction",
                                                      "Direction of rotation",
                                                      CLUTTER_TYPE_ROTATE_DIRECTION,
                                                      CLUTTER_ROTATE_CW,
                                                      CLUTTER_PARAM_READWRITE));
  
  /**
   * ClutterBehaviourEllipse::knot-reached:
   * @pathb: the object which received the signal
   * @knot: the #ClutterKnot reached
   *
   * This signal is emitted at the end of each frame.
   *
   * Since: 0.2
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
 * @center: center of the ellipse
 * @width: width of the ellipse
 * @height: heigh of the ellipse
 *
 * Creates a behaviour that drives actors along the elliptical path.
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
                               ClutterRotateDirection  dir)
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
                     "direction", dir,
                     NULL);

  return CLUTTER_BEHAVIOUR (bc);
}

/**
 * clutter_behaviour_ellipse_set_center
 * @self: a #ClutterBehaviourEllipse
 * @knot: a #ClutterKnot center for the bezier
 *
 * Sets the center of the ellipse path to the point represented by knot.
 * 
 * Since: 0.4
 */
void
clutter_behaviour_ellipse_set_center (ClutterBehaviourEllipse * self,
                                      ClutterKnot             * knot)
{
  self->priv->center = *knot;
}

/**
 * clutter_behaviour_ellipse_get_center
 * @self: a #ClutterBehaviourEllipse
 * @knot: a #ClutterKnot where to store the center of the ellipse
 *
 * Gets the center of the ellipse path.
 * 
 * Since: 0.4
 */
void
clutter_behaviour_ellipse_get_center (ClutterBehaviourEllipse  * self,
                                      ClutterKnot              * knot)
{
  *knot = self->priv->center;
}

