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

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>


static void mutter_shaped_texture_dispose (GObject *object);
static void mutter_shaped_texture_finalize (GObject *object);

static void mutter_shaped_texture_paint (ClutterActor *actor);
static void mutter_shaped_texture_pick (ClutterActor *actor,
					const ClutterColor *color);

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
  CoglHandle mask_texture;
  CoglHandle material;
#if 1 /* see workaround comment in mutter_shaped_texture_paint */
  CoglHandle material_workaround;
#endif

  guint mask_width, mask_height;

  GArray *rectangles;
};

static void
mutter_shaped_texture_class_init (MutterShapedTextureClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  ClutterActorClass *actor_class = (ClutterActorClass *) klass;

  gobject_class->dispose = mutter_shaped_texture_dispose;
  gobject_class->finalize = mutter_shaped_texture_finalize;

  actor_class->paint = mutter_shaped_texture_paint;
  actor_class->pick = mutter_shaped_texture_pick;

  g_type_class_add_private (klass, sizeof (MutterShapedTexturePrivate));
}

static void
mutter_shaped_texture_init (MutterShapedTexture *self)
{
  MutterShapedTexturePrivate *priv;

  priv = self->priv = MUTTER_SHAPED_TEXTURE_GET_PRIVATE (self);

  priv->rectangles = g_array_new (FALSE, FALSE, sizeof (XRectangle));

  priv->mask_texture = COGL_INVALID_HANDLE;
}

static void
mutter_shaped_texture_dispose (GObject *object)
{
  MutterShapedTexture *self = (MutterShapedTexture *) object;
  MutterShapedTexturePrivate *priv = self->priv;

  mutter_shaped_texture_dirty_mask (self);

  if (priv->material != COGL_INVALID_HANDLE)
    {
      cogl_material_unref (priv->material);
      priv->material = COGL_INVALID_HANDLE;
    }
#if 1 /* see comment in mutter_shaped_texture_paint */
  if (priv->material_workaround != COGL_INVALID_HANDLE)
    {
      cogl_material_unref (priv->material_workaround);
      priv->material_workaround = COGL_INVALID_HANDLE;
    }
#endif

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
mutter_shaped_texture_dirty_mask (MutterShapedTexture *stex)
{
  MutterShapedTexturePrivate *priv = stex->priv;

  if (priv->mask_texture != COGL_INVALID_HANDLE)
    {
      GLuint mask_gl_tex;
      GLenum mask_gl_target;

      cogl_texture_get_gl_texture (priv->mask_texture,
                                   &mask_gl_tex, &mask_gl_target);

      if (mask_gl_target == CGL_TEXTURE_RECTANGLE_ARB)
        glDeleteTextures (1, &mask_gl_tex);

      cogl_texture_unref (priv->mask_texture);
      priv->mask_texture = COGL_INVALID_HANDLE;
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

      if (paint_gl_target == CGL_TEXTURE_RECTANGLE_ARB)
        {
          GLuint tex;

          glGenTextures (1, &tex);
          glBindTexture (CGL_TEXTURE_RECTANGLE_ARB, tex);
          glPixelStorei (GL_UNPACK_ROW_LENGTH, tex_width);
          glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
          glPixelStorei (GL_UNPACK_SKIP_ROWS, 0);
          glPixelStorei (GL_UNPACK_SKIP_PIXELS, 0);
          glTexImage2D (CGL_TEXTURE_RECTANGLE_ARB, 0,
                        GL_ALPHA, tex_width, tex_height,
                        0, GL_ALPHA, GL_UNSIGNED_BYTE, mask_data);

          priv->mask_texture
            = cogl_texture_new_from_foreign (tex,
                                             CGL_TEXTURE_RECTANGLE_ARB,
                                             tex_width, tex_height,
                                             0, 0,
                                             COGL_PIXEL_FORMAT_A_8);
        }
      else
        priv->mask_texture = cogl_texture_new_from_data (tex_width, tex_height,
                                                         -1, FALSE,
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
#if 1 /* please see comment below about workaround */
  guint depth;
#endif

  if (!CLUTTER_ACTOR_IS_REALIZED (CLUTTER_ACTOR (stex)))
    clutter_actor_realize (CLUTTER_ACTOR (stex));

  paint_tex = clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (stex));

  tex_width = cogl_texture_get_width (paint_tex);
  tex_height = cogl_texture_get_height (paint_tex);

  if (tex_width == 0 || tex_width == 0) /* no contents yet */
    return;

  /* If there are no rectangles fallback to the regular paint
     method */
  if (priv->rectangles->len < 1)
    {
      CLUTTER_ACTOR_CLASS (mutter_shaped_texture_parent_class)
        ->paint (actor);
      return;
    }

  if (paint_tex == COGL_INVALID_HANDLE)
    return;

  mutter_shaped_texture_ensure_mask (stex);

  if (priv->material == COGL_INVALID_HANDLE)
    {
      priv->material = cogl_material_new ();

      /* Replace the RGB from layer 1 with the RGB from layer 0 */
      cogl_material_set_layer_combine_function
        (priv->material, 1,
         COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB,
         COGL_MATERIAL_LAYER_COMBINE_FUNC_REPLACE);
      cogl_material_set_layer_combine_arg_src
        (priv->material, 1, 0,
         COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB,
         COGL_MATERIAL_LAYER_COMBINE_SRC_PREVIOUS);

      /* Modulate the alpha in layer 1 with the alpha from the
         previous layer */
      cogl_material_set_layer_combine_function
        (priv->material, 1,
         COGL_MATERIAL_LAYER_COMBINE_CHANNELS_ALPHA,
         COGL_MATERIAL_LAYER_COMBINE_FUNC_MODULATE);
      cogl_material_set_layer_combine_arg_src
        (priv->material, 1, 0,
         COGL_MATERIAL_LAYER_COMBINE_CHANNELS_ALPHA,
         COGL_MATERIAL_LAYER_COMBINE_SRC_PREVIOUS);
      cogl_material_set_layer_combine_arg_src
        (priv->material, 1, 1,
         COGL_MATERIAL_LAYER_COMBINE_CHANNELS_ALPHA,
         COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE);
    }
  material = priv->material;

#if 1
  /* This was added as a workaround. It seems that with the intel
   * drivers when multi-texturing using an RGB TFP texture, the
   * texture is actually setup internally as an RGBA texture, where
   * the alpha channel is mostly 0.0 so you only see a shimmer of the
   * window. This workaround forcibly defines the alpha channel as
   * 1.0. Maybe there is some clutter/cogl state that is interacting
   * with this that is being overlooked, but for now this seems to
   * work. */
  g_object_get (stex, "pixmap-depth", &depth, NULL);
  if (depth == 24)
    {
      if (priv->material_workaround == COGL_INVALID_HANDLE)
        {
          material = priv->material_workaround = cogl_material_new ();

          /* Replace the RGB from layer 1 with the RGB from layer 0 */
          cogl_material_set_layer_combine_function
            (material, 1,
             COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB,
             COGL_MATERIAL_LAYER_COMBINE_FUNC_REPLACE);
          cogl_material_set_layer_combine_arg_src
            (material, 1, 0,
             COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB,
             COGL_MATERIAL_LAYER_COMBINE_SRC_PREVIOUS);

          /* Use the alpha from layer 1 modulated with the alpha from
             the primary color */
          cogl_material_set_layer_combine_function
            (material, 1,
             COGL_MATERIAL_LAYER_COMBINE_CHANNELS_ALPHA,
             COGL_MATERIAL_LAYER_COMBINE_FUNC_MODULATE);
          cogl_material_set_layer_combine_arg_src
            (material, 1, 0,
             COGL_MATERIAL_LAYER_COMBINE_CHANNELS_ALPHA,
             COGL_MATERIAL_LAYER_COMBINE_SRC_PRIMARY_COLOR);
          cogl_material_set_layer_combine_arg_src
            (material, 1, 1,
             COGL_MATERIAL_LAYER_COMBINE_CHANNELS_ALPHA,
             COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE);
        }

      material = priv->material_workaround;
    }
#endif

  cogl_material_set_layer (material, 0, paint_tex);
  cogl_material_set_layer (material, 1, priv->mask_texture);

  {
    CoglColor color;
    cogl_color_set_from_4ub (&color, 255, 255, 255,
                             clutter_actor_get_paint_opacity (actor));
    cogl_material_set_color (material, &color);
  }

  cogl_set_source (material);

  clutter_actor_get_allocation_box (actor, &alloc);
  cogl_rectangle (0, 0,
                  CLUTTER_UNITS_TO_FLOAT (alloc.x2 - alloc.x1),
                  CLUTTER_UNITS_TO_FLOAT (alloc.y2 - alloc.y1));
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

      if (tex_width == 0 || tex_width == 0) /* no contents yet */
	return;

      mutter_shaped_texture_ensure_mask (stex);

      cogl_set_source_color4ub (color->red, color->green, color->blue,
                                 color->alpha);

      clutter_actor_get_allocation_box (actor, &alloc);

      /* Paint the mask rectangle in the given color */
      cogl_set_source_texture (priv->mask_texture);
      cogl_rectangle_with_texture_coords (0, 0,
                                          CLUTTER_UNITS_TO_FLOAT (alloc.x2 - alloc.x1),
                                          CLUTTER_UNITS_TO_FLOAT (alloc.y2 - alloc.y1),
                                          0, 0, 1, 1);
    }
}

ClutterActor *
mutter_shaped_texture_new (void)
{
  ClutterActor *self = g_object_new (MUTTER_TYPE_SHAPED_TEXTURE, NULL);

  return self;
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
