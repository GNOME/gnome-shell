/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright © 2011  Intel Corporation.
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
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

/**
 * SECTION:clutter-drop-action
 * @Title: ClutterDropAction
 * @short_description: An action for drop targets
 *
 * #ClutterDropAction is a #ClutterAction that allows a #ClutterActor
 * implementation to control what happens when an actor dragged using
 * a #ClutterDragAction crosses the target area or when a dragged actor
 * is released (or "dropped") on the target area.
 *
 * A trivial use of #ClutterDropAction consists in connecting to the
 * #ClutterDropAction::drop signal and handling the drop from there,
 * for instance:
 *
 * |[<!-- language="C" -->
 *   ClutterAction *action = clutter_drop_action ();
 *
 *   g_signal_connect (action, "drop", G_CALLBACK (on_drop), NULL);
 *   clutter_actor_add_action (an_actor, action);
 * ]|
 *
 * The #ClutterDropAction::can-drop can be used to control whether the
 * #ClutterDropAction::drop signal is going to be emitted; returning %FALSE
 * from a handler connected to the #ClutterDropAction::can-drop signal will
 * cause the #ClutterDropAction::drop signal to be skipped when the input
 * device button is released.
 *
 * It's important to note that #ClutterDropAction will only work with
 * actors dragged using #ClutterDragAction.
 *
 * See [drop-action.c](https://git.gnome.org/browse/clutter/tree/examples/drop-action.c?h=clutter-1.18)
 * for an example of how to use #ClutterDropAction.
 *
 * #ClutterDropAction is available since Clutter 1.8
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-drop-action.h"

#include "clutter-actor-meta-private.h"
#include "clutter-actor-private.h"
#include "clutter-drag-action.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-stage-private.h"

struct _ClutterDropActionPrivate
{
  ClutterActor *actor;
  ClutterActor *stage;

  gulong mapped_id;
};

typedef struct _DropTarget {
  ClutterActor *stage;

  gulong capture_id;

  GHashTable *actions;

  ClutterDropAction *last_action;
} DropTarget;

enum
{
  CAN_DROP,
  OVER_IN,
  OVER_OUT,
  DROP,
  DROP_CANCEL,

  LAST_SIGNAL
};

static guint drop_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (ClutterDropAction, clutter_drop_action, CLUTTER_TYPE_ACTION)

static void
drop_target_free (gpointer _data)
{
  DropTarget *data = _data;

  g_signal_handler_disconnect (data->stage, data->capture_id);
  g_hash_table_destroy (data->actions);
  g_free (data);
}

static gboolean
on_stage_capture (ClutterStage *stage,
                  ClutterEvent *event,
                  gpointer      user_data)
{
  DropTarget *data = user_data;
  gfloat event_x, event_y;
  ClutterActor *actor, *drag_actor;
  ClutterDropAction *drop_action;
  ClutterInputDevice *device;
  gboolean was_reactive;

  switch (clutter_event_type (event))
    {
    case CLUTTER_MOTION:
    case CLUTTER_BUTTON_RELEASE:
      if (clutter_event_type (event) == CLUTTER_MOTION &&
          !(clutter_event_get_state (event) & CLUTTER_BUTTON1_MASK))
        return CLUTTER_EVENT_PROPAGATE;

      if (clutter_event_type (event) == CLUTTER_BUTTON_RELEASE &&
          clutter_event_get_button (event) != CLUTTER_BUTTON_PRIMARY)
        return CLUTTER_EVENT_PROPAGATE;

      device = clutter_event_get_device (event);
      drag_actor = _clutter_stage_get_pointer_drag_actor (stage, device);
      if (drag_actor == NULL)
        return CLUTTER_EVENT_PROPAGATE;
      break;

    case CLUTTER_TOUCH_UPDATE:
    case CLUTTER_TOUCH_END:
      drag_actor = _clutter_stage_get_touch_drag_actor (stage,
                                                        clutter_event_get_event_sequence (event));
      if (drag_actor == NULL)
        return CLUTTER_EVENT_PROPAGATE;
      break;

    default:
      return CLUTTER_EVENT_PROPAGATE;
    }

  clutter_event_get_coords (event, &event_x, &event_y);

  /* get the actor under the cursor, excluding the dragged actor; we
   * use reactivity because it won't cause any scene invalidation
   */
  was_reactive = clutter_actor_get_reactive (drag_actor);
  clutter_actor_set_reactive (drag_actor, FALSE);

  actor = clutter_stage_get_actor_at_pos (stage, CLUTTER_PICK_REACTIVE,
                                          event_x,
                                          event_y);
  if (actor == NULL || actor == CLUTTER_ACTOR (stage))
    {
      if (data->last_action != NULL)
        {
          ClutterActorMeta *meta = CLUTTER_ACTOR_META (data->last_action);

          g_signal_emit (data->last_action, drop_signals[OVER_OUT], 0,
                         clutter_actor_meta_get_actor (meta));

          data->last_action = NULL;
        }

      goto out;
    }

  drop_action = g_hash_table_lookup (data->actions, actor);

  if (drop_action == NULL)
    {
      if (data->last_action != NULL)
        {
          ClutterActorMeta *meta = CLUTTER_ACTOR_META (data->last_action);

          g_signal_emit (data->last_action, drop_signals[OVER_OUT], 0,
                         clutter_actor_meta_get_actor (meta));

          data->last_action = NULL;
        }

      goto out;
    }
  else
    {
      if (data->last_action != drop_action)
        {
          ClutterActorMeta *meta;

          if (data->last_action != NULL)
            {
              meta = CLUTTER_ACTOR_META (data->last_action);

              g_signal_emit (data->last_action, drop_signals[OVER_OUT], 0,
                             clutter_actor_meta_get_actor (meta));
            }

          meta = CLUTTER_ACTOR_META (drop_action);

          g_signal_emit (drop_action, drop_signals[OVER_IN], 0,
                         clutter_actor_meta_get_actor (meta));
        }

      data->last_action = drop_action;
    }

out:
  if (clutter_event_type (event) == CLUTTER_BUTTON_RELEASE ||
      clutter_event_type (event) == CLUTTER_TOUCH_END)
    {
      if (data->last_action != NULL)
        {
          ClutterActorMeta *meta = CLUTTER_ACTOR_META (data->last_action);
          gboolean can_drop = FALSE;

          g_signal_emit (data->last_action, drop_signals[CAN_DROP], 0,
                         clutter_actor_meta_get_actor (meta),
                         event_x, event_y,
                         &can_drop);

          if (can_drop)
            {
              g_signal_emit (data->last_action, drop_signals[DROP], 0,
                             clutter_actor_meta_get_actor (meta),
                             event_x, event_y);
            }
	  else
            {
              g_signal_emit (data->last_action, drop_signals[DROP_CANCEL], 0,
                             clutter_actor_meta_get_actor (meta),
                             event_x, event_y);
            }

        }

      data->last_action = NULL;
    }

  if (drag_actor != NULL)
    clutter_actor_set_reactive (drag_actor, was_reactive);

  return CLUTTER_EVENT_PROPAGATE;
}

static void
drop_action_register (ClutterDropAction *self)
{
  ClutterDropActionPrivate *priv = self->priv;
  DropTarget *data;

  g_assert (priv->stage != NULL);

  data = g_object_get_data (G_OBJECT (priv->stage), "__clutter_drop_targets");
  if (data == NULL)
    {
      data = g_new0 (DropTarget, 1);

      data->stage = priv->stage;
      data->actions = g_hash_table_new (NULL, NULL);
      data->capture_id = g_signal_connect (priv->stage, "captured-event",
                                           G_CALLBACK (on_stage_capture),
                                           data);
      g_object_set_data_full (G_OBJECT (priv->stage), "__clutter_drop_targets",
                              data,
                              drop_target_free);
    }

  g_hash_table_replace (data->actions, priv->actor, self);
}

static void
drop_action_unregister (ClutterDropAction *self)
{
  ClutterDropActionPrivate *priv = self->priv;
  DropTarget *data = NULL;

  if (priv->stage != NULL)
    data = g_object_get_data (G_OBJECT (priv->stage), "__clutter_drop_targets");

  if (data == NULL)
    return;

  g_hash_table_remove (data->actions, priv->actor);
  if (g_hash_table_size (data->actions) == 0)
    g_object_set_data (G_OBJECT (data->stage), "__clutter_drop_targets", NULL);
}

static void
on_actor_mapped (ClutterActor      *actor,
                 GParamSpec        *pspec,
                 ClutterDropAction *self)
{
  if (CLUTTER_ACTOR_IS_MAPPED (actor))
    {
      if (self->priv->stage == NULL)
        self->priv->stage = clutter_actor_get_stage (actor);

      drop_action_register (self);
    }
  else
    drop_action_unregister (self);
}

static void
clutter_drop_action_set_actor (ClutterActorMeta *meta,
                               ClutterActor     *actor)
{
  ClutterDropActionPrivate *priv = CLUTTER_DROP_ACTION (meta)->priv;

  if (priv->actor != NULL)
    {
      drop_action_unregister (CLUTTER_DROP_ACTION (meta));

      if (priv->mapped_id != 0)
        g_signal_handler_disconnect (priv->actor, priv->mapped_id);

      priv->stage = NULL;
      priv->actor = NULL;
      priv->mapped_id = 0;
    }

  priv->actor = actor;

  if (priv->actor != NULL)
    {
      priv->stage = clutter_actor_get_stage (actor);
      priv->mapped_id = g_signal_connect (actor, "notify::mapped",
                                          G_CALLBACK (on_actor_mapped),
                                          meta);

      if (priv->stage != NULL)
        drop_action_register (CLUTTER_DROP_ACTION (meta));
    }

  CLUTTER_ACTOR_META_CLASS (clutter_drop_action_parent_class)->set_actor (meta, actor);
}

static gboolean
signal_accumulator (GSignalInvocationHint *ihint,
                    GValue                *return_accu,
                    const GValue          *handler_return,
                    gpointer               user_data)
{
  gboolean continue_emission;

  continue_emission = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, continue_emission);

  return continue_emission;
}

static gboolean
clutter_drop_action_real_can_drop (ClutterDropAction *action,
                                   ClutterActor      *actor,
                                   gfloat             event_x,
                                   gfloat             event_y)
{
  return TRUE;
}

static void
clutter_drop_action_class_init (ClutterDropActionClass *klass)
{
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);

  meta_class->set_actor = clutter_drop_action_set_actor;

  klass->can_drop = clutter_drop_action_real_can_drop;

  /**
   * ClutterDropAction::can-drop:
   * @action: the #ClutterDropAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   * @event_x: the X coordinate (in stage space) of the drop event
   * @event_y: the Y coordinate (in stage space) of the drop event
   *
   * The ::can-drop signal is emitted when the dragged actor is dropped
   * on @actor. The return value of the ::can-drop signal will determine
   * whether or not the #ClutterDropAction::drop signal is going to be
   * emitted on @action.
   *
   * The default implementation of #ClutterDropAction returns %TRUE for
   * this signal.
   *
   * Return value: %TRUE if the drop is accepted, and %FALSE otherwise
   *
   * Since: 1.8
   */
  drop_signals[CAN_DROP] =
    g_signal_new (I_("can-drop"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterDropActionClass, can_drop),
                  signal_accumulator, NULL,
                  _clutter_marshal_BOOLEAN__OBJECT_FLOAT_FLOAT,
                  G_TYPE_BOOLEAN, 3,
                  CLUTTER_TYPE_ACTOR,
                  G_TYPE_FLOAT,
                  G_TYPE_FLOAT);

  /**
   * ClutterDropAction::over-in:
   * @action: the #ClutterDropAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   *
   * The ::over-in signal is emitted when the dragged actor crosses
   * into @actor.
   *
   * Since: 1.8
   */
  drop_signals[OVER_IN] =
    g_signal_new (I_("over-in"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterDropActionClass, over_in),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);

  /**
   * ClutterDropAction::over-out:
   * @action: the #ClutterDropAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   *
   * The ::over-out signal is emitted when the dragged actor crosses
   * outside @actor.
   *
   * Since: 1.8
   */
  drop_signals[OVER_OUT] =
    g_signal_new (I_("over-out"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterDropActionClass, over_out),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);

  /**
   * ClutterDropAction::drop:
   * @action: the #ClutterDropAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   * @event_x: the X coordinate (in stage space) of the drop event
   * @event_y: the Y coordinate (in stage space) of the drop event
   *
   * The ::drop signal is emitted when the dragged actor is dropped
   * on @actor. This signal is only emitted if at least an handler of
   * #ClutterDropAction::can-drop returns %TRUE.
   *
   * Since: 1.8
   */
  drop_signals[DROP] =
    g_signal_new (I_("drop"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterDropActionClass, drop),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT_FLOAT_FLOAT,
                  G_TYPE_NONE, 3,
                  CLUTTER_TYPE_ACTOR,
                  G_TYPE_FLOAT,
                  G_TYPE_FLOAT);


  /**
   * ClutterDropAction::drop-cancel:
   * @action: the #ClutterDropAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   * @event_x: the X coordinate (in stage space) of the drop event
   * @event_y: the Y coordinate (in stage space) of the drop event
   *
   * The ::drop-cancel signal is emitted when the drop is refused
   * by an emission of the #ClutterDropAction::can-drop signal.
   *
   * After the ::drop-cancel signal is fired the active drag is
   * terminated.
   *
   * Since: 1.12
   */
  drop_signals[DROP_CANCEL] =
    g_signal_new (I_("drop-cancel"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterDropActionClass, drop),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT_FLOAT_FLOAT,
                  G_TYPE_NONE, 3,
                  CLUTTER_TYPE_ACTOR,
                  G_TYPE_FLOAT,
                  G_TYPE_FLOAT);
}

static void
clutter_drop_action_init (ClutterDropAction *self)
{
  self->priv = clutter_drop_action_get_instance_private (self);
}

/**
 * clutter_drop_action_new:
 *
 * Creates a new #ClutterDropAction.
 *
 * Use clutter_actor_add_action() to add the action to a #ClutterActor.
 *
 * Return value: the newly created #ClutterDropAction
 *
 * Since: 1.8
 */
ClutterAction *
clutter_drop_action_new (void)
{
  return g_object_new (CLUTTER_TYPE_DROP_ACTION, NULL);
}
