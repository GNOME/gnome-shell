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
 * SECTION:clutter-zoom-action
 * @Title: ClutterZoomAction
 * @Short_Description: Action enabling zooming on actors
 *
 * #ClutterZoomAction is a sub-class of #ClutterGestureAction that
 * implements all the necessary logic for zooming actors using a "pinch"
 * gesture between two touch points.
 *
 * The simplest usage of #ClutterZoomAction consists in adding it to
 * a #ClutterActor and setting it as reactive; for instance, the following
 * code:
 *
 * |[
 *   clutter_actor_add_action (actor, clutter_zoom_action_new ());
 *   clutter_actor_set_reactive (actor, TRUE);
 * ]|
 *
 * will automatically result in the actor to be scale according to the
 * distance between two touch points.
 *
 * Since: 1.12
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "clutter-zoom-action.h"

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-gesture-action-private.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

typedef struct
{
  gfloat start_x;
  gfloat start_y;
  gfloat transformed_start_x;
  gfloat transformed_start_y;

  gfloat update_x;
  gfloat update_y;
  gfloat transformed_update_x;
  gfloat transformed_update_y;
} ZoomPoint;

struct _ClutterZoomActionPrivate
{
  ClutterStage *stage;

  ClutterZoomAxis zoom_axis;

  ZoomPoint points[2];

  ClutterPoint initial_focal_point;
  ClutterPoint focal_point;
  ClutterPoint transformed_focal_point;

  gfloat initial_x;
  gfloat initial_y;
  gfloat initial_z;

  gdouble initial_scale_x;
  gdouble initial_scale_y;

  gdouble zoom_initial_distance;
};

enum
{
  PROP_0,

  PROP_ZOOM_AXIS,

  PROP_LAST
};

static GParamSpec *zoom_props[PROP_LAST] = { NULL, };

enum
{
  ZOOM,

  LAST_SIGNAL
};

static guint zoom_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (ClutterZoomAction, clutter_zoom_action, CLUTTER_TYPE_GESTURE_ACTION)

static void
capture_point_initial_position (ClutterGestureAction *action,
                                ClutterActor         *actor,
                                gint                  index,
                                ZoomPoint            *point)
{
  clutter_gesture_action_get_motion_coords (action, index,
                                            &point->start_x,
                                            &point->start_y);

  point->transformed_start_x = point->update_x = point->start_x;
  point->transformed_start_y = point->update_x = point->start_y;
  clutter_actor_transform_stage_point (actor,
                                       point->start_x, point->start_y,
                                       &point->transformed_start_x,
                                       &point->transformed_start_y);
  point->transformed_update_x = point->transformed_start_x;
  point->transformed_update_y = point->transformed_start_y;
}

static void
capture_point_update_position (ClutterGestureAction *action,
                               ClutterActor         *actor,
                               gint                  index,
                               ZoomPoint            *point)
{
  clutter_gesture_action_get_motion_coords (action, index,
                                            &point->update_x,
                                            &point->update_y);

  point->transformed_update_x = point->update_x;
  point->transformed_update_y = point->update_y;
  clutter_actor_transform_stage_point (actor,
                                       point->update_x, point->update_y,
                                       &point->transformed_update_x,
                                       &point->transformed_update_y);
}

static gboolean
clutter_zoom_action_gesture_begin (ClutterGestureAction *action,
                                   ClutterActor         *actor)
{
  ClutterZoomActionPrivate *priv = ((ClutterZoomAction *) action)->priv;
  gfloat dx, dy;

  capture_point_initial_position (action, actor, 0, &priv->points[0]);
  capture_point_initial_position (action, actor, 1, &priv->points[1]);

  dx = priv->points[1].transformed_start_x - priv->points[0].transformed_start_x;
  dy = priv->points[1].transformed_start_y - priv->points[0].transformed_start_y;
  priv->zoom_initial_distance = sqrt (dx * dx + dy * dy);

  clutter_actor_get_translation (actor,
                                 &priv->initial_x,
                                 &priv->initial_y,
                                 &priv->initial_z);
  clutter_actor_get_scale (actor,
                           &priv->initial_scale_x,
                           &priv->initial_scale_y);

  priv->initial_focal_point.x = (priv->points[0].start_x + priv->points[1].start_x) / 2;
  priv->initial_focal_point.y = (priv->points[0].start_y + priv->points[1].start_y) / 2;
  clutter_actor_transform_stage_point (actor,
                                       priv->initial_focal_point.x,
                                       priv->initial_focal_point.y,
                                       &priv->transformed_focal_point.x,
                                       &priv->transformed_focal_point.y);

  clutter_actor_set_pivot_point (actor,
                                 priv->transformed_focal_point.x / clutter_actor_get_width (actor),
                                 priv->transformed_focal_point.y / clutter_actor_get_height (actor));

  return TRUE;
}

static gboolean
clutter_zoom_action_gesture_progress (ClutterGestureAction *action,
                                      ClutterActor         *actor)
{
  ClutterZoomActionPrivate *priv = ((ClutterZoomAction *) action)->priv;
  gdouble distance, new_scale;
  gfloat dx, dy;
  gboolean retval;

  capture_point_update_position (action, actor, 0, &priv->points[0]);
  capture_point_update_position (action, actor, 1, &priv->points[1]);

  dx = priv->points[1].update_x - priv->points[0].update_x;
  dy = priv->points[1].update_y - priv->points[0].update_y;
  distance = sqrt (dx * dx + dy * dy);

  if (distance == 0)
    return TRUE;

  priv->focal_point.x = (priv->points[0].update_x + priv->points[1].update_x) / 2;
  priv->focal_point.y = (priv->points[0].update_y + priv->points[1].update_y) / 2;

  new_scale = distance / priv->zoom_initial_distance;

  g_signal_emit (action, zoom_signals[ZOOM], 0,
                 actor, &priv->focal_point, new_scale,
                 &retval);

  return TRUE;
}

static void
clutter_zoom_action_gesture_cancel (ClutterGestureAction *action,
                                    ClutterActor         *actor)
{
  ClutterZoomActionPrivate *priv = ((ClutterZoomAction *) action)->priv;

  clutter_actor_set_translation (actor,
                                 priv->initial_x,
                                 priv->initial_y,
                                 priv->initial_z);
  clutter_actor_set_scale (actor, priv->initial_scale_x, priv->initial_scale_y);
}

static gboolean
clutter_zoom_action_real_zoom (ClutterZoomAction *action,
                               ClutterActor      *actor,
                               ClutterPoint      *focal_point,
                               gdouble            factor)
{
  ClutterZoomActionPrivate *priv = action->priv;
  gfloat x, y, z;
  gdouble scale_x, scale_y;
  ClutterVertex out, in;

  in.x = priv->transformed_focal_point.x;
  in.y = priv->transformed_focal_point.y;
  in.z = 0;

  clutter_actor_apply_transform_to_point (actor, &in, &out);

  clutter_actor_get_scale (actor, &scale_x, &scale_y);

  switch (priv->zoom_axis)
    {
    case CLUTTER_ZOOM_BOTH:
      clutter_actor_set_scale (actor, factor, factor);
      break;

    case CLUTTER_ZOOM_X_AXIS:
      clutter_actor_set_scale (actor, factor, scale_y);
      break;

    case CLUTTER_ZOOM_Y_AXIS:
      clutter_actor_set_scale (actor, scale_x, factor);
      break;

    default:
      break;
    }

  x = priv->initial_x + priv->focal_point.x - priv->initial_focal_point.x;
  y = priv->initial_y + priv->focal_point.y - priv->initial_focal_point.y;
  clutter_actor_get_translation (actor, NULL, NULL, &z);
  clutter_actor_set_translation (actor, x, y, z);

  return TRUE;
}

static void
clutter_zoom_action_set_property (GObject      *gobject,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ClutterZoomAction *action = CLUTTER_ZOOM_ACTION (gobject);

  switch (prop_id)
    {
    case PROP_ZOOM_AXIS:
      clutter_zoom_action_set_zoom_axis (action, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_zoom_action_get_property (GObject    *gobject,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ClutterZoomActionPrivate *priv = CLUTTER_ZOOM_ACTION (gobject)->priv;

  switch (prop_id)
    {
    case PROP_ZOOM_AXIS:
      g_value_set_enum (value, priv->zoom_axis);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_zoom_action_dispose (GObject *gobject)
{
  G_OBJECT_CLASS (clutter_zoom_action_parent_class)->dispose (gobject);
}

static void
clutter_zoom_action_class_init (ClutterZoomActionClass *klass)
{
  ClutterGestureActionClass *gesture_class =
    CLUTTER_GESTURE_ACTION_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = clutter_zoom_action_set_property;
  gobject_class->get_property = clutter_zoom_action_get_property;
  gobject_class->dispose = clutter_zoom_action_dispose;

  gesture_class->gesture_begin = clutter_zoom_action_gesture_begin;
  gesture_class->gesture_progress = clutter_zoom_action_gesture_progress;
  gesture_class->gesture_cancel = clutter_zoom_action_gesture_cancel;

  klass->zoom = clutter_zoom_action_real_zoom;

  /**
   * ClutterZoomAction:zoom-axis:
   *
   * Constraints the zooming action to the specified axis
   *
   * Since: 1.12
   */
  zoom_props[PROP_ZOOM_AXIS] =
    g_param_spec_enum ("zoom-axis",
                       P_("Zoom Axis"),
                       P_("Constraints the zoom to an axis"),
                       CLUTTER_TYPE_ZOOM_AXIS,
                       CLUTTER_ZOOM_BOTH,
                       CLUTTER_PARAM_READWRITE);

  g_object_class_install_properties  (gobject_class,
                                      PROP_LAST,
                                      zoom_props);

  /**
   * ClutterZoomAction::zoom:
   * @action: the #ClutterZoomAction that emitted the signal
   * @actor: the #ClutterActor attached to the action
   * @focal_point: the focal point of the zoom
   * @factor: the initial distance between the 2 touch points
   *
   * The ::zoom signal is emitted for each series of touch events that
   * change the distance and focal point between the touch points.
   *
   * The default handler of the signal will call
   * clutter_actor_set_scale() on @actor using the ratio of the first
   * distance between the touch points and the current distance. To
   * override the default behaviour, connect to this signal and return
   * %FALSE.
   *
   * Return value: %TRUE if the zoom should continue, and %FALSE if
   *   the zoom should be cancelled.
   *
   * Since: 1.12
   */
  zoom_signals[ZOOM] =
    g_signal_new (I_("zoom"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterZoomActionClass, zoom),
                  _clutter_boolean_continue_accumulator, NULL,
                  _clutter_marshal_BOOLEAN__OBJECT_BOXED_DOUBLE,
                  G_TYPE_BOOLEAN, 3,
                  CLUTTER_TYPE_ACTOR,
                  CLUTTER_TYPE_POINT,
                  G_TYPE_DOUBLE);
}

static void
clutter_zoom_action_init (ClutterZoomAction *self)
{
  ClutterGestureAction *gesture;

  self->priv = clutter_zoom_action_get_instance_private (self);
  self->priv->zoom_axis = CLUTTER_ZOOM_BOTH;

  gesture = CLUTTER_GESTURE_ACTION (self);
  clutter_gesture_action_set_n_touch_points (gesture, 2);
  clutter_gesture_action_set_threshold_trigger_edge (gesture, CLUTTER_GESTURE_TRIGGER_EDGE_NONE);
}

/**
 * clutter_zoom_action_new:
 *
 * Creates a new #ClutterZoomAction instance
 *
 * Return value: the newly created #ClutterZoomAction
 *
 * Since: 1.12
 */
ClutterAction *
clutter_zoom_action_new (void)
{
  return g_object_new (CLUTTER_TYPE_ZOOM_ACTION, NULL);
}

/**
 * clutter_zoom_action_set_zoom_axis:
 * @action: a #ClutterZoomAction
 * @axis: the axis to constraint the zooming to
 *
 * Restricts the zooming action to a specific axis
 *
 * Since: 1.12
 */
void
clutter_zoom_action_set_zoom_axis (ClutterZoomAction *action,
                                   ClutterZoomAxis    axis)
{
  g_return_if_fail (CLUTTER_IS_ZOOM_ACTION (action));
  g_return_if_fail (axis >= CLUTTER_ZOOM_X_AXIS &&
                    axis <= CLUTTER_ZOOM_BOTH);

  if (action->priv->zoom_axis == axis)
    return;

  action->priv->zoom_axis = axis;

  g_object_notify_by_pspec (G_OBJECT (action), zoom_props[PROP_ZOOM_AXIS]);
}

/**
 * clutter_zoom_action_get_zoom_axis:
 * @action: a #ClutterZoomAction
 *
 * Retrieves the axis constraint set by clutter_zoom_action_set_zoom_axis()
 *
 * Return value: the axis constraint
 *
 * Since: 1.12
 */
ClutterZoomAxis
clutter_zoom_action_get_zoom_axis (ClutterZoomAction *action)
{
  g_return_val_if_fail (CLUTTER_IS_ZOOM_ACTION (action),
                        CLUTTER_ZOOM_BOTH);

  return action->priv->zoom_axis;
}

/**
 * clutter_zoom_action_get_focal_point:
 * @action: a #ClutterZoomAction
 * @point: (out): a #ClutterPoint
 *
 * Retrieves the focal point of the current zoom
 *
 * Since: 1.12
 */
void
clutter_zoom_action_get_focal_point (ClutterZoomAction *action,
                                     ClutterPoint      *point)
{
  g_return_if_fail (CLUTTER_IS_ZOOM_ACTION (action));
  g_return_if_fail (point != NULL);

  *point = action->priv->focal_point;
}

/**
 * clutter_zoom_action_get_transformed_focal_point:
 * @action: a #ClutterZoomAction
 * @point: (out): a #ClutterPoint
 *
 * Retrieves the focal point relative to the actor's coordinates of
 * the current zoom
 *
 * Since: 1.12
 */
void
clutter_zoom_action_get_transformed_focal_point (ClutterZoomAction *action,
                                                 ClutterPoint      *point)
{
  g_return_if_fail (CLUTTER_IS_ZOOM_ACTION (action));
  g_return_if_fail (point != NULL);

  *point = action->priv->transformed_focal_point;
}
