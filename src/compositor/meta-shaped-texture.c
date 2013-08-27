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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

/**
 * SECTION:meta-shaped-texture
 * @title: MetaShapedTexture
 * @short_description: An actor to draw a masked texture.
 */

#include <config.h>

#include <meta/meta-shaped-texture.h>
#include <meta/util.h>
#include "meta-texture-tower.h"

#include "meta-shaped-texture-private.h"
#include "meta-wayland-private.h"
#include <cogl/cogl-wayland-server.h>

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <cogl/cogl-texture-pixmap-x11.h>
#include <gdk/gdk.h> /* for gdk_rectangle_intersect() */

static void meta_shaped_texture_dispose  (GObject    *object);

static void meta_shaped_texture_paint (ClutterActor       *actor);
static void meta_shaped_texture_pick  (ClutterActor       *actor,
				       const ClutterColor *color);

static void meta_shaped_texture_get_preferred_width (ClutterActor *self,
                                                     gfloat        for_height,
                                                     gfloat       *min_width_p,
                                                     gfloat       *natural_width_p);

static void meta_shaped_texture_get_preferred_height (ClutterActor *self,
                                                      gfloat        for_width,
                                                      gfloat       *min_height_p,
                                                      gfloat       *natural_height_p);

static gboolean meta_shaped_texture_get_paint_volume (ClutterActor *self, ClutterPaintVolume *volume);

typedef enum _MetaShapedTextureType
{
  META_SHAPED_TEXTURE_TYPE_X11_PIXMAP,
  META_SHAPED_TEXTURE_TYPE_WAYLAND_SURFACE,
} MetaShapedTextureType;


G_DEFINE_TYPE (MetaShapedTexture, meta_shaped_texture,
               CLUTTER_TYPE_ACTOR);

#define META_SHAPED_TEXTURE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), META_TYPE_SHAPED_TEXTURE, \
                                MetaShapedTexturePrivate))

struct _MetaShapedTexturePrivate
{
  MetaTextureTower *paint_tower;

  MetaShapedTextureType type;
  union {
    struct {
      Pixmap pixmap;
    } x11;
    struct {
      MetaWaylandSurface *surface;
    } wayland;
  };

  CoglTexture *texture;

  CoglTexture *mask_texture;

  cairo_region_t *clip_region;
  cairo_region_t *input_shape_region;
  cairo_region_t *opaque_region;

  guint tex_width, tex_height;

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
  actor_class->pick = meta_shaped_texture_pick;
  actor_class->get_paint_volume = meta_shaped_texture_get_paint_volume;

  g_type_class_add_private (klass, sizeof (MetaShapedTexturePrivate));
}

static void
meta_shaped_texture_init (MetaShapedTexture *self)
{
  MetaShapedTexturePrivate *priv;

  priv = self->priv = META_SHAPED_TEXTURE_GET_PRIVATE (self);

  priv->paint_tower = meta_texture_tower_new ();

  priv->type = META_SHAPED_TEXTURE_TYPE_X11_PIXMAP;
  priv->texture = NULL;

  priv->mask_texture = NULL;
  priv->create_mipmaps = TRUE;
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

  meta_shaped_texture_set_mask_texture (self, NULL);
  meta_shaped_texture_set_clip_region (self, NULL);

  G_OBJECT_CLASS (meta_shaped_texture_parent_class)->dispose (object);
}

static CoglPipeline *
get_unmasked_pipeline (CoglContext *ctx)
{
  return cogl_pipeline_new (ctx);
}

static CoglPipeline *
get_masked_pipeline (CoglContext *ctx)
{
  static CoglPipeline *template = NULL;
  if (G_UNLIKELY (template == NULL))
    {
      template = cogl_pipeline_new (ctx);
      cogl_pipeline_set_layer_combine (template, 1,
                                       "RGBA = MODULATE (PREVIOUS, TEXTURE[A])",
                                       NULL);
    }

  return cogl_pipeline_copy (template);
}

static CoglPipeline *
get_unblended_pipeline (CoglContext *ctx)
{
  static CoglPipeline *template = NULL;
  if (G_UNLIKELY (template == NULL))
    {
      CoglColor color;
      template = cogl_pipeline_new (ctx);
      cogl_color_init_from_4ub (&color, 255, 255, 255, 255);
      cogl_pipeline_set_blend (template,
                               "RGBA = ADD (SRC_COLOR, 0)",
                               NULL);
      cogl_pipeline_set_color (template, &color);
    }

  return cogl_pipeline_copy (template);
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
      width = cogl_texture_get_width (COGL_TEXTURE (cogl_tex));
      height = cogl_texture_get_height (COGL_TEXTURE (cogl_tex));

      if (width != priv->tex_width ||
          height != priv->tex_height)
        {
          priv->tex_width = width;
          priv->tex_height = height;

          clutter_actor_queue_relayout (CLUTTER_ACTOR (stex));
        }
    }
  else
    {
      /* size changed to 0 going to an invalid handle */
      priv->tex_width = 0;
      priv->tex_height = 0;
      clutter_actor_queue_relayout (CLUTTER_ACTOR (stex));
    }

  /* NB: We don't queue a redraw of the actor here because we don't
   * know how much of the buffer has changed with respect to the
   * previous buffer. We only queue a redraw in response to surface
   * damage. */
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
  CoglPipeline *pipeline = NULL;
  CoglTexture *paint_tex;
  ClutterActorBox alloc;
  cairo_region_t *blended_region = NULL;

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

  ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  fb = cogl_get_draw_framebuffer ();

  opacity = clutter_actor_get_paint_opacity (actor);
  clutter_actor_get_allocation_box (actor, &alloc);

  if (priv->opaque_region != NULL && opacity == 255)
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

      if (cairo_region_is_empty (region))
        goto paint_blended;

      opaque_pipeline = get_unblended_pipeline (ctx);
      cogl_pipeline_set_layer_texture (opaque_pipeline, 0, paint_tex);

      n_rects = cairo_region_num_rectangles (region);
      for (i = 0; i < n_rects; i++)
        {
          cairo_rectangle_int_t rect;
          cairo_region_get_rectangle (region, i, &rect);
          paint_clipped_rectangle (fb, opaque_pipeline, &rect, &alloc);
        }

      cogl_object_unref (opaque_pipeline);

      if (priv->clip_region != NULL)
        {
          blended_region = cairo_region_copy (priv->clip_region);
        }
      else
        {
          cairo_rectangle_int_t rect = { 0, 0, tex_width, tex_height };
          blended_region = cairo_region_create_rectangle (&rect);
        }

      cairo_region_subtract (blended_region, priv->opaque_region);

    paint_blended:
      cairo_region_destroy (region);
    }

  if (blended_region == NULL && priv->clip_region != NULL)
    blended_region = cairo_region_reference (priv->clip_region);

  if (blended_region != NULL && cairo_region_is_empty (blended_region))
    goto out;

  if (priv->mask_texture == NULL)
    {
      pipeline = get_unmasked_pipeline (ctx);
    }
  else
    {
      pipeline = get_masked_pipeline (ctx);
      cogl_pipeline_set_layer_texture (pipeline, 1, priv->mask_texture);
    }

  cogl_pipeline_set_layer_texture (pipeline, 0, paint_tex);

  {
    CoglColor color;
    cogl_color_init_from_4ub (&color, opacity, opacity, opacity, opacity);
    cogl_pipeline_set_color (pipeline, &color);
  }

  if (blended_region != NULL)
    {
      int n_rects;

      /* Limit to how many separate rectangles we'll draw; beyond this just
       * fall back and draw the whole thing */
#     define MAX_RECTS 16

      n_rects = cairo_region_num_rectangles (priv->clip_region);
      if (n_rects <= MAX_RECTS)
	{
          int i;
          cairo_rectangle_int_t tex_rect = { 0, 0, tex_width, tex_height };

	  for (i = 0; i < n_rects; i++)
	    {
	      cairo_rectangle_int_t rect;

	      cairo_region_get_rectangle (priv->clip_region, i, &rect);

	      if (!gdk_rectangle_intersect (&tex_rect, &rect, &rect))
		continue;

              paint_clipped_rectangle (fb, pipeline, &rect, &alloc);
            }

          goto out;
	}
    }

  cogl_framebuffer_draw_rectangle (fb, pipeline,
                                   0, 0,
                                   alloc.x2 - alloc.x1,
                                   alloc.y2 - alloc.y1);

 out:
  if (pipeline != NULL)
    cogl_object_unref (pipeline);
  if (blended_region != NULL)
    cairo_region_destroy (blended_region);
}

static void
meta_shaped_texture_pick (ClutterActor       *actor,
			  const ClutterColor *color)
{
  MetaShapedTexture *stex = (MetaShapedTexture *) actor;
  MetaShapedTexturePrivate *priv = stex->priv;

  if (!clutter_actor_should_pick_paint (actor) ||
      (priv->clip_region && cairo_region_is_empty (priv->clip_region)))
    return;

  /* If there is no region then use the regular pick */
  if (priv->input_shape_region == NULL)
    CLUTTER_ACTOR_CLASS (meta_shaped_texture_parent_class)->pick (actor, color);
  else
    {
      int n_rects;
      float *rectangles;
      int i;
      CoglPipeline *pipeline;
      CoglContext *ctx;
      CoglFramebuffer *fb;
      CoglColor cogl_color;

      /* Note: We don't bother trying to intersect the pick and clip regions
       * since needing to copy the region, do the intersection, and probably
       * increase the number of rectangles seems more likely to have a negative
       * effect.
       *
       * NB: Most of the time when just using rectangles for picking then
       * picking shouldn't involve any rendering, and minimizing the number of
       * rectangles has more benefit than reducing the area of the pick
       * region.
       */

      n_rects = cairo_region_num_rectangles (priv->input_shape_region);
      rectangles = g_alloca (sizeof (float) * 4 * n_rects);

      for (i = 0; i < n_rects; i++)
        {
          cairo_rectangle_int_t rect;
          int pos = i * 4;

          cairo_region_get_rectangle (priv->input_shape_region, i, &rect);

          rectangles[pos] = rect.x;
          rectangles[pos + 1] = rect.y;
          rectangles[pos + 2] = rect.x + rect.width;
          rectangles[pos + 3] = rect.y + rect.height;
        }

      ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
      fb = cogl_get_draw_framebuffer ();

      cogl_color_init_from_4ub (&cogl_color, color->red, color->green, color->blue, color->alpha);

      pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_set_color (pipeline, &cogl_color);

      cogl_framebuffer_draw_rectangles (fb, pipeline,
                                        rectangles, n_rects);
      cogl_object_unref (pipeline);
    }
}

static void
meta_shaped_texture_get_preferred_width (ClutterActor *self,
                                         gfloat        for_height,
                                         gfloat       *min_width_p,
                                         gfloat       *natural_width_p)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (self));

  priv = META_SHAPED_TEXTURE (self)->priv;

  if (min_width_p)
    *min_width_p = 0;

  if (natural_width_p)
    *natural_width_p = priv->tex_width;
}

static void
meta_shaped_texture_get_preferred_height (ClutterActor *self,
                                          gfloat        for_width,
                                          gfloat       *min_height_p,
                                          gfloat       *natural_height_p)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (self));

  priv = META_SHAPED_TEXTURE (self)->priv;

  if (min_height_p)
    *min_height_p = 0;

  if (natural_height_p)
    *natural_height_p = priv->tex_height;
}

static gboolean
meta_shaped_texture_get_paint_volume (ClutterActor *self,
                                      ClutterPaintVolume *volume)
{
  return clutter_paint_volume_set_from_allocation (volume, self);
}

ClutterActor *
meta_shaped_texture_new_with_wayland_surface (MetaWaylandSurface *surface)
{
  ClutterActor *actor = g_object_new (META_TYPE_SHAPED_TEXTURE, NULL);
  MetaShapedTexturePrivate *priv = META_SHAPED_TEXTURE (actor)->priv;

  /* XXX: it could probably be better to have a "type" construct-only
   * property or create wayland/x11 subclasses */
  priv->type = META_SHAPED_TEXTURE_TYPE_WAYLAND_SURFACE;

  meta_shaped_texture_set_wayland_surface (META_SHAPED_TEXTURE (actor),
                                           surface);

  return actor;
}

void
meta_shaped_texture_set_wayland_surface (MetaShapedTexture *stex,
                                         MetaWaylandSurface *surface)
{
  MetaShapedTexturePrivate *priv = stex->priv;

  priv->wayland.surface = surface;

  if (surface && surface->buffer_ref.buffer)
    meta_shaped_texture_attach_wayland_buffer (stex,
                                               surface->buffer_ref.buffer);
}

MetaWaylandSurface *
meta_shaped_texture_get_wayland_surface (MetaShapedTexture *stex)
{
  MetaShapedTexturePrivate *priv = stex->priv;
  return priv->wayland.surface;
}

ClutterActor *
meta_shaped_texture_new_with_xwindow (Window xwindow)
{
  return g_object_new (META_TYPE_SHAPED_TEXTURE, NULL);
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

static void
wayland_surface_update_area (MetaShapedTexture *stex,
                             int                x,
                             int                y,
                             int                width,
                             int                height)
{
  MetaShapedTexturePrivate *priv;
  MetaWaylandBuffer *buffer;

  priv = stex->priv;

  g_return_if_fail (priv->type == META_SHAPED_TEXTURE_TYPE_WAYLAND_SURFACE);
  g_return_if_fail (priv->texture != NULL);

  buffer = priv->wayland.surface->buffer_ref.buffer;

  if (buffer)
    {
      struct wl_resource *resource = buffer->resource;
      struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get (resource);

      if (shm_buffer)
        {
          CoglPixelFormat format;

          switch (wl_shm_buffer_get_format (shm_buffer))
            {
#if G_BYTE_ORDER == G_BIG_ENDIAN
            case WL_SHM_FORMAT_ARGB8888:
              format = COGL_PIXEL_FORMAT_ARGB_8888_PRE;
              break;
            case WL_SHM_FORMAT_XRGB8888:
              format = COGL_PIXEL_FORMAT_ARGB_8888;
              break;
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
            case WL_SHM_FORMAT_ARGB8888:
              format = COGL_PIXEL_FORMAT_BGRA_8888_PRE;
              break;
            case WL_SHM_FORMAT_XRGB8888:
              format = COGL_PIXEL_FORMAT_BGRA_8888;
              break;
#endif
            default:
              g_warn_if_reached ();
              format = COGL_PIXEL_FORMAT_ARGB_8888;
            }

          cogl_texture_set_region (priv->texture,
                                   x, y,
                                   x, y,
                                   width, height,
                                   width, height,
                                   format,
                                   wl_shm_buffer_get_stride (shm_buffer),
                                   wl_shm_buffer_get_data (shm_buffer));
        }
    }
}

static void
queue_damage_redraw_with_clip (MetaShapedTexture *stex,
                               int x,
                               int y,
                               int width,
                               int height)
{
  ClutterActor *self = CLUTTER_ACTOR (stex);
  MetaShapedTexturePrivate *priv;
  ClutterActorBox allocation;
  float scale_x;
  float scale_y;
  cairo_rectangle_int_t clip;

  /* NB: clutter_actor_queue_redraw_with_clip expects a box in the actor's
   * coordinate space so we need to convert from surface coordinates to
   * actor coordinates...
   */

  /* Calling clutter_actor_get_allocation_box() is enormously expensive
   * if the actor has an out-of-date allocation, since it triggers
   * a full redraw. clutter_actor_queue_redraw_with_clip() would redraw
   * the whole stage anyways in that case, so just go ahead and do
   * it here.
   */
  if (!clutter_actor_has_allocation (self))
    {
      clutter_actor_queue_redraw (self);
      return;
    }

  priv = stex->priv;

  if (priv->tex_width == 0 || priv->tex_height == 0)
    return;

  clutter_actor_get_allocation_box (self, &allocation);

  scale_x = (allocation.x2 - allocation.x1) / priv->tex_width;
  scale_y = (allocation.y2 - allocation.y1) / priv->tex_height;

  clip.x = x * scale_x;
  clip.y = y * scale_y;
  clip.width = width * scale_x;
  clip.height = height * scale_y;
  clutter_actor_queue_redraw_with_clip (self, &clip);
}

/**
 * meta_shaped_texture_update_area:
 * @stex: #MetaShapedTexture
 * @x: the x coordinate of the damaged area
 * @y: the y coordinate of the damaged area
 * @width: the width of the damaged area
 * @height: the height of the damaged area
 * @unobscured_region: The unobscured region of the window or %NULL if
 * there is no valid one (like when the actor is transformed or
 * has a mapped clone)
 *
 * Repairs the damaged area indicated by @x, @y, @width and @height
 * and queues a redraw for the intersection @visibible_region and
 * the damage area. If @visibible_region is %NULL a redraw will always
 * get queued.
 *
 * Return value: Whether a redraw have been queued or not
 */
gboolean
meta_shaped_texture_update_area (MetaShapedTexture *stex,
				 int                x,
				 int                y,
				 int                width,
				 int                height,
				 cairo_region_t    *unobscured_region)
{
  MetaShapedTexturePrivate *priv;

  priv = stex->priv;

  if (priv->texture == NULL)
    return FALSE;

  switch (priv->type)
    {
    case META_SHAPED_TEXTURE_TYPE_X11_PIXMAP:
      cogl_texture_pixmap_x11_update_area (COGL_TEXTURE_PIXMAP_X11 (priv->texture),
                                           x, y, width, height);
      break;
    case META_SHAPED_TEXTURE_TYPE_WAYLAND_SURFACE:
      wayland_surface_update_area (stex, x, y, width, height);
      break;
    }

  meta_texture_tower_update_area (priv->paint_tower, x, y, width, height);

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

  clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (stex), &clip);

  return TRUE;
}

/**
 * meta_shaped_texture_set_pixmap:
 * @stex: The #MetaShapedTexture
 * @pixmap: The pixmap you want the stex to assume
 */
void
meta_shaped_texture_set_pixmap (MetaShapedTexture *stex,
                                Pixmap             pixmap)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  if (priv->x11.pixmap == pixmap)
    return;

  priv->x11.pixmap = pixmap;

  if (pixmap != None)
    {
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());
      CoglTexture *texture =
        COGL_TEXTURE (cogl_texture_pixmap_x11_new (ctx, pixmap, FALSE, NULL));
      set_cogl_texture (stex, texture);
    }
  else
    set_cogl_texture (stex, NULL);

  if (priv->create_mipmaps)
    meta_texture_tower_set_base_texture (priv->paint_tower,
                                         COGL_TEXTURE (priv->texture));
}

void
meta_shaped_texture_attach_wayland_buffer (MetaShapedTexture  *stex,
                                           MetaWaylandBuffer  *buffer)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  /* TODO: we should change this api to be something like
   * meta_shaped_texture_notify_buffer_attach() since we now maintain
   * a reference to the MetaWaylandSurface where we can access the
   * buffer without it being explicitly passed as an argument.
   */
  g_return_if_fail (priv->wayland.surface->buffer_ref.buffer == buffer);

  if (buffer)
    {
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());
      CoglError *catch_error = NULL;
      CoglTexture *texture =
        COGL_TEXTURE (cogl_wayland_texture_2d_new_from_buffer (ctx,
                                                               buffer->resource,
                                                               &catch_error));
      if (!texture)
        {
          cogl_error_free (catch_error);
        }
      else
        {
          buffer->width = cogl_texture_get_width (texture);
          buffer->height = cogl_texture_get_height (texture);
        }

      set_cogl_texture (stex, texture);
    }
  else
    set_cogl_texture (stex, NULL);

  if (priv->create_mipmaps)
    meta_texture_tower_set_base_texture (priv->paint_tower,
                                         COGL_TEXTURE (priv->texture));
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
 * meta_shaped_texture_set_input_shape_region:
 * @stex: a #MetaShapedTexture
 * @shape_region: the region of the texture that should respond to
 *    input.
 *
 * Determines what region of the texture should accept input. For
 * X based windows this is defined by the ShapeInput region of the
 * window.
 */
void
meta_shaped_texture_set_input_shape_region (MetaShapedTexture *stex,
                                            cairo_region_t    *shape_region)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  if (priv->input_shape_region != NULL)
    {
      cairo_region_destroy (priv->input_shape_region);
      priv->input_shape_region = NULL;
    }

  if (shape_region != NULL)
    {
      cairo_region_reference (shape_region);
      priv->input_shape_region = shape_region;
    }

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stex));
}

/**
 * meta_shaped_texture_set_clip_region:
 * @stex: a #MetaShapedTexture
 * @clip_region: (transfer full): the region of the texture that
 *   is visible and should be painted.
 *
 * Provides a hint to the texture about what areas of the texture
 * are not completely obscured and thus need to be painted. This
 * is an optimization and is not supposed to have any effect on
 * the output.
 *
 * Typically a parent container will set the clip region before
 * painting its children, and then unset it afterwards.
 */
void
meta_shaped_texture_set_clip_region (MetaShapedTexture *stex,
				     cairo_region_t    *clip_region)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  if (priv->clip_region)
    {
      cairo_region_destroy (priv->clip_region);
      priv->clip_region = NULL;
    }

  if (clip_region)
    priv->clip_region = cairo_region_copy (clip_region);
  else
    priv->clip_region = NULL;
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
