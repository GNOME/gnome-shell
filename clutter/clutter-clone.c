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
 * <note><para>This is different from clutter_texture_new_from_actor()
 * which requires support for FBOs in the underlying GL
 * implementation.</para></note>
 *
 * #ClutterClone is available since Clutter 1.0
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-color.h"
#include "clutter-clone.h"
#include "clutter-debug.h"
#include "clutter-main.h"
#include "clutter-private.h"

#include "cogl/cogl.h"

G_DEFINE_TYPE (ClutterClone, clutter_clone, CLUTTER_TYPE_ACTOR);

enum
{
  PROP_0,

  PROP_SOURCE
};

#define CLUTTER_CLONE_GET_PRIVATE(obj)  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_CLONE, ClutterClonePrivate))

struct _ClutterClonePrivate
{
  ClutterActor *clone_source;
};

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
clutter_clone_paint (ClutterActor *self)
{
  ClutterClone *clone = CLUTTER_CLONE (self);
  ClutterClonePrivate *priv = clone->priv;
  ClutterGeometry geom, clone_geom;
  gfloat x_scale, y_scale;
  gboolean was_unmapped = FALSE;

  if (G_UNLIKELY (priv->clone_source == NULL))
    return;

  CLUTTER_NOTE (PAINT,
                "painting clone actor '%s'",
		clutter_actor_get_name (self) ? clutter_actor_get_name (self)
                                              : "unknown");

  /* get our allocated size */
  clutter_actor_get_allocation_geometry (self, &geom);

  /* and get the allocated size of the source */
  clutter_actor_get_allocation_geometry (priv->clone_source, &clone_geom);

  /* We need to scale what the clone-source actor paints to fill our own
   * allocation...
   */
  x_scale = (gfloat) geom.width  / clone_geom.width;
  y_scale = (gfloat) geom.height / clone_geom.height;

  cogl_scale (x_scale, y_scale, 1.0);

  /* The final bits of magic:
   * - We need to make sure that when the clone-source actor's paint
   *   method calls clutter_actor_get_paint_opacity, it traverses to
   *   us and our parent not it's real parent.
   * - We need to stop clutter_actor_paint applying the model view matrix of
   *   the clone source actor.
   */
  _clutter_actor_set_opacity_parent (priv->clone_source, self);
  _clutter_actor_set_enable_model_view_transform (priv->clone_source, FALSE);

  if (!CLUTTER_ACTOR_IS_MAPPED (priv->clone_source))
    {
      _clutter_actor_set_enable_paint_unmapped (priv->clone_source, TRUE);
      was_unmapped = TRUE;
    }

  clutter_actor_paint (priv->clone_source);

  if (was_unmapped)
    _clutter_actor_set_enable_paint_unmapped (priv->clone_source, FALSE);

  _clutter_actor_set_enable_model_view_transform (priv->clone_source, TRUE);
  _clutter_actor_set_opacity_parent (priv->clone_source, NULL);
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

  if (G_UNLIKELY (priv->clone_source == NULL))
    return;

  /* we act like a "foster parent" for the source we are cloning;
   * if the source has not been parented we have to force an
   * allocation on it, so that we can paint it correctly from
   * within out paint() implementation. since the actor does not
   * have a parent, and thus it won't be painted by the usual
   * paint cycle, we can safely give it as much size as it requires
   */
  if (clutter_actor_get_parent (priv->clone_source) == NULL)
    clutter_actor_allocate_preferred_size (priv->clone_source, flags);
}

static void
clutter_clone_set_property (GObject      *gobject,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ClutterClone *clone = CLUTTER_CLONE (gobject);

  switch (prop_id)
    {
    case PROP_SOURCE:
      clutter_clone_set_source (clone, g_value_get_object (value));
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
  GParamSpec *pspec = NULL;

  g_type_class_add_private (gobject_class, sizeof (ClutterClonePrivate));

  actor_class->paint                = clutter_clone_paint;
  actor_class->get_preferred_width  = clutter_clone_get_preferred_width;
  actor_class->get_preferred_height = clutter_clone_get_preferred_height;
  actor_class->allocate             = clutter_clone_allocate;

  gobject_class->dispose      = clutter_clone_dispose;
  gobject_class->set_property = clutter_clone_set_property;
  gobject_class->get_property = clutter_clone_get_property;

  /**
   * ClutterClone:source:
   *
   * This property specifies the source actor being cloned.
   *
   * Since: 1.0
   */
  pspec = g_param_spec_object ("source",
                               "Source",
                               "Specifies the actor to be cloned",
                               CLUTTER_TYPE_ACTOR,
                               G_PARAM_CONSTRUCT |
                               CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_SOURCE, pspec);
}

static void
clutter_clone_init (ClutterClone *self)
{
  ClutterClonePrivate *priv;

  self->priv = priv = CLUTTER_CLONE_GET_PRIVATE (self);

  priv->clone_source = NULL;
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
clone_source_queue_redraw_cb (ClutterActor *source,
			      ClutterActor *origin,
			      ClutterClone *clone)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (clone));
}

static void
clone_source_queue_relayout_cb (ClutterActor *source,
				ClutterClone *clone)
{
  clutter_actor_queue_relayout (CLUTTER_ACTOR (clone));
}

static void
clutter_clone_set_source_internal (ClutterClone *clone,
				   ClutterActor *source)
{
  ClutterClonePrivate *priv;

  g_return_if_fail (CLUTTER_IS_CLONE (clone));
  g_return_if_fail (source == NULL || CLUTTER_IS_ACTOR (source));

  priv = clone->priv;

  if (priv->clone_source)
    {
      g_signal_handlers_disconnect_by_func (priv->clone_source,
					    (void *) clone_source_queue_redraw_cb,
					    clone);
      g_signal_handlers_disconnect_by_func (priv->clone_source,
					    (void *) clone_source_queue_relayout_cb,
					    clone);
      g_object_unref (priv->clone_source);
      priv->clone_source = NULL;
    }

  if (source)
    {
      priv->clone_source = g_object_ref (source);
      g_signal_connect (priv->clone_source, "queue-redraw",
			G_CALLBACK (clone_source_queue_redraw_cb), clone);
      g_signal_connect (priv->clone_source, "queue-relayout",
			G_CALLBACK (clone_source_queue_relayout_cb), clone);
    }

  g_object_notify (G_OBJECT (clone), "source");

  clutter_actor_queue_relayout (CLUTTER_ACTOR (clone));
}

/**
 * clutter_clone_set_source:
 * @clone: a #ClutterClone
 * @source: a #ClutterActor, or %NULL
 *
 * Sets @source as the source actor to be cloned by @clone.
 *
 * Since: 1.0
 */
void
clutter_clone_set_source (ClutterClone *clone,
                          ClutterActor *source)
{
  g_return_if_fail (CLUTTER_IS_CLONE (clone));
  g_return_if_fail (source == NULL || CLUTTER_IS_ACTOR (source));

  clutter_clone_set_source_internal (clone, source);
  clutter_actor_queue_relayout (CLUTTER_ACTOR (clone));
}

/**
 * clutter_clone_get_source:
 * @clone: a #ClutterClone
 *
 * Retrieves the source #ClutterActor being cloned by @clone
 *
 * Return value: (transfer none): the actor source for the clone
 *
 * Since: 1.0
 */
ClutterActor *
clutter_clone_get_source (ClutterClone *clone)
{
  g_return_val_if_fail (CLUTTER_IS_CLONE (clone), NULL);

  return clone->priv->clone_source;
}
