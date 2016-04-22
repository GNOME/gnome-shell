/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cogl-material-compat.h>
#include <cogl-pipeline.h>
#include <cogl-pipeline-private.h>
#include <cogl-types.h>
#include <cogl-matrix.h>
#include <cogl-context-private.h>

CoglMaterial *
cogl_material_new (void)
{
  _COGL_GET_CONTEXT(ctx, NULL);
  return COGL_MATERIAL (cogl_pipeline_new (ctx));
}

CoglMaterial *
cogl_material_copy (CoglMaterial *source)
{
  return COGL_MATERIAL (cogl_pipeline_copy (COGL_PIPELINE (source)));
}

CoglHandle
cogl_material_ref (CoglHandle handle)
{
  return cogl_object_ref (handle);
}

void
cogl_material_unref (CoglHandle handle)
{
  cogl_object_unref (handle);
}

CoglBool
cogl_is_material (CoglHandle handle)
{
  return cogl_is_pipeline (handle);
}

void
cogl_material_set_color (CoglMaterial    *material,
                         const CoglColor *color)
{
  cogl_pipeline_set_color (COGL_PIPELINE (material), color);
}

void
cogl_material_set_color4ub (CoglMaterial *material,
			    uint8_t red,
                            uint8_t green,
                            uint8_t blue,
                            uint8_t alpha)
{
  cogl_pipeline_set_color4ub (COGL_PIPELINE (material),
                              red, green, blue, alpha);
}

void
cogl_material_set_color4f (CoglMaterial *material,
                           float         red,
                           float         green,
                           float         blue,
                           float         alpha)
{
  cogl_pipeline_set_color4f (COGL_PIPELINE (material),
                             red, green, blue, alpha);
}

void
cogl_material_get_color (CoglMaterial *material,
                         CoglColor    *color)
{
  cogl_pipeline_get_color (COGL_PIPELINE (material), color);
}

void
cogl_material_set_ambient (CoglMaterial    *material,
			   const CoglColor *ambient)
{
  cogl_pipeline_set_ambient (COGL_PIPELINE (material), ambient);
}

void
cogl_material_get_ambient (CoglMaterial *material,
                           CoglColor    *ambient)
{
  cogl_pipeline_get_ambient (COGL_PIPELINE (material), ambient);
}

void
cogl_material_set_diffuse (CoglMaterial    *material,
			   const CoglColor *diffuse)
{
  cogl_pipeline_set_diffuse (COGL_PIPELINE (material), diffuse);
}

void
cogl_material_get_diffuse (CoglMaterial *material,
                           CoglColor    *diffuse)
{
  cogl_pipeline_get_diffuse (COGL_PIPELINE (material), diffuse);
}

void
cogl_material_set_ambient_and_diffuse (CoglMaterial    *material,
				       const CoglColor *color)
{
  cogl_pipeline_set_ambient_and_diffuse (COGL_PIPELINE (material), color);

}

void
cogl_material_set_specular (CoglMaterial    *material,
			    const CoglColor *specular)
{
  cogl_pipeline_set_specular (COGL_PIPELINE (material), specular);
}

void
cogl_material_get_specular (CoglMaterial *material,
                            CoglColor    *specular)
{
  cogl_pipeline_get_specular (COGL_PIPELINE (material), specular);
}

void
cogl_material_set_shininess (CoglMaterial *material,
			     float         shininess)
{
  cogl_pipeline_set_shininess (COGL_PIPELINE (material), shininess);
}

float
cogl_material_get_shininess (CoglMaterial *material)
{
  return cogl_pipeline_get_shininess (COGL_PIPELINE (material));
}

void
cogl_material_set_emission (CoglMaterial    *material,
			    const CoglColor *emission)
{
  cogl_pipeline_set_emission (COGL_PIPELINE (material), emission);

}

void
cogl_material_get_emission (CoglMaterial *material,
                            CoglColor    *emission)
{
  cogl_pipeline_get_emission (COGL_PIPELINE (material), emission);

}

void
cogl_material_set_alpha_test_function (CoglMaterial         *material,
				       CoglMaterialAlphaFunc alpha_func,
				       float                 alpha_reference)
{
  cogl_pipeline_set_alpha_test_function (COGL_PIPELINE (material),
                                         alpha_func,
                                         alpha_reference);
}

CoglBool
cogl_material_set_blend (CoglMaterial *material,
                         const char   *blend_string,
                         CoglError   **error)
{
  return cogl_pipeline_set_blend (COGL_PIPELINE (material),
                                  blend_string,
                                  error);
}

void
cogl_material_set_blend_constant (CoglMaterial *material,
                                  const CoglColor *constant_color)
{
  cogl_pipeline_set_blend_constant (COGL_PIPELINE (material), constant_color);
}

void
cogl_material_set_point_size (CoglMaterial *material,
                              float         point_size)
{
  cogl_pipeline_set_point_size (COGL_PIPELINE (material), point_size);
}

float
cogl_material_get_point_size (CoglMaterial *material)
{
  return cogl_pipeline_get_point_size (COGL_PIPELINE (material));
}

CoglHandle
cogl_material_get_user_program (CoglMaterial *material)
{
  return cogl_pipeline_get_user_program (COGL_PIPELINE (material));
}

void
cogl_material_set_user_program (CoglMaterial *material,
                                CoglHandle program)
{
  cogl_pipeline_set_user_program (COGL_PIPELINE (material), program);
}

void
cogl_material_set_layer (CoglMaterial *material,
			 int           layer_index,
			 CoglHandle    texture)
{
  cogl_pipeline_set_layer_texture (COGL_PIPELINE (material),
                                   layer_index, texture);
}

void
cogl_material_remove_layer (CoglMaterial *material,
			    int           layer_index)
{
  cogl_pipeline_remove_layer (COGL_PIPELINE (material), layer_index);
}

CoglBool
cogl_material_set_layer_combine (CoglMaterial *material,
				 int           layer_index,
				 const char   *blend_string,
                                 CoglError   **error)
{
  return cogl_pipeline_set_layer_combine (COGL_PIPELINE (material),
                                          layer_index,
                                          blend_string,
                                          error);
}

void
cogl_material_set_layer_combine_constant (CoglMaterial    *material,
                                          int              layer_index,
                                          const CoglColor *constant)
{
  cogl_pipeline_set_layer_combine_constant (COGL_PIPELINE (material),
                                            layer_index,
                                            constant);
}

void
cogl_material_set_layer_matrix (CoglMaterial     *material,
				int               layer_index,
				const CoglMatrix *matrix)
{
  cogl_pipeline_set_layer_matrix (COGL_PIPELINE (material),
                                  layer_index, matrix);
}

const GList *
cogl_material_get_layers (CoglMaterial *material)
{
  return _cogl_pipeline_get_layers (COGL_PIPELINE (material));
}

int
cogl_material_get_n_layers (CoglMaterial *material)
{
  return cogl_pipeline_get_n_layers (COGL_PIPELINE (material));
}

CoglMaterialLayerType
cogl_material_layer_get_type (CoglMaterialLayer *layer)
{
  return COGL_MATERIAL_LAYER_TYPE_TEXTURE;
}

CoglHandle
cogl_material_layer_get_texture (CoglMaterialLayer *layer)
{
  return _cogl_pipeline_layer_get_texture (COGL_PIPELINE_LAYER (layer));
}

CoglMaterialFilter
cogl_material_layer_get_min_filter (CoglMaterialLayer *layer)
{
  return _cogl_pipeline_layer_get_min_filter (COGL_PIPELINE_LAYER (layer));
}

CoglMaterialFilter
cogl_material_layer_get_mag_filter (CoglMaterialLayer *layer)
{
  return _cogl_pipeline_layer_get_mag_filter (COGL_PIPELINE_LAYER (layer));
}

void
cogl_material_set_layer_filters (CoglMaterial      *material,
                                 int                layer_index,
                                 CoglMaterialFilter min_filter,
                                 CoglMaterialFilter mag_filter)
{
  cogl_pipeline_set_layer_filters (COGL_PIPELINE (material),
                                   layer_index,
                                   min_filter,
                                   mag_filter);
}

CoglBool
cogl_material_set_layer_point_sprite_coords_enabled (CoglMaterial *material,
                                                     int           layer_index,
                                                     CoglBool      enable,
                                                     CoglError   **error)
{
  CoglPipeline *pipeline = COGL_PIPELINE (material);
  return cogl_pipeline_set_layer_point_sprite_coords_enabled (pipeline,
                                                              layer_index,
                                                              enable,
                                                              error);
}

CoglBool
cogl_material_get_layer_point_sprite_coords_enabled (CoglMaterial *material,
                                                     int           layer_index)
{
  CoglPipeline *pipeline = COGL_PIPELINE (material);
  return cogl_pipeline_get_layer_point_sprite_coords_enabled (pipeline,
                                                              layer_index);
}

CoglMaterialWrapMode
cogl_material_get_layer_wrap_mode_s (CoglMaterial *material,
                                     int           layer_index)
{
  return cogl_pipeline_get_layer_wrap_mode_s (COGL_PIPELINE (material),
                                              layer_index);
}

void
cogl_material_set_layer_wrap_mode_s (CoglMaterial        *material,
                                     int                  layer_index,
                                     CoglMaterialWrapMode mode)
{
  cogl_pipeline_set_layer_wrap_mode_s (COGL_PIPELINE (material), layer_index,
                                       mode);
}

CoglMaterialWrapMode
cogl_material_get_layer_wrap_mode_t (CoglMaterial *material,
                                     int           layer_index)
{
  return cogl_pipeline_get_layer_wrap_mode_t (COGL_PIPELINE (material),
                                              layer_index);
}

void
cogl_material_set_layer_wrap_mode_t (CoglMaterial        *material,
                                     int                  layer_index,
                                     CoglMaterialWrapMode mode)
{
  cogl_pipeline_set_layer_wrap_mode_t (COGL_PIPELINE (material), layer_index,
                                       mode);
}

CoglMaterialWrapMode
cogl_material_get_layer_wrap_mode_p (CoglMaterial *material,
                                     int           layer_index)
{
  return cogl_pipeline_get_layer_wrap_mode_p (COGL_PIPELINE (material),
                                              layer_index);
}

void
cogl_material_set_layer_wrap_mode_p (CoglMaterial        *material,
                                     int                  layer_index,
                                     CoglMaterialWrapMode mode)
{
  cogl_pipeline_set_layer_wrap_mode_p (COGL_PIPELINE (material), layer_index,
                                       mode);
}

void
cogl_material_set_layer_wrap_mode (CoglMaterial        *material,
                                   int                  layer_index,
                                   CoglMaterialWrapMode mode)
{
  cogl_pipeline_set_layer_wrap_mode (COGL_PIPELINE (material), layer_index,
                                     mode);
}

CoglMaterialWrapMode
cogl_material_layer_get_wrap_mode_s (CoglMaterialLayer *layer)
{
  return _cogl_pipeline_layer_get_wrap_mode_s (COGL_PIPELINE_LAYER (layer));
}

CoglMaterialWrapMode
cogl_material_layer_get_wrap_mode_t (CoglMaterialLayer *layer)
{
  return _cogl_pipeline_layer_get_wrap_mode_t (COGL_PIPELINE_LAYER (layer));
}

CoglMaterialWrapMode
cogl_material_layer_get_wrap_mode_p (CoglMaterialLayer *layer)
{
  return _cogl_pipeline_layer_get_wrap_mode_p (COGL_PIPELINE_LAYER (layer));
}

void
cogl_material_foreach_layer (CoglMaterial *material,
                             CoglMaterialLayerCallback callback,
                             void *user_data)
{
  cogl_pipeline_foreach_layer (COGL_PIPELINE (material),
                               (CoglPipelineLayerCallback)callback, user_data);
}

CoglBool
cogl_material_set_depth_state (CoglMaterial *material,
                               const CoglDepthState *state,
                               CoglError **error)
{
  return cogl_pipeline_set_depth_state (COGL_PIPELINE (material),
                                        state, error);
}

void
cogl_material_get_depth_state (CoglMaterial *material,
                               CoglDepthState *state_out)
{
  cogl_pipeline_get_depth_state (COGL_PIPELINE (material), state_out);
}

