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
 * SECTION:clutter-clone
 * @short_description: An actor that displays a clone of a source actor
 *
 * #ClutterClone is a #ClutterActor which draws with the paint
 * function of another actor, scaled to fit its own allocation.
 *
 * #ClutterClone can be used to efficiently clone any other actor.
 *
 * Unlike clutter_texture_new_from_actor(), #ClutterClone does not require
 * the presence of support for FBOs in the underlying GL or GLES
 * implementation.
 *
 * #ClutterClone is available since Clutter 1.0
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-actor-private.h"
#include "clutter-clone.h"
#include "clutter-debug.h"
#include "clutter-main.h"
#include "clutter-paint-volume-private.h"
#include "clutter-private.h"

#include "cogl/cogl.h"

struct _ClutterClonePrivate
{
  ClutterActor *clone_source;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClutterClone, clutter_clone, CLUTTER_TYPE_ACTOR)

enum
{
  PROP_0,

  PROP_SOURCE,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

static void clutter_clone_set_source_internal (ClutterClone *clone,
					       ClutterActor *source);
static void
clutter_clone_get_preferred_width (ClutterActor *self,
                                   gfloat        for_height,
                                   gfloat       *min_width_p,
                                   gfloat       *natural_width_p)
{
  ClutterClonePrivate *priv = CLUTTER_CLONE (self)->priv;
  ClutterActor *clone_source = priv->clone_source;

  if (clone_source == NULL)
    {
      if (min_width_p)
        *min_width_p = 0;

      if (natural_width_p)
        *natural_width_p = 0;
    }
  else
    clutter_actor_get_preferred_width (clone_source,
                                       for_height,
                                       min_width_p,
                                       natural_width_p);
}

static void
clutter_clone_get_preferred_height (ClutterActor *self,
                                    gfloat        for_width,
                                    gfloat       *min_height_p,
                                    gfloat       *natural_height_p)
{
  ClutterClonePrivate *priv = CLUTTER_CLONE (self)->priv;
  ClutterActor *clone_source = priv->clone_source;

  if (clone_source == NULL)
    {
      if (min_height_p)
        *min_height_p = 0;

      if (natural_height_p)
        *natural_height_p = 0;
    }
  else
    clutter_actor_get_preferred_height (clone_source,
                                        for_width,
                                        min_height_p,
                                        natural_height_p);
}

static void
clutter_clone_apply_transform (ClutterActor *self, CoglMatrix *matrix)
{
  ClutterClonePrivate *priv = CLUTTER_CLONE (self)->priv;
  ClutterActorBox box, source_box;
  gfloat x_scale, y_scale;

  /* First chain up and apply all the standard ClutterActor
   * transformations... */
  CLUTTER_ACTOR_CLASS (clutter_clone_parent_class)->apply_transform (self,
                                                                     matrix);

  /* if we don't have a source, nothing else to do */
  if (priv->clone_source == NULL)
    return;

  /* get our allocated size */
  clutter_actor_get_allocation_box (self, &box);

  /* and get the allocated size of the source */
  clutter_actor_get_allocation_box (priv->clone_source, &source_box);

  /* We need to scale what the clone-source actor paints to fill our own
   * allocation...
   */
  x_scale = clutter_actor_box_get_width (&box)
          / clutter_actor_box_get_width (&source_box);
  y_scale = clutter_actor_box_get_height (&box)
          / clutter_actor_box_get_height (&source_box);

  cogl_matrix_scale (matrix, x_scale, y_scale, x_scale);
}

static void
clutter_clone_paint (ClutterActor *actor)
{
  ClutterClone *self = CLUTTER_CLONE (actor);
  ClutterClonePrivate *priv = self->priv;
  gboolean was_unmapped = FALSE;

  if (priv->clone_source == NULL)
    return;

  CLUTTER_NOTE (PAINT, "painting clone actor '%s'",
                _clutter_actor_get_debug_name (actor));

  /* The final bits of magic:
   * - We need to override the paint opacity of the actor with our own
   *   opacity.
   * - We need to inform the actor that it's in a clone paint (for the function
   *   clutter_actor_is_in_clone_paint())
   * - We need to stop clutter_actor_paint applying the model view matrix of
   *   the clone source actor.
   */
  _clutter_actor_set_in_clone_paint (priv->clone_source, TRUE);
  _clutter_actor_set_opacity_override (priv->clone_source,
                                       clutter_actor_get_paint_opacity (actor));
  _clutter_actor_set_enable_model_view_transform (priv->clone_source, FALSE);

  if (!CLUTTER_ACTOR_IS_MAPPED (priv->clone_source))
    {
      _clutter_actor_set_enable_paint_unmapped (priv->clone_source, TRUE);
      was_unmapped = TRUE;
    }

  _clutter_actor_push_clone_paint ();
  clutter_actor_paint (priv->clone_source);
  _clutter_actor_pop_clone_paint ();

  if (was_unmapped)
    _clutter_actor_set_enable_paint_unmapped (priv->clone_source, FALSE);

  _clutter_actor_set_enable_model_view_transform (priv->clone_source, TRUE);
  _clutter_actor_set_opacity_override (priv->clone_source, -1);
  _clutter_actor_set_in_clone_paint (priv->clone_source, FALSE);
}

static gboolean
clutter_clone_get_paint_volume (ClutterActor       *actor,
                                ClutterPaintVolume *volume)
{
  ClutterClonePrivate *priv = CLUTTER_CLONE (actor)->priv;
  const ClutterPaintVolume *source_volume;

  /* if the source is not set the paint volume is defined to be empty */
  if (priv->clone_source == NULL)
    return TRUE;

  /* query the volume of the source actor and simply masquarade it as
   * the clones volume... */
  source_volume = clutter_actor_get_paint_volume (priv->clone_source);
  if (source_volume == NULL)
    return FALSE;

  _clutter_paint_volume_set_from_volume (volume, source_volume);
  _clutter_paint_volume_set_reference_actor (volume, actor);

  return TRUE;
}

static gboolean
clutter_clone_has_overlaps (ClutterActor *actor)
{
  ClutterClonePrivate *priv = CLUTTER_CLONE (actor)->priv;

  /* The clone has overlaps iff the source has overlaps */

  if (priv->clone_source == NULL)
    return FALSE;

  return clutter_actor_has_overlaps (priv->clone_source);
}

static void
clutter_clone_allocate (ClutterActor           *self,
                        const ClutterActorBox  *box,
                        ClutterAllocationFlags  flags)
{
  ClutterClonePrivate *priv = CLUTTER_CLONE (self)->priv;
  ClutterActorClass *parent_class;

  /* chain up */
  parent_class = CLUTTER_ACTOR_CLASS (clutter_clone_parent_class);
  parent_class->allocate (self, box, flags);

  if (priv->clone_source == NULL)
    return;

#if 0
  /* XXX - this is wrong: ClutterClone cannot clone unparented
   * actors, as it will break all invariants
   */

  /* we act like a "foster parent" for the source we are cloning;
   * if the source has not been parented we have to force an
   * allocation on it, so that we can paint it correctly from
   * within our paint() implementation. since the actor does not
   * have a parent, and thus it won't be painted by the usual
   * paint cycle, we can safely give it as much size as it requires
   */
  if (clutter_actor_get_parent (priv->clone_source) == NULL)
    clutter_actor_allocate_preferred_size (priv->clone_source, flags);
#endif
}

static void
clutter_clone_set_property (GObject      *gobject,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ClutterClone *self = CLUTTER_CLONE (gobject);

  switch (prop_id)
    {
    case PROP_SOURCE:
      clutter_clone_set_source (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
  }
}

static void
clutter_clone_get_property (GObject    *gobject,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ClutterClonePrivate *priv = CLUTTER_CLONE (gobject)->priv;

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, priv->clone_source);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_clone_dispose (GObject *gobject)
{
  clutter_clone_set_source_internal (CLUTTER_CLONE (gobject), NULL);

  G_OBJECT_CLASS (clutter_clone_parent_class)->dispose (gobject);
}

static void
clutter_clone_class_init (ClutterCloneClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->apply_transform = clutter_clone_apply_transform;
  actor_class->paint = clutter_clone_paint;
  actor_class->get_paint_volume = clutter_clone_get_paint_volume;
  actor_class->get_preferred_width = clutter_clone_get_preferred_width;
  actor_class->get_preferred_height = clutter_clone_get_preferred_height;
  actor_class->allocate = clutter_clone_allocate;
  actor_class->has_overlaps = clutter_clone_has_overlaps;

  gobject_class->dispose = clutter_clone_dispose;
  gobject_class->set_property = clutter_clone_set_property;
  gobject_class->get_property = clutter_clone_get_property;

  /**
   * ClutterClone:source:
   *
   * This property specifies the source actor being cloned.
   *
   * Since: 1.0
   */
  obj_props[PROP_SOURCE] =
    g_param_spec_object ("source",
                         P_("Source"),
                         P_("Specifies the actor to be cloned"),
                         CLUTTER_TYPE_ACTOR,
                         G_PARAM_CONSTRUCT |
                         CLUTTER_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
clutter_clone_init (ClutterClone *self)
{
  self->priv = clutter_clone_get_instance_private (self);
}

/**
 * clutter_clone_new:
 * @source: a #ClutterActor, or %NULL
 *
 * Creates a new #ClutterActor which clones @source/
 *
 * Return value: the newly created #ClutterClone
 *
 * Since: 1.0
 */
ClutterActor *
clutter_clone_new (ClutterActor *source)
{
  return g_object_new (CLUTTER_TYPE_CLONE, "source", source,  NULL);
}

static void
clutter_clone_set_source_internal (ClutterClone *self,
				   ClutterActor *source)
{
  ClutterClonePrivate *priv = self->priv;

  if (priv->clone_source == source)
    return;

  if (priv->clone_source != NULL)
    {
      _clutter_actor_detach_clone (priv->clone_source, CLUTTER_ACTOR (self));
      g_object_unref (priv->clone_source);
      priv->clone_source = NULL;
    }

  if (source != NULL)
    {
      priv->clone_source = g_object_ref (source);
      _clutter_actor_attach_clone (priv->clone_source, CLUTTER_ACTOR (self));
    }

  g_object_notify_by_pspec (G_OBJECT (self), obj_props[PROP_SOURCE]);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

/**
 * clutter_clone_set_source:
 * @self: a #ClutterClone
 * @source: (allow-none): a #ClutterActor, or %NULL
 *
 * Sets @source as the source actor to be cloned by @self.
 *
 * Since: 1.0
 */
void
clutter_clone_set_source (ClutterClone *self,
                          ClutterActor *source)
{
  g_return_if_fail (CLUTTER_IS_CLONE (self));
  g_return_if_fail (source == NULL || CLUTTER_IS_ACTOR (source));

  clutter_clone_set_source_internal (self, source);
  clutter_actor_queue_relayout (CLUTTER_ACTOR (self));
}

/**
 * clutter_clone_get_source:
 * @self: a #ClutterClone
 *
 * Retrieves the source #ClutterActor being cloned by @self.
 *
 * Return value: (transfer none): the actor source for the clone
 *
 * Since: 1.0
 */
ClutterActor *
clutter_clone_get_source (ClutterClone *self)
{
  g_return_val_if_fail (CLUTTER_IS_CLONE (self), NULL);

  return self->priv->clone_source;
}
