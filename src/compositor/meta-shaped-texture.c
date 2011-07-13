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

#include "meta-shaped-texture.h"
#include "meta-texture-tower.h"
#include "meta-texture-rectangle.h"

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

static void meta_shaped_texture_dispose  (GObject    *object);
static void meta_shaped_texture_finalize (GObject    *object);
static void meta_shaped_texture_notify   (GObject    *object,
					  GParamSpec *pspec);

static void meta_shaped_texture_paint (ClutterActor       *actor);
static void meta_shaped_texture_pick  (ClutterActor       *actor,
				       const ClutterColor *color);

static void meta_shaped_texture_update_area (ClutterX11TexturePixmap *texture,
					     int                      x,
					     int                      y,
					     int                      width,
					     int                      height);

static void meta_shaped_texture_dirty_mask (MetaShapedTexture *stex);

#ifdef HAVE_GLX_TEXTURE_PIXMAP
G_DEFINE_TYPE (MetaShapedTexture, meta_shaped_texture,
               CLUTTER_GLX_TYPE_TEXTURE_PIXMAP);
#else /* HAVE_GLX_TEXTURE_PIXMAP */
G_DEFINE_TYPE (MetaShapedTexture, meta_shaped_texture,
               CLUTTER_X11_TYPE_TEXTURE_PIXMAP);
#endif /* HAVE_GLX_TEXTURE_PIXMAP */

#define META_SHAPED_TEXTURE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), META_TYPE_SHAPED_TEXTURE, \
                                MetaShapedTexturePrivate))

struct _MetaShapedTexturePrivate
{
  MetaTextureTower *paint_tower;
  CoglHandle mask_texture;
  CoglHandle material;
  CoglHandle material_unshaped;

  cairo_region_t *clip_region;

  guint mask_width, mask_height;

  GArray *rectangles;

  guint create_mipmaps : 1;
};

static void
meta_shaped_texture_class_init (MetaShapedTextureClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;
  ClutterX11TexturePixmapClass *x11_texture_class = (ClutterX11TexturePixmapClass *) klass;

  gobject_class->dispose = meta_shaped_texture_dispose;
  gobject_class->finalize = meta_shaped_texture_finalize;
  gobject_class->notify = meta_shaped_texture_notify;

  actor_class->paint = meta_shaped_texture_paint;
  actor_class->pick = meta_shaped_texture_pick;

  x11_texture_class->update_area = meta_shaped_texture_update_area;

  g_type_class_add_private (klass, sizeof (MetaShapedTexturePrivate));
}

static void
meta_shaped_texture_init (MetaShapedTexture *self)
{
  MetaShapedTexturePrivate *priv;

  priv = self->priv = META_SHAPED_TEXTURE_GET_PRIVATE (self);

  priv->rectangles = g_array_new (FALSE, FALSE, sizeof (XRectangle));

  priv->paint_tower = meta_texture_tower_new ();
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

  meta_shaped_texture_set_clip_region (self, NULL);

  G_OBJECT_CLASS (meta_shaped_texture_parent_class)->dispose (object);
}

static void
meta_shaped_texture_finalize (GObject *object)
{
  MetaShapedTexture *self = (MetaShapedTexture *) object;
  MetaShapedTexturePrivate *priv = self->priv;

  g_array_free (priv->rectangles, TRUE);

  G_OBJECT_CLASS (meta_shaped_texture_parent_class)->finalize (object);
}

static void
meta_shaped_texture_notify (GObject    *object,
			    GParamSpec *pspec)
{
  if (G_OBJECT_CLASS (meta_shaped_texture_parent_class)->notify)
    G_OBJECT_CLASS (meta_shaped_texture_parent_class)->notify (object, pspec);

  /* It seems like we could just do this out of update_area(), but unfortunately,
   * clutter_glx_texture_pixmap() doesn't call through the vtable on the
   * initial update_area, so we need to look for changes to the texture
   * explicitly.
   */
  if (strcmp (pspec->name, "cogl-texture") == 0)
    {
      MetaShapedTexture *stex = (MetaShapedTexture *) object;
      MetaShapedTexturePrivate *priv = stex->priv;

      meta_shaped_texture_clear (stex);

      if (priv->create_mipmaps)
	meta_texture_tower_set_base_texture (priv->paint_tower,
					       clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (stex)));
    }
}

static void
meta_shaped_texture_dirty_mask (MetaShapedTexture *stex)
{
  MetaShapedTexturePrivate *priv = stex->priv;

  if (priv->mask_texture != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->mask_texture);
      priv->mask_texture = COGL_INVALID_HANDLE;

      if (priv->material != COGL_INVALID_HANDLE)
        cogl_material_set_layer (priv->material, 1, COGL_INVALID_HANDLE);
    }
}

static void
meta_shaped_texture_ensure_mask (MetaShapedTexture *stex)
{
  MetaShapedTexturePrivate *priv = stex->priv;
  CoglHandle paint_tex;
  guint tex_width, tex_height;

  paint_tex = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (stex));

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
      const XRectangle *rect;
      GLenum paint_gl_target;

      /* Create data for an empty image */
      mask_data = g_malloc0 (tex_width * tex_height);

      /* Cut out a hole for each rectangle */
      for (rect = (XRectangle *) priv->rectangles->data
             + priv->rectangles->len;
           rect-- > (XRectangle *) priv->rectangles->data;)
        {
          gint x1 = rect->x, x2 = x1 + rect->width;
          gint y1 = rect->y, y2 = y1 + rect->height;
          guchar *p;

          /* Clip the rectangle to the size of the texture */
          x1 = CLAMP (x1, 0, (gint) tex_width - 1);
          x2 = CLAMP (x2, x1, (gint) tex_width);
          y1 = CLAMP (y1, 0, (gint) tex_height - 1);
          y2 = CLAMP (y2, y1, (gint) tex_height);

          /* Fill the rectangle */
          for (p = mask_data + y1 * tex_width + x1;
               y1 < y2;
               y1++, p += tex_width)
            memset (p, 255, x2 - x1);
        }

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
                                          tex_width,
                                          mask_data);
        }
      else
#endif /* GL_TEXTURE_RECTANGLE_ARB */
        priv->mask_texture = cogl_texture_new_from_data (tex_width, tex_height,
                                                         COGL_TEXTURE_NONE,
                                                         COGL_PIXEL_FORMAT_A_8,
                                                         COGL_PIXEL_FORMAT_ANY,
                                                         tex_width,
                                                         mask_data);

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
    paint_tex = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (stex));

  if (paint_tex == COGL_INVALID_HANDLE)
    return;

  tex_width = cogl_texture_get_width (paint_tex);
  tex_height = cogl_texture_get_height (paint_tex);

  if (tex_width == 0 || tex_height == 0) /* no contents yet */
    return;

  if (priv->rectangles->len < 1)
    {
      /* If there are no rectangles use a single-layer texture */

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

  /* If there are no rectangles then use the regular pick */
  if (priv->rectangles->len < 1)
    CLUTTER_ACTOR_CLASS (meta_shaped_texture_parent_class)
      ->pick (actor, color);
  else if (clutter_actor_should_pick_paint (actor))
    {
      CoglHandle paint_tex;
      ClutterActorBox alloc;
      guint tex_width, tex_height;

      paint_tex = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (stex));

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
meta_shaped_texture_update_area (ClutterX11TexturePixmap *texture,
				 int                      x,
				 int                      y,
				 int                      width,
				 int                      height)
{
  MetaShapedTexture *stex = (MetaShapedTexture *) texture;
  MetaShapedTexturePrivate *priv = stex->priv;

  CLUTTER_X11_TEXTURE_PIXMAP_CLASS (meta_shaped_texture_parent_class)->update_area (texture,
                                                                                      x, y, width, height);

  meta_texture_tower_update_area (priv->paint_tower, x, y, width, height);
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
	clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (stex)) : COGL_INVALID_HANDLE;

      meta_texture_tower_set_base_texture (priv->paint_tower, base_texture);
    }
}

/* This is a workaround for deficiencies in the hack tower:
 *
 * When we call clutter_x11_texture_pixmap_set_pixmap(tp, None),
 * ClutterX11TexturePixmap knows that it has to get rid of the old texture, but
 * clutter_texture_set_cogl_texture(texture, COGL_INVALID_HANDLE) isn't allowed, so
 * it grabs the material for the texture and manually sets the texture in it. This means
 * that the "cogl-texture" property isn't notified, so we don't find out about it.
 *
 * And if we keep the CoglX11TexturePixmap around after the X pixmap is freed, then
 * we'll trigger X errors when we actually try to free it.
 *
 * The only correct thing to do here is to change our code to derive
 * from ClutterActor and get rid of the inheritance hack tower.  Once
 * we want to depend on Clutter-1.4 (which has CoglTexturePixmapX11),
 * that will be very easy to do.
 */
void
meta_shaped_texture_clear (MetaShapedTexture *stex)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  meta_texture_tower_set_base_texture (priv->paint_tower, COGL_INVALID_HANDLE);

  if (priv->material != COGL_INVALID_HANDLE)
    cogl_material_set_layer (priv->material, 0, COGL_INVALID_HANDLE);

  if (priv->material_unshaped != COGL_INVALID_HANDLE)
    cogl_material_set_layer (priv->material_unshaped, 0, COGL_INVALID_HANDLE);
}

void
meta_shaped_texture_clear_rectangles (MetaShapedTexture *stex)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  g_array_set_size (priv->rectangles, 0);
  meta_shaped_texture_dirty_mask (stex);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stex));
}

void
meta_shaped_texture_add_rectangle (MetaShapedTexture *stex,
				   const XRectangle  *rect)
{
  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  meta_shaped_texture_add_rectangles (stex, 1, rect);
}

void
meta_shaped_texture_add_rectangles (MetaShapedTexture *stex,
				    size_t             num_rects,
				    const XRectangle  *rects)
{
  MetaShapedTexturePrivate *priv;

  g_return_if_fail (META_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  g_array_append_vals (priv->rectangles, rects, num_rects);

  meta_shaped_texture_dirty_mask (stex);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stex));
}

/**
 * meta_shaped_texture_set_clip_region:
 * @frame: a #TidyTextureframe
 * @clip_region: (transfer full): the region of the texture that
 *   is visible and should be painted. OWNERSHIP IS ASSUMED BY
 *   THE FUNCTION (for efficiency to avoid a copy.)
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

  priv->clip_region = clip_region;
}
