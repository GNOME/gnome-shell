/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012  Intel Corporation.
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
 *
 * Author:
 *   Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
 */

/**
 * SECTION:clutter-rotate-action
 * @Title: ClutterRotateAction
 * @Short_Description: Action to rotate an actor
 *
 * #ClutterRotateAction is a sub-class of #ClutterGestureAction that implements
 * the logic for recognizing rotate gestures using two touch points.
 *
 * Since: 1.12
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "clutter-rotate-action.h"

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-gesture-action-private.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

struct _ClutterRotateActionPrivate
{
  gfloat initial_vector[2];
  gdouble initial_vector_norm;
  gdouble initial_rotation;
};

enum
{
  ROTATE,

  LAST_SIGNAL
};

static guint rotate_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (ClutterRotateAction, clutter_rotate_action, CLUTTER_TYPE_GESTURE_ACTION)

static gboolean
clutter_rotate_action_real_rotate (ClutterRotateAction *action,
                                   ClutterActor        *actor,
                                   gdouble              angle)
{
  clutter_actor_set_rotation_angle (actor,
                                    CLUTTER_Z_AXIS,
                                    action->priv->initial_rotation + angle);

  return TRUE;
}

static gboolean
clutter_rotate_action_gesture_begin (ClutterGestureAction  *action,
                                     ClutterActor          *actor)
{
  ClutterRotateActionPrivate *priv = CLUTTER_ROTATE_ACTION (action)->priv;
  gfloat p1[2], p2[2];

  /* capture initial vector */
  clutter_gesture_action_get_motion_coords (action, 0, &p1[0], &p1[1]);
  clutter_gesture_action_get_motion_coords (action, 1, &p2[0], &p2[1]);

  priv->initial_vector[0] = p2[0] - p1[0];
  priv->initial_vector[1] = p2[1] - p1[1];

  priv->initial_vector_norm =
    sqrt (priv->initial_vector[0] * priv->initial_vector[0] +
          priv->initial_vector[1] * priv->initial_vector[1]);

  priv->initial_rotation = clutter_actor_get_rotation_angle (actor, CLUTTER_Z_AXIS);

  return TRUE;
}

static gboolean
clutter_rotate_action_gesture_progress (ClutterGestureAction *action,
                                        ClutterActor         *actor)
{
  ClutterRotateActionPrivate *priv = CLUTTER_ROTATE_ACTION (action)->priv;
  gfloat p1[2], p2[2];
  gfloat vector[2];
  gboolean retval;

  /* capture current vector */
  clutter_gesture_action_get_motion_coords (action, 0, &p1[0], &p1[1]);
  clutter_gesture_action_get_motion_coords (action, 1, &p2[0], &p2[1]);

  vector[0] = p2[0] - p1[0];
  vector[1] = p2[1] - p1[1];

  if ((vector[0] == priv->initial_vector[0]) &&
      (vector[1] == priv->initial_vector[1]))
    {
      g_signal_emit (action, rotate_signals[ROTATE], 0,
                     actor, (gdouble) 0.0,
                     &retval);
    }
  else
    {
      gfloat mult[2];
      gfloat norm;
      gdouble angle;

      /* Computes angle between the 2 initial touch points and the
         current position of the 2 touch points. */
      norm = sqrt (vector[0] * vector[0] + vector[1] * vector[1]);
      norm = (priv->initial_vector[0] * vector[0] +
              priv->initial_vector[1] * vector[1]) / (priv->initial_vector_norm * norm);

      if ((norm >= -1.0) && (norm <= 1.0))
        angle = acos (norm);
      else
        angle = 0;

      /* The angle given is comprise between 0 and 180 degrees, we
         need some logic on top to get a value between 0 and 360. */
      mult[0] = priv->initial_vector[0] * vector[1] -
        priv->initial_vector[1] * vector[0];
      mult[1] = priv->initial_vector[1] * vector[0] -
        priv->initial_vector[0] * vector[1];

      if (mult[0] < 0)
        angle = -angle;

      /* Convert radians to degrees */
      angle = angle * 180.0 / G_PI;

      g_signal_emit (action, rotate_signals[ROTATE], 0,
                     actor, angle,
                     &retval);
    }

  return TRUE;
}

static void
clutter_rotate_action_gesture_cancel (ClutterGestureAction *action,
                                      ClutterActor         *actor)
{
  gboolean retval;

  g_signal_emit (action, rotate_signals[ROTATE], 0,
                 actor, (gdouble) 0.0,
                 &retval);
}

static void
clutter_rotate_action_constructed (GObject *gobject)
{
  ClutterGestureAction *gesture;

  gesture = CLUTTER_GESTURE_ACTION (gobject);
  clutter_gesture_action_set_threshold_trigger_edge (gesture, CLUTTER_GESTURE_TRIGGER_EDGE_NONE);
}

static void
clutter_rotate_action_class_init (ClutterRotateActionClass *klass)
{
  ClutterGestureActionClass *gesture_class =
    CLUTTER_GESTURE_ACTION_CLASS (klass);
  GObjectClass *object_class =
    G_OBJECT_CLASS (klass);

  klass->rotate = clutter_rotate_action_real_rotate;

  object_class->constructed = clutter_rotate_action_constructed;

  gesture_class->gesture_begin = clutter_rotate_action_gesture_begin;
  gesture_class->gesture_progress = clutter_rotate_action_gesture_progress;
  gesture_class->gesture_cancel = clutter_rotate_action_gesture_cancel;

  /**
   * ClutterRotateAction::rotate:
   * @action: the #ClutterRotateAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   * @angle: the difference of angle of rotation between the initial
   * rotation and the current rotation
   *
   * The ::rotate signal is emitted when a rotate gesture is
   * recognized on the attached actor and when the gesture is
   * cancelled (in this case with an angle value of 0).
   *
   * Return value: %TRUE if the rotation should continue, and %FALSE if
   *   the rotation should be cancelled.
   *
   * Since: 1.12
   */
  rotate_signals[ROTATE] =
    g_signal_new (I_("rotate"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterRotateActionClass, rotate),
                  _clutter_boolean_continue_accumulator, NULL,
                  _clutter_marshal_BOOLEAN__OBJECT_DOUBLE,
                  G_TYPE_BOOLEAN, 2,
                  CLUTTER_TYPE_ACTOR,
                  G_TYPE_DOUBLE);
}

static void
clutter_rotate_action_init (ClutterRotateAction *self)
{
  ClutterGestureAction *gesture;

  self->priv = clutter_rotate_action_get_instance_private (self);

  gesture = CLUTTER_GESTURE_ACTION (self);
  clutter_gesture_action_set_n_touch_points (gesture, 2);
}

/**
 * clutter_rotate_action_new:
 *
 * Creates a new #ClutterRotateAction instance
 *
 * Return value: the newly created #ClutterRotateAction
 *
 * Since: 1.12
 */
ClutterAction *
clutter_rotate_action_new (void)
{
  return g_object_new (CLUTTER_TYPE_ROTATE_ACTION, NULL);
}
