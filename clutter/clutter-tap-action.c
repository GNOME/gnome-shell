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
 * Based on ClutterPanAction
 * Based on ClutterDragAction, ClutterSwipeAction, and MxKineticScrollView,
 * written by:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 *   Tomeu Vizoso <tomeu.vizoso@collabora.co.uk>
 *   Chris Lord <chris@linux.intel.com>
 */

/**
 * SECTION:clutter-tap-action
 * @Title: ClutterTapAction
 * @Short_Description: Action for tap gestures
 *
 * #ClutterTapAction is a sub-class of #ClutterGestureAction that implements
 * the logic for recognizing mouse clicks and touch tap gestures.
 *
 * The simplest usage of #ClutterTapAction consists in adding it to
 * a #ClutterActor, setting it as reactive and connecting a
 * callback for the #ClutterTapAction::tap signal, along the lines of the
 * following code:
 *
 * |[
 *   clutter_actor_add_action (actor, clutter_tap_action_new ());
 *   clutter_actor_set_reactive (actor, TRUE);
 *   g_signal_connect (action, "tap", G_CALLBACK (on_tap_callback), NULL);
 * ]|
 *
 * Since: 1.14
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-tap-action.h"

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-gesture-action-private.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

enum
{
  TAP,

  LAST_SIGNAL
};

static guint tap_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (ClutterTapAction, clutter_tap_action,
               CLUTTER_TYPE_GESTURE_ACTION);

static void
emit_tap (ClutterTapAction *self,
          ClutterActor     *actor)
{
  g_signal_emit (self, tap_signals[TAP], 0, actor);
}

static void
gesture_end (ClutterGestureAction *gesture,
             ClutterActor         *actor)
{
  emit_tap (CLUTTER_TAP_ACTION (gesture), actor);
}

static void
clutter_tap_action_class_init (ClutterTapActionClass *klass)
{
  ClutterGestureActionClass *gesture_class =
      CLUTTER_GESTURE_ACTION_CLASS (klass);

  gesture_class->gesture_end = gesture_end;

  /**
   * ClutterTapAction::tap:
   * @action: the #ClutterTapAction that emitted the signal
   * @actor: the #ClutterActor attached to the @action
   *
   * The ::tap signal is emitted when the tap gesture is complete.
   *
   * Since: 1.14
   */
  tap_signals[TAP] =
    g_signal_new (I_("tap"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterTapActionClass, tap),
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  CLUTTER_TYPE_ACTOR);
}

static void
clutter_tap_action_init (ClutterTapAction *self)
{
  clutter_gesture_action_set_threshold_trigger_edge (CLUTTER_GESTURE_ACTION (self),
                                                     CLUTTER_GESTURE_TRIGGER_EDGE_BEFORE);
}

/**
 * clutter_tap_action_new:
 *
 * Creates a new #ClutterTapAction instance
 *
 * Return value: the newly created #ClutterTapAction
 *
 * Since: 1.14
 */
ClutterAction *
clutter_tap_action_new (void)
{
  return g_object_new (CLUTTER_TYPE_TAP_ACTION, NULL);
}
