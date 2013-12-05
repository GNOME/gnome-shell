/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012  Collabora Ltd..
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

#ifndef __CLUTTER_GESTURE_ACTION_PRIVATE_H__
#define __CLUTTER_GESTURE_ACTION_PRIVATE_H__

#include <clutter/clutter-gesture-action.h>

G_BEGIN_DECLS

/*< private >
 * ClutterGestureTriggerEdge:
 * @CLUTTER_GESTURE_TRIGGER_EDGE_NONE: Tell #ClutterGestureAction that
 * the gesture must begin immediately and there's no drag limit that
 * will cause its cancellation;
 * @CLUTTER_GESTURE_TRIGGER_EDGE_AFTER: Tell #ClutterGestureAction that
 * it needs to wait until the drag threshold has been exceeded before
 * considering that the gesture has begun;
 * @CLUTTER_GESTURE_TRIGGER_EDGE_BEFORE: Tell #ClutterGestureAction that
 * the gesture must begin immediately and that it must be cancelled
 * once the drag exceed the configured threshold.
 *
 * Enum passed to the clutter_gesture_action_set_threshold_trigger_edge()
 * function.
 */
typedef enum
{
  CLUTTER_GESTURE_TRIGGER_EDGE_NONE  = 0,
  CLUTTER_GESTURE_TRIGGER_EDGE_AFTER,
  CLUTTER_GESTURE_TRIGGER_EDGE_BEFORE
} ClutterGestureTriggerEdge;

G_GNUC_INTERNAL
void                            clutter_gesture_action_set_threshold_trigger_edge       (ClutterGestureAction      *action,
                                                                                         ClutterGestureTriggerEdge  edge);
G_GNUC_INTERNAL
ClutterGestureTriggerEdge       clutter_gesture_action_get_threshold_trigger_egde       (ClutterGestureAction      *action);

G_END_DECLS

#endif /* __CLUTTER_GESTURE_ACTION_PRIVATE_H__ */
