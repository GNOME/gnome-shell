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
 * SECTION:clutter-bind-constraint
 * @Title: ClutterBindConstraint
 * @Short_Description: A constraint binding the position or size of an actor
 *
 * #ClutterBindConstraint is a #ClutterConstraint that binds the
 * position or the size of the #ClutterActor to which it is applied
 * to the the position or the size of another #ClutterActor, or
 * "source".
 *
 * An offset can be applied to the constraint, to avoid overlapping. The offset
 * can also be animated. For instance, the following code will set up three
 * actors to be bound to the same origin:
 *
 * |[<!-- language="C" -->
 * // source
 * rect[0] = clutter_rectangle_new_with_color (&red_color);
 * clutter_actor_set_position (rect[0], x_pos, y_pos);
 * clutter_actor_set_size (rect[0], 100, 100);
 *
 * // second rectangle
 * rect[1] = clutter_rectangle_new_with_color (&green_color);
 * clutter_actor_set_size (rect[1], 100, 100);
 * clutter_actor_set_opacity (rect[1], 0);
 *
 * constraint = clutter_bind_constraint_new (rect[0], CLUTTER_BIND_X, 0.0);
 * clutter_actor_add_constraint_with_name (rect[1], "green-x", constraint);
 * constraint = clutter_bind_constraint_new (rect[0], CLUTTER_BIND_Y, 0.0);
 * clutter_actor_add_constraint_with_name (rect[1], "green-y", constraint);
 *
 * // third rectangle
 * rect[2] = clutter_rectangle_new_with_color (&blue_color);
 * clutter_actor_set_size (rect[2], 100, 100);
 * clutter_actor_set_opacity (rect[2], 0);
 *
 * constraint = clutter_bind_constraint_new (rect[0], CLUTTER_BIND_X, 0.0);
 * clutter_actor_add_constraint_with_name (rect[2], "blue-x", constraint);
 * constraint = clutter_bind_constraint_new (rect[0], CLUTTER_BIND_Y, 0.0);
 * clutter_actor_add_constraint_with_name (rect[2], "blue-y", constraint);
 * ]|
 *
 * The following code animates the second and third rectangles to "expand"
 * them horizontally from underneath the first rectangle:
 *
 * |[<!-- language="C" -->
 * clutter_actor_animate (rect[1], CLUTTER_EASE_OUT_CUBIC, 250,
 *                        "@constraints.green-x.offset", 100.0,
 *                        "opacity", 255,
 *                        NULL);
 * clutter_actor_animate (rect[2], CLUTTER_EASE_OUT_CUBIC, 250,
 *                        "@constraints.blue-x.offset", 200.0,
 *                        "opacity", 255,
 *                        NULL);
 * ]|
 *
 * #ClutterBindConstraint is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "clutter-bind-constraint.h"

#include "clutter-actor-meta-private.h"
#include "clutter-actor-private.h"
#include "clutter-constraint.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"

#define CLUTTER_BIND_CONSTRAINT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BIND_CONSTRAINT, ClutterBindConstraintClass))
#define CLUTTER_IS_BIND_CONSTRAINT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BIND_CONSTRAINT))
#define CLUTTER_BIND_CONSTRAINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BIND_CONSTRAINT, ClutterBindConstraintClass))

struct _ClutterBindConstraint
{
  ClutterConstraint parent_instance;

  ClutterActor *actor;
  ClutterActor *source;
  ClutterBindCoordinate coordinate;
  gfloat offset;
};

struct _ClutterBindConstraintClass
{
  ClutterConstraintClass parent_class;
};

enum
{
  PROP_0,

  PROP_SOURCE,
  PROP_COORDINATE,
  PROP_OFFSET,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE (ClutterBindConstraint,
               clutter_bind_constraint,
               CLUTTER_TYPE_CONSTRAINT);

static void
source_queue_relayout (ClutterActor          *source,
                       ClutterBindConstraint *bind)
{
  if (bind->actor != NULL)
    _clutter_actor_queue_only_relayout (bind->actor);
}

static void
source_destroyed (ClutterActor          *actor,
                  ClutterBindConstraint *bind)
{
  bind->source = NULL;
}

static void
clutter_bind_constraint_update_allocation (ClutterConstraint *constraint,
                                           ClutterActor      *actor,
                                           ClutterActorBox   *allocation)
{
  ClutterBindConstraint *bind = CLUTTER_BIND_CONSTRAINT (constraint);
  gfloat source_width, source_height;
  gfloat actor_width, actor_height;
  ClutterVertex source_position = { 0., };

  if (bind->source == NULL)
    return;

  source_position.x = clutter_actor_get_x (bind->source);
  source_position.y = clutter_actor_get_y (bind->source);
  clutter_actor_get_size (bind->source, &source_width, &source_height);

  clutter_actor_box_get_size (allocation, &actor_width, &actor_height);

  switch (bind->coordinate)
    {
    case CLUTTER_BIND_X:
      allocation->x1 = source_position.x + bind->offset;
      allocation->x2 = allocation->x1 + actor_width;
      break;

    case CLUTTER_BIND_Y:
      allocation->y1 = source_position.y + bind->offset;
      allocation->y2 = allocation->y1 + actor_height;
      break;

    case CLUTTER_BIND_POSITION:
      allocation->x1 = source_position.x + bind->offset;
      allocation->y1 = source_position.y + bind->offset;
      allocation->x2 = allocation->x1 + actor_width;
      allocation->y2 = allocation->y1 + actor_height;
      break;

    case CLUTTER_BIND_WIDTH:
      allocation->x2 = allocation->x1 + source_width + bind->offset;
      break;

    case CLUTTER_BIND_HEIGHT:
      allocation->y2 = allocation->y1 + source_height + bind->offset;
      break;

    case CLUTTER_BIND_SIZE:
      allocation->x2 = allocation->x1 + source_width + bind->offset;
      allocation->y2 = allocation->y1 + source_height + bind->offset;
      break;

    case CLUTTER_BIND_ALL:
      allocation->x1 = source_position.x + bind->offset;
      allocation->y1 = source_position.y + bind->offset;
      allocation->x2 = allocation->x1 + source_width + bind->offset;
      allocation->y2 = allocation->y1 + source_height + bind->offset;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  clutter_actor_box_clamp_to_pixel (allocation);
}

static void
clutter_bind_constraint_set_actor (ClutterActorMeta *meta,
                                   ClutterActor     *new_actor)
{
  ClutterBindConstraint *bind = CLUTTER_BIND_CONSTRAINT (meta);
  ClutterActorMetaClass *parent;

  if (new_actor != NULL &&
      bind->source != NULL &&
      clutter_actor_contains (new_actor, bind->source))
    {
      g_warning (G_STRLOC ": The source actor '%s' is contained "
                 "by the actor '%s' associated to the constraint "
                 "'%s'",
                 _clutter_actor_get_debug_name (bind->source),
                 _clutter_actor_get_debug_name (new_actor),
                 _clutter_actor_meta_get_debug_name (meta));
      return;
    }

  /* store the pointer to the actor, for later use */
  bind->actor = new_actor;

  parent = CLUTTER_ACTOR_META_CLASS (clutter_bind_constraint_parent_class);
  parent->set_actor (meta, new_actor);
}

static void
clutter_bind_constraint_dispose (GObject *gobject)
{
  ClutterBindConstraint *bind = CLUTTER_BIND_CONSTRAINT (gobject);

  if (bind->source != NULL)
    {
      g_signal_handlers_disconnect_by_func (bind->source,
                                            G_CALLBACK (source_destroyed),
                                            bind);
      g_signal_handlers_disconnect_by_func (bind->source,
                                            G_CALLBACK (source_queue_relayout),
                                            bind);
      bind->source = NULL;
    }

  G_OBJECT_CLASS (clutter_bind_constraint_parent_class)->dispose (gobject);
}

static void
clutter_bind_constraint_set_property (GObject      *gobject,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ClutterBindConstraint *bind = CLUTTER_BIND_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_SOURCE:
      clutter_bind_constraint_set_source (bind, g_value_get_object (value));
      break;

    case PROP_COORDINATE:
      clutter_bind_constraint_set_coordinate (bind, g_value_get_enum (value));
      break;

    case PROP_OFFSET:
      clutter_bind_constraint_set_offset (bind, g_value_get_float (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_bind_constraint_get_property (GObject    *gobject,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ClutterBindConstraint *bind = CLUTTER_BIND_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, bind->source);
      break;

    case PROP_COORDINATE:
      g_value_set_enum (value, bind->coordinate);
      break;

    case PROP_OFFSET:
      g_value_set_float (value, bind->offset);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_bind_constraint_class_init (ClutterBindConstraintClass *klass)
{
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);
  ClutterConstraintClass *constraint_class = CLUTTER_CONSTRAINT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = clutter_bind_constraint_set_property;
  gobject_class->get_property = clutter_bind_constraint_get_property;
  gobject_class->dispose = clutter_bind_constraint_dispose;

  meta_class->set_actor = clutter_bind_constraint_set_actor;

  constraint_class->update_allocation = clutter_bind_constraint_update_allocation;
  /**
   * ClutterBindConstraint:source:
   *
   * The #ClutterActor used as the source for the binding.
   *
   * The #ClutterActor must not be contained inside the actor associated
   * to the constraint.
   *
   * Since: 1.4
   */
  obj_props[PROP_SOURCE] =
    g_param_spec_object ("source",
                         P_("Source"),
                         P_("The source of the binding"),
                         CLUTTER_TYPE_ACTOR,
                         CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  /**
   * ClutterBindConstraint:coordinate:
   *
   * The coordinate to be bound
   *
   * Since: 1.4
   */
  obj_props[PROP_COORDINATE] =
    g_param_spec_enum ("coordinate",
                       P_("Coordinate"),
                       P_("The coordinate to bind"),
                       CLUTTER_TYPE_BIND_COORDINATE,
                       CLUTTER_BIND_X,
                       CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  /**
   * ClutterBindConstraint:offset:
   *
   * The offset, in pixels, to be applied to the binding
   *
   * Since: 1.4
   */
  obj_props[PROP_OFFSET] =
    g_param_spec_float ("offset",
                        P_("Offset"),
                        P_("The offset in pixels to apply to the binding"),
                        -G_MAXFLOAT, G_MAXFLOAT,
                        0.0f,
                        CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_properties (gobject_class,
                                     PROP_LAST,
                                     obj_props);
}

static void
clutter_bind_constraint_init (ClutterBindConstraint *self)
{
  self->actor = NULL;
  self->source = NULL;
  self->coordinate = CLUTTER_BIND_X;
  self->offset = 0.0f;
}

/**
 * clutter_bind_constraint_new:
 * @source: (allow-none): the #ClutterActor to use as the source of
 *   the binding, or %NULL
 * @coordinate: the coordinate to bind
 * @offset: the offset to apply to the binding, in pixels
 *
 * Creates a new constraint, binding a #ClutterActor's position to
 * the given @coordinate of the position of @source
 *
 * Return value: the newly created #ClutterBindConstraint
 *
 * Since: 1.4
 */
ClutterConstraint *
clutter_bind_constraint_new (ClutterActor          *source,
                             ClutterBindCoordinate  coordinate,
                             gfloat                 offset)
{
  g_return_val_if_fail (source == NULL || CLUTTER_IS_ACTOR (source), NULL);

  return g_object_new (CLUTTER_TYPE_BIND_CONSTRAINT,
                       "source", source,
                       "coordinate", coordinate,
                       "offset", offset,
                       NULL);
}

/**
 * clutter_bind_constraint_set_source:
 * @constraint: a #ClutterBindConstraint
 * @source: (allow-none): a #ClutterActor, or %NULL to unset the source
 *
 * Sets the source #ClutterActor for the constraint
 *
 * Since: 1.4
 */
void
clutter_bind_constraint_set_source (ClutterBindConstraint *constraint,
                                    ClutterActor          *source)
{
  ClutterActor *old_source, *actor;
  ClutterActorMeta *meta;

  g_return_if_fail (CLUTTER_IS_BIND_CONSTRAINT (constraint));
  g_return_if_fail (source == NULL || CLUTTER_IS_ACTOR (source));

  if (constraint->source == source)
    return;

  meta = CLUTTER_ACTOR_META (constraint);
  actor = clutter_actor_meta_get_actor (meta);
  if (source != NULL && actor != NULL)
    {
      if (clutter_actor_contains (actor, source))
        {
          g_warning (G_STRLOC ": The source actor '%s' is contained "
                     "by the actor '%s' associated to the constraint "
                     "'%s'",
                     _clutter_actor_get_debug_name (source),
                     _clutter_actor_get_debug_name (actor),
                     _clutter_actor_meta_get_debug_name (meta));
          return;
        }
    }

  old_source = constraint->source;
  if (old_source != NULL)
    {
      g_signal_handlers_disconnect_by_func (old_source,
                                            G_CALLBACK (source_destroyed),
                                            constraint);
      g_signal_handlers_disconnect_by_func (old_source,
                                            G_CALLBACK (source_queue_relayout),
                                            constraint);
    }

  constraint->source = source;
  if (constraint->source != NULL)
    {
      g_signal_connect (constraint->source, "queue-relayout",
                        G_CALLBACK (source_queue_relayout),
                        constraint);
      g_signal_connect (constraint->source, "destroy",
                        G_CALLBACK (source_destroyed),
                        constraint);

      if (constraint->actor != NULL)
        clutter_actor_queue_relayout (constraint->actor);
    }

  g_object_notify_by_pspec (G_OBJECT (constraint), obj_props[PROP_SOURCE]);
}

/**
 * clutter_bind_constraint_get_source:
 * @constraint: a #ClutterBindConstraint
 *
 * Retrieves the #ClutterActor set using clutter_bind_constraint_set_source()
 *
 * Return value: (transfer none): a pointer to the source actor
 *
 * Since: 1.4
 */
ClutterActor *
clutter_bind_constraint_get_source (ClutterBindConstraint *constraint)
{
  g_return_val_if_fail (CLUTTER_IS_BIND_CONSTRAINT (constraint), NULL);

  return constraint->source;
}

/**
 * clutter_bind_constraint_set_coordinate:
 * @constraint: a #ClutterBindConstraint
 * @coordinate: the coordinate to bind
 *
 * Sets the coordinate to bind in the constraint
 *
 * Since: 1.4
 */
void
clutter_bind_constraint_set_coordinate (ClutterBindConstraint *constraint,
                                        ClutterBindCoordinate  coordinate)
{
  g_return_if_fail (CLUTTER_IS_BIND_CONSTRAINT (constraint));

  if (constraint->coordinate == coordinate)
    return;

  constraint->coordinate = coordinate;

  if (constraint->actor != NULL)
    clutter_actor_queue_relayout (constraint->actor);

  g_object_notify_by_pspec (G_OBJECT (constraint), obj_props[PROP_COORDINATE]);
}

/**
 * clutter_bind_constraint_get_coordinate:
 * @constraint: a #ClutterBindConstraint
 *
 * Retrieves the bound coordinate of the constraint
 *
 * Return value: the bound coordinate
 *
 * Since: 1.4
 */
ClutterBindCoordinate
clutter_bind_constraint_get_coordinate (ClutterBindConstraint *constraint)
{
  g_return_val_if_fail (CLUTTER_IS_BIND_CONSTRAINT (constraint),
                        CLUTTER_BIND_X);

  return constraint->coordinate;
}

/**
 * clutter_bind_constraint_set_offset:
 * @constraint: a #ClutterBindConstraint
 * @offset: the offset to apply, in pixels
 *
 * Sets the offset to be applied to the constraint
 *
 * Since: 1.4
 */
void
clutter_bind_constraint_set_offset (ClutterBindConstraint *constraint,
                                    gfloat                 offset)
{
  g_return_if_fail (CLUTTER_IS_BIND_CONSTRAINT (constraint));

  if (fabs (constraint->offset - offset) < 0.00001f)
    return;

  constraint->offset = offset;

  if (constraint->actor != NULL)
    clutter_actor_queue_relayout (constraint->actor);

  g_object_notify_by_pspec (G_OBJECT (constraint), obj_props[PROP_OFFSET]);
}

/**
 * clutter_bind_constraint_get_offset:
 * @constraint: a #ClutterBindConstraint
 *
 * Retrieves the offset set using clutter_bind_constraint_set_offset()
 *
 * Return value: the offset, in pixels
 *
 * Since: 1.4
 */
gfloat
clutter_bind_constraint_get_offset (ClutterBindConstraint *bind)
{
  g_return_val_if_fail (CLUTTER_IS_BIND_CONSTRAINT (bind), 0.0);

  return bind->offset;
}
