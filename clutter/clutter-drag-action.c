/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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
 * SECTION:clutter-drag-action
 * @Title: ClutterDragAction
 * @Short_Description: Action enabling dragging on actors
 *
 * #ClutterDragAction is a sub-class of #ClutterAction that implements
 * all the necessary logic for dragging actors.
 *
 * The simplest usage of #ClutterDragAction consists in adding it to
 * a #ClutterActor and setting it as reactive; for instance, the following
 * code:
 *
 * |[
 *   clutter_actor_add_action (actor, clutter_drag_action_new ());
 *   clutter_actor_set_reactive (actor, TRUE);
 * ]|
 *
 * will automatically result in the actor moving to follow the pointer
 * whenever the pointer's button is pressed over the actor and moved
 * across the stage.
 *
 * The #ClutterDragAction will signal the begin and the end of a dragging
 * through the #ClutterDragAction::drag-begin and #ClutterDragAction::drag-end
 * signals, respectively. Each pointer motion during a drag will also result
 * in the #ClutterDragAction::drag-motion signal to be emitted.
 *
 * It is also possible to set another #ClutterActor as the dragged actor
 * by calling clutter_drag_action_set_drag_handle() from within a handle
 * of the #ClutterDragAction::drag-begin signal. The drag handle must be
 * parented and exist between the emission of #ClutterDragAction::drag-begin
 * and #ClutterDragAction::drag-end.
 *
 * <example id="drag-action-example">
 *   <title>A simple draggable actor</title>
 *   <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../examples/drag-action.c">
 *   <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 *   </programlisting>
 *   <para>The example program above allows dragging the rectangle around
 *   the stage using a #ClutterDragAction. When pressing the
 *   <keycap>Shift</keycap> key the actor that is going to be dragged is a
 *   separate rectangle, and when the drag ends, the original rectangle will
 *   be animated to the final coordinates.</para>
 * </example>
 *
 * #ClutterDragAction is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-drag-action.h"

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

struct _ClutterDragActionPrivate
{
  ClutterStage *stage;

  gint x_drag_threshold;
  gint y_drag_threshold;
  ClutterActor *drag_handle;
  ClutterDragAxis drag_axis;
  ClutterRect drag_area;

  ClutterInputDevice *device;
  ClutterEventSequence *sequence;
  gulong button_press_id;
  gulong touch_begin_id;
  gulong capture_id;

  gfloat press_x;
  gfloat press_y;
  ClutterModifierType press_state;

  gfloat last_motion_x;
  gfloat last_motion_y;
  ClutterModifierType last_motion_state;
  ClutterInputDevice *last_motion_device;

  gfloat transformed_press_x;
  gfloat transformed_press_y;

  guint emit_delayed_press    : 1;
  guint in_drag               : 1;
  guint motion_events_enabled : 1;
  guint drag_area_set         : 1;
};

enum
{
  PROP_0,

  PROP_X_DRAG_THRESHOLD,
  PROP_Y_DRAG_THRESHOLD,
  PROP_DRAG_HANDLE,
  PROP_DRAG_AXIS,
  PROP_DRAG_AREA,
  PROP_DRAG_AREA_SET,

  PROP_LAST
};

static GParamSpec *drag_props[PROP_LAST] = { NULL, };

enum
{
  DRAG_BEGIN,
  DRAG_PROGRESS,
  DRAG_MOTION,
  DRAG_END,

  LAST_SIGNAL
};

static guint drag_signals[LAST_SIGNAL] = { 0, };

/* forward declaration */
static gboolean on_captured_event (ClutterActor      *stage,
                                   ClutterEvent      *event,
                                   ClutterDragAction *action);

G_DEFINE_TYPE_WITH_PRIVATE (ClutterDragAction, clutter_drag_action, CLUTTER_TYPE_ACTION)

static void
get_drag_threshold (ClutterDragAction *action,
                    gint              *x_threshold,
                    gint              *y_threshold)
{
  ClutterDragActionPrivate *priv = action->priv;
  ClutterSettings *settings = clutter_settings_get_default ();
  gint x_res, y_res, default_threshold;

  g_object_get (settings, "dnd-drag-threshold", &default_threshold, NULL);

  if (priv->x_drag_threshold < 0)
    x_res = default_threshold;
  else
    x_res = priv->x_drag_threshold;

  if (priv->y_drag_threshold < 0)
    y_res = default_threshold;
  else
    y_res = priv->y_drag_threshold;

  if (x_threshold != NULL)
    *x_threshold = x_res;

  if (y_threshold != NULL)
    *y_threshold = y_res;
}

static void
emit_drag_begin (ClutterDragAction *action,
                 ClutterActor      *actor,
                 ClutterEvent      *event)
{
  ClutterDragActionPrivate *priv = action->priv;

  if (priv->stage != NULL)
    {
      clutter_stage_set_motion_events_enabled (priv->stage, FALSE);
      if (clutter_event_type (event) == CLUTTER_TOUCH_BEGIN)
        _clutter_stage_add_touch_drag_actor (priv->stage,
                                             clutter_event_get_event_sequence (event),
                                             priv->drag_handle != NULL
                                             ? priv->drag_handle
                                             : actor);
      else
        _clutter_stage_add_pointer_drag_actor (priv->stage,
                                               clutter_event_get_device (event),
                                               priv->drag_handle != NULL
                                               ? priv->drag_handle
                                               : actor);
    }

  g_signal_emit (action, drag_signals[DRAG_BEGIN], 0,
                 actor,
                 priv->press_x, priv->press_y,
                 priv->press_state);
}

static void
emit_drag_motion (ClutterDragAction *action,
                  ClutterActor      *actor,
                  ClutterEvent      *event)
{
  ClutterDragActionPrivate *priv = action->priv;
  ClutterActor *drag_handle = NULL;
  gfloat delta_x, delta_y;
  gfloat motion_x, motion_y;
  gboolean can_emit_drag_motion = TRUE;

  clutter_event_get_coords (event, &priv->last_motion_x, &priv->last_motion_y);
  priv->last_motion_state = clutter_event_get_state (event);
  priv->last_motion_device = clutter_event_get_device (event);

  if (priv->drag_handle != NULL && !priv->emit_delayed_press)
    drag_handle = priv->drag_handle;
  else
    drag_handle = actor;

  motion_x = motion_y = 0.0f;
  clutter_actor_transform_stage_point (drag_handle,
                                       priv->last_motion_x,
                                       priv->last_motion_y,
                                       &motion_x, &motion_y);

  delta_x = delta_y = 0.0f;

  switch (priv->drag_axis)
    {
    case CLUTTER_DRAG_AXIS_NONE:
      delta_x = motion_x - priv->transformed_press_x;
      delta_y = motion_y - priv->transformed_press_y;
      break;

    case CLUTTER_DRAG_X_AXIS:
      delta_x = motion_x - priv->transformed_press_x;
      break;

    case CLUTTER_DRAG_Y_AXIS:
      delta_y = motion_y - priv->transformed_press_y;
      break;

    default:
      g_assert_not_reached ();
      return;
    }

  if (priv->emit_delayed_press)
    {
      gint x_drag_threshold, y_drag_threshold;

      get_drag_threshold (action, &x_drag_threshold, &y_drag_threshold);

      if (ABS (delta_x) >= x_drag_threshold ||
          ABS (delta_y) >= y_drag_threshold)
        {
          priv->emit_delayed_press = FALSE;

          emit_drag_begin (action, actor, event);
        }
      else
        return;
    }

  g_signal_emit (action, drag_signals[DRAG_PROGRESS], 0,
                 actor,
                 delta_x, delta_y,
                 &can_emit_drag_motion);

  if (can_emit_drag_motion)
    {
      g_signal_emit (action, drag_signals[DRAG_MOTION], 0,
                     actor,
                     delta_x, delta_y);
    }
}

static void
emit_drag_end (ClutterDragAction *action,
               ClutterActor      *actor,
               ClutterEvent      *event)
{
  ClutterDragActionPrivate *priv = action->priv;

  /* ::drag-end may result in the destruction of the actor, which in turn
   * will lead to the removal and finalization of the action, so we need
   * to keep the action alive for the entire emission sequence
   */
  g_object_ref (action);

  /* if we have an event, update our own state, otherwise we'll
   * just use the currently stored state when emitting the ::drag-end
   * signal
   */
  if (event != NULL)
    {
      clutter_event_get_coords (event, &priv->last_motion_x, &priv->last_motion_y);
      priv->last_motion_state = clutter_event_get_state (event);
      priv->last_motion_device = clutter_event_get_device (event);
    }

  priv->in_drag = FALSE;

  /* we might not have emitted ::drag-begin yet */
  if (!priv->emit_delayed_press)
    g_signal_emit (action, drag_signals[DRAG_END], 0,
                   actor,
                   priv->last_motion_x, priv->last_motion_y,
                   priv->last_motion_state);

  if (priv->stage == NULL)
    goto out;

  /* disconnect the capture */
  if (priv->capture_id != 0)
    {
      g_signal_handler_disconnect (priv->stage, priv->capture_id);
      priv->capture_id = 0;
    }

  clutter_stage_set_motion_events_enabled (priv->stage,
                                           priv->motion_events_enabled);

  if (priv->last_motion_device != NULL && event != NULL)
    {
      if (clutter_event_type (event) == CLUTTER_BUTTON_RELEASE)
        _clutter_stage_remove_pointer_drag_actor (priv->stage,
                                                  priv->last_motion_device);
      else
        _clutter_stage_remove_touch_drag_actor (priv->stage,
                                                priv->sequence);
    }

out:
  priv->last_motion_device = NULL;
  priv->sequence = NULL;

  g_object_unref (action);
}

static gboolean
on_captured_event (ClutterActor      *stage,
                   ClutterEvent      *event,
                   ClutterDragAction *action)
{
  ClutterDragActionPrivate *priv = action->priv;
  ClutterActor *actor;

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (action));

  if (!priv->in_drag)
    return CLUTTER_EVENT_PROPAGATE;

  if (clutter_event_get_device (event) != priv->device)
    return CLUTTER_EVENT_PROPAGATE;

  switch (clutter_event_type (event))
    {
    case CLUTTER_TOUCH_UPDATE:
      if (clutter_event_get_event_sequence (event) == priv->sequence)
        emit_drag_motion (action, actor, event);
      break;

    case CLUTTER_MOTION:
      {
        ClutterModifierType mods = clutter_event_get_state (event);

        /* we might miss a button-release event in case of grabs,
         * so we need to check whether the button is still down
         * during a motion event
         */
        if (mods & CLUTTER_BUTTON1_MASK)
          emit_drag_motion (action, actor, event);
        else
          emit_drag_end (action, actor, event);
      }
      break;

    case CLUTTER_TOUCH_END:
    case CLUTTER_TOUCH_CANCEL:
      if (clutter_event_get_event_sequence (event) == priv->sequence)
        emit_drag_end (action, actor, event);
      break;

    case CLUTTER_BUTTON_RELEASE:
      if (priv->in_drag)
        emit_drag_end (action, actor, event);
      break;

    case CLUTTER_ENTER:
    case CLUTTER_LEAVE:
      if (priv->in_drag)
        return CLUTTER_EVENT_STOP;
      break;

    default:
      break;
    }

  return CLUTTER_EVENT_PROPAGATE;
}

static gboolean
on_drag_begin (ClutterActor      *actor,
               ClutterEvent      *event,
               ClutterDragAction *action)
{
  ClutterDragActionPrivate *priv = action->priv;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (action)))
    return CLUTTER_EVENT_PROPAGATE;

  /* dragging is only performed using the primary button */
  switch (clutter_event_type (event))
    {
    case CLUTTER_BUTTON_PRESS:
      if (clutter_event_get_button (event) != CLUTTER_BUTTON_PRIMARY)
        return CLUTTER_EVENT_PROPAGATE;
      break;

    case CLUTTER_TOUCH_BEGIN:
      if (priv->sequence != NULL)
        return CLUTTER_EVENT_PROPAGATE;
      priv->sequence = clutter_event_get_event_sequence (event);
      break;

    default:
      return CLUTTER_EVENT_PROPAGATE;
    }

  if (priv->stage == NULL)
    priv->stage = CLUTTER_STAGE (clutter_actor_get_stage (actor));

  clutter_event_get_coords (event, &priv->press_x, &priv->press_y);
  priv->press_state = clutter_event_get_state (event);

  priv->device = clutter_event_get_device (event);

  priv->last_motion_x = priv->press_x;
  priv->last_motion_y = priv->press_y;

  priv->transformed_press_x = priv->press_x;
  priv->transformed_press_y = priv->press_y;
  clutter_actor_transform_stage_point (actor, priv->press_x, priv->press_y,
                                       &priv->transformed_press_x,
                                       &priv->transformed_press_y);

  priv->motion_events_enabled =
    clutter_stage_get_motion_events_enabled (priv->stage);

  if (priv->x_drag_threshold == 0 || priv->y_drag_threshold == 0)
    emit_drag_begin (action, actor, event);
  else
    priv->emit_delayed_press = TRUE;

  priv->in_drag = TRUE;
  priv->capture_id = g_signal_connect_after (priv->stage, "captured-event",
                                             G_CALLBACK (on_captured_event),
                                             action);

  return CLUTTER_EVENT_PROPAGATE;
}

static void
clutter_drag_action_set_actor (ClutterActorMeta *meta,
                               ClutterActor     *actor)
{
  ClutterDragActionPrivate *priv = CLUTTER_DRAG_ACTION (meta)->priv;

  if (priv->button_press_id != 0)
    {
      ClutterActor *old_actor;

      old_actor = clutter_actor_meta_get_actor (meta);
      if (old_actor != NULL)
        {
          g_signal_handler_disconnect (old_actor, priv->button_press_id);
          g_signal_handler_disconnect (old_actor, priv->touch_begin_id);
        }

      priv->button_press_id = 0;
      priv->touch_begin_id = 0;
    }

  if (priv->capture_id != 0)
    {
      if (priv->stage != NULL)
        g_signal_handler_disconnect (priv->stage, priv->capture_id);

      priv->capture_id = 0;
      priv->stage = NULL;
    }

  clutter_drag_action_set_drag_handle (CLUTTER_DRAG_ACTION (meta), NULL);

  priv->in_drag = FALSE;

  if (actor != NULL)
    {
      priv->button_press_id = g_signal_connect (actor, "button-press-event",
                                                G_CALLBACK (on_drag_begin),
                                                meta);
      priv->touch_begin_id = g_signal_connect (actor, "touch-event",
                                               G_CALLBACK (on_drag_begin),
                                               meta);
    }

  CLUTTER_ACTOR_META_CLASS (clutter_drag_action_parent_class)->set_actor (meta, actor);
}

static gboolean
clutter_drag_action_real_drag_progress (ClutterDragAction *action,
                                        ClutterActor      *actor,
                                        gfloat             delta_x,
                                        gfloat             delta_y)
{
  return TRUE;
}

static void
clutter_drag_action_real_drag_motion (ClutterDragAction *action,
                                      ClutterActor      *actor,
                                      gfloat             delta_x,
                                      gfloat             delta_y)
{
  ClutterActor *drag_handle;
  gfloat x, y;

  if (action->priv->drag_handle != NULL)
    drag_handle = action->priv->drag_handle;
  else
    drag_handle = actor;

  clutter_actor_get_position (drag_handle, &x, &y);

  x += delta_x;
  y += delta_y;

  if (action->priv->drag_area_set)
    {
      ClutterRect *drag_area = &action->priv->drag_area;

      x = CLAMP (x, drag_area->origin.x, drag_area->origin.x + drag_area->size.width);
      y = CLAMP (y, drag_area->origin.y, drag_area->origin.y + drag_area->size.height);
    }

  clutter_actor_set_position (drag_handle, x, y);
}

static void
clutter_drag_action_set_property (GObject      *gobject,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ClutterDragAction *action = CLUTTER_DRAG_ACTION (gobject);
  ClutterDragActionPrivate *priv = action->priv;

  switch (prop_id)
    {
    case PROP_X_DRAG_THRESHOLD:
      clutter_drag_action_set_drag_threshold (action,
                                              g_value_get_int (value),
                                              priv->y_drag_threshold);
      break;

    case PROP_Y_DRAG_THRESHOLD:
      clutter_drag_action_set_drag_threshold (action,
                                              priv->x_drag_threshold,
                                              g_value_get_int (value));
      break;

    case PROP_DRAG_HANDLE:
      clutter_drag_action_set_drag_handle (action, g_value_get_object (value));
      break;

    case PROP_DRAG_AXIS:
      clutter_drag_action_set_drag_axis (action, g_value_get_enum (value));
      break;

    case PROP_DRAG_AREA:
      clutter_drag_action_set_drag_area (action, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_drag_action_get_property (GObject    *gobject,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ClutterDragActionPrivate *priv = CLUTTER_DRAG_ACTION (gobject)->priv;

  switch (prop_id)
    {
    case PROP_X_DRAG_THRESHOLD:
      {
        gint threshold;

        get_drag_threshold (CLUTTER_DRAG_ACTION (gobject), &threshold, NULL);
        g_value_set_int (value, threshold);
      }
      break;

    case PROP_Y_DRAG_THRESHOLD:
      {
        gint threshold;

        get_drag_threshold (CLUTTER_DRAG_ACTION (gobject), NULL, &threshold);
        g_value_set_int (value, threshold);
      }
      break;

    case PROP_DRAG_HANDLE:
      g_value_set_object (value, priv->drag_handle);
      break;

    case PROP_DRAG_AXIS:
      g_value_set_enum (value, priv->drag_axis);
      break;

    case PROP_DRAG_AREA:
      g_value_set_boxed (value, &priv->drag_area);
      break;

    case PROP_DRAG_AREA_SET:
      g_value_set_boolean (value, priv->drag_area_set);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_drag_action_dispose (GObject *gobject)
{
  ClutterDragActionPrivate *priv = CLUTTER_DRAG_ACTION (gobject)->priv;

  /* if we're being disposed while a capture is still present, we
   * need to reset the state we are currently holding
   */
  if (priv->last_motion_device != NULL)
    {
      _clutter_stage_remove_pointer_drag_actor (priv->stage,
                                                priv->last_motion_device);
      priv->last_motion_device = NULL;
    }

  if (priv->sequence != NULL)
    {
      _clutter_stage_remove_touch_drag_actor (priv->stage,
                                              priv->sequence);
      priv->sequence = NULL;
    }

  if (priv->capture_id != 0)
    {
      clutter_stage_set_motion_events_enabled (priv->stage,
                                               priv->motion_events_enabled);

      if (priv->stage != NULL)
        g_signal_handler_disconnect (priv->stage, priv->capture_id);

      priv->capture_id = 0;
      priv->stage = NULL;
    }

  if (priv->button_press_id != 0)
    {
      ClutterActor *actor;

      actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (gobject));
      if (actor != NULL)
        {
          g_signal_handler_disconnect (actor, priv->button_press_id);
          g_signal_handler_disconnect (actor, priv->touch_begin_id);
        }

      priv->button_press_id = 0;
      priv->touch_begin_id = 0;
    }

  clutter_drag_action_set_drag_handle (CLUTTER_DRAG_ACTION (gobject), NULL);
  clutter_drag_action_set_drag_area (CLUTTER_DRAG_ACTION (gobject), NULL);

  G_OBJECT_CLASS (clutter_drag_action_parent_class)->dispose (gobject);
}

static void
clutter_drag_action_class_init (ClutterDragActionClass *klass)
{
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  meta_class->set_actor = clutter_drag_action_set_actor;

  klass->drag_progress = clutter_drag_action_real_drag_progress;
  klass->drag_motion = clutter_drag_action_real_drag_motion;

  /**
   * ClutterDragAction:x-drag-threshold:
   *
   * The horizontal threshold, in pixels, that the cursor must travel
   * in order to begin a drag action.
   *
   * When set to a positive value, #ClutterDragAction will only emit
   * #ClutterDragAction::drag-begin if the pointer has moved
   * horizontally at least of the given amount of pixels since
   * the button press event.
   *
   * When set to -1, #ClutterDragAction will use the default threshold
   * stored in the #ClutterSettings:dnd-drag-threshold property of
   * #ClutterSettings.
   *
   * When read, this property will always return a valid drag
   * threshold, either as set or the default one.
   *
   * Since: 1.4
   */
  drag_props[PROP_X_DRAG_THRESHOLD] =
    g_param_spec_int ("x-drag-threshold",
                      P_("Horizontal Drag Threshold"),
                      P_("The horizontal amount of pixels required to start dragging"),
                      -1, G_MAXINT,
                      0,
                      CLUTTER_PARAM_READWRITE);

  /**
   * ClutterDragAction:y-drag-threshold:
   *
   * The vertical threshold, in pixels, that the cursor must travel
   * in order to begin a drag action.
   *
   * When set to a positive value, #ClutterDragAction will only emit
   * #ClutterDragAction::drag-begin if the pointer has moved
   * vertically at least of the given amount of pixels since
   * the button press event.
   *
   * When set to -1, #ClutterDragAction will use the value stored
   * in the #ClutterSettings:dnd-drag-threshold property of
   * #ClutterSettings.
   *
   * When read, this property will always return a valid drag
   * threshold, either as set or the default one.
   *
   * Since: 1.4
   */
  drag_props[PROP_Y_DRAG_THRESHOLD] =
    g_param_spec_int ("y-drag-threshold",
                      P_("Vertical Drag Threshold"),
                      P_("The vertical amount of pixels required to start dragging"),
                      -1, G_MAXINT,
                      0,
                      CLUTTER_PARAM_READWRITE);

  /**
   * ClutterDragAction:drag-handle:
   *
   * The #ClutterActor that is effectively being dragged
   *
   * A #ClutterDragAction will, be default, use the #ClutterActor that
   * has been attached to the action; it is possible to create a
   * separate #ClutterActor and use it instead.
   *
   * Setting this property has no effect on the #ClutterActor argument
   * passed to the #ClutterDragAction signals
   *
   * Since: 1.4
   */
  drag_props[PROP_DRAG_HANDLE] =
    g_param_spec_object ("drag-handle",
                         P_("Drag Handle"),
                         P_("The actor that is being dragged"),
                         CLUTTER_TYPE_ACTOR,
                         CLUTTER_PARAM_READWRITE);

  /**
   * ClutterDragAction:drag-axis:
   *
   * Constraints the dragging action to the specified axis
   *
   * Since: 1.4
   */
  drag_props[PROP_DRAG_AXIS] =
    g_param_spec_enum ("drag-axis",
                       P_("Drag Axis"),
                       P_("Constraints the dragging to an axis"),
                       CLUTTER_TYPE_DRAG_AXIS,
                       CLUTTER_DRAG_AXIS_NONE,
                       CLUTTER_PARAM_READWRITE);

  /**
   * ClutterDragAction:drag-area:
   *
   * Constains the dragging action (or in particular, the resulting
   * actor position) to the specified #ClutterRect, in parent's
   * coordinates.
   *
   * Since: 1.12
   */
  drag_props[PROP_DRAG_AREA] =
    g_param_spec_boxed ("drag-area",
			P_("Drag Area"),
			P_("Constrains the dragging to a rectangle"),
			CLUTTER_TYPE_RECT,
			CLUTTER_PARAM_READWRITE);

  /**
   * ClutterDragAction:drag-area-set:
   *
   * Whether the #ClutterDragAction:drag-area property has been set.
   *
   * Since: 1.12
   */
  drag_props[PROP_DRAG_AREA_SET] =
    g_param_spec_boolean ("drag-area-set",
			  P_("Drag Area Set"),
			  P_("Whether the drag area is set"),
			  FALSE,
			  CLUTTER_PARAM_READABLE);


  gobject_class->set_property = clutter_drag_action_set_property;
  gobject_class->get_property = clutter_drag_action_get_property;
  gobject_class->dispose = clutter_drag_action_dispose;
  g_object_class_install_properties  (gobject_class,
                                      PROP_LAST,
                                      drag_props);

  /**
   * ClutterDragAction::drag-begin:
   * @action: the #ClutterDragAction that emitted the signal
   * @actor: the #ClutterActor attached to the action
   * @event_x: the X coordinate (in stage space) of the press event
   * @event_y: the Y coordinate (in stage space) of the press event
   * @modifiers: the modifiers of the press event
   *
   * The ::drag-begin signal is emitted when the #ClutterDragAction
   * starts the dragging
   *
   * The emission of this signal can be delayed by using the
   * #ClutterDragAction:x-drag-threshold and
   * #ClutterDragAction:y-drag-threshold properties
   *
   * Since: 1.4
   */
  drag_signals[DRAG_BEGIN] =
    g_signal_new (I_("drag-begin"),
                  CLUTTER_TYPE_DRAG_ACTION,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterDragActionClass, drag_begin),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT_FLOAT_FLOAT_FLAGS,
                  G_TYPE_NONE, 4,
                  CLUTTER_TYPE_ACTOR,
                  G_TYPE_FLOAT,
                  G_TYPE_FLOAT,
                  CLUTTER_TYPE_MODIFIER_TYPE);

  /**
   * ClutterDragAction::drag-progress:
   * @action: the #ClutterDragAction that emitted the signal
   * @actor: the #ClutterActor attached to the action
   * @delta_x: the X component of the distance between the press event
   *   that began the dragging and the current position of the pointer,
   *   as of the latest motion event
   * @delta_y: the Y component of the distance between the press event
   *   that began the dragging and the current position of the pointer,
   *   as of the latest motion event
   *
   * The ::drag-progress signal is emitted for each motion event after
   * the #ClutterDragAction::drag-begin signal has been emitted.
   *
   * The components of the distance between the press event and the
   * latest motion event are computed in the actor's coordinate space,
   * to take into account eventual transformations. If you want the
   * stage coordinates of the latest motion event you can use
   * clutter_drag_action_get_motion_coords().
   *
   * The default handler will emit #ClutterDragAction::drag-motion,
   * if #ClutterDragAction::drag-progress emission returns %TRUE.
   *
   * Return value: %TRUE if the drag should continue, and %FALSE
   *   if it should be stopped.
   *
   * Since: 1.12
   */
  drag_signals[DRAG_PROGRESS] =
    g_signal_new (I_("drag-progress"),
                  CLUTTER_TYPE_DRAG_ACTION,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterDragActionClass, drag_progress),
                  _clutter_boolean_continue_accumulator, NULL,
                  _clutter_marshal_BOOLEAN__OBJECT_FLOAT_FLOAT,
                  G_TYPE_BOOLEAN, 3,
                  CLUTTER_TYPE_ACTOR,
                  G_TYPE_FLOAT,
                  G_TYPE_FLOAT);

  /**
   * ClutterDragAction::drag-motion:
   * @action: the #ClutterDragAction that emitted the signal
   * @actor: the #ClutterActor attached to the action
   * @delta_x: the X component of the distance between the press event
   *   that began the dragging and the current position of the pointer,
   *   as of the latest motion event
   * @delta_y: the Y component of the distance between the press event
   *   that began the dragging and the current position of the pointer,
   *   as of the latest motion event
   *
   * The ::drag-motion signal is emitted for each motion event after
   * the #ClutterDragAction::drag-begin signal has been emitted.
   *
   * The components of the distance between the press event and the
   * latest motion event are computed in the actor's coordinate space,
   * to take into account eventual transformations. If you want the
   * stage coordinates of the latest motion event you can use
   * clutter_drag_action_get_motion_coords().
   *
   * The default handler of the signal will call clutter_actor_move_by()
   * either on @actor or, if set, of #ClutterDragAction:drag-handle using
   * the @delta_x and @delta_y components of the dragging motion. If you
   * want to override the default behaviour, you can connect to the
   * #ClutterDragAction::drag-progress signal and return %FALSE from the
   * handler.
   *
   * Since: 1.4
   */
  drag_signals[DRAG_MOTION] =
    g_signal_new (I_("drag-motion"),
                  CLUTTER_TYPE_DRAG_ACTION,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterDragActionClass, drag_motion),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT_FLOAT_FLOAT,
                  G_TYPE_NONE, 3,
                  CLUTTER_TYPE_ACTOR,
                  G_TYPE_FLOAT,
                  G_TYPE_FLOAT);

  /**
   * ClutterDragAction::drag-end:
   * @action: the #ClutterDragAction that emitted the signal
   * @actor: the #ClutterActor attached to the action
   * @event_x: the X coordinate (in stage space) of the release event
   * @event_y: the Y coordinate (in stage space) of the release event
   * @modifiers: the modifiers of the release event
   *
   * The ::drag-end signal is emitted at the end of the dragging,
   * when the pointer button's is released
   *
   * This signal is emitted if and only if the #ClutterDragAction::drag-begin
   * signal has been emitted first
   *
   * Since: 1.4
   */
  drag_signals[DRAG_END] =
    g_signal_new (I_("drag-end"),
                  CLUTTER_TYPE_DRAG_ACTION,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterDragActionClass, drag_end),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT_FLOAT_FLOAT_FLAGS,
                  G_TYPE_NONE, 4,
                  CLUTTER_TYPE_ACTOR,
                  G_TYPE_FLOAT,
                  G_TYPE_FLOAT,
                  CLUTTER_TYPE_MODIFIER_TYPE);
}

static void
clutter_drag_action_init (ClutterDragAction *self)
{
  self->priv = clutter_drag_action_get_instance_private (self);
}

/**
 * clutter_drag_action_new:
 *
 * Creates a new #ClutterDragAction instance
 *
 * Return value: the newly created #ClutterDragAction
 *
 * Since: 1.4
 */
ClutterAction *
clutter_drag_action_new (void)
{
  return g_object_new (CLUTTER_TYPE_DRAG_ACTION, NULL);
}

/**
 * clutter_drag_action_set_drag_threshold:
 * @action: a #ClutterDragAction
 * @x_threshold: a distance on the horizontal axis, in pixels, or
 *   -1 to use the default drag threshold from #ClutterSettings
 * @y_threshold: a distance on the vertical axis, in pixels, or
 *   -1 to use the default drag threshold from #ClutterSettings
 *
 * Sets the horizontal and vertical drag thresholds that must be
 * cleared by the pointer before @action can begin the dragging.
 *
 * If @x_threshold or @y_threshold are set to -1 then the default
 * drag threshold stored in the #ClutterSettings:dnd-drag-threshold
 * property of #ClutterSettings will be used.
 *
 * Since: 1.4
 */
void
clutter_drag_action_set_drag_threshold (ClutterDragAction *action,
                                        gint               x_threshold,
                                        gint               y_threshold)
{
  ClutterDragActionPrivate *priv;
  GObject *self;

  g_return_if_fail (CLUTTER_IS_DRAG_ACTION (action));

  self = G_OBJECT (action);
  priv = action->priv;

  g_object_freeze_notify (self);

  if (priv->x_drag_threshold != x_threshold)
    {
      priv->x_drag_threshold = x_threshold;

      g_object_notify_by_pspec (self, drag_props[PROP_X_DRAG_THRESHOLD]);
    }

  if (priv->y_drag_threshold != y_threshold)
    {
      priv->y_drag_threshold = y_threshold;

      g_object_notify_by_pspec (self, drag_props[PROP_Y_DRAG_THRESHOLD]);
    }

  g_object_thaw_notify (self);
}

/**
 * clutter_drag_action_get_drag_threshold:
 * @action: a #ClutterDragAction
 * @x_threshold: (out): return location for the horizontal drag
 *   threshold value, in pixels
 * @y_threshold: (out): return location for the vertical drag
 *   threshold value, in pixels
 *
 * Retrieves the values set by clutter_drag_action_set_drag_threshold().
 *
 * If the #ClutterDragAction:x-drag-threshold property or the
 * #ClutterDragAction:y-drag-threshold property have been set to -1 then
 * this function will return the default drag threshold value as stored
 * by the #ClutterSettings:dnd-drag-threshold property of #ClutterSettings.
 *
 * Since: 1.4
 */
void
clutter_drag_action_get_drag_threshold (ClutterDragAction *action,
                                        guint             *x_threshold,
                                        guint             *y_threshold)
{
  gint x_res, y_res;

  g_return_if_fail (CLUTTER_IS_DRAG_ACTION (action));

  get_drag_threshold (action, &x_res, &y_res);

  if (x_threshold != NULL)
    *x_threshold = x_res;

  if (y_threshold != NULL)
    *y_threshold = y_res;
}

static void
on_drag_handle_destroy (ClutterActor      *handle,
                        ClutterDragAction *action)
{
  ClutterDragActionPrivate *priv = action->priv;
  ClutterActor *actor;

  /* make sure we reset the state */
  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (action));
  if (priv->in_drag)
    emit_drag_end (action, actor, NULL);

  priv->drag_handle = NULL;
}

/**
 * clutter_drag_action_set_drag_handle:
 * @action: a #ClutterDragAction
 * @handle: (allow-none): a #ClutterActor, or %NULL to unset
 *
 * Sets the actor to be used as the drag handle.
 *
 * Since: 1.4
 */
void
clutter_drag_action_set_drag_handle (ClutterDragAction *action,
                                     ClutterActor      *handle)
{
  ClutterDragActionPrivate *priv;

  g_return_if_fail (CLUTTER_IS_DRAG_ACTION (action));
  g_return_if_fail (handle == NULL || CLUTTER_IS_ACTOR (handle));

  priv = action->priv;

  if (priv->drag_handle == handle)
    return;

  if (priv->drag_handle != NULL)
    g_signal_handlers_disconnect_by_func (priv->drag_handle,
                                          G_CALLBACK (on_drag_handle_destroy),
                                          action);

  priv->drag_handle = handle;

  priv->transformed_press_x = priv->press_x;
  priv->transformed_press_y = priv->press_y;

  if (priv->drag_handle != NULL)
    {
      clutter_actor_transform_stage_point (priv->drag_handle,
                                           priv->press_x,
                                           priv->press_y,
					   &priv->transformed_press_x,
					   &priv->transformed_press_y);
      g_signal_connect (priv->drag_handle, "destroy",
			G_CALLBACK (on_drag_handle_destroy),
			action);
    }

  g_object_notify_by_pspec (G_OBJECT (action), drag_props[PROP_DRAG_HANDLE]);
}

/**
 * clutter_drag_action_get_drag_handle:
 * @action: a #ClutterDragAction
 *
 * Retrieves the drag handle set by clutter_drag_action_set_drag_handle()
 *
 * Return value: (transfer none): a #ClutterActor, used as the drag
 *   handle, or %NULL if none was set
 *
 * Since: 1.4
 */
ClutterActor *
clutter_drag_action_get_drag_handle (ClutterDragAction *action)
{
  g_return_val_if_fail (CLUTTER_IS_DRAG_ACTION (action), NULL);

  return action->priv->drag_handle;
}

/**
 * clutter_drag_action_set_drag_axis:
 * @action: a #ClutterDragAction
 * @axis: the axis to constraint the dragging to
 *
 * Restricts the dragging action to a specific axis
 *
 * Since: 1.4
 */
void
clutter_drag_action_set_drag_axis (ClutterDragAction *action,
                                   ClutterDragAxis    axis)
{
  ClutterDragActionPrivate *priv;

  g_return_if_fail (CLUTTER_IS_DRAG_ACTION (action));
  g_return_if_fail (axis >= CLUTTER_DRAG_AXIS_NONE &&
                    axis <= CLUTTER_DRAG_Y_AXIS);

  priv = action->priv;

  if (priv->drag_axis == axis)
    return;

  priv->drag_axis = axis;

  g_object_notify_by_pspec (G_OBJECT (action), drag_props[PROP_DRAG_AXIS]);
}

/**
 * clutter_drag_action_get_drag_axis:
 * @action: a #ClutterDragAction
 *
 * Retrieves the axis constraint set by clutter_drag_action_set_drag_axis()
 *
 * Return value: the axis constraint
 *
 * Since: 1.4
 */
ClutterDragAxis
clutter_drag_action_get_drag_axis (ClutterDragAction *action)
{
  g_return_val_if_fail (CLUTTER_IS_DRAG_ACTION (action),
                        CLUTTER_DRAG_AXIS_NONE);

  return action->priv->drag_axis;
}

/**
 * clutter_drag_action_get_press_coords:
 * @action: a #ClutterDragAction
 * @press_x: (out): return location for the press event's X coordinate
 * @press_y: (out): return location for the press event's Y coordinate
 *
 * Retrieves the coordinates, in stage space, of the press event
 * that started the dragging
 *
 * Since: 1.4
 */
void
clutter_drag_action_get_press_coords (ClutterDragAction *action,
                                      gfloat            *press_x,
                                      gfloat            *press_y)
{
  g_return_if_fail (CLUTTER_IS_DRAG_ACTION (action));

  if (press_x)
    *press_x = action->priv->press_x;

  if (press_y)
    *press_y = action->priv->press_y;
}

/**
 * clutter_drag_action_get_motion_coords:
 * @action: a #ClutterDragAction
 * @motion_x: (out): return location for the latest motion
 *   event's X coordinate
 * @motion_y: (out): return location for the latest motion
 *   event's Y coordinate
 *
 * Retrieves the coordinates, in stage space, of the latest motion
 * event during the dragging
 *
 * Since: 1.4
 */
void
clutter_drag_action_get_motion_coords (ClutterDragAction *action,
                                       gfloat            *motion_x,
                                       gfloat            *motion_y)
{
  g_return_if_fail (CLUTTER_IS_DRAG_ACTION (action));

  if (motion_x)
    *motion_x = action->priv->last_motion_x;

  if (motion_y)
    *motion_y = action->priv->last_motion_y;
}

/**
 * clutter_drag_action_get_drag_area:
 * @action: a #ClutterDragAction
 * @drag_area: (out caller-allocates): a #ClutterRect to be filled
 *
 * Retrieves the "drag area" associated with @action, that
 * is a #ClutterRect that constrains the actor movements,
 * in parents coordinates.
 *
 * Returns: %TRUE if the actor is actually constrained (and thus
 *          @drag_area is valid), %FALSE otherwise
 */
gboolean
clutter_drag_action_get_drag_area (ClutterDragAction *action,
				   ClutterRect       *drag_area)
{
  g_return_val_if_fail (CLUTTER_IS_DRAG_ACTION (action), FALSE);

  if (drag_area != NULL)
    *drag_area = action->priv->drag_area;
  return action->priv->drag_area_set;
}

/**
 * clutter_drag_action_set_drag_area:
 * @action: a #ClutterDragAction
 * @drag_area: (allow-none): a #ClutterRect
 *
 * Sets @drag_area to constrain the dragging of the actor associated
 * with @action, so that it position is always within @drag_area, expressed
 * in parent's coordinates.
 * If @drag_area is %NULL, the actor is not constrained.
 */
void
clutter_drag_action_set_drag_area (ClutterDragAction *action,
				   const ClutterRect *drag_area)
{
  ClutterDragActionPrivate *priv;

  g_return_if_fail (CLUTTER_IS_DRAG_ACTION (action));

  priv = action->priv;

  if (drag_area != NULL)
    {
      priv->drag_area = *drag_area;
      priv->drag_area_set = TRUE;
    }
  else
    priv->drag_area_set = FALSE;

  g_object_notify_by_pspec (G_OBJECT (action), drag_props[PROP_DRAG_AREA_SET]);
  g_object_notify_by_pspec (G_OBJECT (action), drag_props[PROP_DRAG_AREA]);
}
