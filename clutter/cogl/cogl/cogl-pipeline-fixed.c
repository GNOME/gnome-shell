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

#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"

#ifdef COGL_PIPELINE_BACKEND_FIXED

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-handle.h"

#include "cogl-texture-private.h"
#include "cogl-blend-string.h"
#include "cogl-profile.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>

const CoglPipelineBackend _cogl_pipeline_fixed_backend;

static int
_cogl_pipeline_backend_fixed_get_max_texture_units (void)
{
  _COGL_GET_CONTEXT (ctx, 0);

  /* This function is called quite often so we cache the value to
     avoid too many GL calls */
  if (ctx->max_texture_units == -1)
    {
      ctx->max_texture_units = 1;
      GE (glGetIntegerv (GL_MAX_TEXTURE_UNITS,
                         &ctx->max_texture_units));
    }

  return ctx->max_texture_units;
}

static gboolean
_cogl_pipeline_backend_fixed_start (CoglPipeline *pipeline,
                                    int n_layers,
                                    unsigned long pipelines_difference,
                                    int n_tex_coord_attribs)
{
  CoglHandle user_program;

  if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_DISABLE_FIXED))
    return FALSE;

  user_program = cogl_pipeline_get_user_program (pipeline);
  if (user_program != COGL_INVALID_HANDLE)
    return FALSE;

  _cogl_use_program (0, COGL_PIPELINE_PROGRAM_TYPE_FIXED);
  return TRUE;
}

static gboolean
_cogl_pipeline_backend_fixed_add_layer (CoglPipeline *pipeline,
                                        CoglPipelineLayer *layer,
                                        unsigned long layers_difference)
{
  CoglTextureUnit *unit =
    _cogl_get_texture_unit (_cogl_pipeline_layer_get_unit_index (layer));
  int unit_index = unit->index;
  int n_rgb_func_args;
  int n_alpha_func_args;

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* XXX: Beware that since we are changing the active texture unit we
   * must make sure we don't call into other Cogl components that may
   * temporarily bind texture objects to query/modify parameters since
   * they will end up binding texture unit 1. See
   * _cogl_bind_gl_texture_transient for more details.
   */
  _cogl_set_active_texture_unit (unit_index);

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_COMBINE)
    {
      CoglPipelineLayer *authority =
        _cogl_pipeline_layer_get_authority (layer,
                                            COGL_PIPELINE_LAYER_STATE_COMBINE);
      CoglPipelineLayerBigState *big_state = authority->big_state;

      GE (glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE));

      /* Set the combiner functions... */
      GE (glTexEnvi (GL_TEXTURE_ENV,
                     GL_COMBINE_RGB,
                     big_state->texture_combine_rgb_func));
      GE (glTexEnvi (GL_TEXTURE_ENV,
                     GL_COMBINE_ALPHA,
                     big_state->texture_combine_alpha_func));

      /*
       * Setup the function arguments...
       */

      /* For the RGB components... */
      n_rgb_func_args =
        _cogl_get_n_args_for_combine_func (big_state->texture_combine_rgb_func);

      GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB,
                     big_state->texture_combine_rgb_src[0]));
      GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB,
                     big_state->texture_combine_rgb_op[0]));
      if (n_rgb_func_args > 1)
        {
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB,
                         big_state->texture_combine_rgb_src[1]));
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB,
                         big_state->texture_combine_rgb_op[1]));
        }
      if (n_rgb_func_args > 2)
        {
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC2_RGB,
                         big_state->texture_combine_rgb_src[2]));
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_RGB,
                         big_state->texture_combine_rgb_op[2]));
        }

      /* For the Alpha component */
      n_alpha_func_args =
        _cogl_get_n_args_for_combine_func (big_state->texture_combine_alpha_func);

      GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA,
                     big_state->texture_combine_alpha_src[0]));
      GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA,
                     big_state->texture_combine_alpha_op[0]));
      if (n_alpha_func_args > 1)
        {
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA,
                         big_state->texture_combine_alpha_src[1]));
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA,
                         big_state->texture_combine_alpha_op[1]));
        }
      if (n_alpha_func_args > 2)
        {
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_SRC2_ALPHA,
                         big_state->texture_combine_alpha_src[2]));
          GE (glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_ALPHA,
                         big_state->texture_combine_alpha_op[2]));
        }
    }

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT)
    {
      CoglPipelineLayer *authority =
        _cogl_pipeline_layer_get_authority
        (layer, COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT);
      CoglPipelineLayerBigState *big_state = authority->big_state;

      GE (glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR,
                      big_state->texture_combine_constant));
    }

  return TRUE;
}

static gboolean
_cogl_pipeline_backend_fixed_end (CoglPipeline *pipeline,
                                  unsigned long pipelines_difference)
{
  if (pipelines_difference & COGL_PIPELINE_STATE_FOG)
    {
      CoglPipeline *authority =
        _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_FOG);
      CoglPipelineFogState *fog_state = &authority->big_state->fog_state;

      if (fog_state->enabled)
        {
          GLfloat fogColor[4];
          GLenum gl_mode = GL_LINEAR;

          fogColor[0] = cogl_color_get_red_float (&fog_state->color);
          fogColor[1] = cogl_color_get_green_float (&fog_state->color);
          fogColor[2] = cogl_color_get_blue_float (&fog_state->color);
          fogColor[3] = cogl_color_get_alpha_float (&fog_state->color);

          GE (glEnable (GL_FOG));

          GE (glFogfv (GL_FOG_COLOR, fogColor));

#if HAVE_COGL_GLES
          switch (fog_state->mode)
            {
            case COGL_FOG_MODE_LINEAR:
              gl_mode = GL_LINEAR;
              break;
            case COGL_FOG_MODE_EXPONENTIAL:
              gl_mode = GL_EXP;
              break;
            case COGL_FOG_MODE_EXPONENTIAL_SQUARED:
              gl_mode = GL_EXP2;
              break;
            }
#endif
          /* TODO: support other modes for GLES2 */

          /* NB: GLES doesn't have glFogi */
          GE (glFogf (GL_FOG_MODE, gl_mode));
          GE (glHint (GL_FOG_HINT, GL_NICEST));

          GE (glFogf (GL_FOG_DENSITY, fog_state->density));
          GE (glFogf (GL_FOG_START, fog_state->z_near));
          GE (glFogf (GL_FOG_END, fog_state->z_far));
        }
      else
        GE (glDisable (GL_FOG));
    }

  return TRUE;
}

const CoglPipelineBackend _cogl_pipeline_fixed_backend =
{
  _cogl_pipeline_backend_fixed_get_max_texture_units,
  _cogl_pipeline_backend_fixed_start,
  _cogl_pipeline_backend_fixed_add_layer,
  NULL, /* passthrough */
  _cogl_pipeline_backend_fixed_end,
  NULL, /* pipeline_change_notify */
  NULL, /* pipeline_set_parent_notify */
  NULL, /* layer_change_notify */
  NULL /* free_priv */
};

#endif /* COGL_PIPELINE_BACKEND_FIXED */

