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

#include "clutter-debug.h"
#include "clutter-private.h"

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

  if (priv->target != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->target);
      priv->target = COGL_INVALID_HANDLE;
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
update_fbo (ClutterEffect *effect)
{
  ClutterOffscreenEffect *self = CLUTTER_OFFSCREEN_EFFECT (effect);
  ClutterOffscreenEffectPrivate *priv = self->priv;
  gfloat width, height;
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

  /* the target should at least be big enough to contain the
   * transformed allocation of the actor
   *
   * FIXME - this is actually not enough: we need the paint area
   * to make this work reliably
   */
  clutter_actor_get_transformed_size (priv->actor, &width, &height);
  if (fabsf (priv->target_width - width) < 0.00001f &&
      fabsf (priv->target_height - height) < 0.0001f)
    return TRUE;

  if (priv->target != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (priv->target);
      cogl_handle_unref (priv->offscreen);
    }

  priv->target = cogl_material_new ();

  texture = clutter_offscreen_effect_create_texture (self, width, height);
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

static void
get_screen_offsets (ClutterActor *actor,
                    gfloat       *x_offset,
                    gfloat       *y_offset)
{
  ClutterVertex verts[4];
  gfloat x_min = G_MAXFLOAT, y_min = G_MAXFLOAT;
  gfloat v[4] = { 0, };
  gint i;

  /* Get the actors allocation transformed into screen coordinates.
   *
   * XXX: Note: this may not be a bounding box for the actor, since an
   * actor with depth may escape the box due to its perspective
   * projection. */
  clutter_actor_get_abs_allocation_vertices (actor, verts);

  for (i = 0; i < G_N_ELEMENTS (verts); ++i)
    {
      if (verts[i].x < x_min)
        x_min = verts[i].x;

      if (verts[i].y < y_min)
        y_min = verts[i].y;
    }

  /* XXX: It's not good enough to round by simply truncating the fraction here
   * via a cast, as it results in offscreen rendering being offset by 1 pixel
   * in many cases... */
#define ROUND(x) ((x) >= 0 ? (long)((x) + 0.5) : (long)((x) - 0.5))

  *x_offset = ROUND (x_min);
  *y_offset = ROUND (y_min);

#undef ROUND

  /* since we're setting up a viewport with a negative offset to paint
   * in an FBO with the same modelview and projection matrices as the
   * stage, we need to offset the computed absolute allocation vertices
   * with the current viewport's X and Y offsets. this works even with
   * the default case where the viewport is set up by Clutter to be
   * (0, 0, stage_width, stage_height)
   */
  cogl_get_viewport (v);
  *x_offset -= v[0];
  *y_offset -= v[1];
}

static gboolean
clutter_offscreen_effect_pre_paint (ClutterEffect *effect)
{
  ClutterOffscreenEffect *self = CLUTTER_OFFSCREEN_EFFECT (effect);
  ClutterOffscreenEffectPrivate *priv = self->priv;
  ClutterPerspective perspective;
  CoglColor transparent;
  CoglMatrix modelview;
  gfloat width, height;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  if (priv->actor == NULL)
    return FALSE;

  if (!update_fbo (effect))
    return FALSE;

  /* get the current modelview matrix so that we can copy it
   * on the new framebuffer
   */
  cogl_get_modelview_matrix (&modelview);

  clutter_stage_get_perspective (CLUTTER_STAGE (priv->stage), &perspective);
  clutter_actor_get_size (priv->stage, &width, &height);

  get_screen_offsets (priv->actor, &priv->x_offset, &priv->y_offset);

  /* let's draw offscreen */
  cogl_push_framebuffer (priv->offscreen);

  /* set up the viewport so that it has the same size of the stage,
   * and it has its origin at the same position of the stage's; also
   * set up the perspective to be the same as the stage's
   */
  cogl_set_viewport (-priv->x_offset, -priv->y_offset, width, height);
  cogl_perspective (perspective.fovy,
                    perspective.aspect,
                    perspective.z_near,
                    perspective.z_far);

  cogl_color_init_from_4ub (&transparent, 0, 0, 0, 0);
  cogl_clear (&transparent,
              COGL_BUFFER_BIT_COLOR |
              COGL_BUFFER_BIT_DEPTH);

  cogl_push_matrix ();

  cogl_set_modelview_matrix (&modelview);

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

  /* paint the texture at the same position as the actor would be,
   * in Stage coordinates, since we set up the modelview matrix to
   * be exactly as the stage sets it up, plus the eventual offsets
   * due to offscreen effects stacking
   */
  cogl_rectangle_with_texture_coords (0, 0,
                                      priv->target_width,
                                      priv->target_height,
                                      0.0, 0.0,
                                      1.0, 1.0);
}

static void
clutter_offscreen_effect_post_paint (ClutterEffect *effect)
{
  ClutterOffscreenEffect *self = CLUTTER_OFFSCREEN_EFFECT (effect);
  ClutterOffscreenEffectPrivate *priv = self->priv;
  ClutterPerspective perspective;
  CoglMatrix modelview, matrix;
  gfloat width, height;
  gfloat z_camera;

  if (priv->offscreen == COGL_INVALID_HANDLE ||
      priv->target == COGL_INVALID_HANDLE ||
      priv->actor == NULL)
    return;

  cogl_pop_matrix ();
  cogl_pop_framebuffer ();

  clutter_stage_get_perspective (CLUTTER_STAGE (priv->stage), &perspective);
  clutter_actor_get_size (priv->stage, &width, &height);

  cogl_get_modelview_matrix (&modelview);
  cogl_get_projection_matrix (&matrix);
  z_camera = 0.5f * matrix.xx;

  /* obliterate the current modelview matrix and reset it to be
   * the same as the stage's at the beginning of a paint run; this
   * is done to paint the target material in screen coordinates at
   * the same place as the actor would have been
   */
  cogl_matrix_init_identity (&matrix);
  cogl_matrix_translate (&matrix, -0.5f, -0.5f, -z_camera);
  cogl_matrix_scale (&matrix, 1.0f / width, -1.0f / height, 1.0f / width);
  cogl_matrix_translate (&matrix, 0.0f, -1.0f * height, 0.0f);
  cogl_set_modelview_matrix (&matrix);

  cogl_push_matrix ();

  cogl_translate (priv->x_offset, priv->y_offset, 0.0f);

  /* paint the target material; this is virtualized for
   * sub-classes that require special hand-holding
   */
  clutter_offscreen_effect_paint_target (self);

  cogl_pop_matrix ();

  /* reset the modelview matrix */
  cogl_set_modelview_matrix (&modelview);
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
 * Return value: a handle to a Cogl texture, or %COGL_INVALID_HANDLE
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
