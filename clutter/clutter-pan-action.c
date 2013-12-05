/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
 * Copyright (C) 2011  Robert Bosch Car Multimedia GmbH.
 * Copyright (C) 2012  Collabora Ltd.
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
 *   Emanuele Aina <emanuele.aina@collabora.com>
 *
 * Based on ClutterDragAction, ClutterSwipeAction, and MxKineticScrollView,
 * written by:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 *   Tomeu Vizoso <tomeu.vizoso@collabora.co.uk>
 *   Chris Lord <chris@linux.intel.com>
 */

/**
 * SECTION:clutter-pan-action
 * @Title: ClutterPanAction
 * @Short_Description: Action for pan gestures
 *
 * #ClutterPanAction is a sub-class of #ClutterGestureAction that implements
 * the logic for recognizing pan gestures.
 *
 * The simplest usage of #ClutterPanAction consists in adding it to
 * a #ClutterActor with a child and setting it as reactive; for instance,
 * the following code:
 *
 * |[
 *   clutter_actor_add_action (actor, clutter_pan_action_new ());
 *   clutter_actor_set_reactive (actor, TRUE);
 * ]|
 *
 * will automatically result in the actor children to be moved
 * when dragging.
 *
 * Since: 1.12
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-pan-action.h"

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-gesture-action-private.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include <math.h>

#define FLOAT_EPSILON   (1e-15)

static const gfloat min_velocity = 0.1f; // measured in px/ms
static const gfloat reference_fps = 60.0f; // the fps assumed for the deceleration rate
static const gfloat default_deceleration_rate = 0.95f;
static const gfloat default_acceleration_factor = 1.0f;

typedef enum
{
  PAN_STATE_INACTIVE,
  PAN_STATE_PANNING,
  PAN_STATE_INTERPOLATING
} PanState;

struct _ClutterPanActionPrivate
{
  ClutterPanAxis pan_axis;

  PanState state;

  /* Variables for storing acceleration information */
  ClutterTimeline *deceleration_timeline;
  gfloat target_x;
  gfloat target_y;
  gfloat dx;
  gfloat dy;
  gdouble deceleration_rate;
  gdouble acceleration_factor;

  /* Inertial motion tracking */
  gfloat interpolated_x;
  gfloat interpolated_y;
  gfloat release_x;
  gfloat release_y;

  guint should_interpolate : 1;
};

enum
{
  PROP_0,

  PROP_PAN_AXIS,
  PROP_INTERPOLATE,
  PROP_DECELERATION,
  PROP_ACCELERATION_FACTOR,

  PROP_LAST
};

static GParamSpec *pan_props[PROP_LAST] = { NULL, };

enum
{
  PAN,
  PAN_STOPPED,

  LAST_SIGNAL
};

static guint pan_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (ClutterPanAction, clutter_pan_action, CLUTTER_TYPE_GESTURE_ACTION)

static void
emit_pan (ClutterPanAction *self,
          ClutterActor     *actor,
          gboolean          is_interpolated)
{
  gboolean retval;
  g_signal_emit (self, pan_signals[PAN], 0, actor, is_interpolated, &retval);
}

static void
emit_pan_stopped (ClutterPanAction *self,
                  ClutterActor     *actor)
{
  ClutterPanActionPrivate *priv = self->priv;

  g_signal_emit (self, pan_signals[PAN_STOPPED], 0, actor);
  priv->state = PAN_STATE_INACTIVE;
}

static void
on_deceleration_stopped (ClutterTimeline           *timeline,
                         gboolean                   is_finished,
                         ClutterPanAction *self)
{
  ClutterPanActionPrivate *priv = self->priv;
  ClutterActor *actor;

  g_object_unref (timeline);
  priv->deceleration_timeline = NULL;

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (self));
  emit_pan_stopped (self, actor);
}

static void
on_deceleration_new_frame (ClutterTimeline     *timeline,
                           gint                 elapsed_time,
                           ClutterPanAction    *self)
{
  ClutterPanActionPrivate *priv = self->priv;
  ClutterActor *actor;
  gdouble progress;
  gfloat interpolated_x, interpolated_y;

  progress = clutter_timeline_get_progress (timeline);

  interpolated_x = priv->target_x * progress;
  interpolated_y = priv->target_y * progress;
  priv->dx = interpolated_x - priv->interpolated_x;
  priv->dy = interpolated_y - priv->interpolated_y;
  priv->interpolated_x = interpolated_x;
  priv->interpolated_y = interpolated_y;

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (self));
  emit_pan (self, actor, TRUE);
}

static gboolean
gesture_prepare (ClutterGestureAction  *gesture,
                 ClutterActor          *actor)
{
  ClutterPanAction *self = CLUTTER_PAN_ACTION (gesture);
  ClutterPanActionPrivate *priv = self->priv;

  if (priv->state == PAN_STATE_INTERPOLATING && priv->deceleration_timeline)
    clutter_timeline_stop (priv->deceleration_timeline);

  return TRUE;
}

static gboolean
gesture_begin (ClutterGestureAction  *gesture,
               ClutterActor          *actor)
{
  ClutterPanAction *self = CLUTTER_PAN_ACTION (gesture);
  ClutterPanActionPrivate *priv = self->priv;

  priv->state = PAN_STATE_PANNING;
  priv->interpolated_x = priv->interpolated_y = 0.0f;
  priv->dx = priv->dy = 0.0f;

  return TRUE;
}

static gboolean
gesture_progress (ClutterGestureAction *gesture,
                  ClutterActor         *actor)
{
  ClutterPanAction *self = CLUTTER_PAN_ACTION (gesture);

  emit_pan (self, actor, FALSE);

  return TRUE;
}

static void
gesture_cancel (ClutterGestureAction *gesture,
                ClutterActor         *actor)
{
  ClutterPanAction *self = CLUTTER_PAN_ACTION (gesture);
  ClutterPanActionPrivate *priv = self->priv;

  priv->state = PAN_STATE_INACTIVE;
}

static void
gesture_end (ClutterGestureAction *gesture,
             ClutterActor         *actor)
{
  ClutterPanAction *self = CLUTTER_PAN_ACTION (gesture);
  ClutterPanActionPrivate *priv = self->priv;
  gfloat velocity, velocity_x, velocity_y;
  gfloat delta_x, delta_y;
  gfloat tau;
  gint duration;

  clutter_gesture_action_get_release_coords (CLUTTER_GESTURE_ACTION (self), 0, &priv->release_x, &priv->release_y);

  if (!priv->should_interpolate)
    {
      priv->state = PAN_STATE_INACTIVE;
      return;
    }

  priv->state = PAN_STATE_INTERPOLATING;

  clutter_gesture_action_get_motion_delta (gesture, 0, &delta_x, &delta_y);
  velocity = clutter_gesture_action_get_velocity (gesture, 0, &velocity_x, &velocity_y);

  /* Exponential timing constant v(t) = v(0) * exp(-t/tau)
   * tau = 1000ms / (frame_per_second * - ln(decay_per_frame))
   * with frame_per_second = 60 and decay_per_frame = 0.95, tau ~= 325ms
   * see http://ariya.ofilabs.com/2011/10/flick-list-with-its-momentum-scrolling-and-deceleration.html */
  tau = 1000.0f / (reference_fps * - logf (priv->deceleration_rate));

  /* See where the decreasing velocity reaches $min_velocity px/ms
   * v(t) = v(0) * exp(-t/tau) = min_velocity
   * t = - tau * ln( min_velocity / |v(0)|) */
  duration = - tau * logf (min_velocity / (ABS (velocity) * priv->acceleration_factor));

  /* Target point: x(t) = v(0) * tau * [1 - exp(-t/tau)] */
  priv->target_x = velocity_x * priv->acceleration_factor * tau * (1 - exp ((float)-duration / tau));
  priv->target_y = velocity_y * priv->acceleration_factor * tau * (1 - exp ((float)-duration / tau));

  if (ABS (velocity) * priv->acceleration_factor > min_velocity && duration > FLOAT_EPSILON)
    {
      priv->interpolated_x = priv->interpolated_y = 0.0f;
      priv->deceleration_timeline = clutter_timeline_new (duration);
      clutter_timeline_set_progress_mode (priv->deceleration_timeline, CLUTTER_EASE_OUT_EXPO);

      g_signal_connect (priv->deceleration_timeline, "new_frame",
                        G_CALLBACK (on_deceleration_new_frame), self);
      g_signal_connect (priv->deceleration_timeline, "stopped",
                        G_CALLBACK (on_deceleration_stopped), self);
      clutter_timeline_start (priv->deceleration_timeline);
    }
  else
    {
      emit_pan_stopped (self, actor);
    }
}

static gboolean
clutter_pan_action_real_pan (ClutterPanAction *self,
                             ClutterActor     *actor,
                             gboolean          is_interpolated)
{
  ClutterPanActionPrivate *priv = self->priv;
  gfloat dx, dy;
  ClutterMatrix transform;

  clutter_pan_action_get_motion_delta (self, 0, &dx, &dy);

  switch (priv->pan_axis)
    {
    case CLUTTER_PAN_AXIS_NONE:
      break;

    case CLUTTER_PAN_X_AXIS:
      dy = 0.0f;
      break;

    case CLUTTER_PAN_Y_AXIS:
      dx = 0.0f;
      break;
    }

  clutter_actor_get_child_transform (actor, &transform);
  cogl_matrix_translate (&transform, dx, dy, 0.0f);
  clutter_actor_set_child_transform (actor, &transform);
  return TRUE;
}

static void
clutter_pan_action_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ClutterPanAction *self = CLUTTER_PAN_ACTION (gobject);

  switch (prop_id)
    {
    case PROP_PAN_AXIS:
      clutter_pan_action_set_pan_axis (self, g_value_get_enum (value));
      break;

    case PROP_INTERPOLATE :
      clutter_pan_action_set_interpolate (self, g_value_get_boolean (value));
      break;

    case PROP_DECELERATION :
      clutter_pan_action_set_deceleration (self, g_value_get_double (value));
      break;

    case PROP_ACCELERATION_FACTOR :
      clutter_pan_action_set_acceleration_factor (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_pan_action_get_property (GObject    *gobject,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ClutterPanAction *self = CLUTTER_PAN_ACTION (gobject);
  ClutterPanActionPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_PAN_AXIS:
      g_value_set_enum (value, priv->pan_axis);
      break;

    case PROP_INTERPOLATE :
      g_value_set_boolean (value, priv->should_interpolate);
      break;

    case PROP_DECELERATION :
      g_value_set_double (value, priv->deceleration_rate);
      break;

    case PROP_ACCELERATION_FACTOR :
      g_value_set_double (value, priv->acceleration_factor);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_pan_action_dispose (GObject *gobject)
{
  ClutterPanActionPrivate *priv = CLUTTER_PAN_ACTION (gobject)->priv;

  g_clear_object (&priv->deceleration_timeline);

  G_OBJECT_CLASS (clutter_pan_action_parent_class)->dispose (gobject);
}

static void
clutter_pan_action_set_actor (ClutterActorMeta *meta,
                              ClutterActor     *actor)
{
  ClutterPanAction *self = CLUTTER_PAN_ACTION (meta);
  ClutterPanActionPrivate *priv = self->priv;
  ClutterActor *old_actor;

  old_actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (self));
  if (old_actor != actor)
    {
      /* make sure we reset the state */
      if (priv->state == PAN_STATE_INTERPOLATING)
        g_clear_object (&priv->deceleration_timeline);
    }

  CLUTTER_ACTOR_META_CLASS (clutter_pan_action_parent_class)->set_actor (meta, actor);
}


static void
clutter_pan_action_class_init (ClutterPanActionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);
  ClutterGestureActionClass *gesture_class =
      CLUTTER_GESTURE_ACTION_CLASS (klass);

  klass->pan = clutter_pan_action_real_pan;

  gesture_class->gesture_prepare = gesture_prepare;
  gesture_class->gesture_begin = gesture_begin;
  gesture_class->gesture_progress = gesture_progress;
  gesture_class->gesture_cancel = gesture_cancel;
  gesture_class->gesture_end = gesture_end;

  meta_class->set_actor = clutter_pan_action_set_actor;

  /**
   * ClutterPanAction:pan-axis:
   *
   * Constraints the panning action to the specified axis
   *
   * Since: 1.12
   */
  pan_props[PROP_PAN_AXIS] =
    g_param_spec_enum ("pan-axis",
                       P_("Pan Axis"),
                       P_("Constraints the panning to an axis"),
                       CLUTTER_TYPE_PAN_AXIS,
                       CLUTTER_PAN_AXIS_NONE,
                       CLUTTER_PARAM_READWRITE);

  /**
   * ClutterPanAction:interpolate:
   *
   * Whether interpolated events emission is enabled.
   *
   * Since: 1.12
   */
  pan_props[PROP_INTERPOLATE] =
    g_param_spec_boolean ("interpolate",
                          P_("Interpolate"),
                          P_("Whether interpolated events emission is enabled."),
                          FALSE,
                          CLUTTER_PARAM_READWRITE);

  /**
   * ClutterPanAction:deceleration:
   *
   * The rate at which the interpolated panning will decelerate in
   *
   * #ClutterPanAction will emit interpolated ::pan events with decreasing
   * scroll deltas, using the rate specified by this property.
   *
   * Since: 1.12
   */
  pan_props[PROP_DECELERATION] =
    g_param_spec_double ("deceleration",
                         P_("Deceleration"),
                         P_("Rate at which the interpolated panning will decelerate in"),
                         FLOAT_EPSILON, 1.0, default_deceleration_rate,
                         CLUTTER_PARAM_READWRITE);

  /**
   * ClutterPanAction:acceleration-factor:
   *
   * The initial acceleration factor
   *
   * The kinetic momentum measured at the time of releasing the pointer will
   * be multiplied by the factor specified by this property before being used
   * to generate interpolated ::pan events.
   *
   * Since: 1.12
   */
  pan_props[PROP_ACCELERATION_FACTOR] =
    g_param_spec_double ("acceleration-factor",
                         P_("Initial acceleration factor"),
                         P_("Factor applied to the momentum when starting the interpolated phase"),
                         1.0, G_MAXDOUBLE, default_acceleration_factor,
                         CLUTTER_PARAM_READWRITE);

  gobject_class->set_property = clutter_pan_action_set_property;
  gobject_class->get_property = clutter_pan_action_get_property;
  gobject_class->dispose = clutter_pan_action_dispose;
  g_object_class_install_properties  (gobject_class,
                                      PROP_LAST,
                                      pan_props);

  /**
   * ClutterPanAction::pan:
   * @action: the #ClutterPanAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   * @is_interpolated: if the event is the result of interpolating
   *                   the motion velocity at the end of the drag
   *
   * The ::pan signal is emitted to keep track of the motion during
   * a pan gesture. @is_interpolated is set to %TRUE during the
   * interpolation phase of the pan, after the drag has ended and
   * the :interpolate property was set to %TRUE.
   *
   * Return value: %TRUE if the pan should continue, and %FALSE if
   *   the pan should be cancelled.
   *
   * Since: 1.12
   */
  pan_signals[PAN] =
    g_signal_new (I_("pan"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterPanActionClass, pan),
                  _clutter_boolean_continue_accumulator, NULL,
                  _clutter_marshal_BOOLEAN__OBJECT_BOOLEAN,
                  G_TYPE_BOOLEAN, 2,
                  CLUTTER_TYPE_ACTOR,
                  G_TYPE_BOOLEAN);

  /**
   * ClutterPanAction::pan-stopped:
   * @action: the #ClutterPanAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   *
   * The ::pan-stopped signal is emitted at the end of the interpolation
   * phase of the pan action, only when :interpolate is set to %TRUE.
   *
   * Since: 1.12
   */
  pan_signals[PAN_STOPPED] =
    g_signal_new (I_("pan-stopped"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterPanActionClass, pan_stopped),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);
}

static void
clutter_pan_action_init (ClutterPanAction *self)
{
  ClutterGestureAction *gesture;

  self->priv = clutter_pan_action_get_instance_private (self);
  self->priv->deceleration_rate = default_deceleration_rate;
  self->priv->acceleration_factor = default_acceleration_factor;
  self->priv->state = PAN_STATE_INACTIVE;

  gesture = CLUTTER_GESTURE_ACTION (self);
  clutter_gesture_action_set_threshold_trigger_edge (gesture, CLUTTER_GESTURE_TRIGGER_EDGE_AFTER);
}

/**
 * clutter_pan_action_new:
 *
 * Creates a new #ClutterPanAction instance
 *
 * Return value: the newly created #ClutterPanAction
 *
 * Since: 1.12
 */
ClutterAction *
clutter_pan_action_new (void)
{
  return g_object_new (CLUTTER_TYPE_PAN_ACTION, NULL);
}

/**
 * clutter_pan_action_set_pan_axis:
 * @self: a #ClutterPanAction
 * @axis: the axis to constraint the panning to
 *
 * Restricts the panning action to a specific axis
 *
 * Since: 1.12
 */
void
clutter_pan_action_set_pan_axis (ClutterPanAction *self,
                                 ClutterPanAxis    axis)
{
  ClutterPanActionPrivate *priv;

  g_return_if_fail (CLUTTER_IS_PAN_ACTION (self));
  g_return_if_fail (axis >= CLUTTER_PAN_AXIS_NONE &&
                    axis <= CLUTTER_PAN_Y_AXIS);

  priv = self->priv;

  if (priv->pan_axis == axis)
    return;

  priv->pan_axis = axis;

  g_object_notify_by_pspec (G_OBJECT (self), pan_props[PROP_PAN_AXIS]);
}

/**
 * clutter_pan_action_get_pan_axis:
 * @self: a #ClutterPanAction
 *
 * Retrieves the axis constraint set by clutter_pan_action_set_pan_axis()
 *
 * Return value: the axis constraint
 *
 * Since: 1.12
 */
ClutterPanAxis
clutter_pan_action_get_pan_axis (ClutterPanAction *self)
{
  g_return_val_if_fail (CLUTTER_IS_PAN_ACTION (self),
                        CLUTTER_PAN_AXIS_NONE);

  return self->priv->pan_axis;
}

/**
 * clutter_pan_action_set_interpolate:
 * @self: a #ClutterPanAction
 * @should_interpolate: whether to enable interpolated pan events
 *
 * Sets whether the action should emit interpolated ::pan events
 * after the drag has ended, to emulate the gesture kinetic inertia.
 *
 * Since: 1.12
 */
void
clutter_pan_action_set_interpolate (ClutterPanAction *self,
                                    gboolean          should_interpolate)
{
  ClutterPanActionPrivate *priv;

  g_return_if_fail (CLUTTER_IS_PAN_ACTION (self));

  priv = self->priv;

  should_interpolate = !!should_interpolate;

  if (priv->should_interpolate == should_interpolate)
    return;

  priv->should_interpolate = should_interpolate;

  g_object_notify_by_pspec (G_OBJECT (self), pan_props[PROP_INTERPOLATE]);
}

/**
 * clutter_pan_action_get_interpolate:
 * @self: a #ClutterPanAction
 *
 * Checks if the action should emit ::pan events even after releasing
 * the pointer during a panning gesture, to emulate some kind of
 * kinetic inertia.
 *
 * Return value: %TRUE if interpolated events emission is active.
 *
 * Since: 1.12
 */
gboolean
clutter_pan_action_get_interpolate (ClutterPanAction *self)
{
  g_return_val_if_fail (CLUTTER_IS_PAN_ACTION (self),
                        FALSE);

  return self->priv->should_interpolate;
}

/**
 * clutter_pan_action_set_deceleration:
 * @self: A #ClutterPanAction
 * @rate: The deceleration rate
 *
 * Sets the deceleration rate of the interpolated ::pan events generated
 * after a pan gesture. This is approximately the value that the momentum
 * at the time of releasing the pointer is divided by every 60th of a second.
 *
 * Since: 1.12
 */
void
clutter_pan_action_set_deceleration (ClutterPanAction *self,
                                     gdouble           rate)
{
  g_return_if_fail (CLUTTER_IS_PAN_ACTION (self));
  g_return_if_fail (rate <= 1.0);
  g_return_if_fail (rate > 0.0);

  self->priv->deceleration_rate = rate;
  g_object_notify_by_pspec (G_OBJECT (self), pan_props[PROP_DECELERATION]);
}

/**
 * clutter_pan_action_get_deceleration:
 * @self: A #ClutterPanAction
 *
 * Retrieves the deceleration rate of interpolated ::pan events.
 *
 * Return value: The deceleration rate of the interpolated events.
 *
 * Since: 1.12
 */
gdouble
clutter_pan_action_get_deceleration (ClutterPanAction *self)
{
  g_return_val_if_fail (CLUTTER_IS_PAN_ACTION (self), 0.95);
  return self->priv->deceleration_rate;
}

/**
 * clutter_pan_action_set_acceleration_factor:
 * @self: A #ClutterPanAction
 * @factor: The acceleration factor
 *
 * Factor applied to the momentum velocity at the time of releasing the
 * pointer when generating interpolated ::pan events.
 *
 * Since: 1.12
 */
void
clutter_pan_action_set_acceleration_factor (ClutterPanAction *self,
                                            gdouble           factor)
{
  g_return_if_fail (CLUTTER_IS_PAN_ACTION (self));
  g_return_if_fail (factor >= 0.0);

  self->priv->acceleration_factor = factor;
  g_object_notify_by_pspec (G_OBJECT (self), pan_props[PROP_ACCELERATION_FACTOR]);
}

/**
 * clutter_pan_action_get_acceleration_factor:
 * @self: A #ClutterPanAction
 *
 * Retrieves the initial acceleration factor for interpolated ::pan events.
 *
 * Return value: The initial acceleration factor for interpolated events.
 *
 * Since: 1.12
 */
gdouble
clutter_pan_action_get_acceleration_factor (ClutterPanAction *self)
{
  g_return_val_if_fail (CLUTTER_IS_PAN_ACTION (self), 1.0);
  return self->priv->acceleration_factor;
}

/**
 * clutter_pan_action_get_interpolated_coords:
 * @self: A #ClutterPanAction
 * @interpolated_x: (out) (allow-none): return location for the latest
 *   interpolated event's X coordinate
 * @interpolated_y: (out) (allow-none): return location for the latest
 *   interpolated event's Y coordinate
 *
 * Retrieves the coordinates, in stage space, of the latest interpolated
 * event, analogous to clutter_gesture_action_get_motion_coords().
 *
 * Since: 1.12
 */
void
clutter_pan_action_get_interpolated_coords (ClutterPanAction *self,
                                            gfloat           *interpolated_x,
                                            gfloat           *interpolated_y)
{
  ClutterPanActionPrivate *priv;

  g_return_if_fail (CLUTTER_IS_PAN_ACTION (self));

  priv = self->priv;

  if (interpolated_x)
    *interpolated_x = priv->release_x + priv->interpolated_x;

  if (interpolated_y)
    *interpolated_y = priv->release_y + priv->interpolated_y;
}

/**
 * clutter_pan_action_get_interpolated_delta:
 * @self: A #ClutterPanAction
 * @delta_x: (out) (allow-none): return location for the X delta since
 *   the latest interpolated event
 * @delta_y: (out) (allow-none): return location for the Y delta since
 *   the latest interpolated event
 *
 * Retrieves the delta, in stage space, since the latest interpolated
 * event, analogous to clutter_gesture_action_get_motion_delta().
 *
 * Return value: the distance since the latest interpolated event
 *
 * Since: 1.12
 */
gfloat
clutter_pan_action_get_interpolated_delta (ClutterPanAction *self,
                                           gfloat           *delta_x,
                                           gfloat           *delta_y)
{
  ClutterPanActionPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_PAN_ACTION (self), 0.0f);

  priv = self->priv;

  if (delta_x)
    *delta_x = priv->dx;

  if (delta_y)
    *delta_y = priv->dy;

  return sqrt ((priv->dx * priv->dx) + (priv->dy * priv->dy));
}

/**
 * clutter_pan_action_get_motion_delta:
 * @self: A #ClutterPanAction
 * @point: the touch point index, with 0 being the first touch
 *   point received by the action
 * @delta_x: (out) (allow-none): return location for the X delta
 * @delta_y: (out) (allow-none): return location for the Y delta
 *
 * Retrieves the delta, in stage space, dependent on the current state
 * of the #ClutterPanAction. If it is inactive, both fields will be
 * set to 0. If it is panning by user action, the values will be equivalent
 * to those returned by clutter_gesture_action_get_motion_delta().
 * If it is interpolating with some form of kinetic scrolling, the values
 * will be equivalent to those returned by
 * clutter_pan_action_get_interpolated_delta(). This is a convenience
 * method designed to be used in replacement "pan" signal handlers.
 *
 * Since: 1.14
 */
gfloat
clutter_pan_action_get_motion_delta (ClutterPanAction *self,
                                     guint             point,
                                     gfloat           *delta_x,
                                     gfloat           *delta_y)
{
  ClutterPanActionPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_PAN_ACTION (self), 0.0f);

  priv = self->priv;

  switch (priv->state)
    {
    case PAN_STATE_INACTIVE:
      if (delta_x)
        *delta_x = 0;

      if (delta_y)
        *delta_y = 0;

      return 0;
    case PAN_STATE_PANNING:
      return clutter_gesture_action_get_motion_delta (CLUTTER_GESTURE_ACTION (self),
                                                      point, delta_x, delta_y);
    case PAN_STATE_INTERPOLATING:
      return clutter_pan_action_get_interpolated_delta (self, delta_x, delta_y);
    default:
      g_assert_not_reached ();
    }
}

/**
 * clutter_pan_action_get_motion_coords:
 * @self: A #ClutterPanAction
 * @point: the touch point index, with 0 being the first touch
 *   point received by the action
 * @motion_x: (out) (allow-none): return location for the X coordinate
 * @motion_y: (out) (allow-none): return location for the Y coordinate
 *
 * Retrieves the coordinates, in stage space, dependent on the current state
 * of the #ClutterPanAction. If it is inactive, both fields will be
 * set to 0. If it is panning by user action, the values will be equivalent
 * to those returned by clutter_gesture_action_get_motion_coords().
 * If it is interpolating with some form of kinetic scrolling, the values
 * will be equivalent to those returned by
 * clutter_pan_action_get_interpolated_coords(). This is a convenience
 * method designed to be used in replacement "pan" signal handlers.
 *
 * Since: 1.14
 */
void
clutter_pan_action_get_motion_coords (ClutterPanAction *self,
                                      guint             point,
                                      gfloat           *motion_x,
                                      gfloat           *motion_y)
{
  ClutterPanActionPrivate *priv;

  g_return_if_fail (CLUTTER_IS_PAN_ACTION (self));

  priv = self->priv;

  switch (priv->state)
    {
    case PAN_STATE_INACTIVE:
      if (motion_x)
        *motion_x = 0;

      if (motion_y)
        *motion_y = 0;
      break;
    case PAN_STATE_PANNING:
      clutter_gesture_action_get_motion_coords (CLUTTER_GESTURE_ACTION (self),
                                                point, motion_x, motion_y);
      break;
    case PAN_STATE_INTERPOLATING:
      clutter_pan_action_get_interpolated_coords (self, motion_x, motion_y);
      break;
    default:
      g_assert_not_reached ();
    }
}
