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

  PROP_PARENT_TEXTURE,

  PROP_LEFT,
  PROP_TOP,
  PROP_RIGHT,
  PROP_BOTTOM
};

G_DEFINE_TYPE (TidyTextureFrame, tidy_texture_frame, CLUTTER_TYPE_ACTOR);

#define TIDY_TEXTURE_FRAME_GET_PRIVATE(obj)     (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TIDY_TYPE_TEXTURE_FRAME, TidyTextureFramePrivate))

struct _TidyTextureFramePrivate
{
  ClutterTexture *parent_texture;

  gfloat left;
  gfloat top;
  gfloat right;
  gfloat bottom;

  CoglHandle material;

  guint needs_paint : 1;
};

static void
tidy_texture_frame_get_preferred_width (ClutterActor *self,
                                        gfloat        for_height,
                                        gfloat       *min_width_p,
                                        gfloat       *natural_width_p)
{
  TidyTextureFramePrivate *priv = TIDY_TEXTURE_FRAME (self)->priv;

  if (G_UNLIKELY (priv->parent_texture == NULL))
    {
      if (min_width_p)
        *min_width_p = 0;

      if (natural_width_p)
        *natural_width_p = 0;
    }
  else
    {
      ClutterActorClass *klass;

      /* by directly querying the parent texture's class implementation
       * we are going around any override mechanism the parent texture
       * might have in place, and we ask directly for the original
       * preferred width
       */
      klass = CLUTTER_ACTOR_GET_CLASS (priv->parent_texture);
      klass->get_preferred_width (CLUTTER_ACTOR (priv->parent_texture),
                                  for_height,
                                  min_width_p,
                                  natural_width_p);
    }
}

static void
tidy_texture_frame_get_preferred_height (ClutterActor *self,
                                         gfloat        for_width,
                                         gfloat       *min_height_p,
                                         gfloat       *natural_height_p)
{
  TidyTextureFramePrivate *priv = TIDY_TEXTURE_FRAME (self)->priv;

  if (G_UNLIKELY (priv->parent_texture == NULL))
    {
      if (min_height_p)
        *min_height_p = 0;

      if (natural_height_p)
        *natural_height_p = 0;
    }
  else
    {
      ClutterActorClass *klass;

      /* by directly querying the parent texture's class implementation
       * we are going around any override mechanism the parent texture
       * might have in place, and we ask directly for the original
       * preferred height
       */
      klass = CLUTTER_ACTOR_GET_CLASS (priv->parent_texture);
      klass->get_preferred_height (CLUTTER_ACTOR (priv->parent_texture),
                                   for_width,
                                   min_height_p,
                                   natural_height_p);
    }
}

static void
tidy_texture_frame_realize (ClutterActor *self)
{
  TidyTextureFramePrivate *priv = TIDY_TEXTURE_FRAME (self)->priv;

  if (priv->material != COGL_INVALID_HANDLE)
    return;

  priv->material = cogl_material_new ();

  CLUTTER_ACTOR_SET_FLAGS (self, CLUTTER_ACTOR_REALIZED);
}

static void
tidy_texture_frame_unrealize (ClutterActor *self)
{
  TidyTextureFramePrivate *priv = TIDY_TEXTURE_FRAME (self)->priv;

  if (priv->material == COGL_INVALID_HANDLE)
    return;

  cogl_material_unref (priv->material);
  priv->material = COGL_INVALID_HANDLE;

  CLUTTER_ACTOR_UNSET_FLAGS (self, CLUTTER_ACTOR_REALIZED);
}

static void
tidy_texture_frame_paint (ClutterActor *self)
{
  TidyTextureFramePrivate *priv = TIDY_TEXTURE_FRAME (self)->priv;
  CoglHandle cogl_texture = COGL_INVALID_HANDLE;
  ClutterActorBox box = { 0, };
  gfloat width, height;
  gfloat tex_width, tex_height;
  gfloat ex, ey;
  gfloat tx1, ty1, tx2, ty2;
  guint8 opacity;

  /* no need to paint stuff if we don't have a texture */
  if (G_UNLIKELY (priv->parent_texture == NULL))
    return;

  if (!priv->needs_paint)
    return;

  /* parent texture may have been hidden, so need to make sure it gets
   * realized
   */
  if (!CLUTTER_ACTOR_IS_REALIZED (priv->parent_texture))
    clutter_actor_realize (CLUTTER_ACTOR (priv->parent_texture));

  cogl_texture = clutter_texture_get_cogl_texture (priv->parent_texture);
  if (cogl_texture == COGL_INVALID_HANDLE)
    return;

  tex_width  = cogl_texture_get_width (cogl_texture);
  tex_height = cogl_texture_get_height (cogl_texture);

  clutter_actor_get_allocation_box (self, &box);
  width = box.x2 - box.x1;
  height = box.y2 - box.y1;

  tx1 = priv->left / tex_width;
  tx2 = (tex_width - priv->right) / tex_width;
  ty1 = priv->top / tex_height;
  ty2 = (tex_height - priv->bottom) / tex_height;

  ex = width - priv->right;
  if (ex < 0)
    ex = priv->right; 		/* FIXME ? */

  ey = height - priv->bottom;
  if (ey < 0)
    ey = priv->bottom; 		/* FIXME ? */

  opacity = clutter_actor_get_paint_opacity (self);

  g_assert (priv->material != COGL_INVALID_HANDLE);

  /* set the source material using the parent texture's COGL handle */
  cogl_material_set_color4ub (priv->material, opacity, opacity, opacity, opacity);
  cogl_material_set_layer (priv->material, 0, cogl_texture);
  cogl_set_source (priv->material);

  /* top left corner */
  cogl_rectangle_with_texture_coords (0, 0, priv->left, priv->top,
                                      0.0, 0.0,
                                      tx1, ty1);

  /* top middle */
  cogl_rectangle_with_texture_coords (priv->left, 0, ex, priv->top,
                                      tx1, 0.0,
                                      tx2, ty1);

  /* top right */
  cogl_rectangle_with_texture_coords (ex, 0, width, priv->top,
                                      tx2, 0.0,
                                      1.0, ty1);

  /* mid left */
  cogl_rectangle_with_texture_coords (0, priv->top, priv->left, ey,
                                      0.0, ty1,
                                      tx1, ty2);

  /* center */
  cogl_rectangle_with_texture_coords (priv->left, priv->top, ex, ey,
                                      tx1, ty1,
                                      tx2, ty2);

  /* mid right */
  cogl_rectangle_with_texture_coords (ex, priv->top, width, ey,
                                      tx2, ty1,
                                      1.0, ty2);
  
  /* bottom left */
  cogl_rectangle_with_texture_coords (0, ey, priv->left, height,
                                      0.0, ty2,
                                      tx1, 1.0);

  /* bottom center */
  cogl_rectangle_with_texture_coords (priv->left, ey, ex, height,
                                      tx1, ty2,
                                      tx2, 1.0);

  /* bottom right */
  cogl_rectangle_with_texture_coords (ex, ey, width, height,
                                      tx2, ty2,
                                      1.0, 1.0);
}

static inline void
tidy_texture_frame_set_frame_internal (TidyTextureFrame *frame,
                                       gfloat            left,
                                       gfloat            top,
                                       gfloat            right,
                                       gfloat            bottom)
{
  TidyTextureFramePrivate *priv = frame->priv;
  GObject *gobject = G_OBJECT (frame);
  gboolean changed = FALSE;

  g_object_freeze_notify (gobject);

  if (priv->top != top)
    {
      priv->top = top;
      g_object_notify (gobject, "top");
      changed = TRUE;
    }

  if (priv->right != right)
    {
      priv->right = right;
      g_object_notify (gobject, "right");
      changed = TRUE;
    }

  if (priv->bottom != bottom)
    {
      priv->bottom = bottom;
      g_object_notify (gobject, "bottom");
      changed = TRUE;
    }

  if (priv->left != left)
    {
      priv->left = left;
      g_object_notify (gobject, "left");
      changed = TRUE;
    }

  if (changed && CLUTTER_ACTOR_IS_VISIBLE (frame))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (frame));

  g_object_thaw_notify (gobject);
}

static void
tidy_texture_frame_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  TidyTextureFrame *frame = TIDY_TEXTURE_FRAME (gobject);
  TidyTextureFramePrivate *priv = frame->priv;

  switch (prop_id)
    {
    case PROP_PARENT_TEXTURE:
      tidy_texture_frame_set_parent_texture (frame,
                                             g_value_get_object (value));
      break;

    case PROP_TOP:
      tidy_texture_frame_set_frame_internal (frame,
                                             priv->left,
                                             g_value_get_float (value),
                                             priv->right,
                                             priv->bottom);
      break;

    case PROP_RIGHT:
      tidy_texture_frame_set_frame_internal (frame,
                                             priv->top,
                                             g_value_get_float (value),
                                             priv->bottom,
                                             priv->left);
      break;

    case PROP_BOTTOM:
      tidy_texture_frame_set_frame_internal (frame,
                                             priv->top,
                                             priv->right,
                                             g_value_get_float (value),
                                             priv->left);
      break;

    case PROP_LEFT:
      tidy_texture_frame_set_frame_internal (frame,
                                             priv->top,
                                             priv->right,
                                             priv->bottom,
                                             g_value_get_float (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tidy_texture_frame_get_property (GObject    *gobject,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  TidyTextureFramePrivate *priv = TIDY_TEXTURE_FRAME (gobject)->priv;

  switch (prop_id)
    {
    case PROP_PARENT_TEXTURE:
      g_value_set_object (value, priv->parent_texture);
      break;

    case PROP_LEFT:
      g_value_set_float (value, priv->left);
      break;

    case PROP_TOP:
      g_value_set_float (value, priv->top);
      break;

    case PROP_RIGHT:
      g_value_set_float (value, priv->right);
      break;

    case PROP_BOTTOM:
      g_value_set_float (value, priv->bottom);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tidy_texture_frame_dispose (GObject *gobject)
{
  TidyTextureFramePrivate *priv = TIDY_TEXTURE_FRAME (gobject)->priv;

  if (priv->parent_texture)
    {
      g_object_unref (priv->parent_texture);
      priv->parent_texture = NULL;
    }

  if (priv->material)
    {
      cogl_material_unref (priv->material);
      priv->material = COGL_INVALID_HANDLE;
    }

  G_OBJECT_CLASS (tidy_texture_frame_parent_class)->dispose (gobject);
}

static void
tidy_texture_frame_class_init (TidyTextureFrameClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (gobject_class, sizeof (TidyTextureFramePrivate));

  actor_class->get_preferred_width =
    tidy_texture_frame_get_preferred_width;
  actor_class->get_preferred_height =
    tidy_texture_frame_get_preferred_height;
  actor_class->realize = tidy_texture_frame_realize;
  actor_class->unrealize = tidy_texture_frame_unrealize;
  actor_class->paint = tidy_texture_frame_paint;

  gobject_class->set_property = tidy_texture_frame_set_property;
  gobject_class->get_property = tidy_texture_frame_get_property;
  gobject_class->dispose = tidy_texture_frame_dispose;

  pspec = g_param_spec_object ("parent-texture",
                               "Parent Texture",
                               "The parent ClutterTexture",
                               CLUTTER_TYPE_TEXTURE,
                               TIDY_PARAM_READWRITE |
                               G_PARAM_CONSTRUCT);
  g_object_class_install_property (gobject_class, PROP_PARENT_TEXTURE, pspec);

  pspec = g_param_spec_float ("left",
                              "Left",
                              "Left offset",
			      0, G_MAXFLOAT,
                              0,
                              TIDY_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_LEFT, pspec);

  pspec = g_param_spec_float ("top",
                              "Top",
                              "Top offset",
                              0, G_MAXFLOAT,
                              0,
                              TIDY_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TOP, pspec);

  pspec = g_param_spec_float ("bottom",
                              "Bottom",
                              "Bottom offset",
                              0, G_MAXFLOAT,
                              0,
                              TIDY_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_BOTTOM, pspec);

  pspec = g_param_spec_float ("right",
                              "Right",
                              "Right offset",
                              0, G_MAXFLOAT,
                              0,
                              TIDY_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_RIGHT, pspec);
}

static void
tidy_texture_frame_init (TidyTextureFrame *self)
{
  TidyTextureFramePrivate *priv;

  self->priv = priv = TIDY_TEXTURE_FRAME_GET_PRIVATE (self);

  priv->material = COGL_INVALID_HANDLE;
}

/**
 * tidy_texture_frame_new:
 * @texture: a #ClutterTexture or %NULL
 * @left: left margin preserving its content
 * @top: top margin preserving its content
 * @right: right margin preserving its content
 * @bottom: bottom margin preserving its content
 *
 * A #TidyTextureFrame is a specialized texture that efficiently clones
 * an area of the given @texture while keeping preserving portions of the
 * same texture.
 *
 * A #TidyTextureFrame can be used to make a rectangular texture fit a
 * given size without stretching its borders.
 *
 * Return value: the newly created #TidyTextureFrame
 */
ClutterActor*
tidy_texture_frame_new (ClutterTexture *texture, 
			gfloat          left,
			gfloat          top,
			gfloat          right,
			gfloat          bottom)
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

ClutterTexture *
tidy_texture_frame_get_parent_texture (TidyTextureFrame *frame)
{
  g_return_val_if_fail (TIDY_IS_TEXTURE_FRAME (frame), NULL);

  return frame->priv->parent_texture;
}

void
tidy_texture_frame_set_parent_texture (TidyTextureFrame *frame,
                                       ClutterTexture   *texture)
{
  TidyTextureFramePrivate *priv;
  gboolean was_visible;

  g_return_if_fail (TIDY_IS_TEXTURE_FRAME (frame));
  g_return_if_fail (texture == NULL || CLUTTER_IS_TEXTURE (texture));

  priv = frame->priv;

  was_visible = CLUTTER_ACTOR_IS_VISIBLE (frame);

  if (priv->parent_texture == texture)
    return;

  if (priv->parent_texture)
    {
      g_object_unref (priv->parent_texture);
      priv->parent_texture = NULL;

      if (was_visible)
        clutter_actor_hide (CLUTTER_ACTOR (frame));
    }

  if (texture)
    {
      priv->parent_texture = g_object_ref (texture);

      if (was_visible && CLUTTER_ACTOR_IS_VISIBLE (priv->parent_texture))
        clutter_actor_show (CLUTTER_ACTOR (frame));
    }

  clutter_actor_queue_relayout (CLUTTER_ACTOR (frame));

  g_object_notify (G_OBJECT (frame), "parent-texture");
}

void
tidy_texture_frame_set_frame (TidyTextureFrame *frame,
                              gfloat            top,
                              gfloat            right,
                              gfloat            bottom,
                              gfloat            left)
{
  g_return_if_fail (TIDY_IS_TEXTURE_FRAME (frame));

  tidy_texture_frame_set_frame_internal (frame, top, right, bottom, left);
}

void
tidy_texture_frame_get_frame (TidyTextureFrame *frame,
                              gfloat           *top,
                              gfloat           *right,
                              gfloat           *bottom,
                              gfloat           *left)
{
  TidyTextureFramePrivate *priv;

  g_return_if_fail (TIDY_IS_TEXTURE_FRAME (frame));

  priv = frame->priv;

  if (top)
    *top = priv->top;

  if (right)
    *right = priv->right;

  if (bottom)
    *bottom = priv->bottom;

  if (left)
    *left = priv->left;
}

/**
 * tidy_texture_frame_set_needs_paint:
 * @frame: a #TidyTextureframe
 * @needs_paint: if %FALSE, the paint will be skipped
 *
 * Provides a hint to the texture frame that it is totally obscured
 * and doesn't need to be painted. This would typically be called
 * by a parent container if it detects the condition prior to
 * painting its children and then unset afterwards.
 *
 * Since it is not supposed to have any effect on display, it does
 * not queue a repaint.
 */
void
tidy_texture_frame_set_needs_paint (TidyTextureFrame *frame,
				    gboolean          needs_paint)
{
  TidyTextureFramePrivate *priv;

  g_return_if_fail (TIDY_IS_TEXTURE_FRAME (frame));

  priv = frame->priv;

  priv->needs_paint = needs_paint;
}
