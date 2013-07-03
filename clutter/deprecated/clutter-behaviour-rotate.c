/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:clutter-behaviour-rotate
 * @short_description: A behaviour controlling rotation
 *
 * A #ClutterBehaviourRotate rotate actors between a starting and ending
 * angle on a given axis.
 *
 * The #ClutterBehaviourRotate is available since version 0.4.
 *
 * Deprecated: 1.6: Use the #ClutterActor rotation properties and
 *   clutter_actor_animate(), or #ClutterAnimator, or #ClutterState
 *   instead.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include "deprecated/clutter-actor.h"

#include "clutter-alpha.h"
#include "clutter-behaviour.h"
#include "clutter-behaviour-rotate.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"
#include "clutter-private.h"

struct _ClutterBehaviourRotatePrivate
{
  gdouble angle_start;
  gdouble angle_end;

  ClutterRotateAxis axis;
  ClutterRotateDirection direction;

  gint center_x;
  gint center_y;
  gint center_z;
};

enum
{
  PROP_0,

  PROP_ANGLE_START,
  PROP_ANGLE_END,
  PROP_AXIS,
  PROP_DIRECTION,
  PROP_CENTER_X,
  PROP_CENTER_Y,
  PROP_CENTER_Z,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE_WITH_PRIVATE (ClutterBehaviourRotate,
                            clutter_behaviour_rotate,
                            CLUTTER_TYPE_BEHAVIOUR)

typedef struct {
  gdouble angle;
} RotateFrameClosure;

static void
alpha_notify_foreach (ClutterBehaviour *behaviour,
		      ClutterActor     *actor,
		      gpointer          data)
{
  RotateFrameClosure *closure = data;
  ClutterBehaviourRotate *rotate_behaviour;
  ClutterBehaviourRotatePrivate *priv;

  rotate_behaviour = CLUTTER_BEHAVIOUR_ROTATE (behaviour);
  priv = rotate_behaviour->priv;

  clutter_actor_set_rotation (actor, priv->axis,
                              closure->angle,
                              priv->center_x,
                              priv->center_y,
                              priv->center_z);
}

static inline float
clamp_angle (float a)
{
  float a1, a2;
  gint rounds;

  rounds = a / 360.0;
  a1 = rounds * 360.0;
  a2 = a - a1;

  return a2;
}

static void
clutter_behaviour_rotate_alpha_notify (ClutterBehaviour *behaviour,
                                       gdouble           alpha_value)
{
  ClutterBehaviourRotate *rotate_behaviour;
  ClutterBehaviourRotatePrivate *priv;
  RotateFrameClosure closure;
  gdouble start, end;

  rotate_behaviour = CLUTTER_BEHAVIOUR_ROTATE (behaviour);
  priv = rotate_behaviour->priv;

  closure.angle = 0;
  start         = priv->angle_start;
  end           = priv->angle_end;

  if (priv->direction == CLUTTER_ROTATE_CW && start >= end)
    end += 360.0;
  else if (priv->direction == CLUTTER_ROTATE_CCW && start <= end)
    end -= 360.0;

  closure.angle = (end - start) * alpha_value + start;

  clutter_behaviour_actors_foreach (behaviour,
				    alpha_notify_foreach,
				    &closure);
}

static void
clutter_behaviour_rotate_set_property (GObject      *gobject,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ClutterBehaviourRotate *rotate;
  ClutterBehaviourRotatePrivate *priv;

  rotate = CLUTTER_BEHAVIOUR_ROTATE (gobject);
  priv = rotate->priv;

  switch (prop_id)
    {
    case PROP_ANGLE_START:
      priv->angle_start = g_value_get_double (value);
      break;

    case PROP_ANGLE_END:
      priv->angle_end = g_value_get_double (value);
      break;

    case PROP_AXIS:
      priv->axis = g_value_get_enum (value);
      break;

    case PROP_DIRECTION:
      priv->direction = g_value_get_enum (value);
      break;

    case PROP_CENTER_X:
      clutter_behaviour_rotate_set_center (rotate,
					   g_value_get_int (value),
					   priv->center_y,
					   priv->center_z);
      break;

    case PROP_CENTER_Y:
      clutter_behaviour_rotate_set_center (rotate,
					   priv->center_x,
					   g_value_get_int (value),
					   priv->center_z);
      break;

    case PROP_CENTER_Z:
      clutter_behaviour_rotate_set_center (rotate,
					   priv->center_x,
					   priv->center_y,
					   g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_rotate_get_property (GObject    *gobject,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ClutterBehaviourRotatePrivate *priv;

  priv = CLUTTER_BEHAVIOUR_ROTATE (gobject)->priv;

  switch (prop_id)
    {
    case PROP_ANGLE_START:
      g_value_set_double (value, priv->angle_start);
      break;

    case PROP_ANGLE_END:
      g_value_set_double (value, priv->angle_end);
      break;

    case PROP_AXIS:
      g_value_set_enum (value, priv->axis);
      break;

    case PROP_DIRECTION:
      g_value_set_enum (value, priv->direction);
      break;

    case PROP_CENTER_X:
      g_value_set_int (value, priv->center_x);
      break;

    case PROP_CENTER_Y:
      g_value_set_int (value, priv->center_y);
      break;

    case PROP_CENTER_Z:
      g_value_set_int (value, priv->center_z);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_rotate_class_init (ClutterBehaviourRotateClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBehaviourClass *behaviour_class = CLUTTER_BEHAVIOUR_CLASS (klass);
  GParamSpec *pspec = NULL;

  gobject_class->set_property = clutter_behaviour_rotate_set_property;
  gobject_class->get_property = clutter_behaviour_rotate_get_property;

  behaviour_class->alpha_notify = clutter_behaviour_rotate_alpha_notify;

  /**
   * ClutterBehaviourRotate:angle-start:
   *
   * The initial angle from whence the rotation should start.
   *
   * Since: 0.4
   */
  pspec = g_param_spec_double ("angle-start",
                               P_("Angle Begin"),
                               P_("Initial angle"),
                               0.0, 360.0,
                               0.0,
                               CLUTTER_PARAM_READWRITE);
  obj_props[PROP_ANGLE_START] = pspec;
  g_object_class_install_property (gobject_class,
                                   PROP_ANGLE_START,
                                   pspec);

  /**
   * ClutterBehaviourRotate:angle-end:
   *
   * The final angle to where the rotation should end.
   *
   * Since: 0.4
   */
  pspec = g_param_spec_double ("angle-end",
                               P_("Angle End"),
                               P_("Final angle"),
                               0.0, 360.0,
                               0.0,
                               CLUTTER_PARAM_READWRITE);
  obj_props[PROP_ANGLE_END] = pspec;
  g_object_class_install_property (gobject_class,
                                   PROP_ANGLE_END,
                                   pspec);

  /**
   * ClutterBehaviourRotate:axis:
   *
   * The axis of rotation.
   *
   * Since: 0.4
   */
  pspec = g_param_spec_enum ("axis",
                             P_("Axis"),
                             P_("Axis of rotation"),
                             CLUTTER_TYPE_ROTATE_AXIS,
                             CLUTTER_Z_AXIS,
                             CLUTTER_PARAM_READWRITE);
  obj_props[PROP_AXIS] = pspec;
  g_object_class_install_property (gobject_class,
                                   PROP_AXIS,
                                   pspec);

  /**
   * ClutterBehaviourRotate:direction:
   *
   * The direction of the rotation.
   *
   * Since: 0.4
   */
  pspec = g_param_spec_enum ("direction",
                             P_("Direction"),
                             P_("Direction of rotation"),
                             CLUTTER_TYPE_ROTATE_DIRECTION,
                             CLUTTER_ROTATE_CW,
                             CLUTTER_PARAM_READWRITE);
  obj_props[PROP_DIRECTION] = pspec;
  g_object_class_install_property (gobject_class,
                                   PROP_DIRECTION,
                                   pspec);

  /**
   * ClutterBehaviourRotate:center-x:
   *
   * The x center of rotation.
   *
   * Since: 0.4
   */
  pspec = g_param_spec_int ("center-x",
                            P_("Center X"),
                            P_("X coordinate of the center of rotation"),
                            -G_MAXINT, G_MAXINT,
                            0,
                            CLUTTER_PARAM_READWRITE);
  obj_props[PROP_CENTER_X] = pspec;
  g_object_class_install_property (gobject_class,
                                   PROP_CENTER_X,
                                   pspec);

  /**
   * ClutterBehaviourRotate:center-y:
   *
   * The y center of rotation.
   *
   * Since: 0.4
   */
  pspec = g_param_spec_int ("center-y",
                            P_("Center Y"),
                            P_("Y coordinate of the center of rotation"),
                            -G_MAXINT, G_MAXINT,
                            0,
                            CLUTTER_PARAM_READWRITE);
  obj_props[PROP_CENTER_Y] = pspec;
  g_object_class_install_property (gobject_class,
                                   PROP_CENTER_Y,
                                   pspec);

  /**
   * ClutterBehaviourRotate:center-z:
   *
   * The z center of rotation.
   *
   * Since: 0.4
   */
  pspec = g_param_spec_int ("center-z",
                            P_("Center Z"),
                            P_("Z coordinate of the center of rotation"),
                            -G_MAXINT, G_MAXINT,
                            0,
                            CLUTTER_PARAM_READWRITE);
  obj_props[PROP_CENTER_Z] = pspec;
  g_object_class_install_property (gobject_class,
                                   PROP_CENTER_Z,
                                   pspec);
}

static void
clutter_behaviour_rotate_init (ClutterBehaviourRotate *self)
{
  self->priv = clutter_behaviour_rotate_get_instance_private (self);

  self->priv->angle_start = 0.0;
  self->priv->angle_end = 0.0;

  self->priv->axis = CLUTTER_Z_AXIS;
  self->priv->direction = CLUTTER_ROTATE_CW;

  self->priv->center_x = 0;
  self->priv->center_y = 0;
  self->priv->center_z = 0;
}

/**
 * clutter_behaviour_rotate_new:
 * @alpha: (allow-none): a #ClutterAlpha instance, or %NULL
 * @axis: the rotation axis
 * @direction: the rotation direction
 * @angle_start: the starting angle in degrees, between 0 and 360.
 * @angle_end: the final angle in degrees, between 0 and 360.
 *
 * Creates a new #ClutterBehaviourRotate. This behaviour will rotate actors
 * bound to it on @axis, following @direction, between @angle_start and
 * @angle_end. Angles >= 360 degrees will be clamped to the canonical interval
 * <0, 360), if angle_start == angle_end, the behaviour will carry out a
 * single rotation of 360 degrees.
 *
 * If @alpha is not %NULL, the #ClutterBehaviour will take ownership
 * of the #ClutterAlpha instance. In the case when @alpha is %NULL,
 * it can be set later with clutter_behaviour_set_alpha().
 *
 * Return value: the newly created #ClutterBehaviourRotate.
 *
 * Since: 0.4
 */
ClutterBehaviour *
clutter_behaviour_rotate_new (ClutterAlpha           *alpha,
                              ClutterRotateAxis       axis,
                              ClutterRotateDirection  direction,
                              gdouble                 angle_start,
                              gdouble                 angle_end)
{
  g_return_val_if_fail (alpha == NULL || CLUTTER_IS_ALPHA (alpha), NULL);

  return g_object_new (CLUTTER_TYPE_BEHAVIOUR_ROTATE,
                       "alpha", alpha,
                       "axis", axis,
                       "direction", direction,
                       "angle-start", angle_start,
                       "angle-end", angle_end,
                       NULL);
}

/**
 * clutter_behaviour_rotate_get_axis:
 * @rotate: a #ClutterBehaviourRotate
 *
 * Retrieves the #ClutterRotateAxis used by the rotate behaviour.
 *
 * Return value: the rotation axis
 *
 * Since: 0.4
 */
ClutterRotateAxis
clutter_behaviour_rotate_get_axis (ClutterBehaviourRotate *rotate)
{
  g_return_val_if_fail (CLUTTER_IS_BEHAVIOUR_ROTATE (rotate), CLUTTER_Z_AXIS);

  return rotate->priv->axis;
}

/**
 * clutter_behaviour_rotate_set_axis:
 * @rotate: a #ClutterBehaviourRotate
 * @axis: a #ClutterRotateAxis
 *
 * Sets the axis used by the rotate behaviour.
 *
 * Since: 0.4
 */
void
clutter_behaviour_rotate_set_axis (ClutterBehaviourRotate *rotate,
                                   ClutterRotateAxis       axis)
{
  ClutterBehaviourRotatePrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_ROTATE (rotate));

  priv = rotate->priv;

  if (priv->axis != axis)
    {
      priv->axis = axis;

      g_object_notify_by_pspec (G_OBJECT (rotate), obj_props[PROP_AXIS]);
    }
}

/**
 * clutter_behaviour_rotate_get_direction:
 * @rotate: a #ClutterBehaviourRotate
 *
 * Retrieves the #ClutterRotateDirection used by the rotate behaviour.
 *
 * Return value: the rotation direction
 *
 * Since: 0.4
 */
ClutterRotateDirection
clutter_behaviour_rotate_get_direction (ClutterBehaviourRotate *rotate)
{
  g_return_val_if_fail (CLUTTER_IS_BEHAVIOUR_ROTATE (rotate),
                        CLUTTER_ROTATE_CW);

  return rotate->priv->direction;
}

/**
 * clutter_behaviour_rotate_set_direction:
 * @rotate: a #ClutterBehaviourRotate
 * @direction: the rotation direction
 *
 * Sets the rotation direction used by the rotate behaviour.
 *
 * Since: 0.4
 */
void
clutter_behaviour_rotate_set_direction (ClutterBehaviourRotate *rotate,
                                        ClutterRotateDirection  direction)
{
  ClutterBehaviourRotatePrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_ROTATE (rotate));

  priv = rotate->priv;

  if (priv->direction != direction)
    {
      priv->direction = direction;

      g_object_notify_by_pspec (G_OBJECT (rotate), obj_props[PROP_DIRECTION]);
    }
}

/**
 * clutter_behaviour_rotate_get_bounds:
 * @rotate: a #ClutterBehaviourRotate
 * @angle_start: (out): return value for the initial angle
 * @angle_end: (out): return value for the final angle
 *
 * Retrieves the rotation boundaries of the rotate behaviour.
 *
 * Since: 0.4
 */
void
clutter_behaviour_rotate_get_bounds (ClutterBehaviourRotate *rotate,
                                     gdouble                *angle_start,
                                     gdouble                *angle_end)
{
  ClutterBehaviourRotatePrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_ROTATE (rotate));

  priv = rotate->priv;

  if (angle_start)
    *angle_start = priv->angle_start;

  if (angle_end)
    *angle_end = priv->angle_end;
}

/**
 * clutter_behaviour_rotate_set_bounds:
 * @rotate: a #ClutterBehaviourRotate
 * @angle_start: initial angle in degrees, between 0 and 360.
 * @angle_end: final angle in degrees, between 0 and 360.
 *
 * Sets the initial and final angles of a rotation behaviour; angles >= 360
 * degrees get clamped to the canonical interval <0, 360).
 *
 * Since: 0.4
 */
void
clutter_behaviour_rotate_set_bounds (ClutterBehaviourRotate *rotate,
                                     gdouble                 angle_start,
                                     gdouble                 angle_end)
{
  ClutterBehaviourRotatePrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_ROTATE (rotate));

  priv = rotate->priv;

  g_object_freeze_notify (G_OBJECT (rotate));

  if (priv->angle_start != angle_start)
    {
      priv->angle_start = clamp_angle (angle_start);

      g_object_notify_by_pspec (G_OBJECT (rotate), obj_props[PROP_ANGLE_START]);
    }

  if (priv->angle_end != angle_end)
    {
      priv->angle_end = clamp_angle (angle_end);

      g_object_notify_by_pspec (G_OBJECT (rotate), obj_props[PROP_ANGLE_END]);
    }

  g_object_thaw_notify (G_OBJECT (rotate));
}

/**
 * clutter_behaviour_rotate_set_center:
 * @rotate: a #ClutterBehaviourRotate
 * @x: X axis center of rotation
 * @y: Y axis center of rotation
 * @z: Z axis center of rotation
 *
 * Sets the center of rotation. The coordinates are relative to the plane
 * normal to the rotation axis set with clutter_behaviour_rotate_set_axis().
 *
 * Since: 0.4
 */
void
clutter_behaviour_rotate_set_center (ClutterBehaviourRotate *rotate,
				     gint                    x,
				     gint                    y,
				     gint                    z)
{
  ClutterBehaviourRotatePrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_ROTATE (rotate));

  priv = rotate->priv;

  g_object_freeze_notify (G_OBJECT (rotate));

  if (priv->center_x != x)
    {
      priv->center_x = x;
      g_object_notify_by_pspec (G_OBJECT (rotate), obj_props[PROP_CENTER_X]);
    }

  if (priv->center_y != y)
    {
      priv->center_y = y;
      g_object_notify_by_pspec (G_OBJECT (rotate), obj_props[PROP_CENTER_Y]);
    }

  if (priv->center_z != z)
    {
      priv->center_z = z;
      g_object_notify_by_pspec (G_OBJECT (rotate), obj_props[PROP_CENTER_Z]);
    }

  g_object_thaw_notify (G_OBJECT (rotate));
}

/**
 * clutter_behaviour_rotate_get_center:
 * @rotate: a #ClutterBehaviourRotate
 * @x: (out): return location for the X center of rotation
 * @y: (out): return location for the Y center of rotation
 * @z: (out): return location for the Z center of rotation
 *
 * Retrieves the center of rotation set using
 * clutter_behaviour_rotate_set_center().
 *
 * Since: 0.4
 */
void
clutter_behaviour_rotate_get_center (ClutterBehaviourRotate *rotate,
				     gint                   *x,
				     gint                   *y,
				     gint                   *z)
{
  ClutterBehaviourRotatePrivate *priv;

  g_return_if_fail (CLUTTER_IS_BEHAVIOUR_ROTATE (rotate));

  priv = rotate->priv;

  if (x)
    *x = priv->center_x;

  if (y)
    *y = priv->center_y;

  if (z)
    *z = priv->center_z;
}
