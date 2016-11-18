/*
 * Authored By Neil Roberts  <neil@linux.intel.com>
 * and Jasper St. Pierre <jstpierre@mecheye.net>
 *
 * Copyright (C) 2008 Intel Corporation
 * Copyright (C) 2012 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:meta-shaped-texture
 * @title: MetaShapedTexture
 * @short_description: An actor to draw a masked texture.
 */

#include <config.h>

#include <meta/meta-shaped-texture.h>
#include "meta-shaped-texture-private.h"

#include <cogl/cogl.h>
#include <gdk/gdk.h> /* for gdk_rectangle_intersect() */

#include "clutter-utils.h"
#include "meta-texture-tower.h"

#include "meta-cullable.h"

static void meta_shaped_texture_dispose  (GObject    *object);

static void meta_shaped_texture_paint (ClutterActor       *actor);

static void meta_shaped_texture_get_preferred_width (ClutterActor *self,
                                                     gfloat        for_height,
                                                     gfloat       *min_width_p,
                                                     gfloat       *natural_width_p);

static void meta_shaped_texture_get_preferred_height (ClutterActor *self,
                                                      gfloat        for_width,
                                                      gfloat       *min_height_p,
                                                      gfloat       *natural_height_p);

static gboolean meta_shaped_texture_get_paint_volume (ClutterActor *self, ClutterPaintVolume *volume);

static void cullable_iface_init (MetaCullableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaShapedTexture, meta_shaped_texture, CLUTTER_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (META_TYPE_CULLABLE, cullable_iface_init));

#define META_SHAPED_TEXTURE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), META_TYPE_SHAPED_TEXTURE, \
                                MetaShapedTexturePrivate))

enum {
  SIZE_CHANGED,

  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL];

struct _MetaShapedTexturePrivate
{
  MetaTextureTower *paint_tower;

  CoglTexture *texture;
  CoglTexture *mask_texture;
  CoglSnippet *snippet;

  CoglPipeline *base_pipeline;
  CoglPipeline *masked_pipeline;
  CoglPipeline *unblended_pipeline;

  gboolean is_y_inverted;

  /* The region containing only fully opaque pixels */
  cairo_region_t *opaque_region;

  /* MetaCullable regions, see that documentation for more details */
  cairo_region_t *clip_region;
  cairo_region_t *unobscured_region;

  guint tex_width, tex_height;
  guint fallback_width, fallback_height;

  guint create_mipmaps : 1;
};

static void
meta_shaped_texture_class_init (MetaShapedTextureClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;

  gobject_class->dispose = meta_shaped_texture_dispose;

  actor_class->get_preferred_width = meta_shaped_texture_get_preferred_width;
  actor_class->get_preferred_height = meta_shaped_texture_get_preferred_height;
  actor_class->paint = meta_shaped_texture_paint;
  actor_class->get_paint_volume = meta_shaped_texture_get_paint_volume;

  signals[SIZE_CHANGED] = g_signal_new ("size-changed",
                                        G_TYPE_FROM_CLASS (gobject_class),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, NULL, NULL,
                                        G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (MetaShapedTexturePrivate));
}

static void
meta_shaped_texture_init (MetaShapedTexture *self)
{
  MetaShapedTexturePrivate *priv;

  priv = self->priv = META_SHAPED_TEXTURE_GET_PRIVATE (self);

  priv->paint_tower = meta_texture_tower_new ();

  priv->texture = NULL;
  priv->mask_texture = NULL;
  priv->create_mipmaps = TRUE;
  priv->is_y_inverted = TRUE;
}

static void
set_unobscured_region (MetaShapedTexture *self,
                       cairo_region_t    *unobscured_region)
{
  MetaShapedTexturePrivate *priv = self->priv;

  g_clear_pointer (&priv->unobscured_region, (GDestroyNotify) cairo_region_destroy);
  if (unobscured_region)
    {
      guint width, height;

      if (priv->texture)
        {
          width = priv->tex_width;
          height = priv->tex_height;
        }
      else
        {
          width = priv->fallback_width;
          height = priv->fallback_height;
        }

      cairo_rectangle_int_t bounds = { 0, 0, width, height };
      priv->unobscured_region = cairo_region_copy (unobscured_region);
      cairo_region_intersect_rectangle (priv->unobscured_region, &bounds);
    }
}

static void
set_clip_region (MetaShapedTexture *self,
                 cairo_region_t    *clip_region)
{
  MetaShapedTexturePrivate *priv = self->priv;

  g_clear_pointer (&priv->clip_region, (GDestroyNotify) cairo_region_destroy);
  if (clip_region)
    priv->clip_region = cairo_region_copy (clip_region);
}

static void
meta_shaped_texture_reset_pipelines (MetaShapedTexture *stex)
{
  MetaShapedTexturePrivate *priv = stex->priv;

  g_clear_pointer (&priv->base_pipeline, cogl_object_unref);
  g_clear_pointer (&priv->masked_pipeline, cogl_object_unref);
  g_clear_pointer (&priv->unblended_pipeline, cogl_object_unref);
}

static void
meta_shaped_texture_dispose (GObject *object)
{
  MetaShapedTexture *self = (MetaShapedTexture *) object;
  MetaShapedTexturePrivate *priv = self->priv;

  if (priv->paint_tower)
    meta_texture_tower_free (priv->paint_tower);
  priv->paint_tower = NULL;

  g_clear_pointer (&priv->texture, cogl_object_unref);
  g_clear_pointer (&priv->opaque_region, cairo_region_destroy);

  meta_shaped_texture_set_mask_texture (self, NULL);
  set_unobscured_region (self, NULL);
  set_clip_region (self, NULL);

  meta_shaped_texture_reset_pipelines (self);

  g_clear_pointer (&priv->snippet, cogl_object_unref);

  G_OBJECT_CLASS (meta_shaped_texture_parent_class)->dispose (object);
}

static CoglPipeline *
get_base_pipeline (MetaShapedTexture *stex,
                   CoglContext       *ctx)
{
  MetaShapedTexturePrivate *priv = stex->priv;
  CoglPipeline *pipeline;

  if (priv->base_pipeline)
    return priv->base_pipeline;

  pipeline = cogl_pipeline_new (ctx);
  cogl_pipeline_set_layer_wrap_mode_s (pipeline, 0,
                                       COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
  cogl_pipeline_set_layer_wrap_mode_t (pipeline, 0,
                                       COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
  cogl_pipeline_set_layer_wrap_mode_s (pipeline, 1,
                                       COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
  cogl_pipeline_set_layer_wrap_mode_t (pipeline, 1,
                                       COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
  if (!priv->is_y_inverted)
    {
      CoglMatrix matrix;

      cogl_matrix_init_identity (&matrix);
      cogl_matrix_scale (&matrix, 1, -1, 1);
      cogl_matrix_translate (&matrix, 0, -1, 0);
      cogl_pipeline_set_layer_matrix (pipeline, 0, &matrix);
    }

  if (priv->snippet)
    cogl_pipeline_add_layer_snippet (pipeline, 0, priv->snippet);

  priv->base_pipeline = pipeline;

  return priv->base_pipeline;
}

static CoglPipeline *
get_unmasked_pipeline (MetaShapedTexture *stex,
                       CoglContext       *ctx)
{
  return get_base_pipeline (stex, ctx);
}

static CoglPipeline *
get_masked_pipeline (MetaShapedTexture *stex,
                     CoglContext       *ctx)
{
  MetaShapedTexturePrivate *priv = stex->priv;
  CoglPipeline *pipeline;

  if (priv->masked_pipeline)
    return priv->masked_pipeline;

  pipeline = cogl_pipeline_copy (get_base_pipeline (stex, ctx));
  cogl_pipeline_set_layer_combine (pipeline, 1,
                                   "RGBA = MODULATE (PREVIOUS, TEXTURE[A])",
                                   NULL);

  priv->masked_pipeline = pipeline;

  return pipeline;
}

static CoglPipeline *
get_unblended_pipeline (MetaShapedTexture *stex,
                        CoglContext       *ctx)
{
  MetaShapedTexturePrivate *priv = stex->priv;
  CoglPipeline *pipeline;
  CoglColor color;

  if (priv->unblended_pipeline)
    return priv->unblended_pipeline;

  pipeline = cogl_pipeline_copy (get_base_pipeline (stex, ctx));
  cogl_color_init_from_4ub (&color, 255, 255, 255, 255);
  cogl_pipeline_set_blend (pipeline,
                           "RGBA = ADD (SRC_COLOR, 0)",
                           NULL);
  cogl_pipeline_set_color (pipeline, &color);

  priv->unblended_pipeline = pipeline;

  return pipeline;
}

static void
paint_clipped_rectangle (CoglFramebuffer       *fb,
                         CoglPipeline          *pipeline,
                         cairo_rectangle_int_t *rect,
                         ClutterActorBox       *alloc)
{
  float coords[8];
  float x1, y1, x2, y2;

  x1 = rect->x;
  y1 = rect->y;
  x2 = rect->x + rect->width;
  y2 = rect->y + rect->height;

  coords[0] = rect->x / (alloc->x2 - alloc->x1);
  coords[1] = rect->y / (alloc->y2 - alloc->y1);
  coords[2] = (rect->x + rect->width) / (alloc->x2 - alloc->x1);
  coords[3] = (rect->y + rect->height) / (alloc->y2 - alloc->y1);

  coords[4] = coords[0];
  coords[5] = coords[1];
  coords[6] = coords[2];
  coords[7] = coords[3];

  cogl_framebuffer_draw_multitextured_rectangle (fb, pipeline,
                                                 x1, y1, x2, y2,
                                                 &coords[0], 8);
}

static void
set_cogl_texture (MetaShapedTexture *stex,
                  CoglTexture       *cogl_tex)
{
  MetaShapedTexturePrivate *priv;
  guint width, height;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  if (priv->texture)
    cogl_object_unref (priv->texture);

  priv->texture = cogl_tex;

  if (cogl_tex != NULL)
    {
      cogl_object_ref (cogl_tex);
      width = cogl_texture_get_width (COGL_TEXTURE (cogl_tex));
      height = cogl_texture_get_height (COGL_TEXTURE (cogl_tex));
    }
  else
    {
      width = 0;
      height = 0;
    }

  if (priv->tex_width != width ||
      priv->tex_height != height)
    {
      priv->tex_width = width;
      priv->tex_height = height;
      meta_shaped_texture_set_mask_texture (stex, NULL);
      clutter_actor_queue_relayout (CLUTTER_ACTOR (stex));
      g_signal_emit (stex, signals[SIZE_CHANGED], 0);
    }

  /* NB: We don't queue a redraw of the actor here because we don't
   * know how much of the buffer has changed with respect to the
   * previous buffer. We only queue a redraw in response to surface
   * damage. */

  if (priv->create_mipmaps)
    meta_texture_tower_set_base_texture (priv->paint_tower, cogl_tex);
}

static void
meta_shaped_texture_paint (ClutterActor *actor)
{
  MetaShapedTexture *stex = (MetaShapedTexture *) actor;
  MetaShapedTexturePrivate *priv = stex->priv;
  guint tex_width, tex_height;
  guchar opacity;
  CoglContext *ctx;
  CoglFramebuffer *fb;
  CoglTexture *paint_tex;
  ClutterActorBox alloc;
  CoglPipelineFilter filter;

  if (priv->clip_region && cairo_region_is_empty (priv->clip_region))
    return;

  if (!CLUTTER_ACTOR_IS_REALIZED (CLUTTER_ACTOR (stex)))
    clutter_actor_realize (CLUTTER_ACTOR (stex));

  /* The GL EXT_texture_from_pixmap extension does allow for it to be
   * used together with SGIS_generate_mipmap, however this is very
   * rarely supported. Also, even when it is supported there
   * are distinct performance implications from:
   *
   *  - Updating mipmaps that we don't need
   *  - Having to reallocate pixmaps on the server into larger buffers
   *
   * So, we just unconditionally use our mipmap emulation code. If we
   * wanted to use SGIS_generate_mipmap, we'd have to  query COGL to
   * see if it was supported (no API currently), and then if and only
   * if that was the case, set the clutter texture quality to HIGH.
   * Setting the texture quality to high without SGIS_generate_mipmap
   * support for TFP textures will result in fallbacks to XGetImage.
   */
  if (priv->create_mipmaps)
    paint_tex = meta_texture_tower_get_paint_texture (priv->paint_tower);
  else
    paint_tex = COGL_TEXTURE (priv->texture);

  if (paint_tex == NULL)
    return;

  tex_width = priv->tex_width;
  tex_height = priv->tex_height;

  if (tex_width == 0 || tex_height == 0) /* no contents yet */
    return;

  cairo_rectangle_int_t tex_rect = { 0, 0, tex_width, tex_height };

  /* Use nearest-pixel interpolation if the texture is unscaled. This
   * improves performance, especially with software rendering.
   */

  filter = COGL_PIPELINE_FILTER_LINEAR;

  if (meta_actor_painting_untransformed (tex_width, tex_height, NULL, NULL))
    filter = COGL_PIPELINE_FILTER_NEAREST;

  ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  fb = cogl_get_draw_framebuffer ();

  opacity = clutter_actor_get_paint_opacity (actor);
  clutter_actor_get_allocation_box (actor, &alloc);

  cairo_region_t *blended_region;
  gboolean use_opaque_region = (priv->opaque_region != NULL && opacity == 255);

  if (use_opaque_region)
    {
      if (priv->clip_region != NULL)
        blended_region = cairo_region_copy (priv->clip_region);
      else
        blended_region = cairo_region_create_rectangle (&tex_rect);

      cairo_region_subtract (blended_region, priv->opaque_region);
    }
  else
    {
      if (priv->clip_region != NULL)
        blended_region = cairo_region_reference (priv->clip_region);
      else
        blended_region = NULL;
    }

  /* Limit to how many separate rectangles we'll draw; beyond this just
   * fall back and draw the whole thing */
#define MAX_RECTS 16

  if (blended_region != NULL)
    {
      int n_rects = cairo_region_num_rectangles (blended_region);
      if (n_rects > MAX_RECTS)
        {
          /* Fall back to taking the fully blended path. */
          use_opaque_region = FALSE;

          cairo_region_destroy (blended_region);
          blended_region = NULL;
        }
    }

  /* First, paint the unblended parts, which are part of the opaque region. */
  if (use_opaque_region)
    {
      CoglPipeline *opaque_pipeline;
      cairo_region_t *region;
      int n_rects;
      int i;

      if (priv->clip_region != NULL)
        {
          region = cairo_region_copy (priv->clip_region);
          cairo_region_intersect (region, priv->opaque_region);
        }
      else
        {
          region = cairo_region_reference (priv->opaque_region);
        }

      if (!cairo_region_is_empty (region))
        {
          opaque_pipeline = get_unblended_pipeline (stex, ctx);
          cogl_pipeline_set_layer_texture (opaque_pipeline, 0, paint_tex);
          cogl_pipeline_set_layer_filters (opaque_pipeline, 0, filter, filter);

          n_rects = cairo_region_num_rectangles (region);
          for (i = 0; i < n_rects; i++)
            {
              cairo_rectangle_int_t rect;
              cairo_region_get_rectangle (region, i, &rect);
              paint_clipped_rectangle (fb, opaque_pipeline, &rect, &alloc);
            }
        }

      cairo_region_destroy (region);
    }

  /* Now, go ahead and paint the blended parts. */

  /* We have three cases:
   *   1) blended_region has rectangles - paint the rectangles.
   *   2) blended_region is empty - don't paint anything
   *   3) blended_region is NULL - paint fully-blended.
   *
   *   1) and 3) are the times where we have to paint stuff. This tests
   *   for 1) and 3).
   */
  if (blended_region == NULL || !cairo_region_is_empty (blended_region))
    {
      CoglPipeline *blended_pipeline;

      if (priv->mask_texture == NULL)
        {
          blended_pipeline = get_unmasked_pipeline (stex, ctx);
        }
      else
        {
          blended_pipeline = get_masked_pipeline (stex, ctx);
          cogl_pipeline_set_layer_texture (blended_pipeline, 1, priv->mask_texture);
          cogl_pipeline_set_layer_filters (blended_pipeline, 1, filter, filter);
        }

      cogl_pipeline_set_layer_texture (blended_pipeline, 0, paint_tex);
      cogl_pipeline_set_layer_filters (blended_pipeline, 0, filter, filter);

      CoglColor color;
      cogl_color_init_from_4ub (&color, opacity, opacity, opacity, opacity);
      cogl_pipeline_set_color (blended_pipeline, &color);

      if (blended_region != NULL)
        {
          /* 1) blended_region is not empty. Paint the rectangles. */
          int i;
          int n_rects = cairo_region_num_rectangles (blended_region);

          for (i = 0; i < n_rects; i++)
            {
              cairo_rectangle_int_t rect;
              cairo_region_get_rectangle (blended_region, i, &rect);

              if (!gdk_rectangle_intersect (&tex_rect, &rect, &rect))
                continue;

              paint_clipped_rectangle (fb, blended_pipeline, &rect, &alloc);
            }
        }
      else
        {
          /* 3) blended_region is NULL. Do a full paint. */
          cogl_framebuffer_draw_rectangle (fb, blended_pipeline,
                                           0, 0,
                                           alloc.x2 - alloc.x1,
                                           alloc.y2 - alloc.y1);
        }
    }

  if (blended_region != NULL)
    cairo_region_destroy (blended_region);
}

static void
meta_shaped_texture_get_preferred_width (ClutterActor *self,
                                         gfloat        for_height,
                                         gfloat       *min_width_p,
                                         gfloat       *natural_width_p)
{
  MetaShapedTexturePrivate *priv = META_SHAPED_TEXTURE (self)->priv;
  guint width;

  if (priv->texture)
    width = priv->tex_width;
  else
    width = priv->fallback_width;

  if (min_width_p)
    *min_width_p = width;
  if (natural_width_p)
    *natural_width_p = width;
}

static void
meta_shaped_texture_get_preferred_height (ClutterActor *self,
                                          gfloat        for_width,
                                          gfloat       *min_height_p,
                                          gfloat       *natural_height_p)
{
  MetaShapedTexturePrivate *priv = META_SHAPED_TEXTURE (self)->priv;
  guint height;

  if (priv->texture)
    height = priv->tex_height;
  else
    height = priv->fallback_height;

  if (min_height_p)
    *min_height_p = height;
  if (natural_height_p)
    *natural_height_p = height;
}

static cairo_region_t *
effective_unobscured_region (MetaShapedTexture *self)
{
  MetaShapedTexturePrivate *priv = self->priv;
  ClutterActor *actor;

  /* Fail if we have any mapped clones. */
  actor = CLUTTER_ACTOR (self);
  do
    {
      if (clutter_actor_has_mapped_clones (actor))
        return NULL;
      actor = clutter_actor_get_parent (actor);
    }
  while (actor != NULL);

  return priv->unobscured_region;
}

static gboolean
get_unobscured_bounds (MetaShapedTexture     *self,
                       cairo_rectangle_int_t *unobscured_bounds)
{
  cairo_region_t *unobscured_region = effective_unobscured_region (self);

  if (unobscured_region)
    {
      cairo_region_get_extents (unobscured_region, unobscured_bounds);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
meta_shaped_texture_get_paint_volume (ClutterActor *actor,
                                      ClutterPaintVolume *volume)
{
  MetaShapedTexture *self = META_SHAPED_TEXTURE (actor);
  ClutterActorBox box;
  cairo_rectangle_int_t unobscured_bounds;

  if (!clutter_actor_has_allocation (actor))
    return FALSE;

  clutter_actor_get_allocation_box (actor, &box);

  if (get_unobscured_bounds (self, &unobscured_bounds))
    {
      box.x1 = MAX (unobscured_bounds.x, box.x1);
      box.x2 = MIN (unobscured_bounds.x + unobscured_bounds.width, box.x2);
      box.y1 = MAX (unobscured_bounds.y, box.y1);
      box.y2 = MIN (unobscured_bounds.y + unobscured_bounds.height, box.y2);
    }
  box.x2 = MAX (box.x2, box.x1);
  box.y2 = MAX (box.y2, box.y1);

  clutter_paint_volume_union_box (volume, &box);
  return TRUE;
}

void
meta_shaped_texture_set_create_mipmaps (MetaShapedTexture *stex,
					gboolean           create_mipmaps)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  create_mipmaps = create_mipmaps != FALSE;

  if (create_mipmaps != priv->create_mipmaps)
    {
      CoglTexture *base_texture;
      priv->create_mipmaps = create_mipmaps;
      base_texture = create_mipmaps ? priv->texture : NULL;
      meta_texture_tower_set_base_texture (priv->paint_tower, base_texture);
    }
}

void
meta_shaped_texture_set_mask_texture (MetaShapedTexture *stex,
                                      CoglTexture       *mask_texture)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  g_clear_pointer (&priv->mask_texture, cogl_object_unref);

  if (mask_texture != NULL)
    {
      priv->mask_texture = mask_texture;
      cogl_object_ref (priv->mask_texture);
    }

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stex));
}

gboolean
meta_shaped_texture_is_obscured (MetaShapedTexture *self)
{
  cairo_region_t *unobscured_region = effective_unobscured_region (self);

  if (unobscured_region)
    return cairo_region_is_empty (unobscured_region);
  else
    return FALSE;
}

/**
 * meta_shaped_texture_update_area:
 * @stex: #MetaShapedTexture
 * @x: the x coordinate of the damaged area
 * @y: the y coordinate of the damaged area
 * @width: the width of the damaged area
 * @height: the height of the damaged area
 *
 * Repairs the damaged area indicated by @x, @y, @width and @height
 * and potentially queues a redraw.
 *
 * Return value: Whether a redraw have been queued or not
 */
gboolean
meta_shaped_texture_update_area (MetaShapedTexture *stex,
				 int                x,
				 int                y,
				 int                width,
				 int                height)
{
  MetaShapedTexturePrivate *priv;
  cairo_region_t *unobscured_region;
  const cairo_rectangle_int_t clip = { x, y, width, height };

  priv = stex->priv;

  if (priv->texture == NULL)
    return FALSE;

  meta_texture_tower_update_area (priv->paint_tower, x, y, width, height);

  unobscured_region = effective_unobscured_region (stex);
  if (unobscured_region)
    {
      cairo_region_t *intersection;

      if (cairo_region_is_empty (unobscured_region))
        return FALSE;

      intersection = cairo_region_copy (unobscured_region);
      cairo_region_intersect_rectangle (intersection, &clip);

      if (!cairo_region_is_empty (intersection))
        {
          cairo_rectangle_int_t damage_rect;
          cairo_region_get_extents (intersection, &damage_rect);
          clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (stex), &damage_rect);
          cairo_region_destroy (intersection);
          return TRUE;
        }

      cairo_region_destroy (intersection);
      return FALSE;
    }
  else
    {
      clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (stex), &clip);
      return TRUE;
    }
}

/**
 * meta_shaped_texture_set_texture:
 * @stex: The #MetaShapedTexture
 * @pixmap: The #CoglTexture to display
 */
void
meta_shaped_texture_set_texture (MetaShapedTexture *stex,
                                 CoglTexture       *texture)
{
  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  set_cogl_texture (stex, texture);
}

/**
 * meta_shaped_texture_set_is_y_inverted: (skip)
 */
void
meta_shaped_texture_set_is_y_inverted (MetaShapedTexture *stex,
                                       gboolean           is_y_inverted)
{
  MetaShapedTexturePrivate *priv = stex->priv;

  if (priv->is_y_inverted == is_y_inverted)
    return;

  meta_shaped_texture_reset_pipelines (stex);

  priv->is_y_inverted = is_y_inverted;
}

/**
 * meta_shaped_texture_set_snippet: (skip)
 */
void
meta_shaped_texture_set_snippet (MetaShapedTexture *stex,
                                 CoglSnippet       *snippet)
{
  MetaShapedTexturePrivate *priv = stex->priv;

  if (priv->snippet == snippet)
    return;

  meta_shaped_texture_reset_pipelines (stex);

  g_clear_pointer (&priv->snippet, cogl_object_unref);
  if (snippet)
    priv->snippet = cogl_object_ref (snippet);
}

/**
 * meta_shaped_texture_get_texture:
 * @stex: The #MetaShapedTexture
 *
 * Returns: (transfer none): the unshaped texture
 */
CoglTexture *
meta_shaped_texture_get_texture (MetaShapedTexture *stex)
{
  g_return_val_if_fail (META_IS_SHAPED_TEXTURE (stex), NULL);
  return COGL_TEXTURE (stex->priv->texture);
}

/**
 * meta_shaped_texture_set_opaque_region:
 * @stex: a #MetaShapedTexture
 * @opaque_region: (transfer full): the region of the texture that
 *   can have blending turned off.
 *
 * As most windows have a large portion that does not require blending,
 * we can easily turn off blending if we know the areas that do not
 * require blending. This sets the region where we will not blend for
 * optimization purposes.
 */
void
meta_shaped_texture_set_opaque_region (MetaShapedTexture *stex,
                                       cairo_region_t    *opaque_region)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  if (priv->opaque_region)
    cairo_region_destroy (priv->opaque_region);

  if (opaque_region)
    priv->opaque_region = cairo_region_reference (opaque_region);
  else
    priv->opaque_region = NULL;
}

cairo_region_t *
meta_shaped_texture_get_opaque_region (MetaShapedTexture *stex)
{
  MetaShapedTexturePrivate *priv = stex->priv;
  return priv->opaque_region;
}

/**
 * meta_shaped_texture_get_image:
 * @stex: A #MetaShapedTexture
 * @clip: A clipping rectangle, to help prevent extra processing.
 * In the case that the clipping rectangle is partially or fully
 * outside the bounds of the texture, the rectangle will be clipped.
 *
 * Flattens the two layers of the shaped texture into one ARGB32
 * image by alpha blending the two images, and returns the flattened
 * image.
 *
 * Returns: (transfer full): a new cairo surface to be freed with
 * cairo_surface_destroy().
 */
cairo_surface_t *
meta_shaped_texture_get_image (MetaShapedTexture     *stex,
                               cairo_rectangle_int_t *clip)
{
  CoglTexture *texture, *mask_texture;
  cairo_rectangle_int_t texture_rect = { 0, 0, 0, 0 };
  cairo_surface_t *surface;

  g_return_val_if_fail (META_IS_SHAPED_TEXTURE (stex), NULL);

  texture = COGL_TEXTURE (stex->priv->texture);

  if (texture == NULL)
    return NULL;

  texture_rect.width = cogl_texture_get_width (texture);
  texture_rect.height = cogl_texture_get_height (texture);

  if (clip != NULL)
    {
      /* GdkRectangle is just a typedef of cairo_rectangle_int_t,
       * so we can use the gdk_rectangle_* APIs on these. */
      if (!gdk_rectangle_intersect (&texture_rect, clip, clip))
        return NULL;
    }

  if (clip != NULL)
    texture = cogl_texture_new_from_sub_texture (texture,
                                                 clip->x,
                                                 clip->y,
                                                 clip->width,
                                                 clip->height);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        cogl_texture_get_width (texture),
                                        cogl_texture_get_height (texture));

  cogl_texture_get_data (texture, CLUTTER_CAIRO_FORMAT_ARGB32,
                         cairo_image_surface_get_stride (surface),
                         cairo_image_surface_get_data (surface));

  cairo_surface_mark_dirty (surface);

  if (clip != NULL)
    cogl_object_unref (texture);

  mask_texture = stex->priv->mask_texture;
  if (mask_texture != NULL)
    {
      cairo_t *cr;
      cairo_surface_t *mask_surface;

      if (clip != NULL)
        mask_texture = cogl_texture_new_from_sub_texture (mask_texture,
                                                          clip->x,
                                                          clip->y,
                                                          clip->width,
                                                          clip->height);

      mask_surface = cairo_image_surface_create (CAIRO_FORMAT_A8,
                                                 cogl_texture_get_width (mask_texture),
                                                 cogl_texture_get_height (mask_texture));

      cogl_texture_get_data (mask_texture, COGL_PIXEL_FORMAT_A_8,
                             cairo_image_surface_get_stride (mask_surface),
                             cairo_image_surface_get_data (mask_surface));

      cairo_surface_mark_dirty (mask_surface);

      cr = cairo_create (surface);
      cairo_set_source_surface (cr, mask_surface, 0, 0);
      cairo_set_operator (cr, CAIRO_OPERATOR_DEST_IN);
      cairo_paint (cr);
      cairo_destroy (cr);

      cairo_surface_destroy (mask_surface);

      if (clip != NULL)
        cogl_object_unref (mask_texture);
    }

  return surface;
}

void
meta_shaped_texture_set_fallback_size (MetaShapedTexture *self,
                                       guint              fallback_width,
                                       guint              fallback_height)
{
  MetaShapedTexturePrivate *priv = self->priv;

  priv->fallback_width = fallback_width;
  priv->fallback_height = fallback_height;
}

static void
meta_shaped_texture_cull_out (MetaCullable   *cullable,
                              cairo_region_t *unobscured_region,
                              cairo_region_t *clip_region)
{
  MetaShapedTexture *self = META_SHAPED_TEXTURE (cullable);
  MetaShapedTexturePrivate *priv = self->priv;

  set_unobscured_region (self, unobscured_region);
  set_clip_region (self, clip_region);

  if (clutter_actor_get_paint_opacity (CLUTTER_ACTOR (self)) == 0xff)
    {
      if (priv->opaque_region)
        {
          if (unobscured_region)
            cairo_region_subtract (unobscured_region, priv->opaque_region);
          if (clip_region)
            cairo_region_subtract (clip_region, priv->opaque_region);
        }
    }
}

static void
meta_shaped_texture_reset_culling (MetaCullable *cullable)
{
  MetaShapedTexture *self = META_SHAPED_TEXTURE (cullable);
  set_clip_region (self, NULL);
}

static void
cullable_iface_init (MetaCullableInterface *iface)
{
  iface->cull_out = meta_shaped_texture_cull_out;
  iface->reset_culling = meta_shaped_texture_reset_culling;
}

ClutterActor *
meta_shaped_texture_new (void)
{
  return g_object_new (META_TYPE_SHAPED_TEXTURE, NULL);
}
