/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jonas Ådahl <jadahl@gmail.com>
 */

/**
 * SECTION:barrier-native
 * @Title: MetaBarrierImplNative
 * @Short_Description: Pointer barriers implementation for the native backend
 */

#include "config.h"

#include <stdlib.h>

#include <meta/barrier.h>
#include <meta/util.h>
#include "backends/meta-backend-private.h"
#include "backends/meta-barrier-private.h"
#include "backends/native/meta-backend-native.h"
#include "backends/native/meta-backend-native-private.h"
#include "backends/native/meta-barrier-native.h"

struct _MetaBarrierManagerNative
{
  GHashTable *barriers;
};

typedef enum {
  /* The barrier is active and responsive to pointer motion. */
  META_BARRIER_STATE_ACTIVE,

  /* An intermediate state after a pointer hit the pointer barrier. */
  META_BARRIER_STATE_HIT,

  /* The barrier was hit by a pointer and is still within the hit box and
   * has not been released.*/
  META_BARRIER_STATE_HELD,

  /* The pointer was released by the user. If the following motion hits
   * the barrier, it will pass through. */
  META_BARRIER_STATE_RELEASE,

  /* An intermediate state when the pointer has left the barrier. */
  META_BARRIER_STATE_LEFT,
} MetaBarrierState;

struct _MetaBarrierImplNativePrivate
{
  MetaBarrier              *barrier;
  MetaBarrierManagerNative *manager;

  gboolean                  is_active;
  MetaBarrierState          state;
  int                       trigger_serial;
  guint32                   last_event_time;
  MetaBarrierDirection      blocked_dir;
};

G_DEFINE_TYPE_WITH_PRIVATE (MetaBarrierImplNative, meta_barrier_impl_native,
                            META_TYPE_BARRIER_IMPL)

static int
next_serial (void)
{
  static int barrier_serial = 1;

  barrier_serial++;

  /* If it wraps, avoid 0 as it's not a valid serial. */
  if (barrier_serial == 0)
    barrier_serial++;

  return barrier_serial;
}

typedef struct _Vector2
{
  float x, y;
} Vector2;

static float
vector2_cross_product (Vector2 a, Vector2 b)
{
  return a.x * b.y - a.y * b.x;
}

static Vector2
vector2_add (Vector2 a, Vector2 b)
{
  return (Vector2) {
    .x = a.x + b.x,
    .y = a.y + b.y,
  };
}

static Vector2
vector2_subtract (Vector2 a, Vector2 b)
{
  return (Vector2) {
    .x = a.x - b.x,
    .y = a.y - b.y,
  };
}

static Vector2
vector2_multiply_constant (float c, Vector2 a)
{
  return (Vector2) {
    .x = c * a.x,
    .y = c * a.y,
  };
}

typedef struct _Line2
{
  Vector2 a;
  Vector2 b;
} Line2;

static gboolean
lines_intersect (Line2 *line1, Line2 *line2, Vector2 *intersection)
{
  Vector2 p = line1->a;
  Vector2 r = vector2_subtract (line1->b, line1->a);
  Vector2 q = line2->a;
  Vector2 s = vector2_subtract (line2->b, line2->a);
  float rxs;
  float sxr;
  float t;
  float u;

  /*
   * The line (p, r) and (q, s) intersects where
   *
   *   p + t r = q + u s
   *
   * Calculate t:
   *
   *   (p + t r) × s = (q + u s) × s
   *   p × s + t (r × s) = q × s + u (s × s)
   *   p × s + t (r × s) = q × s
   *   t (r × s) = q × s - p × s
   *   t (r × s) = (q - p) × s
   *   t = ((q - p) × s) / (r × s)
   *
   * Using the same method, for u we get:
   *
   *   u = ((p - q) × r) / (s × r)
   */

  rxs = vector2_cross_product (r, s);
  sxr = vector2_cross_product (s, r);

  /* If r × s = 0 then the lines are either parallel or collinear. */
  if (fabs ( rxs) < DBL_MIN)
    return FALSE;

  t = vector2_cross_product (vector2_subtract (q, p), s) / rxs;
  u = vector2_cross_product (vector2_subtract (p, q), r) / sxr;


  /* The lines only intersect if 0 ≤ t ≤ 1 and 0 ≤ u ≤ 1. */
  if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0)
    return FALSE;

  *intersection = vector2_add (p, vector2_multiply_constant (t, r));

  return TRUE;
}

static gboolean
is_barrier_horizontal (MetaBarrier *barrier)
{
  return barrier->priv->y1 == barrier->priv->y2;
}

static gboolean
is_barrier_blocking_directions (MetaBarrier         *barrier,
                                MetaBarrierDirection directions)
{
  /* Barriers doesn't block parallel motions. */
  if (is_barrier_horizontal (barrier))
    {
      if ((directions & (META_BARRIER_DIRECTION_POSITIVE_Y |
                         META_BARRIER_DIRECTION_NEGATIVE_Y)) == 0)
        return FALSE;
    }
  else
    {
      if ((directions & (META_BARRIER_DIRECTION_POSITIVE_X |
                         META_BARRIER_DIRECTION_NEGATIVE_X)) == 0)
        return FALSE;
    }

  return (barrier->priv->directions & directions) != directions;
}

static void
dismiss_pointer (MetaBarrierImplNative *self)
{
  MetaBarrierImplNativePrivate *priv =
    meta_barrier_impl_native_get_instance_private (self);

  priv->state = META_BARRIER_STATE_LEFT;
}

static Line2
barrier_to_line (MetaBarrier *barrier)
{
  return (Line2) {
    .a = (Vector2) {
      .x = MIN (barrier->priv->x1, barrier->priv->x2),
      .y = MIN (barrier->priv->y1, barrier->priv->y2),
    },
    .b = (Vector2) {
      .x = MAX (barrier->priv->x1, barrier->priv->x2),
      .y = MAX (barrier->priv->y1, barrier->priv->y2),
    },
  };
}

/*
 * Calculate the hit box for a held motion. The hit box is a 2 px wide region
 * in the opposite direction of every direction the barrier blocks. The purpose
 * of this is to allow small movements without receiving a "left" signal. This
 * heuristic comes from the X.org pointer barrier implementation.
 */
static Line2
calculate_barrier_hit_box (MetaBarrier *barrier)
{
  Line2 hit_box = barrier_to_line (barrier);

  if (is_barrier_horizontal (barrier))
    {
      if (is_barrier_blocking_directions (barrier,
                                          META_BARRIER_DIRECTION_POSITIVE_Y))
        hit_box.a.y -= 2.0f;
      if (is_barrier_blocking_directions (barrier,
                                          META_BARRIER_DIRECTION_NEGATIVE_Y))
        hit_box.b.y += 2.0f;
    }
  else
    {
      if (is_barrier_blocking_directions (barrier,
                                          META_BARRIER_DIRECTION_POSITIVE_X))
        hit_box.a.x -= 2.0f;
      if (is_barrier_blocking_directions (barrier,
                                          META_BARRIER_DIRECTION_NEGATIVE_X))
        hit_box.b.x += 2.0f;
    }

  return hit_box;
}

static gboolean
is_within_box (Line2 box, Vector2 point)
{
  return (point.x >= box.a.x && point.x < box.b.x &&
          point.y >= box.a.y && point.y < box.b.y);
}

static void
maybe_release_barrier (gpointer key,
                       gpointer value,
                       gpointer user_data)
{
  MetaBarrierImplNative *self = key;
  MetaBarrierImplNativePrivate *priv =
    meta_barrier_impl_native_get_instance_private (self);
  MetaBarrier *barrier = priv->barrier;
  Line2 *motion = user_data;
  Line2 hit_box;

  if (priv->state != META_BARRIER_STATE_HELD)
    return;

  /* Release if we end up outside barrier end points. */
  if (is_barrier_horizontal (barrier))
    {
      if (motion->b.x > MAX (barrier->priv->x1, barrier->priv->x2) ||
          motion->b.x < MIN (barrier->priv->x1, barrier->priv->x2))
        {
          dismiss_pointer (self);
          return;
        }
    }
  else
    {
      if (motion->b.y > MAX (barrier->priv->y1, barrier->priv->y2) ||
          motion->b.y < MIN (barrier->priv->y1, barrier->priv->y2))
        {
          dismiss_pointer (self);
          return;
        }
    }

  /* Release if we don't intersect and end up outside of hit box. */
  hit_box = calculate_barrier_hit_box (barrier);
  if (!is_within_box (hit_box, motion->b))
    {
      dismiss_pointer (self);
      return;
    }
}

static void
maybe_release_barriers (MetaBarrierManagerNative *manager,
                        float                     prev_x,
                        float                     prev_y,
                        float                     x,
                        float                     y)
{
  Line2 motion = {
    .a = {
      .x = prev_x,
      .y = prev_y,
    },
    .b = {
      .x = x,
      .y = y,
    },
  };

  g_hash_table_foreach (manager->barriers,
                        maybe_release_barrier,
                        &motion);
}

typedef struct _MetaClosestBarrierData
{
  struct
  {
    Line2                       motion;
    MetaBarrierDirection        directions;
  } in;

  struct
  {
    float                       closest_distance_2;
    MetaBarrierImplNative      *barrier_impl;
  } out;
} MetaClosestBarrierData;

static void
update_closest_barrier (gpointer key,
                        gpointer value,
                        gpointer user_data)
{
  MetaBarrierImplNative *self = key;
  MetaBarrierImplNativePrivate *priv =
    meta_barrier_impl_native_get_instance_private (self);
  MetaBarrier *barrier = priv->barrier;
  MetaClosestBarrierData *data = user_data;
  Line2 barrier_line;
  Vector2 intersection;
  float dx, dy;
  float distance_2;

  /* Ignore if the barrier is not blocking in any of the motions directions. */
  if (!is_barrier_blocking_directions (barrier, data->in.directions))
    return;

  /* Ignore if the barrier released the pointer. */
  if (priv->state == META_BARRIER_STATE_RELEASE)
    return;

  /* Ignore if we are moving away from barrier. */
  if (priv->state == META_BARRIER_STATE_HELD &&
      (data->in.directions & priv->blocked_dir) == 0)
    return;

  /* Check if the motion intersects with the barrier, and retrieve the
   * intersection point if any. */
  barrier_line = (Line2) {
    .a = {
      .x = barrier->priv->x1,
      .y = barrier->priv->y1
    },
    .b = {
      .x = barrier->priv->x2,
      .y = barrier->priv->y2
    },
  };
  if (!lines_intersect (&barrier_line, &data->in.motion, &intersection))
    return;

  /* Calculate the distance to the barrier and keep track of the closest
   * barrier. */
  dx = intersection.x - data->in.motion.a.x;
  dy = intersection.y - data->in.motion.a.y;
  distance_2 = dx*dx + dy*dy;
  if (data->out.barrier_impl == NULL ||
      distance_2 < data->out.closest_distance_2)
    {
      data->out.barrier_impl = self;
      data->out.closest_distance_2 = distance_2;
    }
}

static gboolean
get_closest_barrier (MetaBarrierManagerNative *manager,
                     float                     prev_x,
                     float                     prev_y,
                     float                     x,
                     float                     y,
                     MetaBarrierDirection      motion_dir,
                     MetaBarrierImplNative   **barrier_impl)
{
  MetaClosestBarrierData closest_barrier_data;

  closest_barrier_data = (MetaClosestBarrierData) {
    .in = {
      .motion = {
        .a = {
          .x = prev_x,
          .y = prev_y,
        },
        .b = {
          .x = x,
          .y = y,
        },
      },
      .directions = motion_dir,
    },
  };

  g_hash_table_foreach (manager->barriers,
                        update_closest_barrier,
                        &closest_barrier_data);

  if (closest_barrier_data.out.barrier_impl != NULL)
    {
      *barrier_impl = closest_barrier_data.out.barrier_impl;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

typedef struct _MetaBarrierEventData
{
  guint32             time;
  float               prev_x;
  float               prev_y;
  float               x;
  float               y;
  float               dx;
  float               dy;
} MetaBarrierEventData;

static void
emit_barrier_event (MetaBarrierImplNative *self,
                    guint32                time,
                    float                  prev_x,
                    float                  prev_y,
                    float                  x,
                    float                  y,
                    float                  dx,
                    float                  dy)
{
  MetaBarrierImplNativePrivate *priv =
    meta_barrier_impl_native_get_instance_private (self);
  MetaBarrier *barrier = priv->barrier;
  MetaBarrierEvent *event = g_slice_new0 (MetaBarrierEvent);
  MetaBarrierState old_state = priv->state;

  switch (priv->state)
    {
    case META_BARRIER_STATE_HIT:
      priv->state = META_BARRIER_STATE_HELD;
      priv->trigger_serial = next_serial ();
      event->dt = 0;

      break;
    case META_BARRIER_STATE_RELEASE:
    case META_BARRIER_STATE_LEFT:
      priv->state = META_BARRIER_STATE_ACTIVE;

      /* Intentional fall-through. */
    case META_BARRIER_STATE_HELD:
      event->dt = time - priv->last_event_time;

      break;
    case META_BARRIER_STATE_ACTIVE:
      g_assert_not_reached (); /* Invalid state. */
    }

  event->ref_count = 1;
  event->event_id = priv->trigger_serial;
  event->time = time;

  event->x = x;
  event->y = y;
  event->dx = dx;
  event->dy = dy;

  event->grabbed = priv->state == META_BARRIER_STATE_HELD;
  event->released = old_state == META_BARRIER_STATE_RELEASE;

  priv->last_event_time = time;

  if (priv->state == META_BARRIER_STATE_HELD)
    _meta_barrier_emit_hit_signal (barrier, event);
  else
    _meta_barrier_emit_left_signal (barrier, event);

  meta_barrier_event_unref (event);
}

static void
maybe_emit_barrier_event (gpointer key, gpointer value, gpointer user_data)
{
  MetaBarrierImplNative *self = key;
  MetaBarrierImplNativePrivate *priv =
    meta_barrier_impl_native_get_instance_private (self);
  MetaBarrierEventData *data = user_data;

  switch (priv->state) {
    case META_BARRIER_STATE_ACTIVE:
      break;
    case META_BARRIER_STATE_HIT:
    case META_BARRIER_STATE_HELD:
    case META_BARRIER_STATE_RELEASE:
    case META_BARRIER_STATE_LEFT:
      emit_barrier_event (self,
                          data->time,
                          data->prev_x,
                          data->prev_y,
                          data->x,
                          data->y,
                          data->dx,
                          data->dy);
      break;
    }
}

/* Clamp (x, y) to the barrier and remove clamped direction from motion_dir. */
static void
clamp_to_barrier (MetaBarrierImplNative *self,
                  MetaBarrierDirection *motion_dir,
                  float *x,
                  float *y)
{
  MetaBarrierImplNativePrivate *priv =
    meta_barrier_impl_native_get_instance_private (self);
  MetaBarrier *barrier = priv->barrier;

  if (is_barrier_horizontal (barrier))
    {
      if (*motion_dir & META_BARRIER_DIRECTION_POSITIVE_Y)
        *y = barrier->priv->y1;
      else if (*motion_dir & META_BARRIER_DIRECTION_NEGATIVE_Y)
        *y = barrier->priv->y1;

      priv->blocked_dir = *motion_dir & (META_BARRIER_DIRECTION_POSITIVE_Y |
                                         META_BARRIER_DIRECTION_NEGATIVE_Y);
      *motion_dir &= ~(META_BARRIER_DIRECTION_POSITIVE_Y |
                       META_BARRIER_DIRECTION_NEGATIVE_Y);
    }
  else
    {
      if (*motion_dir & META_BARRIER_DIRECTION_POSITIVE_X)
        *x = barrier->priv->x1;
      else if (*motion_dir & META_BARRIER_DIRECTION_NEGATIVE_X)
        *x = barrier->priv->x1;

      priv->blocked_dir = *motion_dir & (META_BARRIER_DIRECTION_POSITIVE_X |
                                         META_BARRIER_DIRECTION_NEGATIVE_X);
      *motion_dir &= ~(META_BARRIER_DIRECTION_POSITIVE_X |
                       META_BARRIER_DIRECTION_NEGATIVE_X);
    }

  priv->state = META_BARRIER_STATE_HIT;
}

void
meta_barrier_manager_native_process (MetaBarrierManagerNative *manager,
                                     ClutterInputDevice       *device,
                                     guint32                   time,
                                     float                    *x,
                                     float                    *y)
{
  ClutterPoint prev_pos;
  float prev_x;
  float prev_y;
  float orig_x = *x;
  float orig_y = *y;
  MetaBarrierDirection motion_dir = 0;
  MetaBarrierEventData barrier_event_data;
  MetaBarrierImplNative *barrier_impl;

  if (!clutter_input_device_get_coords (device, NULL, &prev_pos))
    return;

  prev_x = prev_pos.x;
  prev_y = prev_pos.y;

  /* Get the direction of the motion vector. */
  if (prev_x < *x)
    motion_dir |= META_BARRIER_DIRECTION_POSITIVE_X;
  else if (prev_x > *x)
    motion_dir |= META_BARRIER_DIRECTION_NEGATIVE_X;
  if (prev_y < *y)
    motion_dir |= META_BARRIER_DIRECTION_POSITIVE_Y;
  else if (prev_y > *y)
    motion_dir |= META_BARRIER_DIRECTION_NEGATIVE_Y;

  /* Clamp to the closest barrier in any direction until either there are no
   * more barriers to clamp to or all directions have been clamped. */
  while (motion_dir != 0)
    {
      if (get_closest_barrier (manager,
                               prev_x, prev_y,
                               *x, *y,
                               motion_dir,
                               &barrier_impl))
        clamp_to_barrier (barrier_impl, &motion_dir, x, y);
      else
        break;
    }

  /* Potentially release active barrier movements. */
  maybe_release_barriers (manager, prev_x, prev_y, *x, *y);

  /* Initiate or continue barrier interaction. */
  barrier_event_data = (MetaBarrierEventData) {
    .time = time,
    .prev_x = prev_x,
    .prev_y = prev_y,
    .x = *x,
    .y = *y,
    .dx = orig_x - prev_x,
    .dy = orig_y - prev_y,
  };

  g_hash_table_foreach (manager->barriers,
                        maybe_emit_barrier_event,
                        &barrier_event_data);
}

static gboolean
_meta_barrier_impl_native_is_active (MetaBarrierImpl *impl)
{
  MetaBarrierImplNative *self = META_BARRIER_IMPL_NATIVE (impl);
  MetaBarrierImplNativePrivate *priv =
    meta_barrier_impl_native_get_instance_private (self);

  return priv->is_active;
}

static void
_meta_barrier_impl_native_release (MetaBarrierImpl  *impl,
                                   MetaBarrierEvent *event)
{
  MetaBarrierImplNative *self = META_BARRIER_IMPL_NATIVE (impl);
  MetaBarrierImplNativePrivate *priv =
    meta_barrier_impl_native_get_instance_private (self);

  if (priv->state == META_BARRIER_STATE_HELD &&
      event->event_id == priv->trigger_serial)
    priv->state = META_BARRIER_STATE_RELEASE;
}

static void
_meta_barrier_impl_native_destroy (MetaBarrierImpl *impl)
{
  MetaBarrierImplNative *self = META_BARRIER_IMPL_NATIVE (impl);
  MetaBarrierImplNativePrivate *priv =
    meta_barrier_impl_native_get_instance_private (self);

  g_hash_table_remove (priv->manager->barriers, self);
  priv->is_active = FALSE;
}

MetaBarrierImpl *
meta_barrier_impl_native_new (MetaBarrier *barrier)
{
  MetaBarrierImplNative *self;
  MetaBarrierImplNativePrivate *priv;
  MetaBackendNative *native;
  MetaBarrierManagerNative *manager;

  self = g_object_new (META_TYPE_BARRIER_IMPL_NATIVE, NULL);
  priv = meta_barrier_impl_native_get_instance_private (self);

  priv->barrier = barrier;
  priv->is_active = TRUE;

  native = META_BACKEND_NATIVE (meta_get_backend ());
  manager = meta_backend_native_get_barrier_manager (native);
  priv->manager = manager;
  g_hash_table_add (manager->barriers, self);

  return META_BARRIER_IMPL (self);
}

static void
meta_barrier_impl_native_class_init (MetaBarrierImplNativeClass *klass)
{
  MetaBarrierImplClass *impl_class = META_BARRIER_IMPL_CLASS (klass);

  impl_class->is_active = _meta_barrier_impl_native_is_active;
  impl_class->release = _meta_barrier_impl_native_release;
  impl_class->destroy = _meta_barrier_impl_native_destroy;
}

static void
meta_barrier_impl_native_init (MetaBarrierImplNative *self)
{
}

MetaBarrierManagerNative *
meta_barrier_manager_native_new (void)
{
  MetaBarrierManagerNative *manager;

  manager = g_new0 (MetaBarrierManagerNative, 1);

  manager->barriers = g_hash_table_new (NULL, NULL);

  return manager;
}
