/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2008  Intel Corporation.
 *
 * Authored By: Robert Bragg <robert@linux.intel.com>
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

/**
 * SECTION:clutter-actor-clone
 * @short_description: An actor that displays a scaled clone of another
 *                     actor.
 *
 * #ClutterActorClone is a #ClutterActor which draws with the paint
 * function of another actor, scaled to fit its own allocation.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-color.h"
#include "clutter-debug.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-actor-clone.h"

#include "cogl/cogl.h"

G_DEFINE_TYPE (ClutterActorClone, clutter_actor_clone, CLUTTER_TYPE_ACTOR);

enum
{
  PROP_0,

  PROP_CLONE_SOURCE
};

#define CLUTTER_ACTOR_CLONE_GET_PRIVATE(obj) \
          (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                        CLUTTER_TYPE_ACTOR_CLONE, \
                                        ClutterActorClonePrivate))

struct _ClutterActorClonePrivate
{
  ClutterActor   *clone_source;
};

static void
clutter_actor_clone_get_preferred_width (ClutterActor *self,
                                         ClutterUnit   for_height,
                                         ClutterUnit  *min_width_p,
                                         ClutterUnit  *natural_width_p)
{
  ClutterActorClonePrivate *priv = CLUTTER_ACTOR_CLONE (self)->priv;
  ClutterActor             *clone_source = priv->clone_source;

  clutter_actor_get_preferred_width (clone_source,
                                     for_height,
                                     min_width_p,
                                     natural_width_p);
}

static void
clutter_actor_clone_get_preferred_height (ClutterActor *self,
                                          ClutterUnit   for_width,
                                          ClutterUnit  *min_height_p,
                                          ClutterUnit  *natural_height_p)
{
  ClutterActorClonePrivate *priv = CLUTTER_ACTOR_CLONE (self)->priv;
  ClutterActor             *clone_source = priv->clone_source;

  clutter_actor_get_preferred_height (clone_source,
                                      for_width,
                                      min_height_p,
                                      natural_height_p);
}

static void
clutter_actor_clone_paint (ClutterActor *self)
{
  ClutterActorClone        *clone = CLUTTER_ACTOR_CLONE (self);
  ClutterActorClonePrivate *priv = clone->priv;
  ClutterGeometry           geom;
  ClutterGeometry           clone_source_geom;
  float                     x_scale;
  float                     y_scale;

  CLUTTER_NOTE (PAINT,
                "painting clone actor '%s'",
		clutter_actor_get_name (self) ? clutter_actor_get_name (self)
                                              : "unknown");

  clutter_actor_get_allocation_geometry (self, &geom);
  clutter_actor_get_allocation_geometry (priv->clone_source,
                                         &clone_source_geom);

  /* We need to scale what the clone-source actor paints to fill our own
   * allocation... */

  x_scale = (float)geom.width / clone_source_geom.width;
  y_scale = (float)geom.height / clone_source_geom.height;

  cogl_scale (COGL_FIXED_FROM_FLOAT (x_scale), COGL_FIXED_FROM_FLOAT (y_scale));

  /* The final bits of magic:
   * - We need to make sure that when the clone-source actor's paint method
   *   calls clutter_actor_get_paint_opacity, it traverses our parent not it's
   *   real parent.
   * - We need to stop clutter_actor_paint applying the model view matrix of
   *   the clone source actor.
   */
  _clutter_actor_set_opacity_parent (priv->clone_source,
                                     clutter_actor_get_parent (self));
  _clutter_actor_set_enable_model_view_transform (priv->clone_source, FALSE);

  clutter_actor_paint (priv->clone_source);

  _clutter_actor_set_enable_model_view_transform (priv->clone_source, TRUE);
  _clutter_actor_set_opacity_parent (priv->clone_source, NULL);
}

static void
clutter_actor_clone_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
  ClutterActorClone *clone = CLUTTER_ACTOR_CLONE(object);

  switch (prop_id)
    {
    case PROP_CLONE_SOURCE:
      clone->priv->clone_source = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_actor_clone_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  ClutterActorClonePrivate *priv = CLUTTER_ACTOR_CLONE(object)->priv;

  switch (prop_id)
    {
    case PROP_CLONE_SOURCE:
      g_value_set_object (value, priv->clone_source);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
clutter_actor_clone_finalize (GObject *object)
{
  G_OBJECT_CLASS (clutter_actor_clone_parent_class)->finalize (object);
}

static void
clutter_actor_clone_dispose (GObject *object)
{
  G_OBJECT_CLASS (clutter_actor_clone_parent_class)->dispose (object);
}

static void
clutter_actor_clone_class_init (ClutterActorCloneClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint        = clutter_actor_clone_paint;
  actor_class->get_preferred_width =
    clutter_actor_clone_get_preferred_width;
  actor_class->get_preferred_height =
    clutter_actor_clone_get_preferred_height;

  gobject_class->finalize     = clutter_actor_clone_finalize;
  gobject_class->dispose      = clutter_actor_clone_dispose;
  gobject_class->set_property = clutter_actor_clone_set_property;
  gobject_class->get_property = clutter_actor_clone_get_property;

  /**
   * ClutterActorClone:clone-source
   *
   * This specifies the source actor being cloned
   *
   * Since: 1.0
   */
  g_object_class_install_property
        (gobject_class,
         PROP_CLONE_SOURCE,
         g_param_spec_object ("clone-source",
                              "Clone Source",
                              "Specifies the actor to be cloned",
                              CLUTTER_TYPE_ACTOR,
                              G_PARAM_CONSTRUCT_ONLY
                              |G_PARAM_WRITABLE
                              |G_PARAM_READABLE));

  g_type_class_add_private (gobject_class, sizeof (ClutterActorClonePrivate));
}

static void
clutter_actor_clone_init (ClutterActorClone *self)
{
  ClutterActorClonePrivate *priv;

  self->priv = priv = CLUTTER_ACTOR_CLONE_GET_PRIVATE (self);

  priv->clone_source = NULL;
}

/**
 * clutter_actor_clone_new:
 *
 * Creates a new #ClutterActor which clones clone_source.
 *
 * Return value: a new #ClutterActor
 *
 * Since: 1.0
 */
ClutterActor *
clutter_actor_clone_new (ClutterActor *clone_source)
{
  return g_object_new (CLUTTER_TYPE_ACTOR_CLONE,
                       "clone-source", clone_source,
                       NULL);
}

/**
 * clutter_actor_clone_get_clone_source:
 *
 * @clone: a #ClutterActorClone
 *
 * Retrieves the source #ClutterActor being clone by @clone
 *
 * Return value: the actor source for the clone
 *
 * Since: 1.0
 */
ClutterActor *
clutter_actor_clone_get_clone_source (ClutterActorClone *clone)
{
  ClutterActorClonePrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_ACTOR_CLONE (clone), NULL);

  priv = clone->priv;

  return priv->clone_source;
}

