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
 * SECTION:ClutterBindConstraint
 * @Title: ClutterBindConstraint
 * @Short_Description: A constraint binding the position of an actor
 *
 * #ClutterBindConstraint is a #ClutterConstraint that binds the position of
 * the #ClutterActor to which it is applied to the the position of another
 * #ClutterActor.
 *
 * #ClutterBindConstraint is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-bind-constraint.h"

#include "clutter-constraint.h"
#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"

#define CLUTTER_BIND_CONSTRAINT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BIND_CONSTRAINT, ClutterBindConstraintClass))
#define CLUTTER_IS_BIND_CONSTRAINT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BIND_CONSTRAINT))
#define CLUTTER_BIND_CONSTRAINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BIND_CONSTRAINT, ClutterBindConstraintClass))

typedef struct _ClutterBindConstraintClass      ClutterBindConstraintClass;

struct _ClutterBindConstraint
{
  ClutterConstraint parent_instance;

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
  PROP_OFFSET
};

G_DEFINE_TYPE (ClutterBindConstraint,
               clutter_bind_constraint,
               CLUTTER_TYPE_CONSTRAINT);

static void
update_actor_position (ClutterBindConstraint *bind)
{
  ClutterVertex source_position;
  ClutterActor *actor;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (bind)))
    return;

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (bind));
  if (actor == NULL)
    return;

  source_position.x = clutter_actor_get_x (bind->source);
  source_position.y = clutter_actor_get_y (bind->source);
  source_position.z = clutter_actor_get_depth (bind->source);

  switch (bind->coordinate)
    {
    case CLUTTER_BIND_X:
      clutter_actor_set_x (actor, source_position.x + bind->offset);
      break;

    case CLUTTER_BIND_Y:
      clutter_actor_set_y (actor, source_position.y + bind->offset);
      break;

    case CLUTTER_BIND_Z:
      clutter_actor_set_depth (actor, source_position.z + bind->offset);
      break;
    }
}

static void
source_position_changed (GObject               *gobject,
                         GParamSpec            *pspec,
                         ClutterBindConstraint *bind)
{
  if (strcmp (pspec->name, "x") == 0 ||
      strcmp (pspec->name, "y") == 0 ||
      strcmp (pspec->name, "depth") == 0)
    {
      update_actor_position (bind);
    }
}

static void
source_destroyed (ClutterActor          *actor,
                  ClutterBindConstraint *bind)
{
  bind->source = NULL;
}

static void
_clutter_bind_constraint_set_source (ClutterBindConstraint *bind,
                                     ClutterActor          *source)
{
  ClutterActor *old_source = bind->source;

  if (old_source != NULL)
    {
      g_signal_handlers_disconnect_by_func (old_source,
                                            G_CALLBACK (source_destroyed),
                                            bind);
      g_signal_handlers_disconnect_by_func (old_source,
                                            G_CALLBACK (source_position_changed),
                                            bind);
    }

  bind->source = source;
  g_signal_connect (bind->source, "notify",
                    G_CALLBACK (source_position_changed),
                    bind);
  g_signal_connect (bind->source, "destroy",
                    G_CALLBACK (source_destroyed),
                    bind);

  update_actor_position (bind);

  g_object_notify (G_OBJECT (bind), "source");
}

static void
_clutter_bind_constraint_set_coordinate (ClutterBindConstraint *bind,
                                         ClutterBindCoordinate  coord)
{
  if (bind->coordinate == coord)
    return;

  bind->coordinate = coord;

  update_actor_position (bind);

  g_object_notify (G_OBJECT (bind), "coordinate");
}

static void
_clutter_bind_constraint_set_offset (ClutterBindConstraint *bind,
                                     gfloat                 offset)
{
  if (fabs (bind->offset - offset) < 0.00001f)
    return;

  bind->offset = offset;

  update_actor_position (bind);

  g_object_notify (G_OBJECT (bind), "offset");
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
      _clutter_bind_constraint_set_source (bind, g_value_get_object (value));
      break;

    case PROP_COORDINATE:
      _clutter_bind_constraint_set_coordinate (bind, g_value_get_enum (value));
      break;

    case PROP_OFFSET:
      _clutter_bind_constraint_set_offset (bind, g_value_get_float (value));
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
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  gobject_class->set_property = clutter_bind_constraint_set_property;
  gobject_class->get_property = clutter_bind_constraint_get_property;

  /**
   * ClutterBindConstraint:source:
   *
   * The #ClutterActor used as the source for the binding
   *
   * Since: 1.4
   */
  pspec = g_param_spec_object ("source",
                               "Source",
                               "The source of the binding",
                               CLUTTER_TYPE_ACTOR,
                               CLUTTER_PARAM_READWRITE |
                               G_PARAM_CONSTRUCT);
  g_object_class_install_property (gobject_class, PROP_SOURCE, pspec);

  /**
   * ClutterBindConstraint:coordinate:
   *
   * The coordinate to be bound
   *
   * Since: 1.4
   */
  pspec = g_param_spec_enum ("coordinate",
                             "Coordinate",
                             "The coordinate to bind",
                             CLUTTER_TYPE_BIND_COORDINATE,
                             CLUTTER_BIND_X,
                             CLUTTER_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT);
  g_object_class_install_property (gobject_class, PROP_COORDINATE, pspec);

  /**
   * ClutterBindConstraint:offset:
   *
   * The offset, in pixels, to be applied to the binding
   *
   * Since: 1.4
   */
  pspec = g_param_spec_float ("offset",
                              "Offset",
                              "The offset in pixels to apply to the binding",
                              -G_MAXFLOAT, G_MAXFLOAT,
                              0.0f,
                              CLUTTER_PARAM_READWRITE |
                              G_PARAM_CONSTRUCT);
  g_object_class_install_property (gobject_class, PROP_OFFSET, pspec);
}

static void
clutter_bind_constraint_init (ClutterBindConstraint *self)
{
  self->source = NULL;
  self->coordinate = CLUTTER_BIND_X;
  self->offset = 0.0f;
}

/**
 * clutter_bind_constraint_new:
 * @source: the #ClutterActor to use as the source of the binding
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
  g_return_val_if_fail (CLUTTER_IS_ACTOR (source), NULL);

  return g_object_new (CLUTTER_TYPE_BIND_CONSTRAINT,
                       "source", source,
                       "coordinate", coordinate,
                       "offset", offset,
                       NULL);
}
