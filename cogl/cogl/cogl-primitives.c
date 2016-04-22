/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-debug.h"
#include "cogl-context-private.h"
#include "cogl-journal-private.h"
#include "cogl-texture-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-vertex-buffer-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-attribute-private.h"
#include "cogl-private.h"
#include "cogl-meta-texture.h"
#include "cogl-framebuffer-private.h"
#include "cogl1-context.h"
#include "cogl-primitives-private.h"

#include <string.h>
#include <math.h>

#define _COGL_MAX_BEZ_RECURSE_DEPTH 16

typedef struct _TextureSlicedQuadState
{
  CoglFramebuffer *framebuffer;
  CoglPipeline *pipeline;
  CoglTexture *main_texture;
  float tex_virtual_origin_x;
  float tex_virtual_origin_y;
  float quad_origin_x;
  float quad_origin_y;
  float v_to_q_scale_x;
  float v_to_q_scale_y;
  float quad_len_x;
  float quad_len_y;
  CoglBool flipped_x;
  CoglBool flipped_y;
} TextureSlicedQuadState;

typedef struct _TextureSlicedPolygonState
{
  const CoglTextureVertex *vertices;
  int n_vertices;
  int stride;
  CoglAttribute **attributes;
} TextureSlicedPolygonState;

static void
log_quad_sub_textures_cb (CoglTexture *texture,
                          const float *subtexture_coords,
                          const float *virtual_coords,
                          void *user_data)
{
  TextureSlicedQuadState *state = user_data;
  CoglFramebuffer *framebuffer = state->framebuffer;
  CoglTexture *texture_override;
  float quad_coords[4];

#define TEX_VIRTUAL_TO_QUAD(V, Q, AXIS) \
    do { \
	Q = V - state->tex_virtual_origin_##AXIS; \
	Q *= state->v_to_q_scale_##AXIS; \
	if (state->flipped_##AXIS) \
	    Q = state->quad_len_##AXIS - Q; \
	Q += state->quad_origin_##AXIS; \
    } while (0);

  TEX_VIRTUAL_TO_QUAD (virtual_coords[0], quad_coords[0], x);
  TEX_VIRTUAL_TO_QUAD (virtual_coords[1], quad_coords[1], y);

  TEX_VIRTUAL_TO_QUAD (virtual_coords[2], quad_coords[2], x);
  TEX_VIRTUAL_TO_QUAD (virtual_coords[3], quad_coords[3], y);

#undef TEX_VIRTUAL_TO_QUAD

  COGL_NOTE (DRAW,
             "~~~~~ slice\n"
             "qx1: %f\t"
             "qy1: %f\n"
             "qx2: %f\t"
             "qy2: %f\n"
             "tx1: %f\t"
             "ty1: %f\n"
             "tx2: %f\t"
             "ty2: %f\n",
             quad_coords[0], quad_coords[1],
             quad_coords[2], quad_coords[3],
             subtexture_coords[0], subtexture_coords[1],
             subtexture_coords[2], subtexture_coords[3]);

  /* We only need to override the texture if it's different from the
     main texture */
  if (texture == state->main_texture)
    texture_override = NULL;
  else
    texture_override = texture;

  _cogl_journal_log_quad (framebuffer->journal,
                          quad_coords,
                          state->pipeline,
                          1, /* one layer */
                          texture_override, /* replace the layer0 texture */
                          subtexture_coords,
                          4);
}

typedef struct _ValidateFirstLayerState
{
  CoglPipeline *override_pipeline;
} ValidateFirstLayerState;

static CoglBool
validate_first_layer_cb (CoglPipeline *pipeline,
                         int layer_index,
                         void *user_data)
{
  ValidateFirstLayerState *state = user_data;
  CoglPipelineWrapMode clamp_to_edge =
    COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE;
  CoglPipelineWrapMode wrap_s;
  CoglPipelineWrapMode wrap_t;

  /* We can't use hardware repeat so we need to set clamp to edge
   * otherwise it might pull in edge pixels from the other side. By
   * default WRAP_MODE_AUTOMATIC becomes CLAMP_TO_EDGE so we only need
   * to override if the wrap mode isn't already automatic or
   * clamp_to_edge.
   */
  wrap_s = cogl_pipeline_get_layer_wrap_mode_s (pipeline, layer_index);
  if (wrap_s != COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE &&
      wrap_s != COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
    {
      if (!state->override_pipeline)
        state->override_pipeline = cogl_pipeline_copy (pipeline);
      cogl_pipeline_set_layer_wrap_mode_s (state->override_pipeline,
                                           layer_index, clamp_to_edge);
    }

  wrap_t = cogl_pipeline_get_layer_wrap_mode_t (pipeline, layer_index);
  if (wrap_t != COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE &&
      wrap_t != COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
    {
      if (!state->override_pipeline)
        state->override_pipeline = cogl_pipeline_copy (pipeline);
      cogl_pipeline_set_layer_wrap_mode_t (state->override_pipeline,
                                           layer_index, clamp_to_edge);
    }

  return FALSE;
}

/* This path doesn't currently support multitexturing but is used for
 * CoglTextures that don't support repeating using the GPU so we need to
 * manually emit extra geometry to fake the repeating. This includes:
 *
 * - CoglTexture2DSliced: when made of > 1 slice or if the users given
 *   texture coordinates require repeating,
 * - CoglTexture2DAtlas: if the users given texture coordinates require
 *   repeating,
 * - CoglTextureRectangle: if the users given texture coordinates require
 *   repeating,
 * - CoglTexturePixmap: if the users given texture coordinates require
 *   repeating
 */
/* TODO: support multitexturing */
static void
_cogl_texture_quad_multiple_primitives (CoglFramebuffer *framebuffer,
                                        CoglPipeline *pipeline,
                                        CoglTexture *texture,
                                        int layer_index,
                                        const float *position,
                                        float tx_1,
                                        float ty_1,
                                        float tx_2,
                                        float ty_2)
{
  TextureSlicedQuadState state;
  CoglBool tex_virtual_flipped_x;
  CoglBool tex_virtual_flipped_y;
  CoglBool quad_flipped_x;
  CoglBool quad_flipped_y;
  ValidateFirstLayerState validate_first_layer_state;
  CoglPipelineWrapMode wrap_s, wrap_t;

  wrap_s = cogl_pipeline_get_layer_wrap_mode_s (pipeline, layer_index);
  wrap_t = cogl_pipeline_get_layer_wrap_mode_t (pipeline, layer_index);

  validate_first_layer_state.override_pipeline = NULL;
  cogl_pipeline_foreach_layer (pipeline,
                               validate_first_layer_cb,
                               &validate_first_layer_state);

  state.framebuffer = framebuffer;
  state.main_texture = texture;

  if (validate_first_layer_state.override_pipeline)
    state.pipeline = validate_first_layer_state.override_pipeline;
  else
    state.pipeline = pipeline;

  /* Get together the data we need to transform the virtual texture
   * coordinates of each slice into quad coordinates...
   *
   * NB: We need to consider that the quad coordinates and the texture
   * coordinates may be inverted along the x or y axis, and must preserve the
   * inversions when we emit the final geometry.
   */

#define X0 0
#define Y0 1
#define X1 2
#define Y1 3

  tex_virtual_flipped_x = (tx_1 > tx_2) ? TRUE : FALSE;
  tex_virtual_flipped_y = (ty_1 > ty_2) ? TRUE : FALSE;
  state.tex_virtual_origin_x = tex_virtual_flipped_x ? tx_2 : tx_1;
  state.tex_virtual_origin_y = tex_virtual_flipped_y ? ty_2 : ty_1;

  quad_flipped_x = (position[X0] > position[X1]) ? TRUE : FALSE;
  quad_flipped_y = (position[Y0] > position[Y1]) ? TRUE : FALSE;
  state.quad_origin_x = quad_flipped_x ? position[X1] : position[X0];
  state.quad_origin_y = quad_flipped_y ? position[Y1] : position[Y0];

  /* flatten the two forms of coordinate inversion into one... */
  state.flipped_x = tex_virtual_flipped_x ^ quad_flipped_x;
  state.flipped_y = tex_virtual_flipped_y ^ quad_flipped_y;

  /* We use the _len_AXIS naming here instead of _width and _height because
   * log_quad_slice_cb uses a macro with symbol concatenation to handle both
   * axis, so this is more convenient... */
  state.quad_len_x = fabs (position[X1] - position[X0]);
  state.quad_len_y = fabs (position[Y1] - position[Y0]);

#undef X0
#undef Y0
#undef X1
#undef Y1

  state.v_to_q_scale_x = fabs (state.quad_len_x / (tx_2 - tx_1));
  state.v_to_q_scale_y = fabs (state.quad_len_y / (ty_2 - ty_1));

  /* For backwards compatablity the default wrap mode for cogl_rectangle() is
   * _REPEAT... */
  if (wrap_s == COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
    wrap_s = COGL_PIPELINE_WRAP_MODE_REPEAT;
  if (wrap_t == COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
    wrap_t = COGL_PIPELINE_WRAP_MODE_REPEAT;

  cogl_meta_texture_foreach_in_region (COGL_META_TEXTURE (texture),
                                       tx_1, ty_1, tx_2, ty_2,
                                       wrap_s,
                                       wrap_t,
                                       log_quad_sub_textures_cb,
                                       &state);

  if (validate_first_layer_state.override_pipeline)
    cogl_object_unref (validate_first_layer_state.override_pipeline);
}

typedef struct _ValidateTexCoordsState
{
  int i;
  int n_layers;
  const float *user_tex_coords;
  int user_tex_coords_len;
  float *final_tex_coords;
  CoglPipeline *override_pipeline;
  CoglBool needs_multiple_primitives;
} ValidateTexCoordsState;

/*
 * Validate the texture coordinates for this rectangle.
 */
static CoglBool
validate_tex_coords_cb (CoglPipeline *pipeline,
                        int layer_index,
                        void *user_data)
{
  ValidateTexCoordsState *state = user_data;
  CoglTexture *texture;
  const float *in_tex_coords;
  float *out_tex_coords;
  float default_tex_coords[4] = {0.0, 0.0, 1.0, 1.0};
  CoglTransformResult transform_result;

  state->i++;

  /* FIXME: we should be able to avoid this copying when no
   * transform is required by the texture backend and the user
   * has supplied enough coordinates for all the layers.
   */

  /* If the user didn't supply texture coordinates for this layer
     then use the default coords */
  if (state->i >= state->user_tex_coords_len / 4)
    in_tex_coords = default_tex_coords;
  else
    in_tex_coords = &state->user_tex_coords[state->i * 4];

  out_tex_coords = &state->final_tex_coords[state->i * 4];

  memcpy (out_tex_coords, in_tex_coords, sizeof (float) * 4);

  texture = cogl_pipeline_get_layer_texture (pipeline, layer_index);

  /* NB: NULL textures are handled by _cogl_pipeline_flush_gl_state */
  if (!texture)
    return TRUE;

  /* Convert the texture coordinates to GL.
   */
  transform_result =
    _cogl_texture_transform_quad_coords_to_gl (texture,
                                               out_tex_coords);
  /* If the texture has waste or we are using GL_TEXTURE_RECT we
   * can't handle texture repeating so we can't use the layer if
   * repeating is required.
   *
   * NB: We already know that no texture matrix is being used if the
   * texture doesn't support hardware repeat.
   */
  if (transform_result == COGL_TRANSFORM_SOFTWARE_REPEAT)
    {
      if (state->i == 0)
        {
          if (state->n_layers > 1)
            {
              static CoglBool warning_seen = FALSE;
              if (!warning_seen)
                g_warning ("Skipping layers 1..n of your material since "
                           "the first layer doesn't support hardware "
                           "repeat (e.g. because of waste or use of "
                           "GL_TEXTURE_RECTANGLE_ARB) and you supplied "
                           "texture coordinates outside the range [0,1]."
                           "Falling back to software repeat assuming "
                           "layer 0 is the most important one keep");
              warning_seen = TRUE;
            }

          if (state->override_pipeline)
            cogl_object_unref (state->override_pipeline);
          state->needs_multiple_primitives = TRUE;
          return FALSE;
        }
      else
        {
          static CoglBool warning_seen = FALSE;
          if (!warning_seen)
            g_warning ("Skipping layer %d of your material "
                       "since you have supplied texture coords "
                       "outside the range [0,1] but the texture "
                       "doesn't support hardware repeat (e.g. "
                       "because of waste or use of "
                       "GL_TEXTURE_RECTANGLE_ARB). This isn't "
                       "supported with multi-texturing.", state->i);
          warning_seen = TRUE;

          cogl_pipeline_set_layer_texture (pipeline, layer_index, NULL);
        }
    }

  /* By default WRAP_MODE_AUTOMATIC becomes to CLAMP_TO_EDGE. If
     the texture coordinates need repeating then we'll override
     this to GL_REPEAT. Otherwise we'll leave it at CLAMP_TO_EDGE
     so that it won't blend in pixels from the opposite side when
     the full texture is drawn with GL_LINEAR filter mode */
  if (transform_result == COGL_TRANSFORM_HARDWARE_REPEAT)
    {
      if (cogl_pipeline_get_layer_wrap_mode_s (pipeline, layer_index) ==
          COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
        {
          if (!state->override_pipeline)
            state->override_pipeline = cogl_pipeline_copy (pipeline);
          cogl_pipeline_set_layer_wrap_mode_s (state->override_pipeline,
                                               layer_index,
                                               COGL_PIPELINE_WRAP_MODE_REPEAT);
        }
      if (cogl_pipeline_get_layer_wrap_mode_t (pipeline, layer_index) ==
          COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
        {
          if (!state->override_pipeline)
            state->override_pipeline = cogl_pipeline_copy (pipeline);
          cogl_pipeline_set_layer_wrap_mode_t (state->override_pipeline,
                                               layer_index,
                                               COGL_PIPELINE_WRAP_MODE_REPEAT);
        }
    }

  return TRUE;
}

/* This path supports multitexturing but only when each of the layers is
 * handled with a single GL texture. Also if repeating is necessary then
 * _cogl_texture_can_hardware_repeat() must return TRUE.
 * This includes layers made from:
 *
 * - CoglTexture2DSliced: if only comprised of a single slice with optional
 *   waste, assuming the users given texture coordinates don't require
 *   repeating.
 * - CoglTexture{1D,2D,3D}: always.
 * - CoglTexture2DAtlas: assuming the users given texture coordinates don't
 *   require repeating.
 * - CoglTextureRectangle: assuming the users given texture coordinates don't
 *   require repeating.
 * - CoglTexturePixmap: assuming the users given texture coordinates don't
 *   require repeating.
 */
static CoglBool
_cogl_multitexture_quad_single_primitive (CoglFramebuffer *framebuffer,
                                          CoglPipeline *pipeline,
                                          const float  *position,
                                          const float  *user_tex_coords,
                                          int user_tex_coords_len)
{
  int n_layers = cogl_pipeline_get_n_layers (pipeline);
  ValidateTexCoordsState state;
  float *final_tex_coords = alloca (sizeof (float) * 4 * n_layers);

  state.i = -1;
  state.n_layers = n_layers;
  state.user_tex_coords = user_tex_coords;
  state.user_tex_coords_len = user_tex_coords_len;
  state.final_tex_coords = final_tex_coords;
  state.override_pipeline = NULL;
  state.needs_multiple_primitives = FALSE;

  cogl_pipeline_foreach_layer (pipeline,
                               validate_tex_coords_cb,
                               &state);

  if (state.needs_multiple_primitives)
    return FALSE;

  if (state.override_pipeline)
    pipeline = state.override_pipeline;

  _cogl_journal_log_quad (framebuffer->journal,
                          position,
                          pipeline,
                          n_layers,
                          NULL, /* no texture override */
                          final_tex_coords,
                          n_layers * 4);

  if (state.override_pipeline)
    cogl_object_unref (state.override_pipeline);

  return TRUE;
}

typedef struct _ValidateLayerState
{
  CoglContext *ctx;
  int i;
  int first_layer;
  CoglPipeline *override_source;
  CoglBool all_use_sliced_quad_fallback;
} ValidateLayerState;

static CoglBool
_cogl_rectangles_validate_layer_cb (CoglPipeline *pipeline,
                                    int layer_index,
                                    void *user_data)
{
  ValidateLayerState *state = user_data;
  CoglTexture *texture;

  state->i++;

  /* We need to ensure the mipmaps are ready before deciding
   * anything else about the texture because the texture storage
   * could completely change if it needs to be migrated out of the
   * atlas and will affect how we validate the layer.
   *
   * FIXME: this needs to be generalized. There could be any
   * number of things that might require a shuffling of the
   * underlying texture storage. We could add two mechanisms to
   * generalize this a bit...
   *
   * 1) add a _cogl_pipeline_layer_update_storage() function that
   * would for instance consider if mipmapping is necessary and
   * potentially migrate the texture from an atlas.
   *
   * 2) allow setting of transient primitive-flags on a pipeline
   * that may affect the outcome of _update_storage(). One flag
   * could indicate that we expect to sample beyond the bounds of
   * the texture border.
   *
   *   flags = COGL_PIPELINE_PRIMITIVE_FLAG_VALID_BORDERS;
   *   _cogl_pipeline_layer_assert_primitive_flags (layer, flags)
   *   _cogl_pipeline_layer_update_storage (layer)
   *   enqueue primitive in journal
   *
   *   when the primitive is dequeued and drawn we should:
   *   _cogl_pipeline_flush_gl_state (pipeline)
   *   draw primitive
   *   _cogl_pipeline_unassert_primitive_flags (layer, flags);
   *
   * _cogl_pipeline_layer_update_storage should take into
   * consideration all the asserted primitive requirements.  (E.g.
   * there could be multiple primitives in the journal - or in a
   * renderlist in the future - that need mipmaps or that need
   * valid contents beyond their borders (for cogl_polygon)
   * meaning they can't work with textures in an atas, so
   * _cogl_pipeline_layer_update_storage would pass on these
   * requirements to the texture atlas backend which would make
   * sure the referenced texture is migrated out of the atlas and
   * mipmaps are generated.)
   */
  _cogl_pipeline_pre_paint_for_layer (pipeline, layer_index);

  texture = cogl_pipeline_get_layer_texture (pipeline, layer_index);

  /* NULL textures are handled by
   * _cogl_pipeline_flush_gl_state */
  if (texture == NULL)
    return TRUE;

  if (state->i == 0)
    state->first_layer = layer_index;

  /* XXX:
   * For now, if the first layer is sliced then all other layers are
   * ignored since we currently don't support multi-texturing with
   * sliced textures. If the first layer is not sliced then any other
   * layers found to be sliced will be skipped. (with a warning)
   *
   * TODO: Add support for multi-texturing rectangles with sliced
   * textures if no texture matrices are in use.
   */
  if (cogl_texture_is_sliced (texture))
    {
      if (state->i == 0)
        {
          if (cogl_pipeline_get_n_layers (pipeline) > 1)
            {
              static CoglBool warning_seen = FALSE;

              if (!state->override_source)
                state->override_source = cogl_pipeline_copy (pipeline);
              _cogl_pipeline_prune_to_n_layers (state->override_source, 1);

              if (!warning_seen)
                g_warning ("Skipping layers 1..n of your pipeline since "
                           "the first layer is sliced. We don't currently "
                           "support any multi-texturing with sliced "
                           "textures but assume layer 0 is the most "
                           "important to keep");
              warning_seen = TRUE;
            }

          state->all_use_sliced_quad_fallback = TRUE;

          return FALSE;
        }
      else
        {
          static CoglBool warning_seen = FALSE;
          CoglTexture2D *tex_2d;

          if (!warning_seen)
            g_warning ("Skipping layer %d of your pipeline consisting of "
                       "a sliced texture (unsuported for multi texturing)",
                       state->i);
          warning_seen = TRUE;

          /* Note: currently only 2D textures can be sliced. */
          tex_2d = state->ctx->default_gl_texture_2d_tex;
          cogl_pipeline_set_layer_texture (pipeline, layer_index,
                                           COGL_TEXTURE (tex_2d));
          return TRUE;
        }
    }

#ifdef COGL_ENABLE_DEBUG
  /* If the texture can't be repeated with the GPU (e.g. because it has
   * waste or if using GL_TEXTURE_RECTANGLE_ARB) then if a texture matrix
   * is also in use we don't know if the result will end up trying
   * to texture from the waste area.
   *
   * Note: we check can_hardware_repeat() first since it's cheaper.
   *
   * Note: cases where the texture coordinates will require repeating
   * will be caught by later validation.
   */
  if (!_cogl_texture_can_hardware_repeat (texture) &&
      _cogl_pipeline_layer_has_user_matrix (pipeline, layer_index))
    {
      static CoglBool warning_seen = FALSE;
      if (!warning_seen)
        g_warning ("layer %d of your pipeline uses a custom "
                   "texture matrix but because the texture doesn't "
                   "support hardware repeating you may see artefacts "
                   "due to sampling beyond the texture's bounds.",
                   state->i);
      warning_seen = TRUE;
    }
#endif

  return TRUE;
}

void
_cogl_framebuffer_draw_multitextured_rectangles (
                                        CoglFramebuffer *framebuffer,
                                        CoglPipeline *pipeline,
                                        CoglMultiTexturedRect *rects,
                                        int n_rects,
                                        CoglBool disable_legacy_state)
{
  CoglContext *ctx = framebuffer->context;
  CoglPipeline *original_pipeline;
  ValidateLayerState state;
  int i;

  original_pipeline = pipeline;

  /*
   * Validate all the layers of the current source pipeline...
   */
  state.ctx = ctx;
  state.i = -1;
  state.first_layer = 0;
  state.override_source = NULL;
  state.all_use_sliced_quad_fallback = FALSE;
  cogl_pipeline_foreach_layer (pipeline,
                               _cogl_rectangles_validate_layer_cb,
                               &state);

  if (state.override_source)
    pipeline = state.override_source;

  if (!disable_legacy_state)
    {
      if (G_UNLIKELY (ctx->legacy_state_set) &&
          _cogl_get_enable_legacy_state ())
        {
          /* If we haven't already made a pipeline copy */
          if (pipeline == original_pipeline)
            pipeline = cogl_pipeline_copy (pipeline);
          _cogl_pipeline_apply_legacy_state (pipeline);
        }
    }

  /*
   * Emit geometry for each of the rectangles...
   */

  for (i = 0; i < n_rects; i++)
    {
      CoglTexture *texture;
      const float default_tex_coords[4] = {0.0, 0.0, 1.0, 1.0};
      const float *tex_coords;

      if (!state.all_use_sliced_quad_fallback)
        {
          CoglBool success =
            _cogl_multitexture_quad_single_primitive (framebuffer,
                                                      pipeline,
                                                      rects[i].position,
                                                      rects[i].tex_coords,
                                                      rects[i].tex_coords_len);

          /* NB: If _cogl_multitexture_quad_single_primitive fails then it
           * means the user tried to use texture repeat with a texture that
           * can't be repeated by the GPU (e.g. due to waste or use of
           * GL_TEXTURE_RECTANGLE_ARB) */
          if (success)
            continue;
        }

      /* If multitexturing failed or we are drawing with a sliced texture
       * then we only support a single layer so we pluck out the texture
       * from the first pipeline layer... */
      texture = cogl_pipeline_get_layer_texture (pipeline, state.first_layer);

      if (rects[i].tex_coords)
        tex_coords = rects[i].tex_coords;
      else
        tex_coords = default_tex_coords;

      COGL_NOTE (DRAW, "Drawing Tex Quad (Multi-Prim Mode)");

      _cogl_texture_quad_multiple_primitives (framebuffer,
                                              pipeline,
                                              texture,
                                              state.first_layer,
                                              rects[i].position,
                                              tex_coords[0],
                                              tex_coords[1],
                                              tex_coords[2],
                                              tex_coords[3]);
    }

  if (pipeline != original_pipeline)
    cogl_object_unref (pipeline);
}

static void
_cogl_rectangles_with_multitexture_coords (
                                        CoglMultiTexturedRect *rects,
                                        int n_rects)
{
  _cogl_framebuffer_draw_multitextured_rectangles (cogl_get_draw_framebuffer (),
                                                   cogl_get_source (),
                                                   rects,
                                                   n_rects,
                                                   FALSE);
}

void
cogl_rectangles (const float *verts,
                 unsigned int n_rects)
{
  CoglMultiTexturedRect *rects;
  int i;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * CoglMultiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_rectangles_with_multitexture_coords.
   */

  rects = g_alloca (n_rects * sizeof (CoglMultiTexturedRect));

  for (i = 0; i < n_rects; i++)
    {
      rects[i].position = &verts[i * 4];
      rects[i].tex_coords = NULL;
      rects[i].tex_coords_len = 0;
    }

  _cogl_rectangles_with_multitexture_coords (rects, n_rects);
}

void
cogl_rectangles_with_texture_coords (const float *verts,
                                     unsigned int n_rects)
{
  CoglMultiTexturedRect *rects;
  int i;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * CoglMultiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_rectangles_with_multitexture_coords.
   */

  rects = g_alloca (n_rects * sizeof (CoglMultiTexturedRect));

  for (i = 0; i < n_rects; i++)
    {
      rects[i].position = &verts[i * 8];
      rects[i].tex_coords = &verts[i * 8 + 4];
      rects[i].tex_coords_len = 4;
    }

  _cogl_rectangles_with_multitexture_coords (rects, n_rects);
}

void
cogl_rectangle_with_texture_coords (float x_1,
			            float y_1,
			            float x_2,
			            float y_2,
			            float tx_1,
			            float ty_1,
			            float tx_2,
			            float ty_2)
{
  const float position[4] = {x_1, y_1, x_2, y_2};
  const float tex_coords[4] = {tx_1, ty_1, tx_2, ty_2};
  CoglMultiTexturedRect rect;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * CoglMultiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_rectangles_with_multitexture_coords.
   */

  rect.position = position;
  rect.tex_coords = tex_coords;
  rect.tex_coords_len = 4;

  _cogl_rectangles_with_multitexture_coords (&rect, 1);
}

void
cogl_rectangle_with_multitexture_coords (float        x_1,
			                 float        y_1,
			                 float        x_2,
			                 float        y_2,
			                 const float *user_tex_coords,
                                         int          user_tex_coords_len)
{
  const float position[4] = {x_1, y_1, x_2, y_2};
  CoglMultiTexturedRect rect;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * CoglMultiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_rectangles_with_multitexture_coords.
   */

  rect.position = position;
  rect.tex_coords = user_tex_coords;
  rect.tex_coords_len = user_tex_coords_len;

  _cogl_rectangles_with_multitexture_coords (&rect, 1);
}

void
cogl_rectangle (float x_1,
                float y_1,
                float x_2,
                float y_2)
{
  const float position[4] = {x_1, y_1, x_2, y_2};
  CoglMultiTexturedRect rect;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * CoglMultiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_rectangles_with_multitexture_coords.
   */

  rect.position = position;
  rect.tex_coords = NULL;
  rect.tex_coords_len = 0;

  _cogl_rectangles_with_multitexture_coords (&rect, 1);
}

void
_cogl_rectangle_immediate (CoglFramebuffer *framebuffer,
                           CoglPipeline *pipeline,
                           float x_1,
                           float y_1,
                           float x_2,
                           float y_2)
{
  /* Draw a rectangle using the vertex array API to avoid going
     through the journal. This should only be used in cases where the
     code might be called while the journal is already being flushed
     such as when flushing the clip state */
  CoglContext *ctx = framebuffer->context;
  float vertices[8] =
    {
      x_1, y_1,
      x_1, y_2,
      x_2, y_1,
      x_2, y_2
    };
  CoglAttributeBuffer *attribute_buffer;
  CoglAttribute *attributes[1];

  attribute_buffer =
    cogl_attribute_buffer_new (ctx, sizeof (vertices), vertices);
  attributes[0] = cogl_attribute_new (attribute_buffer,
                                      "cogl_position_in",
                                      sizeof (float) * 2, /* stride */
                                      0, /* offset */
                                      2, /* n_components */
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  _cogl_framebuffer_draw_attributes (framebuffer,
                                     pipeline,
                                     COGL_VERTICES_MODE_TRIANGLE_STRIP,
                                     0, /* first_index */
                                     4, /* n_vertices */
                                     attributes,
                                     1,
                                     COGL_DRAW_SKIP_JOURNAL_FLUSH |
                                     COGL_DRAW_SKIP_PIPELINE_VALIDATION |
                                     COGL_DRAW_SKIP_FRAMEBUFFER_FLUSH |
                                     COGL_DRAW_SKIP_LEGACY_STATE);


  cogl_object_unref (attributes[0]);
  cogl_object_unref (attribute_buffer);
}

typedef struct _AppendTexCoordsState
{
  const CoglTextureVertex *vertices_in;
  int vertex;
  int layer;
  float *vertices_out;
} AppendTexCoordsState;

static CoglBool
append_tex_coord_attributes_cb (CoglPipeline *pipeline,
                                int layer_index,
                                void *user_data)
{
  AppendTexCoordsState *state = user_data;
  CoglTexture *texture;
  float tx, ty;
  float *t;

  tx = state->vertices_in[state->vertex].tx;
  ty = state->vertices_in[state->vertex].ty;

  /* NULL textures will be handled in
   * _cogl_pipeline_flush_layers_gl_state but there is no need to worry
   * about scaling texture coordinates in this case */
  texture = cogl_pipeline_get_layer_texture (pipeline, layer_index);
  if (texture != NULL)
    _cogl_texture_transform_coords_to_gl (texture, &tx, &ty);

  /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
  t = state->vertices_out + 3 + 2 * state->layer;
  t[0] = tx;
  t[1] = ty;

  state->layer++;

  return TRUE;
}

typedef struct _ValidateState
{
  CoglPipeline *original_pipeline;
  CoglPipeline *pipeline;
} ValidateState;

static CoglBool
_cogl_polygon_validate_layer_cb (CoglPipeline *pipeline,
                                 int layer_index,
                                 void *user_data)
{
  ValidateState *state = user_data;

  /* By default COGL_PIPELINE_WRAP_MODE_AUTOMATIC becomes
   * GL_CLAMP_TO_EDGE but we want the polygon API to use GL_REPEAT to
   * maintain compatibility with previous releases
   */

  if (cogl_pipeline_get_layer_wrap_mode_s (pipeline, layer_index) ==
      COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
    {
      if (state->original_pipeline == state->pipeline)
        state->pipeline = cogl_pipeline_copy (pipeline);

      cogl_pipeline_set_layer_wrap_mode_s (state->pipeline, layer_index,
                                           COGL_PIPELINE_WRAP_MODE_REPEAT);
    }

  if (cogl_pipeline_get_layer_wrap_mode_t (pipeline, layer_index) ==
      COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
    {
      if (state->original_pipeline == state->pipeline)
        state->pipeline = cogl_pipeline_copy (pipeline);

      cogl_pipeline_set_layer_wrap_mode_t (state->pipeline, layer_index,
                                           COGL_PIPELINE_WRAP_MODE_REPEAT);
    }

  return TRUE;
}

void
cogl_polygon (const CoglTextureVertex *vertices,
              unsigned int n_vertices,
	      CoglBool use_color)
{
  CoglPipeline *pipeline;
  ValidateState validate_state;
  int n_layers;
  int n_attributes;
  CoglAttribute **attributes;
  int i;
  unsigned int stride;
  size_t stride_bytes;
  CoglAttributeBuffer *attribute_buffer;
  float *v;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  pipeline = cogl_get_source ();

  validate_state.original_pipeline = pipeline;
  validate_state.pipeline = pipeline;
  cogl_pipeline_foreach_layer (pipeline,
                               _cogl_polygon_validate_layer_cb,
                               &validate_state);
  pipeline = validate_state.pipeline;

  n_layers = cogl_pipeline_get_n_layers (pipeline);

  n_attributes = 1 + n_layers + (use_color ? 1 : 0);
  attributes = g_alloca (sizeof (CoglAttribute *) * n_attributes);

  /* Our data is arranged like:
   * [X, Y, Z, TX0, TY0, TX1, TY1..., R, G, B, A,...] */
  stride = 3 + (2 * n_layers) + (use_color ? 1 : 0);
  stride_bytes = stride * sizeof (float);

  /* Make sure there is enough space in the global vertex array. This
   * is used so we can render the polygon with a single call to OpenGL
   * but still support any number of vertices */
  g_array_set_size (ctx->polygon_vertices, n_vertices * stride);

  attribute_buffer =
    cogl_attribute_buffer_new (ctx, n_vertices * stride_bytes, NULL);

  attributes[0] = cogl_attribute_new (attribute_buffer,
                                      "cogl_position_in",
                                      stride_bytes,
                                      0,
                                      3,
                                      COGL_ATTRIBUTE_TYPE_FLOAT);

  for (i = 0; i < n_layers; i++)
    {
      static const char *names[] = {
          "cogl_tex_coord0_in",
          "cogl_tex_coord1_in",
          "cogl_tex_coord2_in",
          "cogl_tex_coord3_in",
          "cogl_tex_coord4_in",
          "cogl_tex_coord5_in",
          "cogl_tex_coord6_in",
          "cogl_tex_coord7_in"
      };
      char *allocated_name = NULL;
      const char *name;

      if (i < 8)
        name = names[i];
      else
        name = allocated_name = g_strdup_printf ("cogl_tex_coord%d_in", i);

      attributes[i + 1] = cogl_attribute_new (attribute_buffer,
                                              name,
                                              stride_bytes,
                                              /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
                                              12 + 8 * i,
                                              2,
                                              COGL_ATTRIBUTE_TYPE_FLOAT);

      g_free (allocated_name);
    }

  if (use_color)
    {
      attributes[n_attributes - 1] =
        cogl_attribute_new (attribute_buffer,
                            "cogl_color_in",
                            stride_bytes,
                            /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
                            12 + 8 * n_layers,
                            4,
                            COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE);
    }

  /* Convert the vertices into an array of float vertex attributes */
  v = (float *)ctx->polygon_vertices->data;
  for (i = 0; i < n_vertices; i++)
    {
      AppendTexCoordsState append_tex_coords_state;
      uint8_t *c;

      /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
      v[0] = vertices[i].x;
      v[1] = vertices[i].y;
      v[2] = vertices[i].z;

      append_tex_coords_state.vertices_in = vertices;
      append_tex_coords_state.vertex = i;
      append_tex_coords_state.layer = 0;
      append_tex_coords_state.vertices_out = v;
      cogl_pipeline_foreach_layer (pipeline,
                                   append_tex_coord_attributes_cb,
                                   &append_tex_coords_state);

      if (use_color)
        {
          /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
          c = (uint8_t *) (v + 3 + 2 * n_layers);
          c[0] = cogl_color_get_red_byte (&vertices[i].color);
          c[1] = cogl_color_get_green_byte (&vertices[i].color);
          c[2] = cogl_color_get_blue_byte (&vertices[i].color);
          c[3] = cogl_color_get_alpha_byte (&vertices[i].color);
        }

      v += stride;
    }

  v = (float *)ctx->polygon_vertices->data;
  cogl_buffer_set_data (COGL_BUFFER (attribute_buffer),
                        0,
                        v,
                        ctx->polygon_vertices->len * sizeof (float));

  /* XXX: although this may seem redundant, we need to do this since
   * cogl_polygon() can be used with legacy state and its the source stack
   * which track whether legacy state is enabled.
   *
   * (We only have a CoglDrawFlag to disable legacy state not one
   *  to enable it) */
  cogl_push_source (pipeline);

  _cogl_framebuffer_draw_attributes (cogl_get_draw_framebuffer (),
                                     pipeline,
                                     COGL_VERTICES_MODE_TRIANGLE_FAN,
                                     0, n_vertices,
                                     attributes,
                                     n_attributes,
                                     0 /* no draw flags */);

  cogl_pop_source ();

  if (pipeline != validate_state.original_pipeline)
    cogl_object_unref (pipeline);

  cogl_object_unref (attribute_buffer);

  for (i = 0; i < n_attributes; i++)
    cogl_object_unref (attributes[i]);
}
