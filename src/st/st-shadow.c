/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-shadow.c: Boxed type holding for -st-shadow attributes
 *
 * Copyright 2009, 2010 Florian Müllner
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "st-shadow.h"
#include "st-private.h"

G_DEFINE_BOXED_TYPE (StShadow, st_shadow, st_shadow_ref, st_shadow_unref)
G_DEFINE_BOXED_TYPE (StShadowHelper, st_shadow_helper, st_shadow_helper_copy, st_shadow_helper_free)

/**
 * st_shadow_new:
 * @color: shadow's color
 * @xoffset: horizontal offset
 * @yoffset: vertical offset
 * @blur: blur radius
 * @spread: spread radius
 * @inset: whether the shadow should be inset
 *
 * Creates a new #StShadow
 *
 * Returns: the newly allocated shadow. Use st_shadow_free() when done
 */
StShadow *
st_shadow_new (CoglColor    *color,
               gdouble       xoffset,
               gdouble       yoffset,
               gdouble       blur,
               gdouble       spread,
               gboolean      inset)
{
  StShadow *shadow;

  shadow = g_atomic_rc_box_new (StShadow);

  shadow->color     = *color;
  shadow->xoffset   = xoffset;
  shadow->yoffset   = yoffset;
  shadow->blur      = blur;
  shadow->spread    = spread;
  shadow->inset     = inset;

  return shadow;
}

/**
 * st_shadow_ref:
 * @shadow: a #StShadow
 *
 * Atomically increments the reference count of @shadow by one.
 *
 * Returns: the passed in #StShadow.
 */
StShadow *
st_shadow_ref (StShadow *shadow)
{
  g_return_val_if_fail (shadow != NULL, NULL);

  return g_atomic_rc_box_acquire (shadow);
}

/**
 * st_shadow_unref:
 * @shadow: a #StShadow
 *
 * Atomically decrements the reference count of @shadow by one.
 * If the reference count drops to 0, all memory allocated by the
 * #StShadow is released.
 */
void
st_shadow_unref (StShadow *shadow)
{
  g_return_if_fail (shadow != NULL);

  g_atomic_rc_box_release (shadow);
}

/**
 * st_shadow_equal:
 * @shadow: a #StShadow
 * @other: a different #StShadow
 *
 * Check if two shadow objects are identical. Note that two shadows may
 * compare non-identically if they differ only by floating point rounding
 * errors.
 *
 * Returns: %TRUE if the two shadows are identical
 */
gboolean
st_shadow_equal (StShadow *shadow,
                 StShadow *other)
{
  g_return_val_if_fail (shadow != NULL, FALSE);
  g_return_val_if_fail (other != NULL, FALSE);

  if (shadow == other)
    return TRUE;

  /* We use strict equality to compare double quantities; this means
   * that, for example, a shadow offset of 0.25in does not necessarily
   * compare equal to a shadow offset of 18pt in this test. Assume
   * that a few false negatives are mostly harmless.
   */

  return (cogl_color_equal (&shadow->color, &other->color) &&
          shadow->xoffset == other->xoffset &&
          shadow->yoffset == other->yoffset &&
          shadow->blur == other->blur &&
          shadow->spread == other->spread &&
          shadow->inset == other->inset);
}

/**
 * st_shadow_get_box:
 * @shadow: a #StShadow
 * @actor_box: the box allocated to a #ClutterAlctor
 * @shadow_box: computed box occupied by @shadow
 *
 * Gets the box used to paint @shadow, which will be partly
 * outside of @actor_box
 */
void
st_shadow_get_box (StShadow              *shadow,
                   const ClutterActorBox *actor_box,
                   ClutterActorBox       *shadow_box)
{
  g_return_if_fail (shadow != NULL);
  g_return_if_fail (actor_box != NULL);
  g_return_if_fail (shadow_box != NULL);

  /* Inset shadows are drawn below the border, so returning
   * the original box is not actually correct; still, it's
   * good enough for the purpose of determining additional space
   * required outside the actor box.
   */
  if (shadow->inset)
    {
      *shadow_box = *actor_box;
      return;
    }

  shadow_box->x1 = actor_box->x1 + shadow->xoffset
                   - shadow->blur - shadow->spread;
  shadow_box->x2 = actor_box->x2 + shadow->xoffset
                   + shadow->blur + shadow->spread;
  shadow_box->y1 = actor_box->y1 + shadow->yoffset
                   - shadow->blur - shadow->spread;
  shadow_box->y2 = actor_box->y2 + shadow->yoffset
                   + shadow->blur + shadow->spread;
}

/**
 * StShadowHelper:
 *
 * A helper for implementing a drop shadow on a actor.
 * The actor is expected to recreate the helper whenever its contents
 * or size change. Then, it would call st_shadow_helper_paint() inside
 * its paint() virtual function.
 */

struct _StShadowHelper {
  StShadow     *shadow;
  CoglPipeline *pipeline;

  gfloat        width;
  gfloat        height;
};

/**
 * st_shadow_helper_new:
 * @shadow: a #StShadow representing the shadow properties
 *
 * Builds a #StShadowHelper that will build a drop shadow
 * using @source as the mask.
 *
 * Returns: (transfer full): a new #StShadowHelper
 */
StShadowHelper *
st_shadow_helper_new (StShadow     *shadow)
{
  StShadowHelper *helper;

  helper = g_new0 (StShadowHelper, 1);
  helper->shadow = st_shadow_ref (shadow);

  return helper;
}

/**
 * st_shadow_helper_update:
 * @helper: a #StShadowHelper
 * @source: a #ClutterActor
 * @paint_context: a #ClutterPaintContext
 *
 * Update @helper from @source.
 */
void
st_shadow_helper_update (StShadowHelper      *helper,
                         ClutterActor        *source,
                         ClutterPaintContext *paint_context)
{
  gfloat width, height;

  clutter_actor_get_size (source, &width, &height);

  if (helper->pipeline == NULL ||
      helper->width != width ||
      helper->height != height)
    {
      if (helper->pipeline)
        g_object_unref (helper->pipeline);

      helper->pipeline = _st_create_shadow_pipeline_from_actor (helper->shadow,
                                                                source,
                                                                paint_context);
      helper->width = width;
      helper->height = height;
    }
}

/**
 * st_shadow_helper_copy:
 * @helper: the #StShadowHelper to copy
 *
 * Returns: (transfer full): a copy of @helper
 */
StShadowHelper *
st_shadow_helper_copy (StShadowHelper *helper)
{
  StShadowHelper *copy;

  copy = g_new (StShadowHelper, 1);
  *copy = *helper;
  if (copy->pipeline)
    g_object_ref (copy->pipeline);
  st_shadow_ref (copy->shadow);

  return copy;
}

/**
 * st_shadow_helper_free:
 * @helper: a #StShadowHelper
 *
 * Free resources associated with @helper.
 */
void
st_shadow_helper_free (StShadowHelper *helper)
{
  if (helper->pipeline)
    g_object_unref (helper->pipeline);
  st_shadow_unref (helper->shadow);

  g_free (helper);
}

/**
 * st_shadow_helper_paint:
 * @helper: a #StShadowHelper
 * @node: a #ClutterPaintNode
 * @actor_box: the bounding box of the shadow
 * @paint_opacity: the opacity at which the shadow is painted
 *
 * Paints the shadow associated with @helper This must only
 * be called from the implementation of ClutterActor::paint().
 */
void
st_shadow_helper_paint (StShadowHelper   *helper,
                        ClutterPaintNode *node,
                        ClutterActorBox  *actor_box,
                        uint8_t           paint_opacity)
{
  _st_paint_shadow_with_opacity (helper->shadow,
                                 node,
                                 helper->pipeline,
                                 actor_box,
                                 paint_opacity);
}
