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
 * SECTION:clutter-click-action
 * @Title: ClutterClickAction
 * @Short_Description: Action for clickable actors
 *
 * #ClutterClickAction is a sub-class of #ClutterAction that implements
 * the logic for clickable actors, by using the low level events of
 * #ClutterActor, such as #ClutterActor::button-press-event and
 * #ClutterActor::button-release-event, to synthesize the high level
 * #ClutterClickAction::clicked signal.
 *
 * To use #ClutterClickAction you just need to apply it to a #ClutterActor
 * using clutter_actor_add_action() and connect to the
 * #ClutterClickAction::clicked signal:
 *
 * |[
 *   ClutterAction *action = clutter_click_action_new ();
 *
 *   clutter_actor_add_action (actor, action);
 *
 *   g_signal_connect (action, "clicked", G_CALLBACK (on_clicked), NULL);
 * ]|
 *
 * #ClutterClickAction is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-click-action.h"

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

struct _ClutterClickActionPrivate
{
  ClutterActor *stage;

  guint event_id;
  gulong capture_id;

  guint press_button;
  ClutterModifierType modifier_state;

  guint is_held    : 1;
  guint is_pressed : 1;
};

enum
{
  PROP_0,

  PROP_HELD,
  PROP_PRESSED,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

enum
{
  CLICKED,

  LAST_SIGNAL
};

static guint click_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (ClutterClickAction, clutter_click_action, CLUTTER_TYPE_ACTION);

/* forward declaration */
static gboolean on_captured_event (ClutterActor       *stage,
                                   ClutterEvent       *event,
                                   ClutterClickAction *action);

static inline void
click_action_set_pressed (ClutterClickAction *action,
                          gboolean            is_pressed)
{
  ClutterClickActionPrivate *priv = action->priv;

  if (priv->is_pressed == is_pressed)
    return;

  priv->is_pressed = is_pressed;
  g_object_notify_by_pspec (G_OBJECT (action), obj_props[PROP_PRESSED]);
}

static gboolean
on_event (ClutterActor       *actor,
          ClutterEvent       *event,
          ClutterClickAction *action)
{
  ClutterClickActionPrivate *priv = action->priv;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (action)))
    return FALSE;

  switch (clutter_event_type (event))
    {
    case CLUTTER_BUTTON_PRESS:
      if (clutter_event_get_click_count (event) != 1)
        return FALSE;

      if (priv->is_held)
        return TRUE;

      if (!clutter_actor_contains (actor, clutter_event_get_source (event)))
        return FALSE;

      priv->is_held = TRUE;
      priv->press_button = clutter_event_get_button (event);
      priv->modifier_state = clutter_event_get_state (event);

      if (priv->stage == NULL)
        priv->stage = clutter_actor_get_stage (actor);

      priv->capture_id = g_signal_connect_after (priv->stage, "captured-event",
                                                 G_CALLBACK (on_captured_event),
                                                 action);

      click_action_set_pressed (action, TRUE);
      break;

    case CLUTTER_ENTER:
      click_action_set_pressed (action, priv->is_held);
      break;

    case CLUTTER_LEAVE:
      click_action_set_pressed (action, priv->is_held);
      break;

    default:
      break;
    }

  return FALSE;
}

static gboolean
on_captured_event (ClutterActor       *stage,
                   ClutterEvent       *event,
                   ClutterClickAction *action)
{
  ClutterClickActionPrivate *priv = action->priv;
  ClutterActor *actor;
  ClutterModifierType modifier_state;

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (action));

  switch (clutter_event_type (event))
    {
    case CLUTTER_BUTTON_RELEASE:
      if (!priv->is_held)
        return TRUE;

      if (clutter_event_get_button (event) != priv->press_button ||
          clutter_event_get_click_count (event) != 1)
        return FALSE;

      priv->is_held = FALSE;

      /* disconnect the capture */
      if (priv->capture_id != 0)
        {
          g_signal_handler_disconnect (priv->stage, priv->capture_id);
          priv->capture_id = 0;
        }

      if (!clutter_actor_contains (actor, clutter_event_get_source (event)))
        return FALSE;

      /* exclude any button-mask so that we can compare
       * the press and release states properly */
      modifier_state = clutter_event_get_state (event) &
                       ~(CLUTTER_BUTTON1_MASK |
                         CLUTTER_BUTTON2_MASK |
                         CLUTTER_BUTTON3_MASK |
                         CLUTTER_BUTTON4_MASK |
                         CLUTTER_BUTTON5_MASK);

      /* if press and release states don't match we
       * simply ignore modifier keys. i.e. modifier keys
       * are expected to be pressed throughout the whole
       * click */
      if (modifier_state != priv->modifier_state)
        priv->modifier_state = 0;

      click_action_set_pressed (action, FALSE);
      g_signal_emit (action, click_signals[CLICKED], 0, actor);
      break;

    default:
      break;
    }

  return FALSE;
}

static void
clutter_click_action_set_actor (ClutterActorMeta *meta,
                                ClutterActor     *actor)
{
  ClutterClickActionPrivate *priv = CLUTTER_CLICK_ACTION (meta)->priv;

  if (priv->event_id != 0)
    {
      ClutterActor *old_actor = clutter_actor_meta_get_actor (meta);

      g_signal_handler_disconnect (old_actor, priv->event_id);
      priv->event_id = 0;
    }

  if (priv->capture_id != 0)
    {
      g_signal_handler_disconnect (priv->stage, priv->capture_id);
      priv->capture_id = 0;
      priv->stage = NULL;
    }

  if (actor != NULL)
    priv->event_id = g_signal_connect (actor, "event",
                                       G_CALLBACK (on_event),
                                       meta);

  CLUTTER_ACTOR_META_CLASS (clutter_click_action_parent_class)->set_actor (meta, actor);
}

static void
clutter_click_action_get_property (GObject    *gobject,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ClutterClickActionPrivate *priv = CLUTTER_CLICK_ACTION (gobject)->priv;

  switch (prop_id)
    {
    case PROP_HELD:
      g_value_set_boolean (value, priv->is_held);
      break;

    case PROP_PRESSED:
      g_value_set_boolean (value, priv->is_pressed);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_click_action_class_init (ClutterClickActionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterClickActionPrivate));

  meta_class->set_actor = clutter_click_action_set_actor;

  gobject_class->get_property = clutter_click_action_get_property;

  /**
   * ClutterClickAction:pressed:
   *
   * Whether the clickable actor should be in "pressed" state
   *
   * Since: 1.4
   */
  obj_props[PROP_PRESSED] =
    g_param_spec_boolean ("pressed",
                          P_("Pressed"),
                          P_("Whether the clickable should be in pressed state"),
                          FALSE,
                          CLUTTER_PARAM_READABLE);

  /**
   * ClutterClickAction:held:
   *
   * Whether the clickable actor has the pointer grabbed
   *
   * Since: 1.4
   */
  obj_props[PROP_HELD] =
    g_param_spec_boolean ("held",
                          P_("Held"),
                          P_("Whether the clickable has a grab"),
                          FALSE,
                          CLUTTER_PARAM_READABLE);

  g_object_class_install_properties (gobject_class,
                                     PROP_LAST,
                                     obj_props);

  /**
   * ClutterClickAction::clicked:
   * @action: the #ClutterClickAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   *
   * The ::clicked signal is emitted when the #ClutterActor to which
   * a #ClutterClickAction has been applied should respond to a
   * pointer button press and release events
   *
   * Since: 1.4
   */
  click_signals[CLICKED] =
    g_signal_new (I_("clicked"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterClickActionClass, clicked),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);
}

static void
clutter_click_action_init (ClutterClickAction *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, CLUTTER_TYPE_CLICK_ACTION,
                                            ClutterClickActionPrivate);
}

/**
 * clutter_click_action_new:
 *
 * Creates a new #ClutterClickAction instance
 *
 * Return value: the newly created #ClutterClickAction
 *
 * Since: 1.4
 */
ClutterAction *
clutter_click_action_new (void)
{
  return g_object_new (CLUTTER_TYPE_CLICK_ACTION, NULL);
}

/**
 * clutter_click_action_release:
 * @action: a #ClutterClickAction
 *
 * Emulates a release of the pointer button, which ungrabs the pointer
 * and unsets the #ClutterClickAction:pressed state.
 *
 * This function is useful to break a grab, for instance after a certain
 * amount of time has passed.
 *
 * Since: 1.4
 */
void
clutter_click_action_release (ClutterClickAction *action)
{
  ClutterClickActionPrivate *priv;

  g_return_if_fail (CLUTTER_IS_CLICK_ACTION (action));

  priv = action->priv;

  if (!priv->is_held)
    return;

  priv->is_held = FALSE;

  /* disconnect the capture */
  if (priv->capture_id != 0)
    {
      g_signal_handler_disconnect (priv->stage, priv->capture_id);
      priv->capture_id = 0;
    }

  click_action_set_pressed (action, FALSE);
}

/**
 * clutter_click_action_get_button:
 * @action: a #ClutterClickAction
 *
 * Retrieves the button that was pressed.
 *
 * Return value: the button value
 *
 * Since: 1.4
 */
guint
clutter_click_action_get_button (ClutterClickAction *action)
{
  g_return_val_if_fail (CLUTTER_IS_CLICK_ACTION (action), 0);

  return action->priv->press_button;
}

/**
 * clutter_click_action_get_state:
 * @action: a #ClutterClickAction
 *
 * Retrieves the modifier state of the click action.
 *
 * Return value: the modifier state parameter, or 0
 *
 * Since: 1.6
 */
ClutterModifierType
clutter_click_action_get_state (ClutterClickAction *action)
{
  g_return_val_if_fail (CLUTTER_IS_CLICK_ACTION (action), 0);

  return action->priv->modifier_state;
}
