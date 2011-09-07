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
#include "cogl-pipeline-private.h"

#include "string.h"

#ifndef GL_FUNC_ADD
#define GL_FUNC_ADD 0x8006
#endif

CoglPipeline *
_cogl_pipeline_get_user_program (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  g_return_val_if_fail (cogl_is_pipeline (pipeline), NULL);

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_USER_SHADER);

  return authority->big_state->user_program;
}

gboolean
_cogl_pipeline_color_equal (CoglPipeline *authority0,
                            CoglPipeline *authority1)
{
  return cogl_color_equal (&authority0->color, &authority1->color);
}

gboolean
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

gboolean
_cogl_pipeline_alpha_func_state_equal (CoglPipeline *authority0,
                                       CoglPipeline *authority1)
{
  CoglPipelineAlphaFuncState *alpha_state0 =
    &authority0->big_state->alpha_state;
  CoglPipelineAlphaFuncState *alpha_state1 =
    &authority1->big_state->alpha_state;

  return alpha_state0->alpha_func == alpha_state1->alpha_func;
}

gboolean
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

gboolean
_cogl_pipeline_blend_state_equal (CoglPipeline *authority0,
                                  CoglPipeline *authority1)
{
  CoglPipelineBlendState *blend_state0 = &authority0->big_state->blend_state;
  CoglPipelineBlendState *blend_state1 = &authority1->big_state->blend_state;

  _COGL_GET_CONTEXT (ctx, FALSE);

#if defined(HAVE_COGL_GLES2) || defined(HAVE_COGL_GL)
  if (ctx->driver != COGL_DRIVER_GLES1)
    {
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
    }
#endif
  if (blend_state0->blend_src_factor_rgb !=
      blend_state1->blend_src_factor_rgb)
    return FALSE;
  if (blend_state0->blend_dst_factor_rgb !=
      blend_state1->blend_dst_factor_rgb)
    return FALSE;
#if defined(HAVE_COGL_GLES2) || defined(HAVE_COGL_GL)
  if (ctx->driver != COGL_DRIVER_GLES1 &&
      (blend_state0->blend_src_factor_rgb == GL_ONE_MINUS_CONSTANT_COLOR ||
       blend_state0->blend_src_factor_rgb == GL_CONSTANT_COLOR ||
       blend_state0->blend_dst_factor_rgb == GL_ONE_MINUS_CONSTANT_COLOR ||
       blend_state0->blend_dst_factor_rgb == GL_CONSTANT_COLOR))
    {
      if (!cogl_color_equal (&blend_state0->blend_constant,
                             &blend_state1->blend_constant))
        return FALSE;
    }
#endif

  return TRUE;
}

gboolean
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

gboolean
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

gboolean
_cogl_pipeline_point_size_equal (CoglPipeline *authority0,
                                 CoglPipeline *authority1)
{
  return authority0->big_state->point_size == authority1->big_state->point_size;
}

gboolean
_cogl_pipeline_logic_ops_state_equal (CoglPipeline *authority0,
                                      CoglPipeline *authority1)
{
  CoglPipelineLogicOpsState *logic_ops_state0 = &authority0->big_state->logic_ops_state;
  CoglPipelineLogicOpsState *logic_ops_state1 = &authority1->big_state->logic_ops_state;

  return logic_ops_state0->color_mask == logic_ops_state1->color_mask;
}

gboolean
_cogl_pipeline_user_shader_equal (CoglPipeline *authority0,
                                  CoglPipeline *authority1)
{
  return (authority0->big_state->user_program ==
          authority1->big_state->user_program);
}

void
cogl_pipeline_get_color (CoglPipeline *pipeline,
                         CoglColor    *color)
{
  CoglPipeline *authority;

  g_return_if_fail (cogl_is_pipeline (pipeline));

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_COLOR);

  *color = authority->color;
}

/* This is used heavily by the cogl journal when logging quads */
void
_cogl_pipeline_get_colorubv (CoglPipeline *pipeline,
                             guint8       *color)
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

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

  _cogl_pipeline_update_blend_enable (pipeline, state);
}

void
cogl_pipeline_set_color4ub (CoglPipeline *pipeline,
			    guint8 red,
                            guint8 green,
                            guint8 blue,
                            guint8 alpha)
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

  g_return_val_if_fail (cogl_is_pipeline (pipeline), FALSE);

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_BLEND_ENABLE);
  return authority->blend_enable;
}

gboolean
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

  g_return_if_fail (cogl_is_pipeline (pipeline));
  g_return_if_fail (enable > 1 &&
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

  _cogl_pipeline_update_blend_enable (pipeline, state);
}

void
cogl_pipeline_get_ambient (CoglPipeline *pipeline,
                           CoglColor    *ambient)
{
  CoglPipeline *authority;

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

  _cogl_pipeline_update_blend_enable (pipeline, state);
}

void
cogl_pipeline_get_diffuse (CoglPipeline *pipeline,
                           CoglColor    *diffuse)
{
  CoglPipeline *authority;

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

  _cogl_pipeline_update_blend_enable (pipeline, state);
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

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

  _cogl_pipeline_update_blend_enable (pipeline, state);
}

float
cogl_pipeline_get_shininess (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  g_return_val_if_fail (cogl_is_pipeline (pipeline), 0);

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

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

  _cogl_pipeline_update_blend_enable (pipeline, state);
}

static void
_cogl_pipeline_set_alpha_test_function (CoglPipeline *pipeline,
                                        CoglPipelineAlphaFunc alpha_func)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_ALPHA_FUNC;
  CoglPipeline *authority;
  CoglPipelineAlphaFuncState *alpha_state;

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

  g_return_val_if_fail (cogl_is_pipeline (pipeline), 0);

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_ALPHA_FUNC);

  return authority->big_state->alpha_state.alpha_func;
}

float
cogl_pipeline_get_alpha_test_reference (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  g_return_val_if_fail (cogl_is_pipeline (pipeline), 0.0f);

  authority =
    _cogl_pipeline_get_authority (pipeline,
                                  COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE);

  return authority->big_state->alpha_state.alpha_func_reference;
}

GLenum
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

void
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

gboolean
cogl_pipeline_set_blend (CoglPipeline *pipeline,
                         const char *blend_description,
                         GError **error)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_BLEND;
  CoglPipeline *authority;
  CoglBlendStringStatement statements[2];
  CoglBlendStringStatement *rgb;
  CoglBlendStringStatement *a;
  GError *internal_error = NULL;
  int count;
  CoglPipelineBlendState *blend_state;

  _COGL_GET_CONTEXT (ctx, FALSE);

  g_return_val_if_fail (cogl_is_pipeline (pipeline), FALSE);

  count =
    _cogl_blend_string_compile (blend_description,
                                COGL_BLEND_STRING_CONTEXT_BLENDING,
                                statements,
                                &internal_error);
  if (!count)
    {
      if (error)
	g_propagate_error (error, internal_error);
      else
	{
	  g_warning ("Cannot compile blend description: %s\n",
		     internal_error->message);
	  g_error_free (internal_error);
	}
      return FALSE;
    }

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
#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES2)
  if (ctx->driver != COGL_DRIVER_GLES1)
    {
      setup_blend_state (rgb,
                         &blend_state->blend_equation_rgb,
                         &blend_state->blend_src_factor_rgb,
                         &blend_state->blend_dst_factor_rgb);
      setup_blend_state (a,
                         &blend_state->blend_equation_alpha,
                         &blend_state->blend_src_factor_alpha,
                         &blend_state->blend_dst_factor_alpha);
    }
  else
#endif
    {
      setup_blend_state (rgb,
                         NULL,
                         &blend_state->blend_src_factor_rgb,
                         &blend_state->blend_dst_factor_rgb);
    }

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

  _cogl_pipeline_update_blend_enable (pipeline, state);

  return TRUE;
}

void
cogl_pipeline_set_blend_constant (CoglPipeline *pipeline,
                                  const CoglColor *constant_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (cogl_is_pipeline (pipeline));

  if (ctx->driver == COGL_DRIVER_GLES1)
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

    _cogl_pipeline_update_blend_enable (pipeline, state);
  }
#endif
}

CoglHandle
cogl_pipeline_get_user_program (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  g_return_val_if_fail (cogl_is_pipeline (pipeline), COGL_INVALID_HANDLE);

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

  g_return_if_fail (cogl_is_pipeline (pipeline));

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
    {
      _cogl_pipeline_set_fragend (pipeline, COGL_PIPELINE_FRAGEND_DEFAULT);
      _cogl_pipeline_set_vertend (pipeline, COGL_PIPELINE_VERTEND_DEFAULT);
    }

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

  _cogl_pipeline_update_blend_enable (pipeline, state);
}

gboolean
cogl_pipeline_set_depth_state (CoglPipeline *pipeline,
                               const CoglDepthState *depth_state,
                               GError **error)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_DEPTH;
  CoglPipeline *authority;
  CoglDepthState *orig_state;

  _COGL_GET_CONTEXT (ctx, FALSE);

  g_return_val_if_fail (cogl_is_pipeline (pipeline), FALSE);
  g_return_val_if_fail (depth_state->magic == COGL_DEPTH_STATE_MAGIC, FALSE);

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
      g_set_error (error,
                   COGL_ERROR,
                   COGL_ERROR_UNSUPPORTED,
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

  g_return_if_fail (cogl_is_pipeline (pipeline));

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_DEPTH);
  *state = authority->big_state->depth_state;
}

CoglColorMask
cogl_pipeline_get_color_mask (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  g_return_val_if_fail (cogl_is_pipeline (pipeline), 0);

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

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

  g_return_if_fail (cogl_is_pipeline (pipeline));

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

float
cogl_pipeline_get_point_size (CoglPipeline *pipeline)
{
  CoglPipeline *authority;

  g_return_val_if_fail (cogl_is_pipeline (pipeline), FALSE);

  authority =
    _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_POINT_SIZE);

  return authority->big_state->point_size;
}

void
cogl_pipeline_set_point_size (CoglPipeline *pipeline,
                              float point_size)
{
  CoglPipelineState state = COGL_PIPELINE_STATE_POINT_SIZE;
  CoglPipeline *authority;

  g_return_if_fail (cogl_is_pipeline (pipeline));

  authority = _cogl_pipeline_get_authority (pipeline, state);

  if (authority->big_state->point_size == point_size)
    return;

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
  guint8 blend_enable = authority->blend_enable;
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

#if defined(HAVE_COGL_GLES2) || defined(HAVE_COGL_GL)
  if (ctx->driver != COGL_DRIVER_GLES1)
    {
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
    }
#endif

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
      guint8 enabled = depth_state->test_enabled;
      CoglDepthTestFunction function = depth_state->test_function;
      hash = _cogl_util_one_at_a_time_hash (hash, &enabled, sizeof (enabled));
      hash = _cogl_util_one_at_a_time_hash (hash, &function, sizeof (function));
    }

  if (depth_state->write_enabled)
    {
      guint8 enabled = depth_state->write_enabled;
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
_cogl_pipeline_hash_point_size_state (CoglPipeline *authority,
                                      CoglPipelineHashState *state)
{
  float point_size = authority->big_state->point_size;
  state->hash = _cogl_util_one_at_a_time_hash (state->hash, &point_size,
                                               sizeof (point_size));
}

void
_cogl_pipeline_hash_logic_ops_state (CoglPipeline *authority,
                                     CoglPipelineHashState *state)
{
  CoglPipelineLogicOpsState *logic_ops_state = &authority->big_state->logic_ops_state;
  state->hash = _cogl_util_one_at_a_time_hash (state->hash, &logic_ops_state->color_mask,
                                               sizeof (CoglColorMask));
}
