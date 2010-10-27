/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-debug.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-journal-private.h"
#include "cogl-texture-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-vertex-buffer-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-vertex-attribute-private.h"

#include <string.h>
#include <math.h>

#define _COGL_MAX_BEZ_RECURSE_DEPTH 16

#ifdef HAVE_COGL_GL
#define glClientActiveTexture ctx->drv.pf_glClientActiveTexture
#endif


typedef struct _TextureSlicedQuadState
{
  CoglPipeline *pipeline;
  float tex_virtual_origin_x;
  float tex_virtual_origin_y;
  float quad_origin_x;
  float quad_origin_y;
  float v_to_q_scale_x;
  float v_to_q_scale_y;
  float quad_len_x;
  float quad_len_y;
  gboolean flipped_x;
  gboolean flipped_y;
  CoglPipelineWrapModeOverrides *wrap_mode_overrides;
} TextureSlicedQuadState;

typedef struct _TextureSlicedPolygonState
{
  const CoglTextureVertex *vertices;
  int n_vertices;
  int stride;
  CoglVertexAttribute **attributes;
} TextureSlicedPolygonState;

static void
log_quad_sub_textures_cb (CoglHandle texture_handle,
                          GLuint gl_handle,
                          GLenum gl_target,
                          const float *subtexture_coords,
                          const float *virtual_coords,
                          void *user_data)
{
  TextureSlicedQuadState *state = user_data;
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

  /* FIXME: when the wrap mode becomes part of the pipeline we need to
   * be able to override the wrap mode when logging a quad. */
  _cogl_journal_log_quad (quad_coords,
                          state->pipeline,
                          1, /* one layer */
                          0, /* don't need to use fallbacks */
                          gl_handle, /* replace the layer0 texture */
                          state->wrap_mode_overrides, /* use GL_CLAMP_TO_EDGE */
                          subtexture_coords,
                          4);
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
_cogl_texture_quad_multiple_primitives (CoglHandle    tex_handle,
                                        CoglPipeline *pipeline,
                                        gboolean      clamp_s,
                                        gboolean      clamp_t,
                                        const float  *position,
                                        float         tx_1,
                                        float         ty_1,
                                        float         tx_2,
                                        float         ty_2)
{
  TextureSlicedQuadState state;
  CoglPipelineWrapModeOverrides wrap_mode_overrides;
  gboolean tex_virtual_flipped_x;
  gboolean tex_virtual_flipped_y;
  gboolean quad_flipped_x;
  gboolean quad_flipped_y;
  CoglHandle first_layer;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* If the wrap mode is clamp to edge then we'll recursively draw the
     stretched part and replace the coordinates */
  if (clamp_s && tx_1 != tx_2)
    {
      float *replacement_position = g_newa (float, 4);
      float old_tx_1 = tx_1, old_tx_2 = tx_2;

      memcpy (replacement_position, position, sizeof (float) * 4);

      tx_1 = CLAMP (tx_1, 0.0f, 1.0f);
      tx_2 = CLAMP (tx_2, 0.0f, 1.0f);

      if (old_tx_1 != tx_1)
        {
          /* Draw the left part of the quad as a stretched copy of tx_1 */
          float tmp_position[] =
            { position[0], position[1],
              (position[0] +
               (position[2] - position[0]) *
               (tx_1 - old_tx_1) / (old_tx_2 - old_tx_1)),
              position[3] };
          _cogl_texture_quad_multiple_primitives (tex_handle, pipeline,
                                                  FALSE, clamp_t,
                                                  tmp_position,
                                                  tx_1, ty_1, tx_1, ty_2);
          replacement_position[0] = tmp_position[2];
        }

      if (old_tx_2 != tx_2)
        {
          /* Draw the right part of the quad as a stretched copy of tx_2 */
          float tmp_position[] =
            { (position[0] +
               (position[2] - position[0]) *
               (tx_2 - old_tx_1) / (old_tx_2 - old_tx_1)),
              position[1], position[2], position[3] };
          _cogl_texture_quad_multiple_primitives (tex_handle, pipeline,
                                                  FALSE, clamp_t,
                                                  tmp_position,
                                                  tx_2, ty_1, tx_2, ty_2);
          replacement_position[2] = tmp_position[0];
        }

      /* If there's no main part left then we don't need to continue */
      if (tx_1 == tx_2)
        return;

      position = replacement_position;
    }

  if (clamp_t && ty_1 != ty_2)
    {
      float *replacement_position = g_newa (float, 4);
      float old_ty_1 = ty_1, old_ty_2 = ty_2;

      memcpy (replacement_position, position, sizeof (float) * 4);

      ty_1 = CLAMP (ty_1, 0.0f, 1.0f);
      ty_2 = CLAMP (ty_2, 0.0f, 1.0f);

      if (old_ty_1 != ty_1)
        {
          /* Draw the top part of the quad as a stretched copy of ty_1 */
          float tmp_position[] =
            { position[0], position[1], position[2],
              (position[1] +
               (position[3] - position[1]) *
               (ty_1 - old_ty_1) / (old_ty_2 - old_ty_1)) };
          _cogl_texture_quad_multiple_primitives (tex_handle, pipeline,
                                                  clamp_s, FALSE,
                                                  tmp_position,
                                                  tx_1, ty_1, tx_2, ty_1);
          replacement_position[1] = tmp_position[3];
        }

      if (old_ty_2 != ty_2)
        {
          /* Draw the bottom part of the quad as a stretched copy of ty_2 */
          float tmp_position[] =
            { position[0],
              (position[1] +
               (position[3] - position[1]) *
               (ty_2 - old_ty_1) / (old_ty_2 - old_ty_1)),
              position[2], position[3] };
          _cogl_texture_quad_multiple_primitives (tex_handle, pipeline,
                                                  clamp_s, FALSE,
                                                  tmp_position,
                                                  tx_1, ty_2, tx_2, ty_2);
          replacement_position[3] = tmp_position[1];
        }

      /* If there's no main part left then we don't need to continue */
      if (ty_1 == ty_2)
        return;

      position = replacement_position;
    }

  state.wrap_mode_overrides = NULL;
  memset (&wrap_mode_overrides, 0, sizeof (wrap_mode_overrides));

  /* We can't use hardware repeat so we need to set clamp to edge
     otherwise it might pull in edge pixels from the other side. By
     default WRAP_MODE_AUTOMATIC becomes CLAMP_TO_EDGE so we only need
     to override if the wrap mode is repeat */
  first_layer = _cogl_pipeline_get_layers (pipeline)->data;
  if (_cogl_pipeline_layer_get_wrap_mode_s (first_layer) ==
      COGL_PIPELINE_WRAP_MODE_REPEAT)
    {
      state.wrap_mode_overrides = &wrap_mode_overrides;
      wrap_mode_overrides.values[0].s =
        COGL_PIPELINE_WRAP_MODE_OVERRIDE_CLAMP_TO_EDGE;
    }
  if (_cogl_pipeline_layer_get_wrap_mode_t (first_layer) ==
      COGL_PIPELINE_WRAP_MODE_REPEAT)
    {
      state.wrap_mode_overrides = &wrap_mode_overrides;
      wrap_mode_overrides.values[0].t =
        COGL_PIPELINE_WRAP_MODE_OVERRIDE_CLAMP_TO_EDGE;
    }

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

  _cogl_texture_foreach_sub_texture_in_region (tex_handle,
                                               tx_1, ty_1, tx_2, ty_2,
                                               log_quad_sub_textures_cb,
                                               &state);
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
static gboolean
_cogl_multitexture_quad_single_primitive (const float  *position,
                                          CoglPipeline *pipeline,
                                          guint32       fallback_layers,
                                          const float  *user_tex_coords,
                                          int           user_tex_coords_len)
{
  int          n_layers = cogl_pipeline_get_n_layers (pipeline);
  float       *final_tex_coords = alloca (sizeof (float) * 4 * n_layers);
  const GList *layers;
  GList       *tmp;
  int          i;
  CoglPipelineWrapModeOverrides wrap_mode_overrides;
  /* This will be set to point to wrap_mode_overrides when an override
     is needed */
  CoglPipelineWrapModeOverrides *wrap_mode_overrides_p = NULL;

  _COGL_GET_CONTEXT (ctx, FALSE);

  memset (&wrap_mode_overrides, 0, sizeof (wrap_mode_overrides));

  /*
   * Validate the texture coordinates for this rectangle.
   */
  layers = _cogl_pipeline_get_layers (pipeline);
  for (tmp = (GList *)layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle          layer = (CoglHandle)tmp->data;
      CoglHandle          tex_handle;
      const float        *in_tex_coords;
      float              *out_tex_coords;
      float               default_tex_coords[4] = {0.0, 0.0, 1.0, 1.0};
      CoglTransformResult transform_result;

      tex_handle = _cogl_pipeline_layer_get_texture (layer);

      /* COGL_INVALID_HANDLE textures are handled by
       * _cogl_pipeline_flush_gl_state */
      if (tex_handle == COGL_INVALID_HANDLE)
        continue;

      /* If the user didn't supply texture coordinates for this layer
         then use the default coords */
      if (i >= user_tex_coords_len / 4)
        in_tex_coords = default_tex_coords;
      else
        in_tex_coords = &user_tex_coords[i * 4];

      out_tex_coords = &final_tex_coords[i * 4];

      memcpy (out_tex_coords, in_tex_coords, sizeof (GLfloat) * 4);

      /* Convert the texture coordinates to GL.
       */
      transform_result =
        _cogl_texture_transform_quad_coords_to_gl (tex_handle,
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
          if (i == 0)
            {
              if (n_layers > 1)
                {
                  static gboolean warning_seen = FALSE;
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
              return FALSE;
            }
          else
            {
              static gboolean warning_seen = FALSE;
              if (!warning_seen)
                g_warning ("Skipping layer %d of your material "
                           "since you have supplied texture coords "
                           "outside the range [0,1] but the texture "
                           "doesn't support hardware repeat (e.g. "
                           "because of waste or use of "
                           "GL_TEXTURE_RECTANGLE_ARB). This isn't "
                           "supported with multi-texturing.", i);
              warning_seen = TRUE;

              /* NB: marking for fallback will replace the layer with
               * a default transparent texture */
              fallback_layers |= (1 << i);
            }
        }

      /* By default WRAP_MODE_AUTOMATIC becomes to CLAMP_TO_EDGE. If
         the texture coordinates need repeating then we'll override
         this to GL_REPEAT. Otherwise we'll leave it at CLAMP_TO_EDGE
         so that it won't blend in pixels from the opposite side when
         the full texture is drawn with GL_LINEAR filter mode */
      if (transform_result == COGL_TRANSFORM_HARDWARE_REPEAT)
        {
          if (_cogl_pipeline_layer_get_wrap_mode_s (layer) ==
              COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
            {
              wrap_mode_overrides.values[i].s
                = COGL_PIPELINE_WRAP_MODE_OVERRIDE_REPEAT;
              wrap_mode_overrides_p = &wrap_mode_overrides;
            }
          if (_cogl_pipeline_layer_get_wrap_mode_t (layer) ==
              COGL_PIPELINE_WRAP_MODE_AUTOMATIC)
            {
              wrap_mode_overrides.values[i].t
                = COGL_PIPELINE_WRAP_MODE_OVERRIDE_REPEAT;
              wrap_mode_overrides_p = &wrap_mode_overrides;
            }
        }
    }

  _cogl_journal_log_quad (position,
                          pipeline,
                          n_layers,
                          fallback_layers,
                          0, /* don't replace the layer0 texture */
                          wrap_mode_overrides_p,
                          final_tex_coords,
                          n_layers * 4);

  return TRUE;
}

struct _CoglMutiTexturedRect
{
  const float *position; /* x0,y0,x1,y1 */
  const float *tex_coords; /* (tx0,ty0,tx1,ty1)(tx0,ty0,tx1,ty1)(... */
  int          tex_coords_len; /* number of floats in tex_coords? */
};

static void
_cogl_rectangles_with_multitexture_coords (
                                        struct _CoglMutiTexturedRect *rects,
                                        int                           n_rects)
{
  CoglPipeline *pipeline;
  const GList *layers;
  int n_layers;
  const GList *tmp;
  guint32 fallback_layers = 0;
  gboolean all_use_sliced_quad_fallback = FALSE;
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  pipeline = cogl_get_source ();

  layers = _cogl_pipeline_get_layers (pipeline);
  n_layers = cogl_pipeline_get_n_layers (pipeline);

  /*
   * Validate all the layers of the current source pipeline...
   */

  for (tmp = layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle     layer = tmp->data;
      CoglHandle     tex_handle;

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
      _cogl_pipeline_layer_pre_paint (layer);

      tex_handle = _cogl_pipeline_layer_get_texture (layer);

      /* COGL_INVALID_HANDLE textures are handled by
       * _cogl_pipeline_flush_gl_state */
      if (tex_handle == COGL_INVALID_HANDLE)
        continue;

      /* XXX:
       * For now, if the first layer is sliced then all other layers are
       * ignored since we currently don't support multi-texturing with
       * sliced textures. If the first layer is not sliced then any other
       * layers found to be sliced will be skipped. (with a warning)
       *
       * TODO: Add support for multi-texturing rectangles with sliced
       * textures if no texture matrices are in use.
       */
      if (cogl_texture_is_sliced (tex_handle))
	{
	  if (i == 0)
	    {
              fallback_layers = ~1; /* fallback all except the first layer */
	      all_use_sliced_quad_fallback = TRUE;
              if (tmp->next)
                {
                  static gboolean warning_seen = FALSE;
                  if (!warning_seen)
                    g_warning ("Skipping layers 1..n of your pipeline since "
                               "the first layer is sliced. We don't currently "
                               "support any multi-texturing with sliced "
                               "textures but assume layer 0 is the most "
                               "important to keep");
                  warning_seen = TRUE;
                }
	      break;
	    }
          else
            {
              static gboolean warning_seen = FALSE;
              if (!warning_seen)
                g_warning ("Skipping layer %d of your pipeline consisting of "
                           "a sliced texture (unsuported for multi texturing)",
                           i);
              warning_seen = TRUE;

              /* NB: marking for fallback will replace the layer with
               * a default transparent texture */
              fallback_layers |= (1 << i);
	      continue;
            }
	}

      /* If the texture can't be repeated with the GPU (e.g. because it has
       * waste or if using GL_TEXTURE_RECTANGLE_ARB) then we don't support
       * multi texturing since we don't know if the result will end up trying
       * to texture from the waste area. */
      if (_cogl_pipeline_layer_has_user_matrix (layer)
          && !_cogl_texture_can_hardware_repeat (tex_handle))
        {
          static gboolean warning_seen = FALSE;
          if (!warning_seen)
            g_warning ("Skipping layer %d of your pipeline since a custom "
                       "texture matrix was given for a texture that can't be "
                       "repeated using the GPU and the result may try to "
                       "sample beyond the bounds of the texture ",
                       i);
          warning_seen = TRUE;

          /* NB: marking for fallback will replace the layer with
           * a default transparent texture */
          fallback_layers |= (1 << i);
          continue;
        }
    }

  /*
   * Emit geometry for each of the rectangles...
   */

  for (i = 0; i < n_rects; i++)
    {
      CoglHandle   first_layer, tex_handle;
      const float  default_tex_coords[4] = {0.0, 0.0, 1.0, 1.0};
      const float *tex_coords;
      gboolean     clamp_s, clamp_t;

      if (!all_use_sliced_quad_fallback)
        {
          gboolean success =
            _cogl_multitexture_quad_single_primitive (rects[i].position,
                                                      pipeline,
                                                      fallback_layers,
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
      layers = _cogl_pipeline_get_layers (pipeline);
      first_layer = layers->data;
      tex_handle = _cogl_pipeline_layer_get_texture (first_layer);

      if (rects[i].tex_coords)
        tex_coords = rects[i].tex_coords;
      else
        tex_coords = default_tex_coords;

      clamp_s = (_cogl_pipeline_layer_get_wrap_mode_s (first_layer) ==
                 COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
      clamp_t = (_cogl_pipeline_layer_get_wrap_mode_t (first_layer) ==
                 COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);

      COGL_NOTE (DRAW, "Drawing Tex Quad (Multi-Prim Mode)");

      _cogl_texture_quad_multiple_primitives (tex_handle,
                                              pipeline,
                                              clamp_s, clamp_t,
                                              rects[i].position,
                                              tex_coords[0],
                                              tex_coords[1],
                                              tex_coords[2],
                                              tex_coords[3]);
    }

#if 0
  /* XXX: The current journal doesn't handle changes to the model view matrix
   * so for now we force a flush at the end of every primitive. */
  _cogl_journal_flush ();
#endif
}

void
cogl_rectangles (const float *verts,
                 unsigned int n_rects)
{
  struct _CoglMutiTexturedRect *rects;
  int i;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * _CoglMutiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_rectangles_with_multitexture_coords.
   */

  rects = g_alloca (n_rects * sizeof (struct _CoglMutiTexturedRect));

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
  struct _CoglMutiTexturedRect *rects;
  int i;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * _CoglMutiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_rectangles_with_multitexture_coords.
   */

  rects = g_alloca (n_rects * sizeof (struct _CoglMutiTexturedRect));

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
  struct _CoglMutiTexturedRect rect;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * _CoglMutiTexturedRect rectangles and pass these on to our work horse;
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
  struct _CoglMutiTexturedRect rect;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * _CoglMutiTexturedRect rectangles and pass these on to our work horse;
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
  struct _CoglMutiTexturedRect rect;

  /* XXX: All the cogl_rectangle* APIs normalize their input into an array of
   * _CoglMutiTexturedRect rectangles and pass these on to our work horse;
   * _cogl_rectangles_with_multitexture_coords.
   */

  rect.position = position;
  rect.tex_coords = NULL;
  rect.tex_coords_len = 0;

  _cogl_rectangles_with_multitexture_coords (&rect, 1);
}

typedef struct _AppendTexCoordsState
{
  const CoglTextureVertex *vertices_in;
  int vertex;
  int layer;
  float *vertices_out;
} AppendTexCoordsState;

gboolean
append_tex_coord_attributes_cb (CoglPipeline *pipeline,
                                int layer_index,
                                void *user_data)
{
  AppendTexCoordsState *state = user_data;
  CoglHandle tex_handle;
  float tx, ty;
  float *t;

  tx = state->vertices_in[state->vertex].tx;
  ty = state->vertices_in[state->vertex].ty;

  /* COGL_INVALID_HANDLE textures will be handled in
   * _cogl_pipeline_flush_layers_gl_state but there is no need to worry
   * about scaling texture coordinates in this case */
  tex_handle = _cogl_pipeline_get_layer_texture (pipeline, layer_index);
  if (tex_handle != COGL_INVALID_HANDLE)
    _cogl_texture_transform_coords_to_gl (tex_handle, &tx, &ty);

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

gboolean
validate_layer_cb (CoglPipeline *pipeline,
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
	      gboolean use_color)
{
  CoglPipeline *pipeline;
  ValidateState validate_state;
  int n_layers;
  int n_attributes;
  CoglVertexAttribute **attributes;
  int i;
  unsigned int stride;
  gsize stride_bytes;
  CoglVertexArray *vertex_array;
  float *v;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  pipeline = cogl_get_source ();

  validate_state.original_pipeline = pipeline;
  validate_state.pipeline = pipeline;
  cogl_pipeline_foreach_layer (pipeline,
                               validate_layer_cb,
                               &validate_state);
  pipeline = validate_state.pipeline;

  n_layers = cogl_pipeline_get_n_layers (pipeline);

  n_attributes = 1 + n_layers + (use_color ? 1 : 0);
  attributes = g_alloca (sizeof (CoglVertexAttribute *) * (n_attributes + 1));
  attributes[n_attributes] = NULL;

  /* Our data is arranged like:
   * [X, Y, Z, TX0, TY0, TX1, TY1..., R, G, B, A,...] */
  stride = 3 + (2 * n_layers) + (use_color ? 1 : 0);
  stride_bytes = stride * sizeof (float);

  /* Make sure there is enough space in the global vertex array. This
   * is used so we can render the polygon with a single call to OpenGL
   * but still support any number of vertices */
  g_array_set_size (ctx->polygon_vertices, n_vertices * stride);

  vertex_array = cogl_vertex_array_new (n_vertices * stride_bytes);

  attributes[0] =
    cogl_vertex_attribute_new (vertex_array,
                               "cogl_position_in",
                               stride_bytes,
                               0,
                               3,
                               COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);

  for (i = 0; i < n_layers; i++)
    {
      const char *names[] = {
          "cogl_tex_coord0_in",
          "cogl_tex_coord1_in",
          "cogl_tex_coord2_in",
          "cogl_tex_coord3_in",
          "cogl_tex_coord4_in",
          "cogl_tex_coord5_in",
          "cogl_tex_coord6_in",
          "cogl_tex_coord7_in"
      };
      char *name = i < 8 ? (char *)names[i] :
        g_strdup_printf ("cogl_tex_coord%d_in", i);

      attributes[i + 1] =
        cogl_vertex_attribute_new (vertex_array,
                                   name,
                                   stride_bytes,
                                   /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
                                   12 + 8 * i,
                                   2,
                                   COGL_VERTEX_ATTRIBUTE_TYPE_FLOAT);
    }

  if (use_color)
    {
      attributes[n_attributes - 1] =
        cogl_vertex_attribute_new (vertex_array,
                                   "cogl_color_in",
                                   stride_bytes,
                                   /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
                                   12 + 8 * n_layers,
                                   4,
                                   COGL_VERTEX_ATTRIBUTE_TYPE_UNSIGNED_BYTE);
    }

  /* Convert the vertices into an array of float vertex attributes */
  v = (float *)ctx->polygon_vertices->data;
  for (i = 0; i < n_vertices; i++)
    {
      AppendTexCoordsState append_tex_coords_state;
      guint8 *c;

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
          c = (guint8 *) (v + 3 + 2 * n_layers);
          c[0] = cogl_color_get_red_byte (&vertices[i].color);
          c[1] = cogl_color_get_green_byte (&vertices[i].color);
          c[2] = cogl_color_get_blue_byte (&vertices[i].color);
          c[3] = cogl_color_get_alpha_byte (&vertices[i].color);
        }

      v += stride;
    }

  v = (float *)ctx->polygon_vertices->data;
  cogl_buffer_set_data (COGL_BUFFER (vertex_array),
                        0,
                        (const guint8 *)v,
                        ctx->polygon_vertices->len * sizeof (float));

  cogl_push_source (pipeline);

  cogl_draw_vertex_attributes_array (COGL_VERTICES_MODE_TRIANGLE_FAN,
                                     0, n_vertices,
                                     attributes);

  cogl_pop_source ();
}

