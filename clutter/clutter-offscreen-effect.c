/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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
 *
 * Authors:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

/**
 * SECTION:clutter-offscreen-effect
 * @short_description: Base class for effects using offscreen buffers
 * @see_also: #ClutterBlurEffect, #ClutterEffect
 *
 * #ClutterOffscreenEffect is an abstract class that can be used by
 * #ClutterEffect sub-classes requiring access to an offscreen buffer.
 *
 * Some effects, like the fragment shader based effects, can only use GL
 * textures, and in order to apply those effects to any kind of actor they
 * require that all drawing operations are applied to an offscreen framebuffer
 * that gets redirected to a texture.
 *
 * #ClutterOffscreenEffect provides all the heavy-lifting for creating the
 * offscreen framebuffer, the redirection and the final paint of the texture on
 * the desired stage.
 *
 * <refsect2 id="ClutterOffscreenEffect-implementing">
 *   <title>Implementing a ClutterOffscreenEffect</title>
 *   <para>Creating a sub-class of #ClutterOffscreenEffect requires, in case
 *   of overriding the #ClutterEffect virtual functions, to chain up to the
 *   #ClutterOffscreenEffect's implementation.</para>
 *   <para>On top of the #ClutterEffect's virtual functions,
 *   #ClutterOffscreenEffect also provides a #ClutterOffscreenEffectClass.paint_target()
 *   function, which encapsulates the effective painting of the texture that
 *   contains the result of the offscreen redirection.</para>
 *   <para>The size of the target material is defined to be as big as the
 *   transformed size of the #ClutterActor using the offscreen effect.
 *   Sub-classes of #ClutterOffscreenEffect can change the texture creation
 *   code to provide bigger textures by overriding the
 *   #ClutterOffscreenEffectClass.create_texture() virtual function; no chain up
 *   to the #ClutterOffscreenEffect implementation is required in this
 *   case.</para>
 * </refsect2>
 *
 * #ClutterOffscreenEffect is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CLUTTER_ENABLE_EXPERIMENTAL_API

#include "clutter-offscreen-effect.h"

#include "cogl/cogl.h"

#include "clutter-actor-private.h"
#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

struct _ClutterOffscreenEffectPrivate
{
  CoglHandle offscreen;
  CoglPipeline *target;
  CoglHandle texture;

  ClutterActor *actor;
  ClutterActor *stage;

  gfloat x_offset;
  gfloat y_offset;

  /* This is the calculated size of the fbo before being passed
     through create_texture(). This needs to be tracked separately so
     that we can detect when a different size is calculated and
     regenerate the fbo */
  int fbo_width;
  int fbo_height;

  gint old_opacity_override;

  /* The matrix that was current the last time the fbo was updated. We
     need to keep track of this to detect when we can reuse the
     contents of the fbo without redrawing the actor. We need the
     actual matrix rather than just detecting queued redraws on the
     actor because any change in the parent hierarchy (even just a
     translation) could cause the actor to look completely different
     and it won't cause a redraw to be queued on the parent's
     children. */
  CoglMatrix last_matrix_drawn;
};

G_DEFINE_ABSTRACT_TYPE (ClutterOffscreenEffect,
                        clutter_offscreen_effect,
                        CLUTTER_TYPE_EFFECT);

static void
clutter_offscreen_effect_set_actor (ClutterActorMeta *meta,
                                    ClutterActor     *actor)
{
  ClutterOffscreenEffect *self = CLUTTER_OFFSCREEN_EFFECT (meta);
  ClutterOffscreenEffectPrivate *priv = self->priv;
  ClutterActorMetaClass *meta_class;

  meta_class = CLUTTER_ACTOR_META_CLASS (clutter_offscreen_effect_parent_class);
  meta_class->set_actor (meta, actor);

  /* clear out the previous state */
  if (priv->offscreen != NULL)
    {
      cogl_handle_unref (priv->offscreen);
      priv->offscreen = NULL;
    }

  /* we keep a back pointer here, to avoid going through the ActorMeta */
  priv->actor = clutter_actor_meta_get_actor (meta);
}

static CoglHandle
clutter_offscreen_effect_real_create_texture (ClutterOffscreenEffect *effect,
                                              gfloat                  width,
                                              gfloat                  height)
{
  CoglError *error = NULL;
  CoglHandle texture = cogl_texture_new_with_size (MAX (width, 1), MAX (height, 1),
                                                   COGL_TEXTURE_NO_SLICING,
                                                   COGL_PIXEL_FORMAT_RGBA_8888_PRE);

  if (!cogl_texture_allocate (texture, &error))
    {
#if CLUTTER_ENABLE_DEBUG
      g_warning ("Unable to allocate texture for offscreen effect: %s", error->message);
#endif /* CLUTTER_ENABLE_DEBUG */
      cogl_error_free (error);
      return NULL;
    }

  return texture;
}

static gboolean
update_fbo (ClutterEffect *effect, int fbo_width, int fbo_height)
{
  ClutterOffscreenEffect *self = CLUTTER_OFFSCREEN_EFFECT (effect);
  ClutterOffscreenEffectPrivate *priv = self->priv;

  priv->stage = clutter_actor_get_stage (priv->actor);
  if (priv->stage == NULL)
    {
      CLUTTER_NOTE (MISC, "The actor '%s' is not part of a stage",
                    clutter_actor_get_name (priv->actor) == NULL
                      ? G_OBJECT_TYPE_NAME (priv->actor)
                      : clutter_actor_get_name (priv->actor));
      return FALSE;
    }

  if (priv->fbo_width == fbo_width &&
      priv->fbo_height == fbo_height &&
      priv->offscreen != NULL)
    return TRUE;

  if (priv->target == NULL)
    {
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());

      priv->target = cogl_pipeline_new (ctx);

      /* We're always going to render the texture at a 1:1 texel:pixel
         ratio so we can use 'nearest' filtering to decrease the
         effects of rounding errors in the geometry calculation */
      cogl_pipeline_set_layer_filters (priv->target,
                                       0, /* layer_index */
                                       COGL_PIPELINE_FILTER_NEAREST,
                                       COGL_PIPELINE_FILTER_NEAREST);
    }

  if (priv->texture != NULL)
    {
      cogl_handle_unref (priv->texture);
      priv->texture = NULL;
    }

  priv->texture =
    clutter_offscreen_effect_create_texture (self, fbo_width, fbo_height);
  if (priv->texture == NULL)
    return FALSE;

  cogl_pipeline_set_layer_texture (priv->target, 0, priv->texture);

  priv->fbo_width = fbo_width;
  priv->fbo_height = fbo_height;

  if (priv->offscreen != NULL)
    cogl_handle_unref (priv->offscreen);

  priv->offscreen = cogl_offscreen_new_to_texture (priv->texture);
  if (priv->offscreen == NULL)
    {
      g_warning ("%s: Unable to create an Offscreen buffer", G_STRLOC);

      cogl_handle_unref (priv->target);
      priv->target = NULL;

      priv->fbo_width = 0;
      priv->fbo_height = 0;

      return FALSE;
    }

  return TRUE;
}

static gboolean
clutter_offscreen_effect_pre_paint (ClutterEffect *effect)
{
  ClutterOffscreenEffect *self = CLUTTER_OFFSCREEN_EFFECT (effect);
  ClutterOffscreenEffectPrivate *priv = self->priv;
  ClutterActorBox box;
  ClutterActor *stage;
  CoglMatrix projection;
  CoglColor transparent;
  gfloat stage_width, stage_height;
  gfloat fbo_width = -1, fbo_height = -1;
  gfloat width, height;
  gfloat xexpand, yexpand;
  int texture_width, texture_height;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  if (priv->actor == NULL)
    return FALSE;

  stage = _clutter_actor_get_stage_internal (priv->actor);
  clutter_actor_get_size (stage, &stage_width, &stage_height);

  /* The paint box is the bounding box of the actor's paint volume in
   * stage coordinates. This will give us the size for the framebuffer
   * we need to redirect its rendering offscreen and its position will
   * be used to setup an offset viewport */
  if (clutter_actor_get_paint_box (priv->actor, &box))
    {
      clutter_actor_box_get_size (&box, &fbo_width, &fbo_height);
      clutter_actor_box_get_origin (&box, &priv->x_offset, &priv->y_offset);

      fbo_width = MIN (fbo_width, stage_width);
      fbo_height = MIN (fbo_height, stage_height);
    }
  else
    {
      fbo_width = stage_width;
      fbo_height = stage_height;
    }

  if (fbo_width == stage_width)
    priv->x_offset = 0.0f;
  if (fbo_height == stage_height)
    priv->y_offset = 0.0f;

  /* First assert that the framebuffer is the right size... */
  if (!update_fbo (effect, fbo_width, fbo_height))
    return FALSE;

  texture_width = cogl_texture_get_width (priv->texture);
  texture_height = cogl_texture_get_height (priv->texture);

  /* get the current modelview matrix so that we can copy it to the
   * framebuffer. We also store the matrix that was last used when we
   * updated the FBO so that we can detect when we don't need to
   * update the FBO to paint a second time */
  cogl_get_modelview_matrix (&priv->last_matrix_drawn);

  /* let's draw offscreen */
  cogl_push_framebuffer (priv->offscreen);

  /* Copy the modelview that would have been used if rendering onscreen */
  cogl_set_modelview_matrix (&priv->last_matrix_drawn);

  /* Set up the viewport so that it has the same size as the stage,
   * but offset it so that the actor of interest lands on our
   * framebuffer. */
  clutter_actor_get_size (priv->stage, &width, &height);

  /* Expand the viewport if the actor is partially off-stage,
   * otherwise the actor will end up clipped to the stage viewport
   */
  xexpand = 0.f;
  if (priv->x_offset < 0.f)
    xexpand = -priv->x_offset;
  if (priv->x_offset + texture_width > width)
    xexpand = MAX (xexpand, (priv->x_offset + texture_width) - width);

  yexpand = 0.f;
  if (priv->y_offset < 0.f)
    yexpand = -priv->y_offset;
  if (priv->y_offset + texture_height > height)
    yexpand = MAX (yexpand, (priv->y_offset + texture_height) - height);

  /* Set the viewport */
  cogl_set_viewport (-(priv->x_offset + xexpand), -(priv->y_offset + yexpand),
                     width + (2 * xexpand), height + (2 * yexpand));

  /* Copy the stage's projection matrix across to the framebuffer */
  _clutter_stage_get_projection_matrix (CLUTTER_STAGE (priv->stage),
                                        &projection);

  /* If we've expanded the viewport, make sure to scale the projection
   * matrix accordingly (as it's been initialised to work with the
   * original viewport and not our expanded one).
   */
  if (xexpand > 0.f || yexpand > 0.f)
    {
      gfloat new_width, new_height;

      new_width = width + (2 * xexpand);
      new_height = height + (2 * yexpand);

      cogl_matrix_scale (&projection,
                         width / new_width,
                         height / new_height,
                         1);
    }

  cogl_set_projection_matrix (&projection);

  cogl_color_init_from_4ub (&transparent, 0, 0, 0, 0);
  cogl_clear (&transparent,
              COGL_BUFFER_BIT_COLOR |
              COGL_BUFFER_BIT_DEPTH);

  cogl_push_matrix ();

  /* Override the actor's opacity to fully opaque - we paint the offscreen
   * texture with the actor's paint opacity, so we need to do this to avoid
   * multiplying the opacity twice.
   */
  priv->old_opacity_override =
    _clutter_actor_get_opacity_override (priv->actor);
  _clutter_actor_set_opacity_override (priv->actor, 0xff);

  return TRUE;
}

static void
clutter_offscreen_effect_real_paint_target (ClutterOffscreenEffect *effect)
{
  ClutterOffscreenEffectPrivate *priv = effect->priv;
  guint8 paint_opacity;

  paint_opacity = clutter_actor_get_paint_opacity (priv->actor);

  cogl_pipeline_set_color4ub (priv->target,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);
  cogl_set_source (priv->target);

  /* At this point we are in stage coordinates translated so if
   * we draw our texture using a textured quad the size of the paint
   * box then we will overlay where the actor would have drawn if it
   * hadn't been redirected offscreen.
   */
  cogl_rectangle_with_texture_coords (0, 0,
                                      cogl_texture_get_width (priv->texture),
                                      cogl_texture_get_height (priv->texture),
                                      0.0, 0.0,
                                      1.0, 1.0);
}

static void
clutter_offscreen_effect_paint_texture (ClutterOffscreenEffect *effect)
{
  ClutterOffscreenEffectPrivate *priv = effect->priv;
  CoglMatrix modelview;

  cogl_push_matrix ();

  /* Now reset the modelview to put us in stage coordinates so
   * we can drawn the result of our offscreen render as a textured
   * quad... */

  cogl_matrix_init_identity (&modelview);
  _clutter_actor_apply_modelview_transform (priv->stage, &modelview);
  cogl_matrix_translate (&modelview, priv->x_offset, priv->y_offset, 0.0f);
  cogl_set_modelview_matrix (&modelview);

  /* paint the target material; this is virtualized for
   * sub-classes that require special hand-holding
   */
  clutter_offscreen_effect_paint_target (effect);

  cogl_pop_matrix ();
}

static void
clutter_offscreen_effect_post_paint (ClutterEffect *effect)
{
  ClutterOffscreenEffect *self = CLUTTER_OFFSCREEN_EFFECT (effect);
  ClutterOffscreenEffectPrivate *priv = self->priv;

  if (priv->offscreen == NULL ||
      priv->target == NULL ||
      priv->actor == NULL)
    return;

  /* Restore the previous opacity override */
  _clutter_actor_set_opacity_override (priv->actor, priv->old_opacity_override);

  cogl_pop_matrix ();
  cogl_pop_framebuffer ();

  clutter_offscreen_effect_paint_texture (self);
}

static void
clutter_offscreen_effect_paint (ClutterEffect           *effect,
                                ClutterEffectPaintFlags  flags)
{
  ClutterOffscreenEffect *self = CLUTTER_OFFSCREEN_EFFECT (effect);
  ClutterOffscreenEffectPrivate *priv = self->priv;
  CoglMatrix matrix;

  cogl_get_modelview_matrix (&matrix);

  /* If we've already got a cached image for the same matrix and the
     actor hasn't been redrawn then we can just use the cached image
     in the fbo */
  if (priv->offscreen == NULL ||
      (flags & CLUTTER_EFFECT_PAINT_ACTOR_DIRTY) ||
      !cogl_matrix_equal (&matrix, &priv->last_matrix_drawn))
    {
      /* Chain up to the parent paint method which will call the pre and
         post paint functions to update the image */
      CLUTTER_EFFECT_CLASS (clutter_offscreen_effect_parent_class)->
        paint (effect, flags);
    }
  else
    clutter_offscreen_effect_paint_texture (self);
}

static void
clutter_offscreen_effect_finalize (GObject *gobject)
{
  ClutterOffscreenEffect *self = CLUTTER_OFFSCREEN_EFFECT (gobject);
  ClutterOffscreenEffectPrivate *priv = self->priv;

  if (priv->offscreen)
    cogl_handle_unref (priv->offscreen);

  if (priv->target)
    cogl_handle_unref (priv->target);

  if (priv->texture)
    cogl_handle_unref (priv->texture);

  G_OBJECT_CLASS (clutter_offscreen_effect_parent_class)->finalize (gobject);
}

static void
clutter_offscreen_effect_class_init (ClutterOffscreenEffectClass *klass)
{
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterOffscreenEffectPrivate));

  klass->create_texture = clutter_offscreen_effect_real_create_texture;
  klass->paint_target = clutter_offscreen_effect_real_paint_target;

  meta_class->set_actor = clutter_offscreen_effect_set_actor;

  effect_class->pre_paint = clutter_offscreen_effect_pre_paint;
  effect_class->post_paint = clutter_offscreen_effect_post_paint;
  effect_class->paint = clutter_offscreen_effect_paint;

  gobject_class->finalize = clutter_offscreen_effect_finalize;
}

static void
clutter_offscreen_effect_init (ClutterOffscreenEffect *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            CLUTTER_TYPE_OFFSCREEN_EFFECT,
                                            ClutterOffscreenEffectPrivate);
}

/**
 * clutter_offscreen_effect_get_texture:
 * @effect: a #ClutterOffscreenEffect
 *
 * Retrieves the texture used as a render target for the offscreen
 * buffer created by @effect
 *
 * You should only use the returned texture when painting. The texture
 * may change after ClutterEffect::pre_paint is called so the effect
 * implementation should update any references to the texture after
 * chaining-up to the parent's pre_paint implementation. This can be
 * used instead of clutter_offscreen_effect_get_target() when the
 * effect subclass wants to paint using its own material.
 *
 * Return value: (transfer none): a #CoglHandle or %COGL_INVALID_HANDLE. The
 *   returned texture is owned by Clutter and it should not be
 *   modified or freed
 *
 * Since: 1.10
 */
CoglHandle
clutter_offscreen_effect_get_texture (ClutterOffscreenEffect *effect)
{
  g_return_val_if_fail (CLUTTER_IS_OFFSCREEN_EFFECT (effect),
                        NULL);

  return effect->priv->texture;
}

/**
 * clutter_offscreen_effect_get_target:
 * @effect: a #ClutterOffscreenEffect
 *
 * Retrieves the material used as a render target for the offscreen
 * buffer created by @effect
 *
 * You should only use the returned #CoglMaterial when painting. The
 * returned material might change between different frames.
 *
 * Return value: (transfer none): a #CoglMaterial or %NULL. The
 *   returned material is owned by Clutter and it should not be
 *   modified or freed
 *
 * Since: 1.4
 */
CoglMaterial *
clutter_offscreen_effect_get_target (ClutterOffscreenEffect *effect)
{
  g_return_val_if_fail (CLUTTER_IS_OFFSCREEN_EFFECT (effect),
                        NULL);

  return (CoglMaterial *)effect->priv->target;
}

/**
 * clutter_offscreen_effect_paint_target:
 * @effect: a #ClutterOffscreenEffect
 *
 * Calls the paint_target() virtual function of the @effect
 *
 * Since: 1.4
 */
void
clutter_offscreen_effect_paint_target (ClutterOffscreenEffect *effect)
{
  g_return_if_fail (CLUTTER_IS_OFFSCREEN_EFFECT (effect));

  CLUTTER_OFFSCREEN_EFFECT_GET_CLASS (effect)->paint_target (effect);
}

/**
 * clutter_offscreen_effect_create_texture:
 * @effect: a #ClutterOffscreenEffect
 * @width: the minimum width of the target texture
 * @height: the minimum height of the target texture
 *
 * Calls the create_texture() virtual function of the @effect
 *
 * Return value: (transfer full): a handle to a Cogl texture, or
 *   %COGL_INVALID_HANDLE. The returned handle has its reference
 *   count increased.
 *
 * Since: 1.4
 */
CoglHandle
clutter_offscreen_effect_create_texture (ClutterOffscreenEffect *effect,
                                         gfloat                  width,
                                         gfloat                  height)
{
  g_return_val_if_fail (CLUTTER_IS_OFFSCREEN_EFFECT (effect),
                        NULL);

  return CLUTTER_OFFSCREEN_EFFECT_GET_CLASS (effect)->create_texture (effect,
                                                                      width,
                                                                      height);
}

/**
 * clutter_offscreen_effect_get_target_size:
 * @effect: a #ClutterOffscreenEffect
 * @width: (out): return location for the target width, or %NULL
 * @height: (out): return location for the target height, or %NULL
 *
 * Retrieves the size of the offscreen buffer used by @effect to
 * paint the actor to which it has been applied.
 *
 * This function should only be called by #ClutterOffscreenEffect
 * implementations, from within the <function>paint_target()</function>
 * virtual function.
 *
 * Return value: %TRUE if the offscreen buffer has a valid size,
 *   and %FALSE otherwise
 *
 * Since: 1.8
 *
 * Deprecated: 1.14: Use clutter_offscreen_effect_get_target_rect() instead
 */
gboolean
clutter_offscreen_effect_get_target_size (ClutterOffscreenEffect *effect,
                                          gfloat                 *width,
                                          gfloat                 *height)
{
  ClutterOffscreenEffectPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_OFFSCREEN_EFFECT (effect), FALSE);

  priv = effect->priv;

  if (priv->texture == NULL)
    return FALSE;

  if (width)
    *width = cogl_texture_get_width (priv->texture);

  if (height)
    *height = cogl_texture_get_height (priv->texture);

  return TRUE;
}

/**
 * clutter_offscreen_effect_get_target_rect:
 * @effect: a #ClutterOffscreenEffect
 * @rect: (out caller-allocates): return location for the target area
 *
 * Retrieves the origin and size of the offscreen buffer used by @effect to
 * paint the actor to which it has been applied.
 *
 * This function should only be called by #ClutterOffscreenEffect
 * implementations, from within the <function>paint_target()</function>
 * virtual function.
 *
 * Return value: %TRUE if the offscreen buffer has a valid rectangle,
 *   and %FALSE otherwise
 *
 * Since: 1.14
 */
gboolean
clutter_offscreen_effect_get_target_rect (ClutterOffscreenEffect *effect,
                                          ClutterRect            *rect)
{
  ClutterOffscreenEffectPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_OFFSCREEN_EFFECT (effect), FALSE);
  g_return_val_if_fail (rect != NULL, FALSE);

  priv = effect->priv;

  if (priv->texture == NULL)
    return FALSE;

  clutter_rect_init (rect,
                     priv->x_offset,
                     priv->y_offset,
                     cogl_texture_get_width (priv->texture),
                     cogl_texture_get_height (priv->texture));

  return TRUE;
}
