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
  guint event_id;

  guint press_button;

  guint is_held    : 1;
  guint is_pressed : 1;
};

enum
{
  PROP_0,

  PROP_HELD,
  PROP_PRESSED
};

enum
{
  CLICKED,

  LAST_SIGNAL
};

static guint click_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (ClutterClickAction, clutter_click_action, CLUTTER_TYPE_ACTION);

static inline void
click_action_set_pressed (ClutterClickAction *action,
                          gboolean            is_pressed)
{
  ClutterClickActionPrivate *priv = action->priv;

  if (priv->is_pressed == is_pressed)
    return;

  priv->is_pressed = is_pressed;
  g_object_notify (G_OBJECT (action), "pressed");
}

static gboolean
actor_contains_source (ClutterActor *actor,
                       ClutterActor *event_source)
{
  while (event_source != NULL && event_source != actor)
    event_source = clutter_actor_get_parent (event_source);

  return event_source != NULL;
}

static gboolean
on_event (ClutterActor       *actor,
          ClutterEvent       *event,
          ClutterClickAction *action)
{
  ClutterClickActionPrivate *priv = action->priv;
  gboolean res = FALSE;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (action)))
    return FALSE;

  switch (clutter_event_type (event))
    {
    case CLUTTER_BUTTON_PRESS:
      if (clutter_event_get_click_count (event) != 1)
        return FALSE;

      if (priv->is_held)
        return TRUE;

      if (!actor_contains_source (actor, clutter_event_get_source (event)))
        return FALSE;

      priv->is_held = TRUE;
      priv->press_button = clutter_event_get_button (event);
      clutter_grab_pointer (actor);

      click_action_set_pressed (action, TRUE);
      res = TRUE;
      break;

    case CLUTTER_BUTTON_RELEASE:
      if (clutter_event_get_button (event) != priv->press_button ||
          clutter_event_get_click_count (event) != 1)
        return FALSE;

      if (!priv->is_held)
        return TRUE;

      priv->is_held = FALSE;
      clutter_ungrab_pointer ();

      if (!actor_contains_source (actor, clutter_event_get_source (event)))
        return FALSE;

      click_action_set_pressed (action, FALSE);
      g_signal_emit (action, click_signals[CLICKED], 0, actor);
      res = TRUE;
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
  GParamSpec *pspec;

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
  pspec = g_param_spec_boolean ("pressed",
                                "Pressed",
                                "Whether the clickable should "
                                "be in pressed state",
                                FALSE,
                                CLUTTER_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_PRESSED, pspec);

  /**
   * ClutterClickAction:held:
   *
   * Whether the clickable actor has the pointer grabbed
   *
   * Since: 1.4
   */
  pspec = g_param_spec_boolean ("held",
                                "Held",
                                "Whether the clickable has a grab",
                                FALSE,
                                CLUTTER_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_HELD, pspec);

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
                  clutter_marshal_VOID__OBJECT,
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
