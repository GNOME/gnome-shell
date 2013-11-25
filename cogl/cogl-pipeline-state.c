/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-color-private.h"
#include "cogl-blend-string.h"
#include "cogl-util.h"
#include "cogl-depth-state-private.h"
#include "cogl-pipeline-state-private.h"
#include "cogl-snippet-private.h"
#include "cogl-error-private.h"

#include <test-fixtures/test-unit.h>

#include "string.h"

#ifndef GL_FUNC_ADD
#define GL_FUNC_ADD 0x8006
#endif

CoglPipeline *
_cogl_pipeline_get_user_program (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), NULL);

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_USER_SHADER);

  return authority->big_state->user_program;
}

CoglBool
_cogl_pipeline_color_equal (CoglPipeline *authority0,
                            CoglPipeline *authority1)
{
  return cogl_color_equal (&authority0->color, &authority1->color);
}

CoglBool
_cogl_pipeline_lighting_state_equal (CoglPipeline *authority0,
                                     CoglPipeline *authority1)
{
  CoglPipelineLightingState *state0 = &authority0->big_state->lighting_state;
  CoglPipelineLightingState *state1 = &authority1->big_state->lighting_state;

  if (memcmp (state0->ambient, state1->ambient, sizeof (float) * 4) != 0)
    return FALSE;
  if (memcmp (state0->diffuse, state1->diffuse, sizeof (float) * 4) != 0)
    return FALSE;
  if (memcmp (state0->specular, state1->specular, sizeof (float) * 4) != 0)
    return FALSE;
  if (memcmp (state0->emission, state1->emission, sizeof (float) * 4) != 0)
    return FALSE;
  if (state0->shininess != state1->shininess)
    return FALSE;

  return TRUE;
}

CoglBool
_cogl_pipeline_alpha_func_state_equal (CoglPipeline *authority0,
                                       CoglPipeline *authority1)
{
  CoglPipelineAlphaFuncState *alpha_state0 =
    &authority0->big_state->alpha_state;
  CoglPipelineAlphaFuncState *alpha_state1 =
    &authority1->big_state->alpha_state;

  return alpha_state0->alpha_func == alpha_state1->alpha_func;
}

CoglBool
_cogl_pipeline_alpha_func_reference_state_equal (CoglPipeline *authority0,
                                                 CoglPipeline *authority1)
{
  CoglPipelineAlphaFuncState *alpha_state0 =
    &authority0->big_state->alpha_state;
  CoglPipelineAlphaFuncState *alpha_state1 =
    &authority1->big_state->alpha_state;

  return (alpha_state0->alpha_func_reference ==
          alpha_state1->alpha_func_reference);
}

CoglBool
_cogl_pipeline_blend_state_equal (CoglPipeline *authority0,
                                  CoglPipeline *authority1)
{
  CoglPipelineBlendState *blend_state0 = &authority0->big_state->blend_state;
  CoglPipelineBlendState *blend_state1 = &authority1->big_state->blend_state;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (blend_state0->blend_equation_rgb != blend_state1->blend_equation_rgb)
    return FALSE;

  if (blend_state0->blend_equation_alpha !=
      blend_state1->blend_equation_alpha)
    return FALSE;
  if (blend_state0->blend_src_factor_alpha !=
      blend_state1->blend_src_factor_alpha)
    return FALSE;
  if (blend_state0->blend_dst_factor_alpha !=
      blend_state1->blend_dst_factor_alpha)
    return FALSE;

  if (blend_state0->blend_src_factor_rgb !=
      blend_state1->blend_src_factor_rgb)
    return FALSE;
  if (blend_state0->blend_dst_factor_rgb !=
      blend_state1->blend_dst_factor_rgb)
    return FALSE;

  if (blend_state0->blend_src_factor_rgb == GL_ONE_MINUS_CONSTANT_COLOR ||
      blend_state0->blend_src_factor_rgb == GL_CONSTANT_COLOR ||
      blend_state0->blend_dst_factor_rgb == GL_ONE_MINUS_CONSTANT_COLOR ||
      blend_state0->blend_dst_factor_rgb == GL_CONSTANT_COLOR)
    {
      if (!cogl_color_equal (&blend_state0->blend_constant,
                             &blend_state1->blend_constant))
        return FALSE;
    }

  return TRUE;
}

CoglBool
_cogl_pipeline_depth_state_equal (CoglPipeline *authority0,
                                  CoglPipeline *authority1)
{
  if (authority0->big_state->depth_state.test_enabled == FALSE &&
      authority1->big_state->depth_state.test_enabled == FALSE)
    return TRUE;
  else
    {
      CoglDepthState *s0 = &authority0->big_state->depth_state;
      CoglDepthState *s1 = &authority1->big_state->depth_state;
      return s0->test_enabled == s1->test_enabled &&
             s0->test_function == s1->test_function &&
             s0->write_enabled == s1->write_enabled &&
             s0->range_near == s1->range_near &&
             s0->range_far == s1->range_far;
    }
}

CoglBool
_cogl_pipeline_fog_state_equal (CoglPipeline *authority0,
                                CoglPipeline *authority1)
{
  CoglPipelineFogState *fog_state0 = &authority0->big_state->fog_state;
  CoglPipelineFogState *fog_state1 = &authority1->big_state->fog_state;

  if (fog_state0->enabled == fog_state1->enabled &&
      cogl_color_equal (&fog_state0->color, &fog_state1->color) &&
      fog_state0->mode == fog_state1->mode &&
      fog_state0->density == fog_state1->density &&
      fog_state0->z_near == fog_state1->z_near &&
      fog_state0->z_far == fog_state1->z_far)
    return TRUE;
  else
    return FALSE;
}

CoglBool
_cogl_pipeline_non_zero_point_size_equal (CoglPipeline *authority0,
                                          CoglPipeline *authority1)
{
  return (authority0->big_state->non_zero_point_size ==
          authority1->big_state->non_zero_point_size);
}

CoglBool
_cogl_pipeline_point_size_equal (CoglPipeline *authority0,
                                 CoglPipeline *authority1)
{
  return authority0->big_state->point_size == authority1->big_state->point_size;
}

CoglBool
_cogl_pipeline_per_vertex_point_size_equal (CoglPipeline *authority0,
                                            CoglPipeline *authority1)
{
  return (authority0->big_state->per_vertex_point_size ==
          authority1->big_state->per_vertex_point_size);
}

CoglBool
_cogl_pipeline_logic_ops_state_equal (CoglPipeline *authority0,
                                      CoglPipeline *authority1)
{
  CoglPipelineLogicOpsState *logic_ops_state0 = &authority0->big_state->logic_ops_state;
  CoglPipelineLogicOpsState *logic_ops_state1 = &authority1->big_state->logic_ops_state;

  return logic_ops_state0->color_mask == logic_ops_state1->color_mask;
}

CoglBool
_cogl_pipeline_cull_face_state_equal (CoglPipeline *authority0,
                                      CoglPipeline *authority1)
{
  CoglPipelineCullFaceState *cull_face_state0
    = &authority0->big_state->cull_face_state;
  CoglPipelineCullFaceState *cull_face_state1
    = &authority1->big_state->cull_face_state;

  /* The cull face state is considered equal if two pipelines are both
     set to no culling. If the front winding property is ever used for
     anything else or the comparison is used not just for drawing then
     this would have to change */

  if (cull_face_state0->mode == COGL_PIPELINE_CULL_FACE_MODE_NONE)
    return cull_face_state1->mode == COGL_PIPELINE_CULL_FACE_MODE_NONE;

  return (cull_face_state0->mode == cull_face_state1->mode &&
          cull_face_state0->front_winding == cull_face_state1->front_winding);
}

CoglBool
_cogl_pipeline_user_shader_equal (CoglPipeline *authority0,
                                  CoglPipeline *authority1)
{
  return (authority0->big_state->user_program ==
          authority1->big_state->user_program);
}

typedef struct
{
  const CoglBoxedValue **dst_values;
  const CoglBoxedValue *src_values;
  int override_count;
} GetUniformsClosure;

static CoglBool
get_uniforms_cb (int uniform_num, void *user_data)
{
  GetUniformsClosure *data = user_data;

  if (data->dst_values[uniform_num] == NULL)
    data->dst_values[uniform_num] = data->src_values + data->override_count;

  data->override_count++;

  return TRUE;
}

static void
_cogl_pipeline_get_all_uniform_values (CoglPipeline *pipeline,
                                       const CoglBoxedValue **values)
{
  GetUniformsClosure data;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  memset (values, 0,
          sizeof (const CoglBoxedValue *) * ctx->n_uniform_names);

  data.dst_values = values;

  do
    {
      if ((pipeline->differences & COGL_PIPELINE_STATE_UNIFORMS))
        {
          const CoglPipelineUniformsState *uniforms_state =
            &pipeline->big_state->uniforms_state;

          data.override_count = 0;
          data.src_values = uniforms_state->override_values;

          _cogl_bitmask_foreach (&uniforms_state->override_mask,
                                 get_uniforms_cb,
                                 &data);
        }
      pipeline = _cogl_pipeline_get_parent (pipeline);
    }
  while (pipeline);
}

CoglBool
_cogl_pipeline_uniforms_state_equal (CoglPipeline *authority0,
                                     CoglPipeline *authority1)
{
  unsigned long *differences;
  const CoglBoxedValue **values0, **values1;
  int n_longs;
  int i;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (authority0 == authority1)
    return TRUE;

  values0 = g_alloca (sizeof (const CoglBoxedValue *) * ctx->n_uniform_names);
  values1 = g_alloca (sizeof (const CoglBoxedValue *) * ctx->n_uniform_names);

  n_longs = COGL_FLAGS_N_LONGS_FOR_SIZE (ctx->n_uniform_names);
  differences = g_alloca (n_longs * sizeof (unsigned long));
  memset (differences, 0, sizeof (unsigned long) * n_longs);
  _cogl_pipeline_compare_uniform_differences (differences,
                                              authority0,
                                              authority1);

  _cogl_pipeline_get_all_uniform_values (authority0, values0);
  _cogl_pipeline_get_all_uniform_values (authority1, values1);

  COGL_FLAGS_FOREACH_START (differences, n_longs, i)
    {
      const CoglBoxedValue *value0 = values0[i];
      const CoglBoxedValue *value1 = values1[i];

      if (value0 == NULL)
        {
          if (value1 != NULL && value1->type != COGL_BOXED_NONE)
            return FALSE;
        }
      else if (value1 == NULL)
        {
          if (value0 != NULL && value0->type != COGL_BOXED_NONE)
            return FALSE;
        }
      else if (!_cogl_boxed_value_equal (value0, value1))
        return FALSE;
    }
  COGL_FLAGS_FOREACH_END;

  return TRUE;
}

CoglBool
_cogl_pipeline_vertex_snippets_state_equal (CoglPipeline *authority0,
                                            CoglPipeline *authority1)
{
  return _cogl_pipeline_snippet_list_equal (&authority0->big_state->
                                            vertex_snippets,
                                            &authority1->big_state->
                                            vertex_snippets);
}

CoglBool
_cogl_pipeline_fragment_snippets_state_equal (CoglPipeline *authority0,
                                              CoglPipeline *authority1)
{
  return _cogl_pipeline_snippet_list_equal (&authority0->big_state->
                                            fragment_snippets,
                                            &authority1->big_state->
                                            fragment_snippets);
}

void
cogl_pipeline_get_color (CoglPipeline *pipeline,
                         CoglColor    *color)
{
  CoglPipeline *authority;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_COLOR);

  *color = authority->color;
}

/* This is used heavily by the cogl journal when logging quads */
void
_cogl_pipeline_get_colorubv (CoglPipeline *pipeline,
                             uint8_t *color)
{
  CoglPipeline *authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_COLOR);

  _cogl_color_get_rgba_4ubv (&authority->color, color);
}

void
cogl_pipeline_set_color (CoglPipeline    *pipeline,
			 const CoglColor *color)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_COLOR;
  CoglPipeline *authority;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  if (cogl_color_equal (color, &authority->color))
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, color, FALSE);

  pipeline->color = *color;

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_color_equal);

  pipeline->dirty_real_blend_enable = TRUE;
}

void
cogl_pipeline_set_color4ub (CoglPipeline *pipeline,
			    uint8_t red,
                            uint8_t green,
                            uint8_t blue,
                            uint8_t alpha)
{
  CoglColor color;
  cogl_color_init_from_4ub (&color, red, green, blue, alpha);
  cogl_pipeline_set_color (pipeline, &color);
}

void
cogl_pipeline_set_color4f (CoglPipeline *pipeline,
			   float red,
                           float green,
                           float blue,
                           float alpha)
{
  CoglColor color;
  cogl_color_init_from_4f (&color, red, green, blue, alpha);
  cogl_pipeline_set_color (pipeline, &color);
}

CoglPipelineBlendEnable
_cogl_pipeline_get_blend_enabled (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), FALSE);

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_BLEND_ENABLE);
  return authority->blend_enable;
}

static CoglBool
_cogl_pipeline_blend_enable_equal (CoglPipeline *authority0,
                                   CoglPipeline *authority1)
{
  return authority0->blend_enable == authority1->blend_enable ? TRUE : FALSE;
}

void
_cogl_pipeline_set_blend_enabled (CoglPipeline *pipeline,
                                  CoglPipelineBlendEnable enable)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_BLEND_ENABLE;
  CoglPipeline *authority;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));
  _COGL_RETURN_IF_FAIL (enable > 1 &&
                        "don't pass TRUE or FALSE to _set_blend_enabled!");

  authority = _cogl_pipeline_get_authority (pipeline, state);

  if (authority->blend_enable == enable)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  pipeline->blend_enable = enable;

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_blend_enable_equal);

  pipeline->dirty_real_blend_enable = TRUE;
}

void
cogl_pipeline_get_ambient (CoglPipeline *pipeline,
                           CoglColor    *ambient)
{
  CoglPipeline *authority;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_LIGHTING);

  cogl_color_init_from_4fv (ambient,
                            authority->big_state->lighting_state.ambient);
}

void
cogl_pipeline_set_ambient (CoglPipeline *pipeline,
			   const CoglColor *ambient)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_LIGHTING;
  CoglPipeline *authority;
  CoglPipelineLightingState *lighting_state;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  lighting_state = &authority->big_state->lighting_state;
  if (cogl_color_equal (ambient, &lighting_state->ambient))
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  lighting_state = &pipeline->big_state->lighting_state;
  lighting_state->ambient[0] = cogl_color_get_red_float (ambient);
  lighting_state->ambient[1] = cogl_color_get_green_float (ambient);
  lighting_state->ambient[2] = cogl_color_get_blue_float (ambient);
  lighting_state->ambient[3] = cogl_color_get_alpha_float (ambient);

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_lighting_state_equal);

  pipeline->dirty_real_blend_enable = TRUE;
}

void
cogl_pipeline_get_diffuse (CoglPipeline *pipeline,
                           CoglColor    *diffuse)
{
  CoglPipeline *authority;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_LIGHTING);

  cogl_color_init_from_4fv (diffuse,
                            authority->big_state->lighting_state.diffuse);
}

void
cogl_pipeline_set_diffuse (CoglPipeline *pipeline,
			   const CoglColor *diffuse)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_LIGHTING;
  CoglPipeline *authority;
  CoglPipelineLightingState *lighting_state;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  lighting_state = &authority->big_state->lighting_state;
  if (cogl_color_equal (diffuse, &lighting_state->diffuse))
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  lighting_state = &pipeline->big_state->lighting_state;
  lighting_state->diffuse[0] = cogl_color_get_red_float (diffuse);
  lighting_state->diffuse[1] = cogl_color_get_green_float (diffuse);
  lighting_state->diffuse[2] = cogl_color_get_blue_float (diffuse);
  lighting_state->diffuse[3] = cogl_color_get_alpha_float (diffuse);


  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_lighting_state_equal);

  pipeline->dirty_real_blend_enable = TRUE;
}

void
cogl_pipeline_set_ambient_and_diffuse (CoglPipeline *pipeline,
				       const CoglColor *color)
{
  cogl_pipeline_set_ambient (pipeline, color);
  cogl_pipeline_set_diffuse (pipeline, color);
}

void
cogl_pipeline_get_specular (CoglPipeline *pipeline,
                            CoglColor    *specular)
{
  CoglPipeline *authority;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_LIGHTING);

  cogl_color_init_from_4fv (specular,
                            authority->big_state->lighting_state.specular);
}

void
cogl_pipeline_set_specular (CoglPipeline *pipeline, const CoglColor *specular)
{
  CoglPipeline *authority;
  CoglPipelineState state = COGL_PIPELINE_STATE_LIGHTING;
  CoglPipelineLightingState *lighting_state;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  lighting_state = &authority->big_state->lighting_state;
  if (cogl_color_equal (specular, &lighting_state->specular))
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  lighting_state = &pipeline->big_state->lighting_state;
  lighting_state->specular[0] = cogl_color_get_red_float (specular);
  lighting_state->specular[1] = cogl_color_get_green_float (specular);
  lighting_state->specular[2] = cogl_color_get_blue_float (specular);
  lighting_state->specular[3] = cogl_color_get_alpha_float (specular);

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_lighting_state_equal);

  pipeline->dirty_real_blend_enable = TRUE;
}

float
cogl_pipeline_get_shininess (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), 0);

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_LIGHTING);

  return authority->big_state->lighting_state.shininess;
}

void
cogl_pipeline_set_shininess (CoglPipeline *pipeline,
			     float shininess)
{
  CoglPipeline *authority;
  CoglPipelineState state = COGL_PIPELINE_STATE_LIGHTING;
  CoglPipelineLightingState *lighting_state;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  if (shininess < 0.0)
    {
      g_warning ("Out of range shininess %f supplied for pipeline\n",
                 shininess);
      return;
    }

  authority = _cogl_pipeline_get_authority (pipeline, state);

  lighting_state = &authority->big_state->lighting_state;

  if (lighting_state->shininess == shininess)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  lighting_state = &pipeline->big_state->lighting_state;
  lighting_state->shininess = shininess;

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_lighting_state_equal);
}

void
cogl_pipeline_get_emission (CoglPipeline *pipeline,
                            CoglColor    *emission)
{
  CoglPipeline *authority;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_LIGHTING);

  cogl_color_init_from_4fv (emission,
                            authority->big_state->lighting_state.emission);
}

void
cogl_pipeline_set_emission (CoglPipeline *pipeline, const CoglColor *emission)
{
  CoglPipeline *authority;
  CoglPipelineState state = COGL_PIPELINE_STATE_LIGHTING;
  CoglPipelineLightingState *lighting_state;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  lighting_state = &authority->big_state->lighting_state;
  if (cogl_color_equal (emission, &lighting_state->emission))
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  lighting_state = &pipeline->big_state->lighting_state;
  lighting_state->emission[0] = cogl_color_get_red_float (emission);
  lighting_state->emission[1] = cogl_color_get_green_float (emission);
  lighting_state->emission[2] = cogl_color_get_blue_float (emission);
  lighting_state->emission[3] = cogl_color_get_alpha_float (emission);

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_lighting_state_equal);

  pipeline->dirty_real_blend_enable = TRUE;
}

static void
_cogl_pipeline_set_alpha_test_function (CoglPipeline *pipeline,
                                        CoglPipelineAlphaFunc alpha_func)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_ALPHA_FUNC;
  CoglPipeline *authority;
  CoglPipelineAlphaFuncState *alpha_state;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  alpha_state = &authority->big_state->alpha_state;
  if (alpha_state->alpha_func == alpha_func)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  alpha_state = &pipeline->big_state->alpha_state;
  alpha_state->alpha_func = alpha_func;

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_alpha_func_state_equal);
}

static void
_cogl_pipeline_set_alpha_test_function_reference (CoglPipeline *pipeline,
                                                  float alpha_reference)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE;
  CoglPipeline *authority;
  CoglPipelineAlphaFuncState *alpha_state;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  alpha_state = &authority->big_state->alpha_state;
  if (alpha_state->alpha_func_reference == alpha_reference)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  alpha_state = &pipeline->big_state->alpha_state;
  alpha_state->alpha_func_reference = alpha_reference;

  _cogl_pipeline_update_authority
    (pipeline, authority, state,
     _cogl_pipeline_alpha_func_reference_state_equal);
}

void
cogl_pipeline_set_alpha_test_function (CoglPipeline *pipeline,
				       CoglPipelineAlphaFunc alpha_func,
				       float alpha_reference)
{
  _cogl_pipeline_set_alpha_test_function (pipeline, alpha_func);
  _cogl_pipeline_set_alpha_test_function_reference (pipeline, alpha_reference);
}

CoglPipelineAlphaFunc
cogl_pipeline_get_alpha_test_function (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), 0);

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_ALPHA_FUNC);

  return authority->big_state->alpha_state.alpha_func;
}

float
cogl_pipeline_get_alpha_test_reference (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), 0.0f);

  authority =
    _cogl_pipeline_get_authority (pipeline,
                                  COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE);

  return authority->big_state->alpha_state.alpha_func_reference;
}

static GLenum
arg_to_gl_blend_factor (CoglBlendStringArgument *arg)
{
  if (arg->source.is_zero)
    return GL_ZERO;
  if (arg->factor.is_one)
    return GL_ONE;
  else if (arg->factor.is_src_alpha_saturate)
    return GL_SRC_ALPHA_SATURATE;
  else if (arg->factor.source.info->type ==
           COGL_BLEND_STRING_COLOR_SOURCE_SRC_COLOR)
    {
      if (arg->factor.source.mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_SRC_COLOR;
          else
            return GL_SRC_COLOR;
        }
      else
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_SRC_ALPHA;
          else
            return GL_SRC_ALPHA;
        }
    }
  else if (arg->factor.source.info->type ==
           COGL_BLEND_STRING_COLOR_SOURCE_DST_COLOR)
    {
      if (arg->factor.source.mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_DST_COLOR;
          else
            return GL_DST_COLOR;
        }
      else
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_DST_ALPHA;
          else
            return GL_DST_ALPHA;
        }
    }
#if defined(HAVE_COGL_GLES2) || defined(HAVE_COGL_GL)
  else if (arg->factor.source.info->type ==
           COGL_BLEND_STRING_COLOR_SOURCE_CONSTANT)
    {
      if (arg->factor.source.mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_CONSTANT_COLOR;
          else
            return GL_CONSTANT_COLOR;
        }
      else
        {
          if (arg->factor.source.one_minus)
            return GL_ONE_MINUS_CONSTANT_ALPHA;
          else
            return GL_CONSTANT_ALPHA;
        }
    }
#endif

  g_warning ("Unable to determine valid blend factor from blend string\n");
  return GL_ONE;
}

static void
setup_blend_state (CoglBlendStringStatement *statement,
                   GLenum *blend_equation,
                   GLint *blend_src_factor,
                   GLint *blend_dst_factor)
{
  switch (statement->function->type)
    {
    case COGL_BLEND_STRING_FUNCTION_ADD:
      *blend_equation = GL_FUNC_ADD;
      break;
    /* TODO - add more */
    default:
      g_warning ("Unsupported blend function given");
      *blend_equation = GL_FUNC_ADD;
    }

  *blend_src_factor = arg_to_gl_blend_factor (&statement->args[0]);
  *blend_dst_factor = arg_to_gl_blend_factor (&statement->args[1]);
}

CoglBool
cogl_pipeline_set_blend (CoglPipeline *pipeline,
                         const char *blend_description,
                         CoglError **error)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_BLEND;
  CoglPipeline *authority;
  CoglBlendStringStatement statements[2];
  CoglBlendStringStatement *rgb;
  CoglBlendStringStatement *a;
  int count;
  CoglPipelineBlendState *blend_state;

  _COGL_GET_CONTEXT (ctx, FALSE);

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), FALSE);

  count =
    _cogl_blend_string_compile (blend_description,
                                COGL_BLEND_STRING_CONTEXT_BLENDING,
                                statements,
                                error);
  if (!count)
    return FALSE;

  if (count == 1)
    rgb = a = statements;
  else
    {
      rgb = &statements[0];
      a = &statements[1];
    }

  authority =
    _cogl_pipeline_get_authority (pipeline, state);

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  blend_state = &pipeline->big_state->blend_state;

  setup_blend_state (rgb,
                     &blend_state->blend_equation_rgb,
                     &blend_state->blend_src_factor_rgb,
                     &blend_state->blend_dst_factor_rgb);
  setup_blend_state (a,
                     &blend_state->blend_equation_alpha,
                     &blend_state->blend_src_factor_alpha,
                     &blend_state->blend_dst_factor_alpha);

  /* If we are the current authority see if we can revert to one of our
   * ancestors being the authority */
  if (pipeline == authority &&
      _cogl_pipeline_get_parent (authority) != NULL)
    {
      CoglPipeline *parent = _cogl_pipeline_get_parent (authority);
      CoglPipeline *old_authority =
        _cogl_pipeline_get_authority (parent, state);

      if (_cogl_pipeline_blend_state_equal (authority, old_authority))
        pipeline->differences &= ~state;
    }

  /* If we weren't previously the authority on this state then we need
   * to extended our differences mask and so it's possible that some
   * of our ancestry will now become redundant, so we aim to reparent
   * ourselves if that's true... */
  if (pipeline != authority)
    {
      pipeline->differences |= state;
      _cogl_pipeline_prune_redundant_ancestry (pipeline);
    }

  pipeline->dirty_real_blend_enable = TRUE;

  return TRUE;
}

void
cogl_pipeline_set_blend_constant (CoglPipeline *pipeline,
                                  const CoglColor *constant_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  if (!_cogl_has_private_feature (ctx, COGL_PRIVATE_FEATURE_BLEND_CONSTANT))
    return;

#if defined(HAVE_COGL_GLES2) || defined(HAVE_COGL_GL)
  {
    CoglPipelineState state = COGL_PIPELINE_STATE_BLEND;
    CoglPipeline *authority;
    CoglPipelineBlendState *blend_state;

    authority = _cogl_pipeline_get_authority (pipeline, state);

    blend_state = &authority->big_state->blend_state;
    if (cogl_color_equal (constant_color, &blend_state->blend_constant))
      return;

    /* - Flush journal primitives referencing the current state.
     * - Make sure the pipeline has no dependants so it may be modified.
     * - If the pipeline isn't currently an authority for the state being
     *   changed, then initialize that state from the current authority.
     */
    _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

    blend_state = &pipeline->big_state->blend_state;
    blend_state->blend_constant = *constant_color;

    _cogl_pipeline_update_authority (pipeline, authority, state,
                                     _cogl_pipeline_blend_state_equal);

    pipeline->dirty_real_blend_enable = TRUE;
  }
#endif
}

CoglHandle
cogl_pipeline_get_user_program (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), COGL_INVALID_HANDLE);

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_USER_SHADER);

  return authority->big_state->user_program;
}

/* XXX: for now we don't mind if the program has vertex shaders
 * attached but if we ever make a similar API public we should only
 * allow attaching of programs containing fragment shaders. Eventually
 * we will have a CoglPipeline abstraction to also cover vertex
 * processing.
 */
void
cogl_pipeline_set_user_program (CoglPipeline *pipeline,
                                CoglHandle program)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_USER_SHADER;
  CoglPipeline *authority;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  if (authority->big_state->user_program == program)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  if (program != COGL_INVALID_HANDLE)
    _cogl_pipeline_set_progend (pipeline, COGL_PIPELINE_PROGEND_UNDEFINED);

  /* If we are the current authority see if we can revert to one of our
   * ancestors being the authority */
  if (pipeline == authority &&
      _cogl_pipeline_get_parent (authority) != NULL)
    {
      CoglPipeline *parent = _cogl_pipeline_get_parent (authority);
      CoglPipeline *old_authority =
        _cogl_pipeline_get_authority (parent, state);

      if (old_authority->big_state->user_program == program)
        pipeline->differences &= ~state;
    }
  else if (pipeline != authority)
    {
      /* If we weren't previously the authority on this state then we
       * need to extended our differences mask and so it's possible
       * that some of our ancestry will now become redundant, so we
       * aim to reparent ourselves if that's true... */
      pipeline->differences |= state;
      _cogl_pipeline_prune_redundant_ancestry (pipeline);
    }

  if (program != COGL_INVALID_HANDLE)
    cogl_handle_ref (program);
  if (authority == pipeline &&
      pipeline->big_state->user_program != COGL_INVALID_HANDLE)
    cogl_handle_unref (pipeline->big_state->user_program);
  pipeline->big_state->user_program = program;

  pipeline->dirty_real_blend_enable = TRUE;
}

CoglBool
cogl_pipeline_set_depth_state (CoglPipeline *pipeline,
                               const CoglDepthState *depth_state,
                               CoglError **error)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_DEPTH;
  CoglPipeline *authority;
  CoglDepthState *orig_state;

  _COGL_GET_CONTEXT (ctx, FALSE);

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), FALSE);
  _COGL_RETURN_VAL_IF_FAIL (depth_state->magic == COGL_DEPTH_STATE_MAGIC, FALSE);

  authority = _cogl_pipeline_get_authority (pipeline, state);

  orig_state = &authority->big_state->depth_state;
  if (orig_state->test_enabled == depth_state->test_enabled &&
      orig_state->write_enabled == depth_state->write_enabled &&
      orig_state->test_function == depth_state->test_function &&
      orig_state->range_near == depth_state->range_near &&
      orig_state->range_far == depth_state->range_far)
    return TRUE;

  if (ctx->driver == COGL_DRIVER_GLES1 &&
      (depth_state->range_near != 0 ||
       depth_state->range_far != 1))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "glDepthRange not available on GLES 1");
      return FALSE;
    }

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  pipeline->big_state->depth_state = *depth_state;

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_depth_state_equal);

  return TRUE;
}

void
cogl_pipeline_get_depth_state (CoglPipeline *pipeline,
                               CoglDepthState *state)
{
  CoglPipeline *authority;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_DEPTH);
  *state = authority->big_state->depth_state;
}

CoglColorMask
cogl_pipeline_get_color_mask (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), 0);

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_LOGIC_OPS);

  return authority->big_state->logic_ops_state.color_mask;
}

void
cogl_pipeline_set_color_mask (CoglPipeline *pipeline,
                              CoglColorMask color_mask)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_LOGIC_OPS;
  CoglPipeline *authority;
  CoglPipelineLogicOpsState *logic_ops_state;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  logic_ops_state = &authority->big_state->logic_ops_state;
  if (logic_ops_state->color_mask == color_mask)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  logic_ops_state = &pipeline->big_state->logic_ops_state;
  logic_ops_state->color_mask = color_mask;

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_logic_ops_state_equal);
}

void
_cogl_pipeline_set_fog_state (CoglPipeline *pipeline,
                              const CoglPipelineFogState *fog_state)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_FOG;
  CoglPipeline *authority;
  CoglPipelineFogState *current_fog_state;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  current_fog_state = &authority->big_state->fog_state;

  if (current_fog_state->enabled == fog_state->enabled &&
      cogl_color_equal (&current_fog_state->color, &fog_state->color) &&
      current_fog_state->mode == fog_state->mode &&
      current_fog_state->density == fog_state->density &&
      current_fog_state->z_near == fog_state->z_near &&
      current_fog_state->z_far == fog_state->z_far)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  pipeline->big_state->fog_state = *fog_state;

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_fog_state_equal);
}

void
cogl_pipeline_set_cull_face_mode (CoglPipeline *pipeline,
                                  CoglPipelineCullFaceMode cull_face_mode)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_CULL_FACE;
  CoglPipeline *authority;
  CoglPipelineCullFaceState *cull_face_state;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  cull_face_state = &authority->big_state->cull_face_state;

  if (cull_face_state->mode == cull_face_mode)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  pipeline->big_state->cull_face_state.mode = cull_face_mode;

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_cull_face_state_equal);
}

void
cogl_pipeline_set_front_face_winding (CoglPipeline *pipeline,
                                      CoglWinding front_winding)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_CULL_FACE;
  CoglPipeline *authority;
  CoglPipelineCullFaceState *cull_face_state;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  cull_face_state = &authority->big_state->cull_face_state;

  if (cull_face_state->front_winding == front_winding)
    return;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  pipeline->big_state->cull_face_state.front_winding = front_winding;

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_cull_face_state_equal);
}

CoglPipelineCullFaceMode
cogl_pipeline_get_cull_face_mode (CoglPipeline *pipeline)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_CULL_FACE;
  CoglPipeline *authority;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline),
                            COGL_PIPELINE_CULL_FACE_MODE_NONE);

  authority = _cogl_pipeline_get_authority (pipeline, state);

  return authority->big_state->cull_face_state.mode;
}

CoglWinding
cogl_pipeline_get_front_face_winding (CoglPipeline *pipeline)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_CULL_FACE;
  CoglPipeline *authority;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline),
                            COGL_PIPELINE_CULL_FACE_MODE_NONE);

  authority = _cogl_pipeline_get_authority (pipeline, state);

  return authority->big_state->cull_face_state.front_winding;
}

float
cogl_pipeline_get_point_size (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), FALSE);

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_POINT_SIZE);

  return authority->big_state->point_size;
}

static void
_cogl_pipeline_set_non_zero_point_size (CoglPipeline *pipeline,
                                        CoglBool value)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_NON_ZERO_POINT_SIZE;
  CoglPipeline *authority;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  pipeline->big_state->non_zero_point_size = !!value;

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_non_zero_point_size_equal);
}

void
cogl_pipeline_set_point_size (CoglPipeline *pipeline,
                              float point_size)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_POINT_SIZE;
  CoglPipeline *authority;

  _COGL_RETURN_IF_FAIL (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  if (authority->big_state->point_size == point_size)
    return;

  /* Changing the point size may additionally modify
   * COGL_PIPELINE_STATE_NON_ZERO_POINT_SIZE. */

  if ((authority->big_state->point_size > 0.0f) != (point_size > 0.0f))
    _cogl_pipeline_set_non_zero_point_size (pipeline, point_size > 0.0f);

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  pipeline->big_state->point_size = point_size;

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_point_size_equal);
}

CoglBool
cogl_pipeline_set_per_vertex_point_size (CoglPipeline *pipeline,
                                         CoglBool enable,
                                         CoglError **error)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_PER_VERTEX_POINT_SIZE;
  CoglPipeline *authority;

  _COGL_GET_CONTEXT (ctx, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), FALSE);

  authority = _cogl_pipeline_get_authority (pipeline, state);

  enable = !!enable;

  if (authority->big_state->per_vertex_point_size == enable)
    return TRUE;

  if (enable && !cogl_has_feature (ctx, COGL_FEATURE_ID_PER_VERTEX_POINT_SIZE))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "Per-vertex point size is not supported");

      return FALSE;
    }

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  pipeline->big_state->per_vertex_point_size = enable;

  _cogl_pipeline_update_authority (pipeline, authority, state,
                                   _cogl_pipeline_point_size_equal);

  return TRUE;
}

CoglBool
cogl_pipeline_get_per_vertex_point_size (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_pipeline (pipeline), FALSE);

  authority =
    _cogl_pipeline_get_authority (pipeline,
                                  COGL_PIPELINE_STATE_PER_VERTEX_POINT_SIZE);

  return authority->big_state->per_vertex_point_size;
}

static CoglBoxedValue *
_cogl_pipeline_override_uniform (CoglPipeline *pipeline,
                                 int location)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_UNIFORMS;
  CoglPipelineUniformsState *uniforms_state;
  int override_index;

  _COGL_GET_CONTEXT (ctx, NULL);

  g_return_val_if_fail (cogl_is_pipeline (pipeline), NULL);
  g_return_val_if_fail (location >= 0, NULL);
  g_return_val_if_fail (location < ctx->n_uniform_names, NULL);

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  uniforms_state = &pipeline->big_state->uniforms_state;

  /* Count the number of bits that are set below this location. That
     should give us the position where our new value should lie */
  override_index = _cogl_bitmask_popcount_upto (&uniforms_state->override_mask,
                                                location);

  _cogl_bitmask_set (&uniforms_state->changed_mask, location, TRUE);

  /* If this pipeline already has an override for this value then we
     can just use it directly */
  if (_cogl_bitmask_get (&uniforms_state->override_mask, location))
    return uniforms_state->override_values + override_index;

  /* We need to create a new override value in the right position
     within the array. This is pretty inefficient but the hope is that
     it will be much more common to modify an existing uniform rather
     than modify a new one so it is more important to optimise the
     former case. */

  if (uniforms_state->override_values == NULL)
    {
      g_assert (override_index == 0);
      uniforms_state->override_values = g_new (CoglBoxedValue, 1);
    }
  else
    {
      /* We need to grow the array and copy in the old values */
      CoglBoxedValue *old_values = uniforms_state->override_values;
      int old_size = _cogl_bitmask_popcount (&uniforms_state->override_mask);

      uniforms_state->override_values = g_new (CoglBoxedValue, old_size + 1);

      /* Copy in the old values leaving a gap for the new value */
      memcpy (uniforms_state->override_values,
              old_values,
              sizeof (CoglBoxedValue) * override_index);
      memcpy (uniforms_state->override_values + override_index + 1,
              old_values + override_index,
              sizeof (CoglBoxedValue) * (old_size - override_index));

      g_free (old_values);
    }

  _cogl_boxed_value_init (uniforms_state->override_values + override_index);

  _cogl_bitmask_set (&uniforms_state->override_mask, location, TRUE);

  return uniforms_state->override_values + override_index;
}

void
cogl_pipeline_set_uniform_1f (CoglPipeline *pipeline,
                              int uniform_location,
                              float value)
{
  CoglBoxedValue *boxed_value;

  boxed_value = _cogl_pipeline_override_uniform (pipeline, uniform_location);

  _cogl_boxed_value_set_1f (boxed_value, value);
}

void
cogl_pipeline_set_uniform_1i (CoglPipeline *pipeline,
                              int uniform_location,
                              int value)
{
  CoglBoxedValue *boxed_value;

  boxed_value = _cogl_pipeline_override_uniform (pipeline, uniform_location);

  _cogl_boxed_value_set_1i (boxed_value, value);
}

void
cogl_pipeline_set_uniform_float (CoglPipeline *pipeline,
                                 int uniform_location,
                                 int n_components,
                                 int count,
                                 const float *value)
{
  CoglBoxedValue *boxed_value;

  boxed_value = _cogl_pipeline_override_uniform (pipeline, uniform_location);

  _cogl_boxed_value_set_float (boxed_value, n_components, count, value);
}

void
cogl_pipeline_set_uniform_int (CoglPipeline *pipeline,
                               int uniform_location,
                               int n_components,
                               int count,
                               const int *value)
{
  CoglBoxedValue *boxed_value;

  boxed_value = _cogl_pipeline_override_uniform (pipeline, uniform_location);

  _cogl_boxed_value_set_int (boxed_value, n_components, count, value);
}

void
cogl_pipeline_set_uniform_matrix (CoglPipeline *pipeline,
                                  int uniform_location,
                                  int dimensions,
                                  int count,
                                  CoglBool transpose,
                                  const float *value)
{
  CoglBoxedValue *boxed_value;

  boxed_value = _cogl_pipeline_override_uniform (pipeline, uniform_location);

  _cogl_boxed_value_set_matrix (boxed_value,
                                dimensions,
                                count,
                                transpose,
                                value);
}

static void
_cogl_pipeline_add_vertex_snippet (CoglPipeline *pipeline,
                                   CoglSnippet *snippet)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_VERTEX_SNIPPETS;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  _cogl_pipeline_snippet_list_add (&pipeline->big_state->vertex_snippets,
                                   snippet);
}

static void
_cogl_pipeline_add_fragment_snippet (CoglPipeline *pipeline,
                                     CoglSnippet *snippet)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS;

  /* - Flush journal primitives referencing the current state.
   * - Make sure the pipeline has no dependants so it may be modified.
   * - If the pipeline isn't currently an authority for the state being
   *   changed, then initialize that state from the current authority.
   */
  _cogl_pipeline_pre_change_notify (pipeline, state, NULL, FALSE);

  _cogl_pipeline_snippet_list_add (&pipeline->big_state->fragment_snippets,
                                   snippet);
}

void
cogl_pipeline_add_snippet (CoglPipeline *pipeline,
                           CoglSnippet *snippet)
{
  g_return_if_fail (cogl_is_pipeline (pipeline));
  g_return_if_fail (cogl_is_snippet (snippet));
  g_return_if_fail (snippet->hook < COGL_SNIPPET_FIRST_LAYER_HOOK);

  if (snippet->hook < COGL_SNIPPET_FIRST_PIPELINE_FRAGMENT_HOOK)
    _cogl_pipeline_add_vertex_snippet (pipeline, snippet);
  else
    _cogl_pipeline_add_fragment_snippet (pipeline, snippet);
}

CoglBool
_cogl_pipeline_has_non_layer_vertex_snippets (CoglPipeline *pipeline)
{
  CoglPipeline *authority =
    _cogl_pipeline_get_authority (pipeline,
                                  COGL_PIPELINE_STATE_VERTEX_SNIPPETS);

  return authority->big_state->vertex_snippets.entries != NULL;
}

static CoglBool
check_layer_has_vertex_snippet (CoglPipelineLayer *layer,
                                void *user_data)
{
  unsigned long state = COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS;
  CoglPipelineLayer *authority =
    _cogl_pipeline_layer_get_authority (layer, state);
  CoglBool *found_vertex_snippet = user_data;

  if (authority->big_state->vertex_snippets.entries)
    {
      *found_vertex_snippet = TRUE;
      return FALSE;
    }

  return TRUE;
}

CoglBool
_cogl_pipeline_has_vertex_snippets (CoglPipeline *pipeline)
{
  CoglBool found_vertex_snippet = FALSE;

  if (_cogl_pipeline_has_non_layer_vertex_snippets (pipeline))
    return TRUE;

  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         check_layer_has_vertex_snippet,
                                         &found_vertex_snippet);

  return found_vertex_snippet;
}

CoglBool
_cogl_pipeline_has_non_layer_fragment_snippets (CoglPipeline *pipeline)
{
  CoglPipeline *authority =
    _cogl_pipeline_get_authority (pipeline,
                                  COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS);

  return authority->big_state->fragment_snippets.entries != NULL;
}

static CoglBool
check_layer_has_fragment_snippet (CoglPipelineLayer *layer,
                                  void *user_data)
{
  unsigned long state = COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS;
  CoglPipelineLayer *authority =
    _cogl_pipeline_layer_get_authority (layer, state);
  CoglBool *found_fragment_snippet = user_data;

  if (authority->big_state->fragment_snippets.entries)
    {
      *found_fragment_snippet = TRUE;
      return FALSE;
    }

  return TRUE;
}

CoglBool
_cogl_pipeline_has_fragment_snippets (CoglPipeline *pipeline)
{
  CoglBool found_fragment_snippet = FALSE;

  if (_cogl_pipeline_has_non_layer_fragment_snippets (pipeline))
    return TRUE;

  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         check_layer_has_fragment_snippet,
                                         &found_fragment_snippet);

  return found_fragment_snippet;
}

void
_cogl_pipeline_hash_color_state (CoglPipeline *authority,
                                 CoglPipelineHashState *state)
{
  state->hash = _cogl_util_one_at_a_time_hash (state->hash, &authority->color,
                                               _COGL_COLOR_DATA_SIZE);
}

void
_cogl_pipeline_hash_blend_enable_state (CoglPipeline *authority,
                                        CoglPipelineHashState *state)
{
  uint8_t blend_enable = authority->blend_enable;
  state->hash = _cogl_util_one_at_a_time_hash (state->hash, &blend_enable, 1);
}

void
_cogl_pipeline_hash_lighting_state (CoglPipeline *authority,
                                    CoglPipelineHashState *state)
{
  CoglPipelineLightingState *lighting_state =
    &authority->big_state->lighting_state;
  state->hash =
    _cogl_util_one_at_a_time_hash (state->hash, lighting_state,
                                   sizeof (CoglPipelineLightingState));
}

void
_cogl_pipeline_hash_alpha_func_state (CoglPipeline *authority,
                                      CoglPipelineHashState *state)
{
  CoglPipelineAlphaFuncState *alpha_state = &authority->big_state->alpha_state;
  state->hash =
    _cogl_util_one_at_a_time_hash (state->hash, &alpha_state->alpha_func,
                                   sizeof (alpha_state->alpha_func));
}

void
_cogl_pipeline_hash_alpha_func_reference_state (CoglPipeline *authority,
                                                CoglPipelineHashState *state)
{
  CoglPipelineAlphaFuncState *alpha_state = &authority->big_state->alpha_state;
  float ref = alpha_state->alpha_func_reference;
  state->hash =
    _cogl_util_one_at_a_time_hash (state->hash, &ref, sizeof (float));
}

void
_cogl_pipeline_hash_blend_state (CoglPipeline *authority,
                                 CoglPipelineHashState *state)
{
  CoglPipelineBlendState *blend_state = &authority->big_state->blend_state;
  unsigned int hash;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!authority->real_blend_enable)
    return;

  hash = state->hash;

  hash =
    _cogl_util_one_at_a_time_hash (hash, &blend_state->blend_equation_rgb,
                                   sizeof (blend_state->blend_equation_rgb));
  hash =
    _cogl_util_one_at_a_time_hash (hash, &blend_state->blend_equation_alpha,
                                   sizeof (blend_state->blend_equation_alpha));
  hash =
    _cogl_util_one_at_a_time_hash (hash, &blend_state->blend_src_factor_alpha,
                                   sizeof (blend_state->blend_src_factor_alpha));
  hash =
    _cogl_util_one_at_a_time_hash (hash, &blend_state->blend_dst_factor_alpha,
                                   sizeof (blend_state->blend_dst_factor_alpha));

  if (blend_state->blend_src_factor_rgb == GL_ONE_MINUS_CONSTANT_COLOR ||
      blend_state->blend_src_factor_rgb == GL_CONSTANT_COLOR ||
      blend_state->blend_dst_factor_rgb == GL_ONE_MINUS_CONSTANT_COLOR ||
      blend_state->blend_dst_factor_rgb == GL_CONSTANT_COLOR)
    {
      hash =
        _cogl_util_one_at_a_time_hash (hash, &blend_state->blend_constant,
                                       sizeof (blend_state->blend_constant));
    }

  hash =
    _cogl_util_one_at_a_time_hash (hash, &blend_state->blend_src_factor_rgb,
                                   sizeof (blend_state->blend_src_factor_rgb));
  hash =
    _cogl_util_one_at_a_time_hash (hash, &blend_state->blend_dst_factor_rgb,
                                   sizeof (blend_state->blend_dst_factor_rgb));

  state->hash = hash;
}

void
_cogl_pipeline_hash_user_shader_state (CoglPipeline *authority,
                                       CoglPipelineHashState *state)
{
  CoglHandle user_program = authority->big_state->user_program;
  state->hash = _cogl_util_one_at_a_time_hash (state->hash, &user_program,
                                               sizeof (user_program));
}

void
_cogl_pipeline_hash_depth_state (CoglPipeline *authority,
                                 CoglPipelineHashState *state)
{
  CoglDepthState *depth_state = &authority->big_state->depth_state;
  unsigned int hash = state->hash;

  if (depth_state->test_enabled)
    {
      uint8_t enabled = depth_state->test_enabled;
      CoglDepthTestFunction function = depth_state->test_function;
      hash = _cogl_util_one_at_a_time_hash (hash, &enabled, sizeof (enabled));
      hash = _cogl_util_one_at_a_time_hash (hash, &function, sizeof (function));
    }

  if (depth_state->write_enabled)
    {
      uint8_t enabled = depth_state->write_enabled;
      float near_val = depth_state->range_near;
      float far_val = depth_state->range_far;
      hash = _cogl_util_one_at_a_time_hash (hash, &enabled, sizeof (enabled));
      hash = _cogl_util_one_at_a_time_hash (hash, &near_val, sizeof (near_val));
      hash = _cogl_util_one_at_a_time_hash (hash, &far_val, sizeof (far_val));
    }

  state->hash = hash;
}

void
_cogl_pipeline_hash_fog_state (CoglPipeline *authority,
                               CoglPipelineHashState *state)
{
  CoglPipelineFogState *fog_state = &authority->big_state->fog_state;
  unsigned long hash = state->hash;

  if (!fog_state->enabled)
    hash = _cogl_util_one_at_a_time_hash (hash, &fog_state->enabled,
                                          sizeof (fog_state->enabled));
  else
    hash = _cogl_util_one_at_a_time_hash (hash, &fog_state,
                                          sizeof (CoglPipelineFogState));

  state->hash = hash;
}

void
_cogl_pipeline_hash_non_zero_point_size_state (CoglPipeline *authority,
                                               CoglPipelineHashState *state)
{
  CoglBool non_zero_point_size = authority->big_state->non_zero_point_size;

  state->hash = _cogl_util_one_at_a_time_hash (state->hash,
                                               &non_zero_point_size,
                                               sizeof (non_zero_point_size));
}

void
_cogl_pipeline_hash_point_size_state (CoglPipeline *authority,
                                      CoglPipelineHashState *state)
{
  float point_size = authority->big_state->point_size;
  state->hash = _cogl_util_one_at_a_time_hash (state->hash, &point_size,
                                               sizeof (point_size));
}

void
_cogl_pipeline_hash_per_vertex_point_size_state (CoglPipeline *authority,
                                                 CoglPipelineHashState *state)
{
  CoglBool per_vertex_point_size = authority->big_state->per_vertex_point_size;
  state->hash = _cogl_util_one_at_a_time_hash (state->hash,
                                               &per_vertex_point_size,
                                               sizeof (per_vertex_point_size));
}

void
_cogl_pipeline_hash_logic_ops_state (CoglPipeline *authority,
                                     CoglPipelineHashState *state)
{
  CoglPipelineLogicOpsState *logic_ops_state = &authority->big_state->logic_ops_state;
  state->hash = _cogl_util_one_at_a_time_hash (state->hash, &logic_ops_state->color_mask,
                                               sizeof (CoglColorMask));
}

void
_cogl_pipeline_hash_cull_face_state (CoglPipeline *authority,
                                     CoglPipelineHashState *state)
{
  CoglPipelineCullFaceState *cull_face_state
    = &authority->big_state->cull_face_state;

  /* The cull face state is considered equal if two pipelines are both
     set to no culling. If the front winding property is ever used for
     anything else or the hashing is used not just for drawing then
     this would have to change */
  if (cull_face_state->mode == COGL_PIPELINE_CULL_FACE_MODE_NONE)
    state->hash =
      _cogl_util_one_at_a_time_hash (state->hash,
                                     &cull_face_state->mode,
                                     sizeof (CoglPipelineCullFaceMode));
  else
    state->hash =
      _cogl_util_one_at_a_time_hash (state->hash,
                                     cull_face_state,
                                     sizeof (CoglPipelineCullFaceState));
}

void
_cogl_pipeline_hash_uniforms_state (CoglPipeline *authority,
                                    CoglPipelineHashState *state)
{
  /* This isn't used anywhere yet because the uniform state doesn't
     affect program generation. It's quite a hassle to implement so
     let's just leave it until something actually needs it */
  g_warn_if_reached ();
}

void
_cogl_pipeline_compare_uniform_differences (unsigned long *differences,
                                            CoglPipeline *pipeline0,
                                            CoglPipeline *pipeline1)
{
  GSList *head0 = NULL;
  GSList *head1 = NULL;
  CoglPipeline *node0;
  CoglPipeline *node1;
  int len0 = 0;
  int len1 = 0;
  int count;
  GSList *common_ancestor0;
  GSList *common_ancestor1;

  /* This algorithm is copied from
     _cogl_pipeline_compare_differences(). It might be nice to share
     the code more */

  for (node0 = pipeline0; node0; node0 = _cogl_pipeline_get_parent (node0))
    {
      GSList *link = alloca (sizeof (GSList));
      link->next = head0;
      link->data = node0;
      head0 = link;
      len0++;
    }
  for (node1 = pipeline1; node1; node1 = _cogl_pipeline_get_parent (node1))
    {
      GSList *link = alloca (sizeof (GSList));
      link->next = head1;
      link->data = node1;
      head1 = link;
      len1++;
    }

  /* NB: There's no point looking at the head entries since we know both
   * pipelines must have the same default pipeline as their root node. */
  common_ancestor0 = head0;
  common_ancestor1 = head1;
  head0 = head0->next;
  head1 = head1->next;
  count = MIN (len0, len1) - 1;
  while (count--)
    {
      if (head0->data != head1->data)
        break;
      common_ancestor0 = head0;
      common_ancestor1 = head1;
      head0 = head0->next;
      head1 = head1->next;
    }

  for (head0 = common_ancestor0->next; head0; head0 = head0->next)
    {
      node0 = head0->data;
      if ((node0->differences & COGL_PIPELINE_STATE_UNIFORMS))
        {
          const CoglPipelineUniformsState *uniforms_state =
            &node0->big_state->uniforms_state;
          _cogl_bitmask_set_flags (&uniforms_state->override_mask,
                                   differences);
        }
    }
  for (head1 = common_ancestor1->next; head1; head1 = head1->next)
    {
      node1 = head1->data;
      if ((node1->differences & COGL_PIPELINE_STATE_UNIFORMS))
        {
          const CoglPipelineUniformsState *uniforms_state =
            &node1->big_state->uniforms_state;
          _cogl_bitmask_set_flags (&uniforms_state->override_mask,
                                   differences);
        }
    }
}

void
_cogl_pipeline_hash_vertex_snippets_state (CoglPipeline *authority,
                                           CoglPipelineHashState *state)
{
  _cogl_pipeline_snippet_list_hash (&authority->big_state->vertex_snippets,
                                    &state->hash);
}

void
_cogl_pipeline_hash_fragment_snippets_state (CoglPipeline *authority,
                                             CoglPipelineHashState *state)
{
  _cogl_pipeline_snippet_list_hash (&authority->big_state->fragment_snippets,
                                    &state->hash);
}

UNIT_TEST (check_blend_constant_ancestry,
           0 /* no requirements */,
           0 /* no known failures */)
{
  CoglPipeline *pipeline = cogl_pipeline_new (test_ctx);
  CoglNode *node;
  int pipeline_length = 0;
  int i;

  /* Repeatedly making a copy of a pipeline and changing the same
   * state (in this case the blend constant) shouldn't cause a long
   * chain of pipelines to be created because the redundant ancestry
   * should be pruned. */

  for (i = 0; i < 20; i++)
    {
      CoglColor color;
      CoglPipeline *tmp_pipeline;

      cogl_color_init_from_4f (&color, i / 20.0f, 0.0f, 0.0f, 1.0f);

      tmp_pipeline = cogl_pipeline_copy (pipeline);
      cogl_object_unref (pipeline);
      pipeline = tmp_pipeline;

      cogl_pipeline_set_blend_constant (pipeline, &color);
    }

  for (node = (CoglNode *) pipeline; node; node = node->parent)
    pipeline_length++;

  g_assert_cmpint (pipeline_length, <=, 2);

  cogl_object_unref (pipeline);
}

UNIT_TEST (check_uniform_ancestry,
           0 /* no requirements */,
           TEST_KNOWN_FAILURE)
{
  CoglPipeline *pipeline = cogl_pipeline_new (test_ctx);
  CoglNode *node;
  int pipeline_length = 0;
  int i;

  /* Repeatedly making a copy of a pipeline and changing a uniform
   * shouldn't cause a long chain of pipelines to be created */

  for (i = 0; i < 20; i++)
    {
      CoglPipeline *tmp_pipeline;
      int uniform_location;

      tmp_pipeline = cogl_pipeline_copy (pipeline);
      cogl_object_unref (pipeline);
      pipeline = tmp_pipeline;

      uniform_location =
        cogl_pipeline_get_uniform_location (pipeline, "a_uniform");

      cogl_pipeline_set_uniform_1i (pipeline, uniform_location, i);
    }

  for (node = (CoglNode *) pipeline; node; node = node->parent)
    pipeline_length++;

  g_assert_cmpint (pipeline_length, <=, 2);

  cogl_object_unref (pipeline);
}
