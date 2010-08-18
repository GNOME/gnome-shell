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

#include "mutter-shaped-texture.h"
#include "mutter-texture-tower.h"

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

static void mutter_shaped_texture_dispose (GObject *object);
static void mutter_shaped_texture_finalize (GObject *object);
static void mutter_shaped_texture_notify (GObject *object,
					  GParamSpec *pspec);

static void mutter_shaped_texture_paint (ClutterActor *actor);
static void mutter_shaped_texture_pick (ClutterActor *actor,
					const ClutterColor *color);

static void mutter_shaped_texture_update_area (ClutterX11TexturePixmap *texture,
                                               int                      x,
                                               int                      y,
                                               int                      width,
                                               int                      height);

static void mutter_shaped_texture_dirty_mask (MutterShapedTexture *stex);

#ifdef HAVE_GLX_TEXTURE_PIXMAP
G_DEFINE_TYPE (MutterShapedTexture, mutter_shaped_texture,
               CLUTTER_GLX_TYPE_TEXTURE_PIXMAP);
#else /* HAVE_GLX_TEXTURE_PIXMAP */
G_DEFINE_TYPE (MutterShapedTexture, mutter_shaped_texture,
               CLUTTER_X11_TYPE_TEXTURE_PIXMAP);
#endif /* HAVE_GLX_TEXTURE_PIXMAP */

#define MUTTER_SHAPED_TEXTURE_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MUTTER_TYPE_SHAPED_TEXTURE, \
                                MutterShapedTexturePrivate))

struct _MutterShapedTexturePrivate
{
  MutterTextureTower *paint_tower;
  CoglHandle mask_texture;
  CoglHandle material;
  CoglHandle material_unshaped;

  MetaRegion *clip_region;

  guint mask_width, mask_height;

  GArray *rectangles;

  guint create_mipmaps : 1;
};

static void
mutter_shaped_texture_class_init (MutterShapedTextureClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;
  ClutterX11TexturePixmapClass *x11_texture_class = (ClutterX11TexturePixmapClass *) klass;

  gobject_class->dispose = mutter_shaped_texture_dispose;
  gobject_class->finalize = mutter_shaped_texture_finalize;
  gobject_class->notify = mutter_shaped_texture_notify;

  actor_class->paint = mutter_shaped_texture_paint;
  actor_class->pick = mutter_shaped_texture_pick;

  x11_texture_class->update_area = mutter_shaped_texture_update_area;

  g_type_class_add_private (klass, sizeof (MutterShapedTexturePrivate));
}

static void
mutter_shaped_texture_init (MutterShapedTexture *self)
{
  MutterShapedTexturePrivate *priv;

  priv = self->priv = MUTTER_SHAPED_TEXTURE_GET_PRIVATE (self);

  priv->rectangles = g_array_new (FALSE, FALSE, sizeof (XRectangle));

  priv->paint_tower = mutter_texture_tower_new ();
  priv->mask_texture = COGL_INVALID_HANDLE;
  priv->create_mipmaps = TRUE;
}

static void
mutter_shaped_texture_dispose (GObject *object)
{
  MutterShapedTexture *self = (MutterShapedTexture *) object;
  MutterShapedTexturePrivate *priv = self->priv;

  if (priv->paint_tower)
    mutter_texture_tower_free (priv->paint_tower);
  priv->paint_tower = NULL;

  mutter_shaped_texture_dirty_mask (self);

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

  mutter_shaped_texture_set_clip_region (self, NULL);

  G_OBJECT_CLASS (mutter_shaped_texture_parent_class)->dispose (object);
}

static void
mutter_shaped_texture_finalize (GObject *object)
{
  MutterShapedTexture *self = (MutterShapedTexture *) object;
  MutterShapedTexturePrivate *priv = self->priv;

  g_array_free (priv->rectangles, TRUE);

  G_OBJECT_CLASS (mutter_shaped_texture_parent_class)->finalize (object);
}

static void
mutter_shaped_texture_notify (GObject    *object,
			      GParamSpec *pspec)
{
  if (G_OBJECT_CLASS (mutter_shaped_texture_parent_class)->notify)
    G_OBJECT_CLASS (mutter_shaped_texture_parent_class)->notify (object, pspec);

  /* It seems like we could just do this out of update_area(), but unfortunately,
   * clutter_glx_texture_pixmap() doesn't call through the vtable on the
   * initial update_area, so we need to look for changes to the texture
   * explicitly.
   */
  if (strcmp (pspec->name, "cogl-texture") == 0)
    {
      MutterShapedTexture *stex = (MutterShapedTexture *) object;
      MutterShapedTexturePrivate *priv = stex->priv;

      mutter_shaped_texture_clear (stex);

      if (priv->create_mipmaps)
	mutter_texture_tower_set_base_texture (priv->paint_tower,
					       clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (stex)));
    }
}

static void
mutter_shaped_texture_dirty_mask (MutterShapedTexture *stex)
{
  MutterShapedTexturePrivate *priv = stex->priv;

  if (priv->mask_texture != COGL_INVALID_HANDLE)
    {
      GLuint mask_gl_tex;
      GLenum mask_gl_target;

      cogl_texture_get_gl_texture (priv->mask_texture,
                                   &mask_gl_tex, &mask_gl_target);

      if (mask_gl_target == GL_TEXTURE_RECTANGLE_ARB)
        glDeleteTextures (1, &mask_gl_tex);

      cogl_handle_unref (priv->mask_texture);
      priv->mask_texture = COGL_INVALID_HANDLE;

      if (priv->material != COGL_INVALID_HANDLE)
        cogl_material_set_layer (priv->material, 1, COGL_INVALID_HANDLE);
    }
}

static void
mutter_shaped_texture_ensure_mask (MutterShapedTexture *stex)
{
  MutterShapedTexturePrivate *priv = stex->priv;
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
    mutter_shaped_texture_dirty_mask (stex);

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

      if (paint_gl_target == GL_TEXTURE_RECTANGLE_ARB)
        {
          GLuint tex;

          glGenTextures (1, &tex);
          glBindTexture (GL_TEXTURE_RECTANGLE_ARB, tex);
          glPixelStorei (GL_UNPACK_ROW_LENGTH, tex_width);
          glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
          glPixelStorei (GL_UNPACK_SKIP_ROWS, 0);
          glPixelStorei (GL_UNPACK_SKIP_PIXELS, 0);
          glTexImage2D (GL_TEXTURE_RECTANGLE_ARB, 0,
                        GL_ALPHA, tex_width, tex_height,
                        0, GL_ALPHA, GL_UNSIGNED_BYTE, mask_data);

          priv->mask_texture
            = cogl_texture_new_from_foreign (tex,
                                             GL_TEXTURE_RECTANGLE_ARB,
                                             tex_width, tex_height,
                                             0, 0,
                                             COGL_PIXEL_FORMAT_A_8);
        }
      else
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
mutter_shaped_texture_paint (ClutterActor *actor)
{
  MutterShapedTexture *stex = (MutterShapedTexture *) actor;
  MutterShapedTexturePrivate *priv = stex->priv;
  CoglHandle paint_tex;
  guint tex_width, tex_height;
  ClutterActorBox alloc;
  CoglHandle material;

  if (priv->clip_region && meta_region_is_empty (priv->clip_region))
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
    paint_tex = mutter_texture_tower_get_paint_texture (priv->paint_tower);
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
	priv->material_unshaped = cogl_material_new ();

      material = priv->material_unshaped;
    }
  else
    {
      mutter_shaped_texture_ensure_mask (stex);

      if (priv->material == COGL_INVALID_HANDLE)
	{
	  priv->material = cogl_material_new ();

	  cogl_material_set_layer_combine (priv->material, 1,
					   "RGBA = MODULATE (PREVIOUS, TEXTURE[A])",
					   NULL);
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
      GdkRectangle *rects;
      int n_rects;
      int i;

      /* Limit to how many separate rectangles we'll draw; beyond this just
       * fall back and draw the whole thing */
#     define MAX_RECTS 16

      /* Would be nice to be able to check the number of rects first */
      meta_region_get_rectangles (priv->clip_region, &rects, &n_rects);
      if (n_rects > MAX_RECTS)
	{
	  g_free (rects);
	  /* Fall through to following code */
	}
      else
	{
	  float coords[8];
          float x1, y1, x2, y2;

	  for (i = 0; i < n_rects; i++)
	    {
	      GdkRectangle *rect = &rects[i];

	      x1 = rect->x;
	      y1 = rect->y;
	      x2 = rect->x + rect->width;
	      y2 = rect->y + rect->height;

              coords[0] = rect->x / (alloc.x2 - alloc.x1);
	      coords[1] = rect->y / (alloc.y2 - alloc.y1);
	      coords[2] = (rect->x + rect->width) / (alloc.x2 - alloc.x1);
	      coords[3] = (rect->y + rect->height) / (alloc.y2 - alloc.y1);

              coords[4] = coords[0];
              coords[5] = coords[1];
              coords[6] = coords[2];
              coords[7] = coords[3];

              cogl_rectangle_with_multitexture_coords (x1, y1, x2, y2,
                                                       &coords[0], 8);
            }

	  g_free (rects);

	  return;
	}
    }

  cogl_rectangle (0, 0,
		  alloc.x2 - alloc.x1,
		  alloc.y2 - alloc.y1);
}

static void
mutter_shaped_texture_pick (ClutterActor *actor,
			    const ClutterColor *color)
{
  MutterShapedTexture *stex = (MutterShapedTexture *) actor;
  MutterShapedTexturePrivate *priv = stex->priv;

  /* If there are no rectangles then use the regular pick */
  if (priv->rectangles->len < 1)
    CLUTTER_ACTOR_CLASS (mutter_shaped_texture_parent_class)
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

      mutter_shaped_texture_ensure_mask (stex);

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
mutter_shaped_texture_update_area (ClutterX11TexturePixmap *texture,
                                   int                      x,
                                   int                      y,
                                   int                      width,
                                   int                      height)
{
  MutterShapedTexture *stex = (MutterShapedTexture *) texture;
  MutterShapedTexturePrivate *priv = stex->priv;

  CLUTTER_X11_TEXTURE_PIXMAP_CLASS (mutter_shaped_texture_parent_class)->update_area (texture,
                                                                                      x, y, width, height);

  mutter_texture_tower_update_area (priv->paint_tower, x, y, width, height);
}

ClutterActor *
mutter_shaped_texture_new (void)
{
  ClutterActor *self = g_object_new (MUTTER_TYPE_SHAPED_TEXTURE, NULL);

  return self;
}

void
mutter_shaped_texture_set_create_mipmaps (MutterShapedTexture *stex,
					  gboolean             create_mipmaps)
{
  MutterShapedTexturePrivate *priv;

  g_return_if_fail (MUTTER_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  create_mipmaps = create_mipmaps != FALSE;

  if (create_mipmaps != priv->create_mipmaps)
    {
      CoglHandle base_texture;

      priv->create_mipmaps = create_mipmaps;

      base_texture = create_mipmaps ?
	clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (stex)) : COGL_INVALID_HANDLE;

      mutter_texture_tower_set_base_texture (priv->paint_tower, base_texture);
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
mutter_shaped_texture_clear (MutterShapedTexture *stex)
{
  MutterShapedTexturePrivate *priv;

  g_return_if_fail (MUTTER_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  mutter_texture_tower_set_base_texture (priv->paint_tower, COGL_INVALID_HANDLE);

  if (priv->material != COGL_INVALID_HANDLE)
    cogl_material_set_layer (priv->material, 0, COGL_INVALID_HANDLE);

  if (priv->material_unshaped != COGL_INVALID_HANDLE)
    cogl_material_set_layer (priv->material_unshaped, 0, COGL_INVALID_HANDLE);
}

void
mutter_shaped_texture_clear_rectangles (MutterShapedTexture *stex)
{
  MutterShapedTexturePrivate *priv;

  g_return_if_fail (MUTTER_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  g_array_set_size (priv->rectangles, 0);
  mutter_shaped_texture_dirty_mask (stex);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stex));
}

void
mutter_shaped_texture_add_rectangle (MutterShapedTexture *stex,
				     const XRectangle *rect)
{
  g_return_if_fail (MUTTER_IS_SHAPED_TEXTURE (stex));

  mutter_shaped_texture_add_rectangles (stex, 1, rect);
}

void
mutter_shaped_texture_add_rectangles (MutterShapedTexture *stex,
				      size_t num_rects,
				      const XRectangle *rects)
{
  MutterShapedTexturePrivate *priv;

  g_return_if_fail (MUTTER_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  g_array_append_vals (priv->rectangles, rects, num_rects);

  mutter_shaped_texture_dirty_mask (stex);
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stex));
}

/**
 * mutter_shaped_texture_set_clip_region:
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
mutter_shaped_texture_set_clip_region (MutterShapedTexture *stex,
				       MetaRegion          *clip_region)
{
  MutterShapedTexturePrivate *priv;

  g_return_if_fail (MUTTER_IS_SHAPED_TEXTURE (stex));

  priv = stex->priv;

  if (priv->clip_region)
    {
      meta_region_destroy (priv->clip_region);
      priv->clip_region = NULL;
    }

  priv->clip_region = clip_region;
}
