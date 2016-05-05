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
 * SECTION:clutter-path-constraint
 * @Title: ClutterPathConstraint
 * @Short_Description: A constraint that follows a path
 *
 * #ClutterPathConstraint is a simple constraint that modifies the allocation
 * of the #ClutterActor to which it has been applied using a #ClutterPath.
 *
 * By setting the #ClutterPathConstraint:offset property it is possible to
 * control how far along the path the #ClutterActor should be.
 *
 * ClutterPathConstraint is available since Clutter 1.6.
 */

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include "clutter-path-constraint.h"

#include "clutter-debug.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

#define CLUTTER_PATH_CONSTRAINT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_PATH_CONSTRAINT, ClutterPathConstraintClass))
#define CLUTTER_IS_PATH_CONSTRAINT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_PATH_CONSTRAINT))
#define CLUTTER_PATH_CONSTRAINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_PATH_CONSTRAINT, ClutterPathConstraintClass))

struct _ClutterPathConstraint
{
  ClutterConstraint parent_instance;

  ClutterPath *path;

  gfloat offset;

  ClutterActor *actor;

  guint current_node;
};

struct _ClutterPathConstraintClass
{
  ClutterConstraintClass parent_class;
};

enum
{
  PROP_0,

  PROP_PATH,
  PROP_OFFSET,

  LAST_PROPERTY
};

enum
{
  NODE_REACHED,

  LAST_SIGNAL
};

G_DEFINE_TYPE (ClutterPathConstraint, clutter_path_constraint, CLUTTER_TYPE_CONSTRAINT);

static GParamSpec *path_properties[LAST_PROPERTY] = { NULL, };
static guint path_signals[LAST_SIGNAL] = { 0, };

static void
clutter_path_constraint_update_allocation (ClutterConstraint *constraint,
                                           ClutterActor      *actor,
                                           ClutterActorBox   *allocation)
{
  ClutterPathConstraint *self = CLUTTER_PATH_CONSTRAINT (constraint);
  gfloat width, height;
  ClutterKnot position;
  guint knot_id;

  if (self->path == NULL)
    return;

  knot_id = clutter_path_get_position (self->path, self->offset, &position);
  clutter_actor_box_get_size (allocation, &width, &height);
  allocation->x1 = position.x;
  allocation->y1 = position.y;
  allocation->x2 = allocation->x1 + width;
  allocation->y2 = allocation->y1 + height;

  if (knot_id != self->current_node)
    {
      self->current_node = knot_id;
      g_signal_emit (self, path_signals[NODE_REACHED], 0,
                     self->actor,
                     self->current_node);
    }
}

static void
clutter_path_constraint_set_actor (ClutterActorMeta *meta,
                                   ClutterActor     *new_actor)
{
  ClutterPathConstraint *path = CLUTTER_PATH_CONSTRAINT (meta);
  ClutterActorMetaClass *parent;

  /* store the pointer to the actor, for later use */
  path->actor = new_actor;

  parent = CLUTTER_ACTOR_META_CLASS (clutter_path_constraint_parent_class);
  parent->set_actor (meta, new_actor);
}

static void
clutter_path_constraint_dispose (GObject *gobject)
{
  ClutterPathConstraint *self = CLUTTER_PATH_CONSTRAINT (gobject);

  if (self->path != NULL)
    {
      g_object_unref (self->path);
      self->path = NULL;
    }

  G_OBJECT_CLASS (clutter_path_constraint_parent_class)->dispose (gobject);
}

static void
clutter_path_constraint_set_property (GObject      *gobject,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ClutterPathConstraint *self = CLUTTER_PATH_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_PATH:
      clutter_path_constraint_set_path (self, g_value_get_object (value));
      break;

    case PROP_OFFSET:
      clutter_path_constraint_set_offset (self, g_value_get_float (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_path_constraint_get_property (GObject    *gobject,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ClutterPathConstraint *self = CLUTTER_PATH_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_object (value, self->path);
      break;

    case PROP_OFFSET:
      g_value_set_float (value, self->offset);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
clutter_path_constraint_class_init (ClutterPathConstraintClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);
  ClutterConstraintClass *constraint_class = CLUTTER_CONSTRAINT_CLASS (klass);

  /**
   * ClutterPathConstraint:path:
   *
   * The #ClutterPath used to constrain the position of an actor.
   *
   * Since: 1.6
   */
  path_properties[PROP_PATH] =
    g_param_spec_object ("path",
                         P_("Path"),
                         P_("The path used to constrain an actor"),
                         CLUTTER_TYPE_PATH,
                         CLUTTER_PARAM_READWRITE);

  /**
   * ClutterPathConstraint:offset:
   *
   * The offset along the #ClutterPathConstraint:path, between -1.0 and 2.0.
   *
   * Since: 1.6
   */
  path_properties[PROP_OFFSET] =
    g_param_spec_float ("offset",
                        P_("Offset"),
                        P_("The offset along the path, between -1.0 and 2.0"),
                        -1.0, 2.0,
                        0.0,
                        CLUTTER_PARAM_READWRITE);

  gobject_class->set_property = clutter_path_constraint_set_property;
  gobject_class->get_property = clutter_path_constraint_get_property;
  gobject_class->dispose = clutter_path_constraint_dispose;
  g_object_class_install_properties (gobject_class,
                                     LAST_PROPERTY,
                                     path_properties);

  meta_class->set_actor = clutter_path_constraint_set_actor;

  constraint_class->update_allocation = clutter_path_constraint_update_allocation;

  /**
   * ClutterPathConstraint::node-reached:
   * @constraint: the #ClutterPathConstraint that emitted the signal
   * @actor: the #ClutterActor using the @constraint
   * @index: the index of the node that has been reached
   *
   * The ::node-reached signal is emitted each time a
   * #ClutterPathConstraint:offset value results in the actor
   * passing a #ClutterPathNode
   *
   * Since: 1.6
   */
  path_signals[NODE_REACHED] =
    g_signal_new (I_("node-reached"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _clutter_marshal_VOID__OBJECT_UINT,
                  G_TYPE_NONE, 2,
                  CLUTTER_TYPE_ACTOR,
                  G_TYPE_UINT);
}

static void
clutter_path_constraint_init (ClutterPathConstraint *self)
{
  self->offset = 0.0f;
  self->current_node = G_MAXUINT;
}

/**
 * clutter_path_constraint_new:
 * @path: (allow-none): a #ClutterPath, or %NULL
 * @offset: the offset along the #ClutterPath
 *
 * Creates a new #ClutterPathConstraint with the given @path and @offset
 *
 * Return value: the newly created #ClutterPathConstraint
 *
 * Since: 1.6
 */
ClutterConstraint *
clutter_path_constraint_new (ClutterPath *path,
                             gfloat       offset)
{
  g_return_val_if_fail (path == NULL || CLUTTER_IS_PATH (path), NULL);

  return g_object_new (CLUTTER_TYPE_PATH_CONSTRAINT,
                       "path", path,
                       "offset", offset,
                       NULL);
}

/**
 * clutter_path_constraint_set_path:
 * @constraint: a #ClutterPathConstraint
 * @path: (allow-none): a #ClutterPath
 *
 * Sets the @path to be followed by the #ClutterPathConstraint.
 *
 * The @constraint will take ownership of the #ClutterPath passed to this
 * function.
 *
 * Since: 1.6
 */
void
clutter_path_constraint_set_path (ClutterPathConstraint *constraint,
                                  ClutterPath           *path)
{
  g_return_if_fail (CLUTTER_IS_PATH_CONSTRAINT (constraint));
  g_return_if_fail (path == NULL || CLUTTER_IS_PATH (path));

  if (constraint->path == path)
    return;

  if (constraint->path != NULL)
    {
      g_object_unref (constraint->path);
      constraint->path = NULL;
    }

  if (path != NULL)
    constraint->path = g_object_ref_sink (path);

  if (constraint->actor != NULL)
    clutter_actor_queue_relayout (constraint->actor);

  g_object_notify_by_pspec (G_OBJECT (constraint), path_properties[PROP_PATH]);
}

/**
 * clutter_path_constraint_get_path:
 * @constraint: a #ClutterPathConstraint
 *
 * Retrieves a pointer to the #ClutterPath used by @constraint.
 *
 * Return value: (transfer none): the #ClutterPath used by the
 *   #ClutterPathConstraint, or %NULL. The returned #ClutterPath is owned
 *   by the constraint and it should not be unreferenced
 *
 * Since: 1.6
 */
ClutterPath *
clutter_path_constraint_get_path (ClutterPathConstraint *constraint)
{
  g_return_val_if_fail (CLUTTER_IS_PATH_CONSTRAINT (constraint), NULL);

  return constraint->path;
}

/**
 * clutter_path_constraint_set_offset:
 * @constraint: a #ClutterPathConstraint
 * @offset: the offset along the path
 *
 * Sets the offset along the #ClutterPath used by @constraint.
 *
 * Since: 1.6
 */
void
clutter_path_constraint_set_offset (ClutterPathConstraint *constraint,
                                    gfloat                 offset)
{
  g_return_if_fail (CLUTTER_IS_PATH_CONSTRAINT (constraint));

  if (constraint->offset == offset)
    return;

  constraint->offset = offset;

  if (constraint->actor != NULL)
    clutter_actor_queue_relayout (constraint->actor);

  g_object_notify_by_pspec (G_OBJECT (constraint), path_properties[PROP_OFFSET]);
}

/**
 * clutter_path_constraint_get_offset:
 * @constraint: a #ClutterPathConstraint
 *
 * Retrieves the offset along the #ClutterPath used by @constraint.
 *
 * Return value: the offset
 *
 * Since: 1.6
 */
gfloat
clutter_path_constraint_get_offset (ClutterPathConstraint *constraint)
{
  g_return_val_if_fail (CLUTTER_IS_PATH_CONSTRAINT (constraint), 0.0);

  return constraint->offset;
}
