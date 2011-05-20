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
 *   #ClutterOffscreenEffect also provides a <function>paint_target()</function>
 *   function, which encapsulates the effective painting of the texture that
 *   contains the result of the offscreen redirection.</para>
 *   <para>The size of the target material is defined to be as big as the
 *   transformed size of the #ClutterActor using the offscreen effect.
 *   Sub-classes of #ClutterOffscreenEffect can change the texture creation
 *   code to provide bigger textures by overriding the
 *   <function>create_texture()</function> virtual function; no chain up
 *   to the #ClutterOffscreenEffect implementation is required in this
 *   case.</para>
 * </refsect2>
 *
 * #ClutterOffscreenEffect is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-offscreen-effect.h"

#include "cogl/cogl.h"

#include "clutter-actor-private.h"
#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

struct _ClutterOffscreenEffectPrivate
{
  CoglHandle offscreen;
  CoglMaterial *target;

  ClutterActor *actor;
  ClutterActor *stage;

  gfloat x_offset;
  gfloat y_offset;

  gfloat target_width;
  gfloat target_height;

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
  if (priv->offscreen != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->offscreen);
      priv->offscreen = COGL_INVALID_HANDLE;
    }

  /* we keep a back pointer here, to avoid going through the ActorMeta */
  priv->actor = clutter_actor_meta_get_actor (meta);
}

static CoglHandle
clutter_offscreen_effect_real_create_texture (ClutterOffscreenEffect *effect,
                                              gfloat                  width,
                                              gfloat                  height)
{
  return cogl_texture_new_with_size (MAX (width, 1), MAX (height, 1),
                                     COGL_TEXTURE_NO_SLICING,
                                     COGL_PIXEL_FORMAT_RGBA_8888_PRE);
}

static gboolean
update_fbo (ClutterEffect *effect, int fbo_width, int fbo_height)
{
  ClutterOffscreenEffect *self = CLUTTER_OFFSCREEN_EFFECT (effect);
  ClutterOffscreenEffectPrivate *priv = self->priv;
  CoglHandle texture;

  priv->stage = clutter_actor_get_stage (priv->actor);
  if (priv->stage == NULL)
    {
      CLUTTER_NOTE (MISC, "The actor '%s' is not part of a stage",
                    clutter_actor_get_name (priv->actor) == NULL
                      ? G_OBJECT_TYPE_NAME (priv->actor)
                      : clutter_actor_get_name (priv->actor));
      return FALSE;
    }

  if (priv->target_width == fbo_width &&
      priv->target_height == fbo_height &&
      priv->offscreen != COGL_INVALID_HANDLE)
    return TRUE;

  if (priv->target == COGL_INVALID_HANDLE)
    {
      priv->target = cogl_material_new ();

      /* We're always going to render the texture at a 1:1 texel:pixel
         ratio so we can use 'nearest' filtering to decrease the
         effects of rounding errors in the geometry calculation */
      cogl_material_set_layer_filters (priv->target,
                                       0, /* layer_index */
                                       COGL_MATERIAL_FILTER_NEAREST,
                                       COGL_MATERIAL_FILTER_NEAREST);
    }

  texture =
    clutter_offscreen_effect_create_texture (self, fbo_width, fbo_height);
  if (texture == COGL_INVALID_HANDLE)
     return FALSE;

  cogl_material_set_layer (priv->target, 0, texture);
  cogl_handle_unref (texture);

  /* we need to use the size of the texture target and not the minimum
   * size we passed to the create_texture() vfunc, as any sub-class might
   * give use a bigger texture
   */
  priv->target_width = cogl_texture_get_width (texture);
  priv->target_height = cogl_texture_get_height (texture);

  if (priv->offscreen != COGL_INVALID_HANDLE)
    cogl_handle_unref (priv->offscreen);

  priv->offscreen = cogl_offscreen_new_to_texture (texture);
  if (priv->offscreen == COGL_INVALID_HANDLE)
    {
      g_warning ("%s: Unable to create an Offscreen buffer", G_STRLOC);

      cogl_handle_unref (priv->target);
      priv->target = COGL_INVALID_HANDLE;

      priv->target_width = 0;
      priv->target_height = 0;

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
  CoglMatrix projection;
  CoglColor transparent;
  CoglMatrix modelview;
  gfloat fbo_width, fbo_height;
  gfloat width, height;
  gfloat xexpand, yexpand;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  if (priv->actor == NULL)
    return FALSE;

  /* The paint box is the bounding box of the actor's paint volume in
   * stage coordinates. This will give us the size for the framebuffer
   * we need to redirect its rendering offscreen and its position will
   * be used to setup an offset viewport */
  if (clutter_actor_get_paint_box (priv->actor, &box))
    {
      clutter_actor_box_get_size (&box, &fbo_width, &fbo_height);
      clutter_actor_box_get_origin (&box, &priv->x_offset, &priv->y_offset);
    }
  else
    {
      /* If we can't get a valid paint box then we fallback to
       * creating a full stage size fbo. */
      ClutterActor *stage = _clutter_actor_get_stage_internal (priv->actor);
      clutter_actor_get_size (stage, &fbo_width, &fbo_height);
      priv->x_offset = 0.0f;
      priv->y_offset = 0.0f;
    }

  /* First assert that the framebuffer is the right size... */
  if (!update_fbo (effect, fbo_width, fbo_height))
    return FALSE;

  /* get the current modelview matrix so that we can copy it
   * to the framebuffer
   */
  cogl_get_modelview_matrix (&modelview);

  /* Store the matrix that was last used when we updated the FBO so
     that we can detect when we don't need to update the FBO to paint
     a second time */
  priv->last_matrix_drawn = modelview;

  /* let's draw offscreen */
  cogl_push_framebuffer (priv->offscreen);

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
  if (priv->x_offset + priv->target_width > width)
    xexpand = MAX (xexpand, (priv->x_offset + priv->target_width) - width);

  yexpand = 0.f;
  if (priv->y_offset < 0.f)
    yexpand = -priv->y_offset;
  if (priv->y_offset + priv->target_height > height)
    yexpand = MAX (yexpand, (priv->y_offset + priv->target_height) - height);

  /* Set the viewport */
  cogl_set_viewport (-(priv->x_offset + xexpand), -(priv->y_offset + yexpand),
                     width + (2 * xexpand), height + (2 * yexpand));

  /* Copy the stage's projection matrix across to the framebuffer */
  _clutter_stage_get_projection_matrix (CLUTTER_STAGE (priv->stage),
                                        &projection);
  cogl_set_projection_matrix (&projection);

  /* If we've expanded the viewport, make sure to scale the modelview
   * matrix accordingly (as it's been initialised to work with the
   * original viewport and not our expanded one).
   */
  if (xexpand > 0.f || yexpand > 0.f)
    {
      CoglMatrix correction;
      gfloat new_width, new_height;

      new_width = width + (2 * xexpand);
      new_height = height + (2 * yexpand);

      cogl_matrix_init_identity (&correction);
      cogl_matrix_scale (&correction,
                         width / new_width,
                         height / new_height,
                         1);

      cogl_matrix_multiply (&correction, &correction, &modelview);
      modelview = correction;
    }

  /* Copy the modelview that would have been used if rendering onscreen */
  cogl_set_modelview_matrix (&modelview);

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

  cogl_material_set_color4ub (priv->target,
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
                                      priv->target_width,
                                      priv->target_height,
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

  if (priv->offscreen == COGL_INVALID_HANDLE ||
      priv->target == COGL_INVALID_HANDLE ||
      priv->actor == NULL)
    return;

  /* Restore the previous opacity override */
  _clutter_actor_set_opacity_override (priv->actor, priv->old_opacity_override);

  cogl_pop_matrix ();
  cogl_pop_framebuffer ();

  clutter_offscreen_effect_paint_texture (self);
}

static void
clutter_offscreen_effect_run (ClutterEffect         *effect,
                              ClutterEffectRunFlags  flags)
{
  ClutterOffscreenEffect *self = CLUTTER_OFFSCREEN_EFFECT (effect);
  ClutterOffscreenEffectPrivate *priv = self->priv;
  CoglMatrix matrix;

  cogl_get_modelview_matrix (&matrix);

  /* If we've already got a cached image for the same matrix and the
     actor hasn't been redrawn then we can just use the cached image
     in the fbo */
  if (priv->offscreen == NULL ||
      (flags & CLUTTER_EFFECT_RUN_ACTOR_DIRTY) ||
      !cogl_matrix_equal (&matrix, &priv->last_matrix_drawn))
    {
      /* Chain up to the parent run method which will call the pre and
         post paint functions to update the image */
      CLUTTER_EFFECT_CLASS (clutter_offscreen_effect_parent_class)->
        run (effect, flags);
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
  effect_class->run = clutter_offscreen_effect_run;

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
                        COGL_INVALID_HANDLE);

  return effect->priv->target;
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
                        COGL_INVALID_HANDLE);

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
 */
gboolean
clutter_offscreen_effect_get_target_size (ClutterOffscreenEffect *effect,
                                          gfloat                 *width,
                                          gfloat                 *height)
{
  ClutterOffscreenEffectPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_OFFSCREEN_EFFECT (effect), FALSE);
  
  priv = effect->priv;

  if (priv->target == NULL)
    return FALSE;

  if (width)
    *width = priv->target_width;

  if (height)
    *height = priv->target_height;

  return TRUE;
}
