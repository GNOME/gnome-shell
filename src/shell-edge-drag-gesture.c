/*
 * Copyright (C) 2025 Jonas Dreßler <verdre@v0yd.nl>
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

#include "config.h"

#include <clutter/clutter.h>

#include "shell-edge-drag-gesture.h"
#include "shell-global.h"

#define EDGE_THRESHOLD 20
#define DRAG_DISTANCE 80
#define CANCEL_THRESHOLD 100
#define CANCEL_TIMEOUT_MS 200

struct _ShellEdgeDragGesture
{
  ClutterGesture parent;

  StSide side;

  unsigned int cancel_timeout_point;
  guint cancel_timeout_id;
};

enum
{
  PROP_0,

  PROP_SIDE,

  PROP_LAST
};

enum
{
  PROGRESS,

  LAST_SIGNAL
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };
static unsigned int obj_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_FINAL_TYPE (ShellEdgeDragGesture, shell_edge_drag_gesture, CLUTTER_TYPE_GESTURE);

static gboolean
get_monitor_for_coords (ShellEdgeDragGesture *self,
                        graphene_point_t     *coords,
                        MtkRectangle         *monitor_geometry_out)
{
  MetaDisplay *display = shell_global_get_display (shell_global_get ());
  unsigned int monitor_index;
  MtkRectangle rect;

  rect.x = coords->x - 1;
  rect.y = coords->y - 1;
  rect.width = 1;
  rect.height = 1;

  monitor_index = meta_display_get_monitor_index_for_rect (display, &rect);
  if (monitor_index == -1)
    return FALSE;

  meta_display_get_monitor_geometry (display, monitor_index, monitor_geometry_out);
  return TRUE;
}

static gboolean
is_near_monitor_edge (ShellEdgeDragGesture *self,
                      unsigned int          point)
{
  graphene_point_t coords;
  MtkRectangle monitor_geometry;

  clutter_gesture_get_point_coords_abs (CLUTTER_GESTURE (self),
                                        point,
                                        &coords);

  if (!get_monitor_for_coords (self, &coords, &monitor_geometry))
    {
      g_assert_not_reached ();
      return FALSE;
    }

  switch (self->side)
    {
    case ST_SIDE_LEFT:
      return coords.x < (float) (monitor_geometry.x + EDGE_THRESHOLD);
    case ST_SIDE_RIGHT:
      return coords.x > (float) (monitor_geometry.x + monitor_geometry.width - EDGE_THRESHOLD);
    case ST_SIDE_TOP:
      return coords.y < (float) (monitor_geometry.y + EDGE_THRESHOLD);
    case ST_SIDE_BOTTOM:
      return coords.y > (float) (monitor_geometry.y + monitor_geometry.height - EDGE_THRESHOLD);
    }

  g_assert_not_reached ();
  return FALSE;
}

static gboolean
exceeds_cancel_threshold (ShellEdgeDragGesture *self,
                          unsigned int          point)
{
  graphene_point_t begin_coords, latest_coords;
  float distance_x, distance_y;

  clutter_gesture_get_point_begin_coords_abs (CLUTTER_GESTURE (self),
                                              point,
                                              &begin_coords);

  clutter_gesture_get_point_coords_abs (CLUTTER_GESTURE (self),
                                        point,
                                        &latest_coords);
  
  graphene_point_distance (&latest_coords, &begin_coords, &distance_x, &distance_y);

  switch (self->side)
    {
    case ST_SIDE_LEFT:
    case ST_SIDE_RIGHT:
      return distance_y > CANCEL_THRESHOLD;

    case ST_SIDE_TOP:
    case ST_SIDE_BOTTOM:
      return distance_x > CANCEL_THRESHOLD;
    }

  g_assert_not_reached ();
  return FALSE;
}

static gboolean
passes_distance_needed (ShellEdgeDragGesture *self,
                        unsigned int          sequence)
{
  graphene_point_t begin_coords, latest_coords;
  MtkRectangle monitor_geometry;

  clutter_gesture_get_point_begin_coords_abs (CLUTTER_GESTURE (self),
                                              sequence,
                                              &begin_coords);

  if (!get_monitor_for_coords (self, &begin_coords, &monitor_geometry))
    {
      g_assert_not_reached ();
      return FALSE;
    }

  clutter_gesture_get_point_coords_abs (CLUTTER_GESTURE (self),
                                        sequence,
                                        &latest_coords);

  switch (self->side)
    {
    case ST_SIDE_LEFT:
      return latest_coords.x > (float) (monitor_geometry.x + DRAG_DISTANCE);
    case ST_SIDE_RIGHT:
      return latest_coords.x < (float) (monitor_geometry.x + monitor_geometry.width - DRAG_DISTANCE);
    case ST_SIDE_TOP:
      return latest_coords.y > (float) (monitor_geometry.y + DRAG_DISTANCE);
    case ST_SIDE_BOTTOM:
      return latest_coords.y < (float) (monitor_geometry.y + monitor_geometry.height - DRAG_DISTANCE);
    }

  g_assert_not_reached ();
  return FALSE;
}

static gboolean
shell_edge_drag_gesture_should_handle_sequence (ClutterGesture     *gesture,
                                                const ClutterEvent *sequence_begin_event)
{
  ClutterEventType event_type = clutter_event_type (sequence_begin_event);

  if (event_type == CLUTTER_TOUCH_BEGIN)
    return TRUE;

  return FALSE;
}

static gboolean
on_cancel_timeout (gpointer data)
{
  ShellEdgeDragGesture *self = data;

  if (is_near_monitor_edge (self, self->cancel_timeout_point))
    clutter_gesture_set_state (CLUTTER_GESTURE (self), CLUTTER_GESTURE_STATE_CANCELLED);

  self->cancel_timeout_id = 0;
  return G_SOURCE_REMOVE;
}

static void
shell_edge_drag_gesture_point_began (ClutterGesture *gesture,
                                     unsigned int    point)
{
  ShellEdgeDragGesture *self = SHELL_EDGE_DRAG_GESTURE (gesture);
  unsigned int n_points = clutter_gesture_get_n_points (gesture);

  if (n_points > 1 ||
      !is_near_monitor_edge (self, point))
    {
      clutter_gesture_set_state (gesture, CLUTTER_GESTURE_STATE_CANCELLED);
      return;
    }

  self->cancel_timeout_point = point;

  g_assert (self->cancel_timeout_id == 0);
  self->cancel_timeout_id = g_timeout_add (CANCEL_TIMEOUT_MS, on_cancel_timeout, self);
}

static void
shell_edge_drag_gesture_point_moved (ClutterGesture *gesture,
                                     unsigned int    point)
{
  ShellEdgeDragGesture *self = SHELL_EDGE_DRAG_GESTURE (gesture);

  if (exceeds_cancel_threshold (self, point))
    {
      clutter_gesture_set_state (gesture, CLUTTER_GESTURE_STATE_CANCELLED);
      return;
    }

  if (clutter_gesture_get_state (gesture) == CLUTTER_GESTURE_STATE_POSSIBLE &&
      !is_near_monitor_edge (self, point))
    clutter_gesture_set_state (gesture, CLUTTER_GESTURE_STATE_RECOGNIZING);

  if (clutter_gesture_get_state (gesture) == CLUTTER_GESTURE_STATE_RECOGNIZING)
    {
      graphene_point_t begin_coords, latest_coords;
      float distance_x, distance_y;

      clutter_gesture_get_point_begin_coords_abs (gesture,
                                                  point,
                                                  &begin_coords);

      clutter_gesture_get_point_coords_abs (gesture,
                                            point,
                                            &latest_coords);

      graphene_point_distance (&latest_coords, &begin_coords, &distance_x, &distance_y);

      switch (self->side)
        {
        case ST_SIDE_LEFT:
        case ST_SIDE_RIGHT:
          g_signal_emit (self, obj_signals[PROGRESS], 0, distance_x);
          break;

        case ST_SIDE_TOP:
        case ST_SIDE_BOTTOM:
          g_signal_emit (self, obj_signals[PROGRESS], 0, distance_y);
          break;
        }

      if (passes_distance_needed (self, point))
        clutter_gesture_set_state (gesture, CLUTTER_GESTURE_STATE_COMPLETED);
    }
}

static void
shell_edge_drag_gesture_point_ended (ClutterGesture *gesture,
                                     unsigned int    point)
{
  clutter_gesture_set_state (gesture, CLUTTER_GESTURE_STATE_CANCELLED);
}

static void
shell_edge_drag_gesture_state_changed (ClutterGesture      *gesture,
                                       ClutterGestureState  old_state,
                                       ClutterGestureState  new_state)
{
  ShellEdgeDragGesture *self = SHELL_EDGE_DRAG_GESTURE (gesture);

  if (new_state == CLUTTER_GESTURE_STATE_CANCELLED ||
      new_state == CLUTTER_GESTURE_STATE_COMPLETED)
    g_clear_handle_id (&self->cancel_timeout_id, g_source_remove);
}

static void
shell_edge_drag_gesture_set_property (GObject      *gobject,
                                      unsigned int  prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ShellEdgeDragGesture *self = SHELL_EDGE_DRAG_GESTURE (gobject);

  switch (prop_id)
    {
    case PROP_SIDE:
      shell_edge_drag_gesture_set_side (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
shell_edge_drag_gesture_get_property (GObject      *gobject,
                                      unsigned int  prop_id,
                                      GValue       *value,
                                      GParamSpec   *pspec)
{
  ShellEdgeDragGesture *self = SHELL_EDGE_DRAG_GESTURE (gobject);

  switch (prop_id)
    {
    case PROP_SIDE:
      g_value_set_enum (value, shell_edge_drag_gesture_get_side (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
shell_edge_drag_gesture_init (ShellEdgeDragGesture *self)
{
  self->side = ST_SIDE_TOP;
}

static void
shell_edge_drag_gesture_class_init (ShellEdgeDragGestureClass *klass)
{
  ClutterGestureClass *gesture_class = CLUTTER_GESTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gesture_class->should_handle_sequence = shell_edge_drag_gesture_should_handle_sequence;
  gesture_class->point_began = shell_edge_drag_gesture_point_began;
  gesture_class->point_moved = shell_edge_drag_gesture_point_moved;
  gesture_class->point_ended = shell_edge_drag_gesture_point_ended;
  gesture_class->state_changed = shell_edge_drag_gesture_state_changed;

  gobject_class->set_property = shell_edge_drag_gesture_set_property;
  gobject_class->get_property = shell_edge_drag_gesture_get_property;

  /**
   * ShellEdgeDragGesture:side:
   *
   * Edge that the gesture may start at. Defaults to the top edge.
   */
  obj_props[PROP_SIDE] =
    g_param_spec_enum ("side", NULL, NULL,
                       CLUTTER_TYPE_GESTURE_STATE,
                       ST_SIDE_TOP,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);

  /**
   * ShellEdgeDragGesture::progress:
   * @gesture: the #ShellEdgeDragGesture that emitted the signal
   * @progress_px: the progress of the gesture in pixels in the selected direction
   *
   * The ::progress signal is emitted when the edge drag has moved
   */
  obj_signals[PROGRESS] =
    g_signal_new ("progress",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_FLOAT);
}

/**
 * shell_edge_drag_gesture_set_side:
 * @self: a #ShellEdgeDragGesture
 * @side: the side
 *
 * Sets the edge of the monitor that the edge drag may start at.
 */
void
shell_edge_drag_gesture_set_side (ShellEdgeDragGesture *self,
                                  StSide                side)
{
  g_return_if_fail (SHELL_IS_EDGE_DRAG_GESTURE (self));
  g_return_if_fail (side >= ST_SIDE_TOP && side <= ST_SIDE_LEFT);

  if (self->side == side)
    return;

  self->side = side;

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_SIDE]);
}

/**
 * shell_edge_drag_gesture_get_side:
 * @self: a #ShellEdgeDragGesture
 *
 * Gets the edge of the monitor that the edge drag may start at.
 *
 * Returns: the side that the edge drag may start at
 */
StSide
shell_edge_drag_gesture_get_side (ShellEdgeDragGesture *self)
{
  g_return_val_if_fail (SHELL_IS_EDGE_DRAG_GESTURE (self), ST_SIDE_TOP);

  return self->side;
}
