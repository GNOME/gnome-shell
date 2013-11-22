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
 * SECTION:clutter-snap-constraint
 * @Title: ClutterSnapConstraint
 * @Short_Description: A constraint snapping two actors together
 *
 * #ClutterSnapConstraint is a constraint the snaps the edges of two
 * actors together, expanding the actor's allocation if necessary.
 *
 * An offset can be applied to the constraint, to provide spacing.
 *
 * #ClutterSnapConstraint is available since Clutter 1.6
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "clutter-snap-constraint.h"

#include "clutter-actor-private.h"
#include "clutter-constraint.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"

#define CLUTTER_SNAP_CONSTRAINT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_SNAP_CONSTRAINT, ClutterSnapConstraintClass))
#define CLUTTER_IS_SNAP_CONSTRAINT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_SNAP_CONSTRAINT))
#define CLUTTER_SNAP_CONSTRAINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_SNAP_CONSTRAINT, ClutterSnapConstraintClass))

struct _ClutterSnapConstraint
{
  ClutterConstraint parent_instance;

  ClutterActor *actor;
  ClutterActor *source;

  ClutterSnapEdge from_edge;
  ClutterSnapEdge to_edge;

  gfloat offset;
};

struct _ClutterSnapConstraintClass
{
  ClutterConstraintClass parent_class;
};

enum
{
  PROP_0,

  PROP_SOURCE,
  PROP_FROM_EDGE,
  PROP_TO_EDGE,
  PROP_OFFSET,

  PROP_LAST
};

G_DEFINE_TYPE (ClutterSnapConstraint,
               clutter_snap_constraint,
               CLUTTER_TYPE_CONSTRAINT);

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

static void
source_queue_relayout (ClutterActor          *source,
                       ClutterSnapConstraint *constraint)
{
  if (constraint->actor != NULL)
    _clutter_actor_queue_only_relayout (constraint->actor);
}

static void
source_destroyed (ClutterActor          *actor,
                  ClutterSnapConstraint *constraint)
{
  constraint->source = NULL;
}

static inline void
warn_horizontal_edge (const gchar  *edge,
                      ClutterActor *actor,
                      ClutterActor *source)
{
  g_warning (G_STRLOC ": the %s edge of actor '%s' can only be snapped "
             "to either the right or the left edge of actor '%s'",
             edge,
             _clutter_actor_get_debug_name (actor),
             _clutter_actor_get_debug_name (source));
}

static inline void
warn_vertical_edge (const gchar  *edge,
                    ClutterActor *actor,
                    ClutterActor *source)
{
  g_warning (G_STRLOC ": the %s edge of actor '%s' can only "
             "be snapped to the top or bottom edge of actor '%s'",
             edge,
             _clutter_actor_get_debug_name (actor),
             _clutter_actor_get_debug_name (source));
}

static void
clutter_snap_constraint_update_allocation (ClutterConstraint *constraint,
                                           ClutterActor      *actor,
                                           ClutterActorBox   *allocation)
{
  ClutterSnapConstraint *self = CLUTTER_SNAP_CONSTRAINT (constraint);
  gfloat source_width, source_height;
  gfloat source_x, source_y;
  gfloat actor_width, actor_height;

  if (self->source == NULL)
    return;

  clutter_actor_get_position (self->source, &source_x, &source_y);
  clutter_actor_get_size (self->source, &source_width, &source_height);

  clutter_actor_box_get_size (allocation, &actor_width, &actor_height);

  switch (self->to_edge)
    {
    case CLUTTER_SNAP_EDGE_LEFT:
      if (self->from_edge == CLUTTER_SNAP_EDGE_LEFT)
        allocation->x1 = source_x + self->offset;
      else if (self->from_edge == CLUTTER_SNAP_EDGE_RIGHT)
        allocation->x2 = source_x + self->offset;
      else
        warn_horizontal_edge ("left", self->actor, self->source);
      break;

    case CLUTTER_SNAP_EDGE_RIGHT:
      if (self->from_edge == CLUTTER_SNAP_EDGE_RIGHT)
        allocation->x2 = source_x + source_width + self->offset;
      else if (self->from_edge == CLUTTER_SNAP_EDGE_LEFT)
        allocation->x1 = source_x + source_width + self->offset;
      else
        warn_horizontal_edge ("right", self->actor, self->source);
      break;

      break;

    case CLUTTER_SNAP_EDGE_TOP:
      if (self->from_edge == CLUTTER_SNAP_EDGE_TOP)
        allocation->y1 = source_y + self->offset;
      else if (self->from_edge == CLUTTER_SNAP_EDGE_BOTTOM)
        allocation->y2 = source_y + self->offset;
      else
        warn_vertical_edge ("top", self->actor, self->source);
      break;

    case CLUTTER_SNAP_EDGE_BOTTOM:
      if (self->from_edge == CLUTTER_SNAP_EDGE_BOTTOM)
        allocation->y2 = source_y + source_height + self->offset;
      else if (self->from_edge == CLUTTER_SNAP_EDGE_TOP)
        allocation->y1 = source_y + source_height + self->offset;
      else
        warn_vertical_edge ("bottom", self->actor, self->source);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  if (allocation->x2 - allocation->x1 < 0)
    allocation->x2 = allocation->x1;

  if (allocation->y2 - allocation->y1 < 0)
    allocation->y2 = allocation->y1;
}

static void
clutter_snap_constraint_set_actor (ClutterActorMeta *meta,
                                   ClutterActor     *new_actor)
{
  ClutterSnapConstraint *self = CLUTTER_SNAP_CONSTRAINT (meta);
  ClutterActorMetaClass *parent;

  /* store the pointer to the actor, for later use */
  self->actor = new_actor;

  parent = CLUTTER_ACTOR_META_CLASS (clutter_snap_constraint_parent_class);
  parent->set_actor (meta, new_actor);
}

static void
clutter_snap_constraint_dispose (GObject *gobject)
{
  ClutterSnapConstraint *snap = CLUTTER_SNAP_CONSTRAINT (gobject);

  if (snap->source != NULL)
    {
      g_signal_handlers_disconnect_by_func (snap->source,
                                            G_CALLBACK (source_destroyed),
                                            snap);
      g_signal_handlers_disconnect_by_func (snap->source,
                                            G_CALLBACK (source_queue_relayout),
                                            snap);
      snap->source = NULL;
    }

  G_OBJECT_CLASS (clutter_snap_constraint_parent_class)->dispose (gobject);
}

static void
clutter_snap_constraint_set_property (GObject      *gobject,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ClutterSnapConstraint *self = CLUTTER_SNAP_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_SOURCE:
      clutter_snap_constraint_set_source (self, g_value_get_object (value));
      break;

    case PROP_FROM_EDGE:
      clutter_snap_constraint_set_edges (self,
                                         g_value_get_enum (value),
                                         self->to_edge);
      break;

    case PROP_TO_EDGE:
      clutter_snap_constraint_set_edges (self,
                                         self->from_edge,
                                         g_value_get_enum (value));
      break;

    case PROP_OFFSET:
      clutter_snap_constraint_set_offset (self, g_value_get_float (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_snap_constraint_get_property (GObject    *gobject,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ClutterSnapConstraint *self = CLUTTER_SNAP_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, self->source);
      break;

    case PROP_FROM_EDGE:
      g_value_set_enum (value, self->from_edge);
      break;

    case PROP_TO_EDGE:
      g_value_set_enum (value, self->to_edge);
      break;

    case PROP_OFFSET:
      g_value_set_float (value, self->offset);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_snap_constraint_class_init (ClutterSnapConstraintClass *klass)
{
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);
  ClutterConstraintClass *constraint_class = CLUTTER_CONSTRAINT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  meta_class->set_actor = clutter_snap_constraint_set_actor;

  constraint_class->update_allocation = clutter_snap_constraint_update_allocation;
  /**
   * ClutterSnapConstraint:source:
   *
   * The #ClutterActor used as the source for the constraint
   *
   *
   */
  obj_props[PROP_SOURCE] =
    g_param_spec_object ("source",
                         P_("Source"),
                         P_("The source of the constraint"),
                         CLUTTER_TYPE_ACTOR,
                         CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  /**
   * ClutterSnapConstraint:from-edge:
   *
   * The edge of the #ClutterActor that should be snapped
   *
   *
   */
  obj_props[PROP_FROM_EDGE] =
    g_param_spec_enum ("from-edge",
                       P_("From Edge"),
                       P_("The edge of the actor that should be snapped"),
                       CLUTTER_TYPE_SNAP_EDGE,
                       CLUTTER_SNAP_EDGE_RIGHT,
                       CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  /**
   * ClutterSnapConstraint:to-edge:
   *
   * The edge of the #ClutterSnapConstraint:source that should be snapped
   *
   *
   */
  obj_props[PROP_TO_EDGE] =
    g_param_spec_enum ("to-edge",
                       P_("To Edge"),
                       P_("The edge of the source that should be snapped"),
                       CLUTTER_TYPE_SNAP_EDGE,
                       CLUTTER_SNAP_EDGE_RIGHT,
                       CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  /**
   * ClutterSnapConstraint:offset:
   *
   * The offset, in pixels, between #ClutterSnapConstraint:from-edge
   * and #ClutterSnapConstraint:to-edge
   *
   *
   */
  obj_props[PROP_OFFSET] =
    g_param_spec_float ("offset",
                        P_("Offset"),
                        P_("The offset in pixels to apply to the constraint"),
                        -G_MAXFLOAT, G_MAXFLOAT,
                        0.0f,
                        CLUTTER_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  gobject_class->dispose = clutter_snap_constraint_dispose;
  gobject_class->set_property = clutter_snap_constraint_set_property;
  gobject_class->get_property = clutter_snap_constraint_get_property;
  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
clutter_snap_constraint_init (ClutterSnapConstraint *self)
{
  self->actor = NULL;
  self->source = NULL;

  self->from_edge = CLUTTER_SNAP_EDGE_RIGHT;
  self->to_edge = CLUTTER_SNAP_EDGE_RIGHT;

  self->offset = 0.0f;
}

/**
 * clutter_snap_constraint_new:
 * @source: (allow-none): the #ClutterActor to use as the source of
 *   the constraint, or %NULL
 * @from_edge: the edge of the actor to use in the constraint
 * @to_edge: the edge of @source to use in the constraint
 * @offset: the offset to apply to the constraint, in pixels
 *
 * Creates a new #ClutterSnapConstraint that will snap a #ClutterActor
 * to the @edge of @source, with the given @offset.
 *
 * Return value: the newly created #ClutterSnapConstraint
 *
 *
 */
ClutterConstraint *
clutter_snap_constraint_new (ClutterActor    *source,
                             ClutterSnapEdge  from_edge,
                             ClutterSnapEdge  to_edge,
                             gfloat           offset)
{
  g_return_val_if_fail (source == NULL || CLUTTER_IS_ACTOR (source), NULL);

  return g_object_new (CLUTTER_TYPE_SNAP_CONSTRAINT,
                       "source", source,
                       "from-edge", from_edge,
                       "to-edge", to_edge,
                       "offset", offset,
                       NULL);
}

/**
 * clutter_snap_constraint_set_source:
 * @constraint: a #ClutterSnapConstraint
 * @source: (allow-none): a #ClutterActor, or %NULL to unset the source
 *
 * Sets the source #ClutterActor for the constraint
 *
 *
 */
void
clutter_snap_constraint_set_source (ClutterSnapConstraint *constraint,
                                    ClutterActor          *source)
{
  ClutterActor *old_source;

  g_return_if_fail (CLUTTER_IS_SNAP_CONSTRAINT (constraint));
  g_return_if_fail (source == NULL || CLUTTER_IS_ACTOR (source));

  if (constraint->source == source)
    return;

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
 * clutter_snap_constraint_get_source:
 * @constraint: a #ClutterSnapConstraint
 *
 * Retrieves the #ClutterActor set using clutter_snap_constraint_set_source()
 *
 * Return value: (transfer none): a pointer to the source actor
 *
 *
 */
ClutterActor *
clutter_snap_constraint_get_source (ClutterSnapConstraint *constraint)
{
  g_return_val_if_fail (CLUTTER_IS_SNAP_CONSTRAINT (constraint), NULL);

  return constraint->source;
}

/**
 * clutter_snap_constraint_set_edges:
 * @constraint: a #ClutterSnapConstraint
 * @from_edge: the edge on the actor
 * @to_edge: the edge on the source
 *
 * Sets the edges to be used by the @constraint
 *
 * The @from_edge is the edge on the #ClutterActor to which @constraint
 * has been added. The @to_edge is the edge of the #ClutterActor inside
 * the #ClutterSnapConstraint:source property.
 *
 *
 */
void
clutter_snap_constraint_set_edges (ClutterSnapConstraint *constraint,
                                   ClutterSnapEdge        from_edge,
                                   ClutterSnapEdge        to_edge)
{
  gboolean from_changed = FALSE, to_changed = FALSE;

  g_return_if_fail (CLUTTER_IS_SNAP_CONSTRAINT (constraint));

  g_object_freeze_notify (G_OBJECT (constraint));

  if (constraint->from_edge != from_edge)
    {
      constraint->from_edge = from_edge;
      g_object_notify_by_pspec (G_OBJECT (constraint),
                                obj_props[PROP_FROM_EDGE]);
      from_changed = TRUE;
    }

  if (constraint->to_edge != to_edge)
    {
      constraint->to_edge = to_edge;
      g_object_notify_by_pspec (G_OBJECT (constraint),
                                obj_props[PROP_TO_EDGE]);
      to_changed = TRUE;
    }

  if ((from_changed || to_changed) &&
      constraint->actor != NULL)
    {
      clutter_actor_queue_relayout (constraint->actor);
    }

  g_object_thaw_notify (G_OBJECT (constraint));
}

/**
 * clutter_snap_constraint_get_edges:
 * @constraint: a #ClutterSnapConstraint
 * @from_edge: (out): return location for the actor's edge, or %NULL
 * @to_edge: (out): return location for the source's edge, or %NULL
 *
 * Retrieves the edges used by the @constraint
 *
 *
 */
void
clutter_snap_constraint_get_edges (ClutterSnapConstraint *constraint,
                                   ClutterSnapEdge       *from_edge,
                                   ClutterSnapEdge       *to_edge)
{
  g_return_if_fail (CLUTTER_IS_SNAP_CONSTRAINT (constraint));

  if (from_edge)
    *from_edge = constraint->from_edge;

  if (to_edge)
    *to_edge = constraint->to_edge;
}

/**
 * clutter_snap_constraint_set_offset:
 * @constraint: a #ClutterSnapConstraint
 * @offset: the offset to apply, in pixels
 *
 * Sets the offset to be applied to the constraint
 *
 *
 */
void
clutter_snap_constraint_set_offset (ClutterSnapConstraint *constraint,
                                    gfloat                 offset)
{
  g_return_if_fail (CLUTTER_IS_SNAP_CONSTRAINT (constraint));

  if (fabs (constraint->offset - offset) < 0.00001f)
    return;

  constraint->offset = offset;

  if (constraint->actor != NULL)
    clutter_actor_queue_relayout (constraint->actor);

  g_object_notify_by_pspec (G_OBJECT (constraint), obj_props[PROP_OFFSET]);
}

/**
 * clutter_snap_constraint_get_offset:
 * @constraint: a #ClutterSnapConstraint
 *
 * Retrieves the offset set using clutter_snap_constraint_set_offset()
 *
 * Return value: the offset, in pixels
 *
 *
 */
gfloat
clutter_snap_constraint_get_offset (ClutterSnapConstraint *constraint)
{
  g_return_val_if_fail (CLUTTER_IS_SNAP_CONSTRAINT (constraint), 0.0);

  return constraint->offset;
}
