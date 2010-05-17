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
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
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
 * </refsect2>
 *
 * #ClutterOffscreenEffect is available since Clutter 1.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-offscreen-effect.h"

#include "cogl/cogl.h"

#include "clutter-debug.h"
#include "clutter-private.h"

struct _ClutterOffscreenEffectPrivate
{
  CoglHandle offscreen;
  CoglHandle target;

  ClutterActor *actor;
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
  ClutterPerspective perspective;
  gfloat width, height, z_camera;
  ClutterActorBox allocation;
  gfloat fb_width, fb_height;
  ClutterActor *stage;
  CoglHandle texture;
  CoglMatrix matrix;

  meta_class = CLUTTER_ACTOR_META_CLASS (clutter_offscreen_effect_parent_class);
  meta_class->set_actor (meta, actor);

  /* clear out the previous state */
  if (priv->offscreen != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->offscreen);
      priv->offscreen = COGL_INVALID_HANDLE;
    }

  if (priv->target != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->target);
      priv->target = COGL_INVALID_HANDLE;
    }

  /* we keep a back pointer here, to avoid going through the ActorMeta */
  priv->actor = clutter_actor_meta_get_actor (meta);
  if (priv->actor == NULL)
    return;

  stage = clutter_actor_get_stage (priv->actor);
  if (stage == NULL)
    {
      CLUTTER_NOTE (MISC, "The actor '%s' is not part of a stage",
                    clutter_actor_get_name (actor) == NULL
                      ? G_OBJECT_TYPE_NAME (actor)
                      : clutter_actor_get_name (actor));

      /* we forcibly disable the effect here */
      clutter_actor_meta_set_enabled (meta, FALSE);
      return;
    }

  clutter_stage_get_perspective (CLUTTER_STAGE (stage), &perspective);
  clutter_actor_get_allocation_box (stage, &allocation);
  clutter_actor_box_get_size (&allocation, &fb_width, &fb_height);

  clutter_actor_get_allocation_box (priv->actor, &allocation);
  clutter_actor_box_get_size (&allocation, &width, &height);

  priv->target = cogl_material_new ();

  texture = cogl_texture_new_with_size (MAX (width, 1), MAX (height, 1),
                                        COGL_TEXTURE_NO_SLICING,
                                        COGL_PIXEL_FORMAT_RGBA_8888_PRE);
  cogl_material_set_layer (priv->target, 0, texture);
  cogl_handle_unref (texture);

  cogl_material_set_layer_filters (priv->target, 0,
                                   COGL_MATERIAL_FILTER_LINEAR,
                                   COGL_MATERIAL_FILTER_LINEAR);

  priv->offscreen = cogl_offscreen_new_to_texture (texture);

  if (priv->offscreen == COGL_INVALID_HANDLE)
    {
      g_warning ("%s: Unable to create an Offscreen buffer", G_STRLOC);

      /* we forcibly disable the effect here */
      clutter_actor_meta_set_enabled (meta, FALSE);
      return;
    }

  cogl_push_framebuffer (priv->offscreen);

  width = cogl_texture_get_width (texture);
  height = cogl_texture_get_height (texture);
  fb_width /= fb_width / width;
  fb_height /= fb_height / height;

  cogl_set_viewport (0, 0, width, height);
  cogl_perspective (perspective.fovy,
                    perspective.aspect,
                    perspective.z_near,
                    perspective.z_far);

  cogl_get_projection_matrix (&matrix);
  z_camera = 0.5 * matrix.xx;

  cogl_matrix_init_identity (&matrix);
  cogl_matrix_translate (&matrix, -0.5f, -0.5f, -z_camera);
  cogl_matrix_scale (&matrix,
                      1.0f / fb_width,
                     -1.0f / fb_height,
                      1.0f / fb_width);
  cogl_matrix_translate (&matrix, 0.0f, -1.0f * fb_height, 0.0f);
  cogl_set_modelview_matrix (&matrix);

  cogl_pop_framebuffer ();
}

static gboolean
clutter_offscreen_effect_pre_paint (ClutterEffect *effect)
{
  ClutterOffscreenEffect *self = CLUTTER_OFFSCREEN_EFFECT (effect);
  ClutterOffscreenEffectPrivate *priv = self->priv;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  if (priv->offscreen != COGL_INVALID_HANDLE)
    {
      CoglColor transparent;

      cogl_push_framebuffer (priv->offscreen);
      cogl_push_matrix ();

      cogl_color_set_from_4ub (&transparent, 0, 0, 0, 0);
      cogl_clear (&transparent,
                  COGL_BUFFER_BIT_COLOR |
                  COGL_BUFFER_BIT_STENCIL |
                  COGL_BUFFER_BIT_DEPTH);

      return TRUE;
    }

  return FALSE;
}

static void
clutter_offscreen_effect_real_paint_target (ClutterOffscreenEffect *effect)
{
  ClutterOffscreenEffectPrivate *priv = effect->priv;
  ClutterActorBox allocation;
  gfloat width, height;
  guint8 paint_opacity;

  paint_opacity = clutter_actor_get_paint_opacity (priv->actor);

  clutter_actor_get_allocation_box (priv->actor, &allocation);
  clutter_actor_box_get_size (&allocation, &width, &height);

  cogl_material_set_color4ub (priv->target,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);
  cogl_set_source (priv->target);
  cogl_rectangle_with_texture_coords (0, 0, width, height,
                                      0.0, 0.0,
                                      1.0, 1.0);
}

static void
clutter_offscreen_effect_post_paint (ClutterEffect *effect)
{
  ClutterOffscreenEffect *self = CLUTTER_OFFSCREEN_EFFECT (effect);
  ClutterOffscreenEffectPrivate *priv = self->priv;

  if (priv->offscreen != COGL_INVALID_HANDLE &&
      priv->target != COGL_INVALID_HANDLE &&
      priv->actor != NULL)
    {
      cogl_pop_matrix ();
      cogl_pop_framebuffer ();

      /* paint the target material; this is virtualized for
       * sub-classes that require special hand-holding
       */
      clutter_offscreen_effect_paint_target (self);
    }
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

  klass->paint_target = clutter_offscreen_effect_real_paint_target;

  meta_class->set_actor = clutter_offscreen_effect_set_actor;

  effect_class->pre_paint = clutter_offscreen_effect_pre_paint;
  effect_class->post_paint = clutter_offscreen_effect_post_paint;

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
 * Return value: (transfer none): a handle for a #CoglMaterial, or
 *   %COGL_INVALID_HANDLE. The returned handle is owned by Clutter
 *   and it should not be modified or freed
 *
 * Since: 1.4
 */
CoglHandle
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
