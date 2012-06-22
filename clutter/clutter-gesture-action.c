/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
 * Copyright (C) 2011  Robert Bosch Car Multimedia GmbH.
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
 *   Tomeu Vizoso <tomeu.vizoso@collabora.co.uk>
 */

/**
 * SECTION:clutter-gesture-action
 * @Title: ClutterGestureAction
 * @Short_Description: Action for gesture gestures
 *
 * #ClutterGestureAction is a sub-class of #ClutterAction that implements
 * the logic for recognizing gesture gestures. It listens for low level events
 * such as #ClutterButtonEvent and #ClutterMotionEvent on the stage to raise
 * the #ClutterGestureAction::gesture-begin, #ClutterGestureAction::gesture-progress,
 * and #ClutterGestureAction::gesture-end signals.
 *
 * To use #ClutterGestureAction you just need to apply it to a #ClutterActor
 * using clutter_actor_add_action() and connect to the signals:
 *
 * |[
 *   ClutterAction *action = clutter_gesture_action_new ();
 *
 *   clutter_actor_add_action (actor, action);
 *
 *   g_signal_connect (action, "gesture-begin", G_CALLBACK (on_gesture_begin), NULL);
 *   g_signal_connect (action, "gesture-progress", G_CALLBACK (on_gesture_progress), NULL);
 *   g_signal_connect (action, "gesture-end", G_CALLBACK (on_gesture_end), NULL);
 * ]|
 *
 * Since: 1.8
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-gesture-action.h"

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

#define MAX_GESTURE_POINTS (10)

typedef struct
{
  ClutterInputDevice *device;
  ClutterEventSequence *sequence;

  gfloat press_x, press_y;
  gfloat last_motion_x, last_motion_y;
  gfloat release_x, release_y;
} GesturePoint;

struct _ClutterGestureActionPrivate
{
  ClutterActor *stage;

  gint requested_nb_points;
  GArray *points;

  guint actor_capture_id;
  gulong stage_capture_id;

  guint in_gesture : 1;
};

enum
{
  GESTURE_BEGIN,
  GESTURE_PROGRESS,
  GESTURE_END,
  GESTURE_CANCEL,

  LAST_SIGNAL
};

static guint gesture_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (ClutterGestureAction, clutter_gesture_action, CLUTTER_TYPE_ACTION);

static GesturePoint *
gesture_register_point (ClutterGestureAction *action, ClutterEvent *event)
{
  ClutterGestureActionPrivate *priv = action->priv;
  GesturePoint *point = NULL;

  if (priv->points->len >= MAX_GESTURE_POINTS)
    return NULL;

  g_array_set_size (priv->points, priv->points->len + 1);
  point = &g_array_index (priv->points, GesturePoint, priv->points->len - 1);

  point->device = clutter_event_get_device (event);

  clutter_event_get_coords (event, &point->press_x, &point->press_y);
  point->last_motion_x = point->press_x;
  point->last_motion_y = point->press_y;

  if (clutter_event_type (event) != CLUTTER_BUTTON_PRESS)
    point->sequence = clutter_event_get_event_sequence (event);
  else
    point->sequence = NULL;

  return point;
}

static GesturePoint *
gesture_find_point (ClutterGestureAction *action,
                    ClutterEvent *event,
                    gint *position)
{
  ClutterGestureActionPrivate *priv = action->priv;
  GesturePoint *point = NULL;
  ClutterEventType type = clutter_event_type (event);
  ClutterInputDevice *device = clutter_event_get_device (event);
  ClutterEventSequence *sequence = NULL;
  gint i;

  if ((type != CLUTTER_BUTTON_PRESS) &&
      (type != CLUTTER_BUTTON_RELEASE) &&
      (type != CLUTTER_MOTION))
    sequence = clutter_event_get_event_sequence (event);

  for (i = 0; i < priv->points->len; i++)
    {
      if ((g_array_index (priv->points, GesturePoint, i).device == device) &&
          (g_array_index (priv->points, GesturePoint, i).sequence == sequence))
        {
          if (position != NULL)
            *position = i;
          point = &g_array_index (priv->points, GesturePoint, i);
          break;
        }
    }

  return point;
}

static void
gesture_unregister_point (ClutterGestureAction *action, gint position)
{
  ClutterGestureActionPrivate *priv = action->priv;

  g_array_remove_index (priv->points, position);
}

static void
cancel_gesture (ClutterGestureAction *action)
{
  ClutterGestureActionPrivate *priv = action->priv;
  ClutterActor *actor;

  priv->in_gesture = FALSE;

  g_signal_handler_disconnect (priv->stage, priv->stage_capture_id);
  priv->stage_capture_id = 0;

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (action));
  g_signal_emit (action, gesture_signals[GESTURE_CANCEL], 0, actor);
}

static gboolean
stage_captured_event_cb (ClutterActor       *stage,
                         ClutterEvent       *event,
                         ClutterGestureAction *action)
{
  ClutterGestureActionPrivate *priv = action->priv;
  ClutterActor *actor;
  gint position;
  gboolean return_value;
  GesturePoint *point;

  if ((point = gesture_find_point (action, event, &position)) == NULL)
    return CLUTTER_EVENT_PROPAGATE;

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (action));

  switch (clutter_event_type (event))
    {
    case CLUTTER_MOTION:
      {
        ClutterModifierType mods = clutter_event_get_state (event);

        /* we might miss a button-release event in case of grabs,
         * so we need to check whether the button is still down
         * during a motion event
         */
        if (!(mods & CLUTTER_BUTTON1_MASK))
          {
            cancel_gesture (action);
            return CLUTTER_EVENT_PROPAGATE;
          }
      }
      /* Follow same code path as a touch event update */

    case CLUTTER_TOUCH_UPDATE:
      clutter_event_get_coords (event,
                                &point->last_motion_x,
                                &point->last_motion_y);

      if (priv->points->len < priv->requested_nb_points)
        return CLUTTER_EVENT_PROPAGATE;

      if (!priv->in_gesture)
        {
          gint drag_threshold;
          ClutterSettings *settings = clutter_settings_get_default ();

          g_object_get (settings,
                        "dnd-drag-threshold", &drag_threshold,
                        NULL);

          if ((ABS (point->press_y - point->last_motion_y) >= drag_threshold) ||
              (ABS (point->press_x - point->last_motion_x) >= drag_threshold))
            {
              priv->in_gesture = TRUE;

              g_signal_emit (action, gesture_signals[GESTURE_BEGIN], 0, actor,
                             &return_value);
              if (!return_value)
                {
                  cancel_gesture (action);
                  return CLUTTER_EVENT_PROPAGATE;
                }
            }
          else
            return CLUTTER_EVENT_PROPAGATE;
        }

      g_signal_emit (action, gesture_signals[GESTURE_PROGRESS], 0, actor,
                     &return_value);
      if (!return_value)
        {
          cancel_gesture (action);
          return CLUTTER_EVENT_PROPAGATE;
        }
      break;

    case CLUTTER_BUTTON_RELEASE:
    case CLUTTER_TOUCH_END:
      {
        clutter_event_get_coords (event, &point->release_x, &point->release_y);

        if (priv->in_gesture &&
            ((priv->points->len - 1) < priv->requested_nb_points))
          {
            priv->in_gesture = FALSE;
            g_signal_emit (action, gesture_signals[GESTURE_END], 0, actor);
          }

        gesture_unregister_point (action, position);
      }
      break;

    case CLUTTER_TOUCH_CANCEL:
      {
        if (priv->in_gesture)
          {
            priv->in_gesture = FALSE;
            cancel_gesture (action);
          }

        gesture_unregister_point (action, position);
      }
      break;

    default:
      break;
    }

  if (priv->points->len == 0)
    {
      g_signal_handler_disconnect (priv->stage, priv->stage_capture_id);
      priv->stage_capture_id = 0;
    }

  return CLUTTER_EVENT_PROPAGATE;
}

static gboolean
actor_captured_event_cb (ClutterActor *actor,
                         ClutterEvent *event,
                         ClutterGestureAction *action)
{
  ClutterGestureActionPrivate *priv = action->priv;
  GesturePoint *point;

  if ((clutter_event_type (event) != CLUTTER_BUTTON_PRESS) &&
      (clutter_event_type (event) != CLUTTER_TOUCH_BEGIN))
    return CLUTTER_EVENT_PROPAGATE;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (action)))
    return CLUTTER_EVENT_PROPAGATE;

  point = gesture_register_point (action, event);

  if (priv->stage == NULL)
    priv->stage = clutter_actor_get_stage (actor);

  if (priv->stage_capture_id == 0)
    priv->stage_capture_id =
      g_signal_connect_after (priv->stage, "captured-event",
                              G_CALLBACK (stage_captured_event_cb),
                              action);

  return CLUTTER_EVENT_PROPAGATE;
}

static void
clutter_gesture_action_set_actor (ClutterActorMeta *meta,
                                  ClutterActor     *actor)
{
  ClutterGestureActionPrivate *priv = CLUTTER_GESTURE_ACTION (meta)->priv;
  ClutterActorMetaClass *meta_class =
    CLUTTER_ACTOR_META_CLASS (clutter_gesture_action_parent_class);

  if (priv->actor_capture_id != 0)
    {
      ClutterActor *old_actor = clutter_actor_meta_get_actor (meta);

      if (old_actor != NULL)
        g_signal_handler_disconnect (old_actor, priv->actor_capture_id);

      priv->actor_capture_id = 0;
    }

  if (priv->stage_capture_id != 0)
    {
      if (priv->stage != NULL)
        g_signal_handler_disconnect (priv->stage, priv->stage_capture_id);

      priv->stage_capture_id = 0;
      priv->stage = NULL;
    }

  if (actor != NULL)
    {
      priv->actor_capture_id =
        g_signal_connect (actor, "captured-event",
                          G_CALLBACK (actor_captured_event_cb),
                          meta);
    }

  meta_class->set_actor (meta, actor);
}

static gboolean
default_event_handler (ClutterGestureAction *action,
                       ClutterActor *actor)
{
  return TRUE;
}

static void
clutter_gesture_action_class_init (ClutterGestureActionClass *klass)
{
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterGestureActionPrivate));

  meta_class->set_actor = clutter_gesture_action_set_actor;

  klass->gesture_begin = default_event_handler;
  klass->gesture_progress = default_event_handler;

  /**
   * ClutterGestureAction::gesture-begin:
   * @action: the #ClutterGestureAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   *
   * The ::gesture_begin signal is emitted when the #ClutterActor to which
   * a #ClutterGestureAction has been applied starts receiving a gesture.
   *
   * Return value: %TRUE if the gesture should start, and %FALSE if
   *   the gesture should be ignored.
   *
   * Since: 1.8
   */
  gesture_signals[GESTURE_BEGIN] =
    g_signal_new (I_("gesture-begin"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterGestureActionClass, gesture_begin),
                  _clutter_boolean_continue_accumulator, NULL,
                  _clutter_marshal_BOOLEAN__OBJECT,
                  G_TYPE_BOOLEAN, 1,
                  CLUTTER_TYPE_ACTOR);

  /**
   * ClutterGestureAction::gesture-progress:
   * @action: the #ClutterGestureAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   *
   * The ::gesture-progress signal is emitted for each motion event after
   * the #ClutterGestureAction::gesture-begin signal has been emitted.
   *
   * Return value: %TRUE if the gesture should continue, and %FALSE if
   *   the gesture should be cancelled.
   *
   * Since: 1.8
   */
  gesture_signals[GESTURE_PROGRESS] =
    g_signal_new (I_("gesture-progress"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterGestureActionClass, gesture_progress),
                  _clutter_boolean_continue_accumulator, NULL,
                  _clutter_marshal_BOOLEAN__OBJECT,
                  G_TYPE_BOOLEAN, 1,
                  CLUTTER_TYPE_ACTOR);

  /**
   * ClutterGestureAction::gesture-end:
   * @action: the #ClutterGestureAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   *
   * The ::gesture-end signal is emitted at the end of the gesture gesture,
   * when the pointer's button is released
   *
   * This signal is emitted if and only if the #ClutterGestureAction::gesture-begin
   * signal has been emitted first.
   *
   * Since: 1.8
   */
  gesture_signals[GESTURE_END] =
    g_signal_new (I_("gesture-end"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterGestureActionClass, gesture_end),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);

  /**
   * ClutterGestureAction::gesture-cancel:
   * @action: the #ClutterGestureAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   *
   * The ::gesture-cancel signal is emitted when the ongoing gesture gets
   * cancelled from the #ClutterGestureAction::gesture-progress signal handler.
   *
   * This signal is emitted if and only if the #ClutterGestureAction::gesture-begin
   * signal has been emitted first.
   *
   * Since: 1.8
   */
  gesture_signals[GESTURE_CANCEL] =
    g_signal_new (I_("gesture-cancel"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterGestureActionClass, gesture_cancel),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);
}

static void
clutter_gesture_action_init (ClutterGestureAction *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, CLUTTER_TYPE_GESTURE_ACTION,
                                            ClutterGestureActionPrivate);

  self->priv->points = g_array_sized_new (FALSE, TRUE, sizeof (GesturePoint), 3);
  self->priv->requested_nb_points = 1;
}

/**
 * clutter_gesture_action_new:
 *
 * Creates a new #ClutterGestureAction instance.
 *
 * Return value: the newly created #ClutterGestureAction
 *
 * Since: 1.8
 */
ClutterAction *
clutter_gesture_action_new (void)
{
  return g_object_new (CLUTTER_TYPE_GESTURE_ACTION, NULL);
}

/**
 * clutter_gesture_action_get_press_coords:
 * @action: a #ClutterGestureAction
 * @device: currently unused, set to 0
 * @press_x: (out): return location for the press event's X coordinate
 * @press_y: (out): return location for the press event's Y coordinate
 *
 * Retrieves the coordinates, in stage space, of the press event
 * that started the dragging for an specific pointer device
 *
 * Since: 1.8
 */
void
clutter_gesture_action_get_press_coords (ClutterGestureAction *action,
                                         guint                 device,
                                         gfloat               *press_x,
                                         gfloat               *press_y)
{
  g_return_if_fail (CLUTTER_IS_GESTURE_ACTION (action));
  g_return_if_fail (action->priv->points->len > device);

  if (press_x)
    *press_x = g_array_index (action->priv->points,
                              GesturePoint,
                              device).press_x;

  if (press_y)
    *press_y = g_array_index (action->priv->points,
                              GesturePoint,
                              device).press_y;
}

/**
 * clutter_gesture_action_get_motion_coords:
 * @action: a #ClutterGestureAction
 * @device: currently unused, set to 0
 * @motion_x: (out): return location for the latest motion
 *   event's X coordinate
 * @motion_y: (out): return location for the latest motion
 *   event's Y coordinate
 *
 * Retrieves the coordinates, in stage space, of the latest motion
 * event during the dragging
 *
 * Since: 1.8
 */
void
clutter_gesture_action_get_motion_coords (ClutterGestureAction *action,
                                          guint                 device,
                                          gfloat               *motion_x,
                                          gfloat               *motion_y)
{
  g_return_if_fail (CLUTTER_IS_GESTURE_ACTION (action));
  g_return_if_fail (action->priv->points->len > device);

  if (motion_x)
    *motion_x = g_array_index (action->priv->points,
                               GesturePoint,
                               device).last_motion_x;

  if (motion_y)
    *motion_y = g_array_index (action->priv->points,
                               GesturePoint,
                               device).last_motion_y;
}

/**
 * clutter_gesture_action_get_release_coords:
 * @action: a #ClutterGestureAction
 * @device: currently unused, set to 0
 * @release_x: (out): return location for the X coordinate of the last release
 * @release_y: (out): return location for the Y coordinate of the last release
 *
 * Retrieves the coordinates, in stage space, of the point where the pointer
 * device was last released.
 *
 * Since: 1.8
 */
void
clutter_gesture_action_get_release_coords (ClutterGestureAction *action,
                                           guint                 device,
                                           gfloat               *release_x,
                                           gfloat               *release_y)
{
  g_return_if_fail (CLUTTER_IS_GESTURE_ACTION (action));
  g_return_if_fail (action->priv->points->len > device);

  if (release_x)
    *release_x = g_array_index (action->priv->points,
                                GesturePoint,
                                device).release_x;

  if (release_y)
    *release_y = g_array_index (action->priv->points,
                                GesturePoint,
                                device).release_y;
}

/**
 * clutter_gesture_action_get_n_touch_points:
 * @action: a #ClutterGestureAction
 *
 * Retrieves the number of requested points to trigger the gesture.
 *
 * Return value: the number of points to trigger the gesture.
 *
 * Since: 1.12
 */
gint
clutter_gesture_action_get_n_touch_points (ClutterGestureAction *action)
{
  g_return_val_if_fail (CLUTTER_IS_GESTURE_ACTION (action), 0);

  return action->priv->requested_nb_points;
}

/**
 * clutter_gesture_action_set_n_touch_points:
 * @action: a #ClutterGestureAction
 * @nb_points: a number of points
 *
 * Sets the number of points needed to trigger the gesture.
 *
 * Since: 1.12
 */
void
clutter_gesture_action_set_n_touch_points (ClutterGestureAction *action,
                                           gint                  nb_points)
{
  ClutterGestureActionPrivate *priv;

  g_return_if_fail (CLUTTER_IS_GESTURE_ACTION (action));
  g_return_if_fail (nb_points >= 1);

  priv = action->priv;

  priv->requested_nb_points = nb_points;

  if (priv->in_gesture)
    {
      if (priv->points->len < priv->requested_nb_points)
        cancel_gesture (action);
    }
  else
    {
      if (priv->points->len >= priv->requested_nb_points)
        {
          ClutterActor *actor =
            clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (action));
          ClutterSettings *settings = clutter_settings_get_default ();
          gint i, drag_threshold;

          g_object_get (settings,
                        "dnd-drag-threshold", &drag_threshold,
                        NULL);

          for (i = 0; i < priv->points->len; i++)
            {
              GesturePoint *point = &g_array_index (priv->points, GesturePoint, i);

              if ((ABS (point->press_y - point->last_motion_y) >= drag_threshold) ||
                  (ABS (point->press_x - point->last_motion_x) >= drag_threshold))
                {
                  gboolean return_value;

                  priv->in_gesture = TRUE;

                  g_signal_emit (action, gesture_signals[GESTURE_BEGIN], 0, actor,
                                 &return_value);

                  if (!return_value)
                    cancel_gesture (action);

                  break;
                }
            }
        }
    }
}
