/*
 * shaped texture
 *
 * An actor to draw a texture clipped to a list of rectangles
 *
 * Authored By Neil Roberts  <neil@linux.intel.com>
 *
 * Copyright (C) 2008 Intel Corporation
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

#include <config.h>

#define CLUTTER_ENABLE_EXPERIMENTAL_API
#define COGL_ENABLE_EXPERIMENTAL_API

#include <meta/meta-shaped-texture.h>
#include "meta-texture-tower.h"
#include "meta-texture-rectangle.h"

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <cogl/cogl-texture-pixmap-x11.h>
#include <gdk/gdk.h> /* for gdk_rectangle_intersect() */
#include <string.h>

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

static void meta_shaped_texture_dirty_mask (MetaShapedTexture *stex);

static gboolean meta_shaped_texture_get_paint_volume (ClutterActor *self, ClutterPaintVolume *volume);

G_DEFINE_TYPE (MetaShapedTexture, meta_shaped_texture,
               CLUTTER_TYPE_ACTOR);

#define META_SHAPED_TEXTURE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), META_TYPE_SHAPED_TEXTURE, \
                                MetaShapedTexturePrivate))

struct _MetaShapedTexturePrivate
{
  MetaTextureTower *paint_tower;
  Pixmap pixmap;
  CoglHandle texture;
  CoglHandle mask_texture;
  CoglHandle material;
  CoglHandle material_unshaped;

  cairo_region_t *clip_region;
  cairo_region_t *shape_region;

  cairo_region_t *overlay_region;
  cairo_path_t *overlay_path;

  guint tex_width, tex_height;
  guint mask_width, mask_height;

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

  priv->shape_region = NULL;
  priv->overlay_path = NULL;
  priv->overlay_region = NULL;
  priv->paint_tower = meta_texture_tower_new ();
  priv->texture = COGL_INVALID_HANDLE;
  priv->mask_texture = COGL_INVALID_HANDLE;
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

  meta_shaped_texture_dirty_mask (self);

  if (priv->material != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->material);
      priv->material = COGL_INVALID_HANDLE;
    }
  if (priv->material_unshaped != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->material_unshaped);
      priv->material_unshaped = COGL_INVALID_HANDLE;
    }
  if (priv->texture != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->texture);
      priv->texture = COGL_INVALID_HANDLE;
    }

  meta_shaped_texture_set_shape_region (self, NULL);
  meta_shaped_texture_set_clip_region (self, NULL);
  meta_shaped_texture_set_overlay_path (self, NULL, NULL);

  G_OBJECT_CLASS (meta_shaped_texture_parent_class)->dispose (object);
}

static void
meta_shaped_texture_dirty_mask (MetaShapedTexture *stex)
{
  MetaShapedTexturePrivate *priv = stex->priv;

  if (priv->mask_texture != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->mask_texture);
      priv->mask_texture = COGL_INVALID_HANDLE;
    }

  if (priv->material != COGL_INVALID_HANDLE)
    cogl_material_set_layer (priv->material, 1, COGL_INVALID_HANDLE);
}

static void
install_overlay_path (MetaShapedTexture *stex,
                      guchar            *mask_data,
                      int                tex_width,
                      int                tex_height,
                      int                stride)
{
  MetaShapedTexturePrivate *priv = stex->priv;
  int i, n_rects;
  cairo_t *cr;
  cairo_rectangle_int_t rect;
  cairo_surface_t *surface;

  if (priv->overlay_region == NULL)
    return;

  surface = cairo_image_surface_create_for_data (mask_data,
                                                 CAIRO_FORMAT_A8,
                                                 tex_width,
                                                 tex_height,
                                                 stride);

  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);

  n_rects = cairo_region_num_rectangles (priv->overlay_region);
  for (i = 0; i < n_rects; i++)
    {
      cairo_region_get_rectangle (priv->overlay_region, i, &rect);
      cairo_rectangle (cr, rect.x, rect.y, rect.width, rect.height);
    }

  cairo_fill_preserve (cr);
  if (priv->overlay_path == NULL)
    {
      /* If we have an overlay region but not an overlay path, then we
       * just need to clear the rectangles in the overlay region. */
      goto out;
    }

  cairo_clip (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  cairo_append_path (cr, priv->overlay_path);
  cairo_fill (cr);

 out:
  cairo_destroy (cr);
  cairo_surface_destroy (surface);
}

static void
meta_shaped_texture_ensure_mask (MetaShapedTexture *stex)
{
  MetaShapedTexturePrivate *priv = stex->priv;
  CoglHandle paint_tex;
  guint tex_width, tex_height;

  paint_tex = priv->texture;

  if (paint_tex == COGL_INVALID_HANDLE)
    return;

  tex_width = cogl_texture_get_width (paint_tex);
  tex_height = cogl_texture_get_height (paint_tex);

  /* If the mask texture we have was created for a different size then
     recreate it */
  if (priv->mask_texture != COGL_INVALID_HANDLE
      && (priv->mask_width != tex_width || priv->mask_height != tex_height))
    meta_shaped_texture_dirty_mask (stex);

  /* If we don't have a mask texture yet then create one */
  if (priv->mask_texture == COGL_INVALID_HANDLE)
    {
      guchar *mask_data;
      int i;
      int n_rects;
      int stride;
      GLenum paint_gl_target;

      /* If we have no shape region and no (or an empty) overlay region, we
       * don't need to create a full mask texture, so quit early. */
      if (priv->shape_region == NULL &&
          (priv->overlay_region == NULL ||
           cairo_region_num_rectangles (priv->overlay_region) == 0))
        {
          return;
        }

      stride = cairo_format_stride_for_width (CAIRO_FORMAT_A8, tex_width);

      /* Create data for an empty image */
      mask_data = g_malloc0 (stride * tex_height);

      n_rects = cairo_region_num_rectangles (priv->shape_region);

      /* Fill in each rectangle. */
      for (i = 0; i < n_rects; i ++)
        {
          cairo_rectangle_int_t rect;
          cairo_region_get_rectangle (priv->shape_region, i, &rect);

          gint x1 = rect.x, x2 = x1 + rect.width;
          gint y1 = rect.y, y2 = y1 + rect.height;
          guchar *p;

          /* Clip the rectangle to the size of the texture */
          x1 = CLAMP (x1, 0, (gint) tex_width - 1);
          x2 = CLAMP (x2, x1, (gint) tex_width);
          y1 = CLAMP (y1, 0, (gint) tex_height - 1);
          y2 = CLAMP (y2, y1, (gint) tex_height);

          /* Fill the rectangle */
          for (p = mask_data + y1 * stride + x1;
               y1 < y2;
               y1++, p += stride)
            memset (p, 255, x2 - x1);
        }

      install_overlay_path (stex, mask_data, tex_width, tex_height, stride);

      cogl_texture_get_gl_texture (paint_tex, NULL, &paint_gl_target);

#ifdef GL_TEXTURE_RECTANGLE_ARB
      if (paint_gl_target == GL_TEXTURE_RECTANGLE_ARB)
        {
          priv->mask_texture
            = meta_texture_rectangle_new (tex_width, tex_height,
                                          0, /* flags */
                                          /* data format */
                                          COGL_PIXEL_FORMAT_A_8,
                                          /* internal GL format */
                                          GL_ALPHA,
                                          /* internal cogl format */
                                          COGL_PIXEL_FORMAT_A_8,
                                          /* rowstride */
                                          stride,
                                          mask_data);
        }
      else
#endif /* GL_TEXTURE_RECTANGLE_ARB */
        {
	  /* Note: we don't allow slicing for this texture because we
           * need to use it with multi-texturing which doesn't support
           * sliced textures */
          priv->mask_texture = cogl_texture_new_from_data (tex_width, tex_height,
                                                           COGL_TEXTURE_NO_SLICING,
                                                           COGL_PIXEL_FORMAT_A_8,
                                                           COGL_PIXEL_FORMAT_ANY,
                                                           stride,
                                                           mask_data);
        }

      g_free (mask_data);

      priv->mask_width = tex_width;
      priv->mask_height = tex_height;
    }
}

static void
meta_shaped_texture_paint (ClutterActor *actor)
{
  MetaShapedTexture *stex = (MetaShapedTexture *) actor;
  MetaShapedTexturePrivate *priv = stex->priv;
  CoglHandle paint_tex;
  guint tex_width, tex_height;
  ClutterActorBox alloc;

  static CoglHandle material_template = COGL_INVALID_HANDLE;
  static CoglHandle material_unshaped_template = COGL_INVALID_HANDLE;

  CoglHandle material;

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
    paint_tex = priv->texture;

  if (paint_tex == COGL_INVALID_HANDLE)
    return;

  tex_width = priv->tex_width;
  tex_height = priv->tex_height;

  if (tex_width == 0 || tex_height == 0) /* no contents yet */
    return;

  if (priv->shape_region == NULL)
    {
      /* No region means an unclipped shape. Use a single-layer texture. */

      if (priv->material_unshaped == COGL_INVALID_HANDLE) 
        {
          if (G_UNLIKELY (material_unshaped_template == COGL_INVALID_HANDLE))
            material_unshaped_template = cogl_material_new ();

          priv->material_unshaped = cogl_material_copy (material_unshaped_template);
        }
        material = priv->material_unshaped;
    }
  else
    {
      meta_shaped_texture_ensure_mask (stex);

      if (priv->material == COGL_INVALID_HANDLE)
	{
	   if (G_UNLIKELY (material_template == COGL_INVALID_HANDLE))
	    {
	      material_template =  cogl_material_new ();
	      cogl_material_set_layer_combine (material_template, 1,
					   "RGBA = MODULATE (PREVIOUS, TEXTURE[A])",
					   NULL);
	    }
	  priv->material = cogl_material_copy (material_template);
	}
      material = priv->material;

      cogl_material_set_layer (material, 1, priv->mask_texture);
    }

  cogl_material_set_layer (material, 0, paint_tex);

  {
    CoglColor color;
    guchar opacity = clutter_actor_get_paint_opacity (actor);
    cogl_color_set_from_4ub (&color, opacity, opacity, opacity, opacity);
    cogl_material_set_color (material, &color);
  }

  cogl_set_source (material);

  clutter_actor_get_allocation_box (actor, &alloc);

  if (priv->clip_region)
    {
      int n_rects;
      int i;
      cairo_rectangle_int_t tex_rect = { 0, 0, tex_width, tex_height };

      /* Limit to how many separate rectangles we'll draw; beyond this just
       * fall back and draw the whole thing */
#     define MAX_RECTS 16

      n_rects = cairo_region_num_rectangles (priv->clip_region);
      if (n_rects <= MAX_RECTS)
	{
	  float coords[8];
          float x1, y1, x2, y2;

	  for (i = 0; i < n_rects; i++)
	    {
	      cairo_rectangle_int_t rect;

	      cairo_region_get_rectangle (priv->clip_region, i, &rect);

	      if (!gdk_rectangle_intersect (&tex_rect, &rect, &rect))
		continue;

	      x1 = rect.x;
	      y1 = rect.y;
	      x2 = rect.x + rect.width;
	      y2 = rect.y + rect.height;

	      coords[0] = rect.x / (alloc.x2 - alloc.x1);
	      coords[1] = rect.y / (alloc.y2 - alloc.y1);
	      coords[2] = (rect.x + rect.width) / (alloc.x2 - alloc.x1);
	      coords[3] = (rect.y + rect.height) / (alloc.y2 - alloc.y1);

              coords[4] = coords[0];
              coords[5] = coords[1];
              coords[6] = coords[2];
              coords[7] = coords[3];

              cogl_rectangle_with_multitexture_coords (x1, y1, x2, y2,
                                                       &coords[0], 8);
            }

	  return;
	}
    }

  cogl_rectangle (0, 0,
		  alloc.x2 - alloc.x1,
		  alloc.y2 - alloc.y1);
}

static void
meta_shaped_texture_pick (ClutterActor       *actor,
			  const ClutterColor *color)
{
  MetaShapedTexture *stex = (MetaShapedTexture *) actor;
  MetaShapedTexturePrivate *priv = stex->priv;

  /* If there is no region then use the regular pick */
  if (priv->shape_region == NULL)
    CLUTTER_ACTOR_CLASS (meta_shaped_texture_parent_class)
      ->pick (actor, color);
  else if (clutter_actor_should_pick_paint (actor))
    {
      CoglHandle paint_tex;
      ClutterActorBox alloc;
      guint tex_width, tex_height;

      paint_tex = priv->texture;

      if (paint_tex == COGL_INVALID_HANDLE)
        return;

      tex_width = cogl_texture_get_width (paint_tex);
      tex_height = cogl_texture_get_height (paint_tex);

      if (tex_width == 0 || tex_height == 0) /* no contents yet */
        return;

      meta_shaped_texture_ensure_mask (stex);

      cogl_set_source_color4ub (color->red, color->green, color->blue,
                                 color->alpha);

      clutter_actor_get_allocation_box (actor, &alloc);

      /* Paint the mask rectangle in the given color */
      cogl_set_source_texture (priv->mask_texture);
      cogl_rectangle_with_texture_coords (0, 0,
                                          alloc.x2 - alloc.x1,
                                          alloc.y2 - alloc.y1,
                                          0, 0, 1, 1);
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
meta_shaped_texture_new (void)
{
  ClutterActor *self = g_object_new (META_TYPE_SHAPED_TEXTURE, NULL);

  return self;
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
      CoglHandle base_texture;
      priv->create_mipmaps = create_mipmaps;
      base_texture = create_mipmaps ?
        priv->texture : COGL_INVALID_HANDLE;
      meta_texture_tower_set_base_texture (priv->paint_tower, base_texture);
    }
}

void
meta_shaped_texture_set_shape_region (MetaShapedTexture *stex,
                                      cairo_region_t    *region)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  if (priv->shape_region != NULL)
    {
      cairo_region_destroy (priv->shape_region);
      priv->shape_region = NULL;
    }

  if (region != NULL)
    {
      cairo_region_reference (region);
      priv->shape_region = region;
    }

  meta_shaped_texture_dirty_mask (stex);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stex));
}

void
meta_shaped_texture_update_area (MetaShapedTexture *stex,
				 int                x,
				 int                y,
				 int                width,
				 int                height)
{
  MetaShapedTexturePrivate *priv;
  const cairo_rectangle_int_t clip = { x, y, width, height };

  priv = stex->priv;

  if (priv->texture == COGL_INVALID_HANDLE)
    return;

  cogl_texture_pixmap_x11_update_area (priv->texture, x, y, width, height);

  meta_texture_tower_update_area (priv->paint_tower, x, y, width, height);

  clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (stex), &clip);
}

static void
set_cogl_texture (MetaShapedTexture *stex,
                  CoglHandle         cogl_tex)
{
  MetaShapedTexturePrivate *priv;
  guint width, height;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  if (priv->texture != COGL_INVALID_HANDLE)
    cogl_handle_unref (priv->texture);

  priv->texture = cogl_tex;

  if (priv->material != COGL_INVALID_HANDLE)
    cogl_material_set_layer (priv->material, 0, cogl_tex);

  if (priv->material_unshaped != COGL_INVALID_HANDLE)
    cogl_material_set_layer (priv->material_unshaped, 0, cogl_tex);

  if (cogl_tex != COGL_INVALID_HANDLE)
    {
      width = cogl_texture_get_width (cogl_tex);
      height = cogl_texture_get_height (cogl_tex);

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

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stex));
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

  if (priv->pixmap == pixmap)
    return;

  priv->pixmap = pixmap;

  if (pixmap != None)
    {
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());
      set_cogl_texture (stex, cogl_texture_pixmap_x11_new (ctx, pixmap, FALSE, NULL));
    }
  else
    set_cogl_texture (stex, COGL_INVALID_HANDLE);

  if (priv->create_mipmaps)
    meta_texture_tower_set_base_texture (priv->paint_tower, priv->texture);
}

/**
 * meta_shaped_texture_get_texture:
 * @stex: The #MetaShapedTexture
 *
 * Returns: (transfer none): the unshaped texture
 */
CoglHandle
meta_shaped_texture_get_texture (MetaShapedTexture *stex)
{
  g_return_val_if_fail (META_IS_SHAPED_TEXTURE (stex), COGL_INVALID_HANDLE);
  return stex->priv->texture;
}

/**
 * meta_shaped_texture_set_overlay_path:
 * @stex: a #MetaShapedTexture
 * @overlay_region: A region containing the parts of the mask to overlay.
 *   All rectangles in this region are wiped clear to full transparency,
 *   and the overlay path is clipped to this region.
 * @overlay_path: (transfer full): This path will be painted onto the mask
 *   texture with a fully opaque source. Due to the lack of refcounting
 *   in #cairo_path_t, ownership of the path is assumed.
 */
void
meta_shaped_texture_set_overlay_path (MetaShapedTexture *stex,
                                      cairo_region_t    *overlay_region,
                                      cairo_path_t      *overlay_path)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  if (priv->overlay_region != NULL)
    {
      cairo_region_destroy (priv->overlay_region);
      priv->overlay_region = NULL;
    }

  if (priv->overlay_path != NULL)
    {
      cairo_path_destroy (priv->overlay_path);
      priv->overlay_path = NULL;
    }

  cairo_region_reference (overlay_region);
  priv->overlay_region = overlay_region;

  /* cairo_path_t does not have refcounting. */
  priv->overlay_path = overlay_path;

  meta_shaped_texture_dirty_mask (stex);
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
  CoglHandle texture, mask_texture;
  cairo_rectangle_int_t texture_rect = { 0, 0, 0, 0 };
  cairo_surface_t *surface;

  g_return_val_if_fail (META_IS_SHAPED_TEXTURE (stex), NULL);

  texture = stex->priv->texture;

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
  if (mask_texture != COGL_INVALID_HANDLE)
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
