/* tidy-texture-frame.h: Expandible texture actor
 *
 * Copyright (C) 2007 OpenedHand
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:tidy-texture-frame
 * @short_description: Stretch a texture to fit the entire allocation
 *
 * #TidyTextureFrame
 *
 */

#include <cogl/cogl.h>

#include "tidy-texture-frame.h"

#define TIDY_PARAM_READABLE     \
        (G_PARAM_READABLE |     \
         G_PARAM_STATIC_NICK | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB)

#define TIDY_PARAM_READWRITE    \
        (G_PARAM_READABLE | G_PARAM_WRITABLE | \
         G_PARAM_STATIC_NICK | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB)

enum
{
  PROP_0,
  PROP_LEFT,
  PROP_TOP,
  PROP_RIGHT,
  PROP_BOTTOM
};

G_DEFINE_TYPE (TidyTextureFrame,
	       tidy_texture_frame,
	       CLUTTER_TYPE_CLONE_TEXTURE);

#define TIDY_TEXTURE_FRAME_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), TIDY_TYPE_TEXTURE_FRAME, TidyTextureFramePrivate))

struct _TidyTextureFramePrivate
{
  gint left, top, right, bottom;
};

static void
tidy_texture_frame_paint (ClutterActor *self)
{
  TidyTextureFramePrivate *priv = TIDY_TEXTURE_FRAME (self)->priv;
  ClutterCloneTexture     *clone_texture = CLUTTER_CLONE_TEXTURE (self);
  ClutterTexture          *parent_texture;
  guint                    width, height;
  guint                    tex_width, tex_height;
  guint                    ex, ey;
  ClutterFixed             tx1, ty1, tx2, ty2;
  ClutterColor             col = { 0xff, 0xff, 0xff, 0xff };
  CoglHandle               cogl_texture;

  priv = TIDY_TEXTURE_FRAME (self)->priv;

  /* no need to paint stuff if we don't have a texture */
  parent_texture = clutter_clone_texture_get_parent_texture (clone_texture);
  if (!parent_texture)
    return;

  /* parent texture may have been hidden, so need to make sure it gets
   * realized
   */
  if (!CLUTTER_ACTOR_IS_REALIZED (parent_texture))
    clutter_actor_realize (CLUTTER_ACTOR (parent_texture));

  cogl_texture = clutter_texture_get_cogl_texture (parent_texture);
  if (cogl_texture == COGL_INVALID_HANDLE)
    return;

  cogl_push_matrix ();

  tex_width  = cogl_texture_get_width (cogl_texture);
  tex_height = cogl_texture_get_height (cogl_texture);

  clutter_actor_get_size (self, &width, &height);

  tx1 = CLUTTER_INT_TO_FIXED (priv->left) / tex_width;
  tx2 = CLUTTER_INT_TO_FIXED (tex_width - priv->right) / tex_width;
  ty1 = CLUTTER_INT_TO_FIXED (priv->top) / tex_height;
  ty2 = CLUTTER_INT_TO_FIXED (tex_height - priv->bottom) / tex_height;

  col.alpha = clutter_actor_get_paint_opacity (self);
  cogl_set_source_color4ub (col.red, col.green, col.blue, col.alpha);

  ex = width - priv->right;
  if (ex < 0)
    ex = priv->right; 		/* FIXME ? */

  ey = height - priv->bottom;
  if (ey < 0)
    ey = priv->bottom; 		/* FIXME ? */

#define FX(x) CLUTTER_INT_TO_FIXED(x)

  /* top left corner */
  cogl_set_source_texture (cogl_texture);
  cogl_rectangle_with_texture_coords (0,
                                      0,
                                      FX(priv->left), /* FIXME: clip if smaller */
                                      FX(priv->top),
                                      0,
                                      0,
                                      tx1,
                                      ty1);

  /* top middle */
  cogl_set_source_texture (cogl_texture);
  cogl_rectangle_with_texture_coords (FX(priv->left),
                                      0,
                                      FX(ex),
                                      FX(priv->top),
                                      tx1,
                                      0,
                                      tx2,
                                      ty1);

  /* top right */
  cogl_set_source_texture (cogl_texture);
  cogl_rectangle_with_texture_coords (FX(ex),
                                      0,
                                      FX(width),
                                      FX(priv->top),
                                      tx2,
                                      0,
                                      CFX_ONE,
                                      ty1);

  /* mid left */
  cogl_set_source_texture (cogl_texture);
  cogl_rectangle_with_texture_coords (
                          0, 
                          FX(priv->top),
                          FX(priv->left),
                          FX(ey),
                          0,
                          ty1,
                          tx1,
                          ty2);

  /* center */
  cogl_set_source_texture (cogl_texture);
  cogl_rectangle_with_texture_coords (
                          FX(priv->left),
                          FX(priv->top),
                          FX(ex),
                          FX(ey),
                          tx1,
                          ty1,
                          tx2,
                          ty2);

  /* mid right */
  cogl_set_source_texture (cogl_texture);
  cogl_rectangle_with_texture_coords (
                          FX(ex),
                          FX(priv->top),
                          FX(width),
                          FX(ey),
                          tx2,
                          ty1,
                          CFX_ONE,
                          ty2);
  
  /* bottom left */
  cogl_set_source_texture (cogl_texture);
  cogl_rectangle_with_texture_coords (
                          0, 
                          FX(ey),
                          FX(priv->left),
                          FX(height),
                          0,
                          ty2,
                          tx1,
                          CFX_ONE);

  /* bottom center */
  cogl_set_source_texture (cogl_texture);
  cogl_rectangle_with_texture_coords (
                          FX(priv->left),
                          FX(ey),
                          FX(ex),
                          FX(height),
                          tx1,
                          ty2,
                          tx2,
                          CFX_ONE);

  /* bottom right */
  cogl_set_source_texture (cogl_texture);
  cogl_rectangle_with_texture_coords (
                          FX(ex),
                          FX(ey),
                          FX(width),
                          FX(height),
                          tx2,
                          ty2,
                          CFX_ONE,
                          CFX_ONE);


  cogl_pop_matrix ();
}


static void
tidy_texture_frame_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
  TidyTextureFrame         *ctexture = TIDY_TEXTURE_FRAME (object);
  TidyTextureFramePrivate  *priv = ctexture->priv;

  switch (prop_id)
    {
    case PROP_LEFT:
      priv->left = g_value_get_int (value);
      break;
    case PROP_TOP:
      priv->top = g_value_get_int (value);
      break;
    case PROP_RIGHT:
      priv->right = g_value_get_int (value);
      break;
    case PROP_BOTTOM:
      priv->bottom = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
tidy_texture_frame_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
  TidyTextureFrame *ctexture = TIDY_TEXTURE_FRAME (object);
  TidyTextureFramePrivate  *priv = ctexture->priv;

  switch (prop_id)
    {
    case PROP_LEFT:
      g_value_set_int (value, priv->left);
      break;
    case PROP_TOP:
      g_value_set_int (value, priv->top);
      break;
    case PROP_RIGHT:
      g_value_set_int (value, priv->right);
      break;
    case PROP_BOTTOM:
      g_value_set_int (value, priv->bottom);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
tidy_texture_frame_class_init (TidyTextureFrameClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint = tidy_texture_frame_paint;

  gobject_class->set_property = tidy_texture_frame_set_property;
  gobject_class->get_property = tidy_texture_frame_get_property;

  g_object_class_install_property
            (gobject_class,
	     PROP_LEFT,
	     g_param_spec_int ("left",
			       "left",
			       "",
			       0, G_MAXINT,
			       0,
			       TIDY_PARAM_READWRITE));

  g_object_class_install_property
            (gobject_class,
	     PROP_TOP,
	     g_param_spec_int ("top",
			       "top",
			       "",
			       0, G_MAXINT,
			       0,
			       TIDY_PARAM_READWRITE));

  g_object_class_install_property
            (gobject_class,
	     PROP_BOTTOM,
	     g_param_spec_int ("bottom",
			       "bottom",
			       "",
			       0, G_MAXINT,
			       0,
			       TIDY_PARAM_READWRITE));

  g_object_class_install_property
            (gobject_class,
	     PROP_RIGHT,
	     g_param_spec_int ("right",
			       "right",
			       "",
			       0, G_MAXINT,
			       0,
			       TIDY_PARAM_READWRITE));

  g_type_class_add_private (gobject_class, sizeof (TidyTextureFramePrivate));
}

static void
tidy_texture_frame_init (TidyTextureFrame *self)
{
  TidyTextureFramePrivate *priv;

  self->priv = priv = TIDY_TEXTURE_FRAME_GET_PRIVATE (self);
}

ClutterActor *
tidy_texture_frame_new (ClutterTexture *texture,
			gint            left,
			gint            top,
			gint            right,
			gint            bottom)
{
  g_return_val_if_fail (texture == NULL || CLUTTER_IS_TEXTURE (texture), NULL);

  return g_object_new (TIDY_TYPE_TEXTURE_FRAME,
 		       "parent-texture", texture,
		       "left", left,
		       "top", top,
		       "right", right,
		       "bottom", bottom,
		       NULL);
}

