/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-state-private.h"
#include "cogl-pipeline-opengl-private.h"

#ifdef COGL_PIPELINE_FRAGEND_FIXED

#include "cogl-context-private.h"
#include "cogl-object-private.h"

#include "cogl-texture-private.h"
#include "cogl-blend-string.h"
#include "cogl-profile.h"
#include "cogl-program-private.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>

#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif

const CoglPipelineFragend _cogl_pipeline_fixed_fragend;

static void
_cogl_disable_texture_unit (int unit_index)
{
  CoglTextureUnit *unit;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  unit = &g_array_index (ctx->texture_units, CoglTextureUnit, unit_index);

  if (unit->enabled_gl_target)
    {
      _cogl_set_active_texture_unit (unit_index);
      GE (ctx, glDisable (unit->enabled_gl_target));
      unit->enabled_gl_target = 0;
    }
}

static int
get_max_texture_units (void)
{
  _COGL_GET_CONTEXT (ctx, 0);

  /* This function is called quite often so we cache the value to
     avoid too many GL calls */
  if (ctx->max_texture_units == -1)
    {
      ctx->max_texture_units = 1;
      GE (ctx, glGetIntegerv (GL_MAX_TEXTURE_UNITS,
                              &ctx->max_texture_units));
    }

  return ctx->max_texture_units;
}

static void
_cogl_pipeline_fragend_fixed_start (CoglPipeline *pipeline,
                                    int n_layers,
                                    unsigned long pipelines_difference)
{
  _cogl_use_fragment_program (0, COGL_PIPELINE_PROGRAM_TYPE_FIXED);
}

static void
translate_sources (CoglPipeline *pipeline,
                   int n_sources,
                   CoglPipelineCombineSource *source_in,
                   GLenum *source_out)
{
  int i;

  /* The texture source numbers specified in the layer combine are the
     layer numbers so we need to map these to unit indices */

  for (i = 0; i < n_sources; i++)
    switch (source_in[i])
      {
      case COGL_PIPELINE_COMBINE_SOURCE_TEXTURE:
        source_out[i] = GL_TEXTURE;
        break;

      case COGL_PIPELINE_COMBINE_SOURCE_CONSTANT:
        source_out[i] = GL_CONSTANT;
        break;

      case COGL_PIPELINE_COMBINE_SOURCE_PRIMARY_COLOR:
        source_out[i] = GL_PRIMARY_COLOR;
        break;

      case COGL_PIPELINE_COMBINE_SOURCE_PREVIOUS:
        source_out[i] = GL_PREVIOUS;
        break;

      default:
        {
          int layer_num = source_in[i] - COGL_PIPELINE_COMBINE_SOURCE_TEXTURE0;
          CoglPipelineGetLayerFlags flags = COGL_PIPELINE_GET_LAYER_NO_CREATE;
          CoglPipelineLayer *layer =
            _cogl_pipeline_get_layer_with_flags (pipeline, layer_num, flags);

          if (layer == NULL)
            {
              static CoglBool warning_seen = FALSE;
              if (!warning_seen)
                {
                  g_warning ("The application is trying to use a texture "
                             "combine with a layer number that does not exist");
                  warning_seen = TRUE;
                }
              source_out[i] = GL_PREVIOUS;
            }
          else
            source_out[i] = (_cogl_pipeline_layer_get_unit_index (layer) +
                             GL_TEXTURE0);
        }
      }
}

static CoglBool
_cogl_pipeline_fragend_fixed_add_layer (CoglPipeline *pipeline,
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

  if (G_UNLIKELY (unit_index >= get_max_texture_units ()))
    {
      _cogl_disable_texture_unit (unit_index);
      /* TODO: although this isn't considered an error that
       * warrants falling back to a different backend we
       * should print a warning here. */
      return TRUE;
    }

  /* Handle enabling or disabling the right texture type */
  if (layers_difference & COGL_PIPELINE_LAYER_STATE_TEXTURE_TYPE)
    {
      CoglTextureType texture_type =
        _cogl_pipeline_layer_get_texture_type (layer);
      GLenum gl_target;

      switch (texture_type)
        {
        case COGL_TEXTURE_TYPE_2D:
          gl_target = GL_TEXTURE_2D;
          break;

        case COGL_TEXTURE_TYPE_3D:
          gl_target = GL_TEXTURE_3D;
          break;

        case COGL_TEXTURE_TYPE_RECTANGLE:
          gl_target = GL_TEXTURE_RECTANGLE_ARB;
          break;
        }

      _cogl_set_active_texture_unit (unit_index);

      /* The common GL code handles binding the right texture so we
         just need to handle enabling and disabling it */

      if (unit->enabled_gl_target != gl_target)
        {
          /* Disable the previous target if it's still enabled */
          if (unit->enabled_gl_target)
            GE (ctx, glDisable (unit->enabled_gl_target));

          /* Enable the new target */
          if (!G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_TEXTURING)))
            {
              GE (ctx, glEnable (gl_target));
              unit->enabled_gl_target = gl_target;
            }
        }
    }
  else
    {
      /* Even though there may be no difference between the last flushed
       * texture state and the current layers texture state it may be that the
       * texture unit has been disabled for some time so we need to assert that
       * it's enabled now.
       */
      if (!G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_TEXTURING)) &&
          unit->enabled_gl_target == 0)
        {
          _cogl_set_active_texture_unit (unit_index);
          GE (ctx, glEnable (unit->gl_target));
          unit->enabled_gl_target = unit->gl_target;
        }
    }

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_COMBINE)
    {
      CoglPipelineLayer *authority =
        _cogl_pipeline_layer_get_authority (layer,
                                            COGL_PIPELINE_LAYER_STATE_COMBINE);
      CoglPipelineLayerBigState *big_state = authority->big_state;
      GLenum sources[3];

      GE (ctx, glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE));

      /* Set the combiner functions... */
      GE (ctx, glTexEnvi (GL_TEXTURE_ENV,
                          GL_COMBINE_RGB,
                          big_state->texture_combine_rgb_func));
      GE (ctx, glTexEnvi (GL_TEXTURE_ENV,
                          GL_COMBINE_ALPHA,
                          big_state->texture_combine_alpha_func));

      /*
       * Setup the function arguments...
       */

      /* For the RGB components... */
      n_rgb_func_args =
        _cogl_get_n_args_for_combine_func (big_state->texture_combine_rgb_func);

      translate_sources (pipeline,
                         n_rgb_func_args,
                         big_state->texture_combine_rgb_src,
                         sources);

      GE (ctx, glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_RGB,
                          sources[0]));
      GE (ctx, glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_RGB,
                          big_state->texture_combine_rgb_op[0]));
      if (n_rgb_func_args > 1)
        {
          GE (ctx, glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_RGB,
                              sources[1]));
          GE (ctx, glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_RGB,
                              big_state->texture_combine_rgb_op[1]));
        }
      if (n_rgb_func_args > 2)
        {
          GE (ctx, glTexEnvi (GL_TEXTURE_ENV, GL_SRC2_RGB,
                              sources[2]));
          GE (ctx, glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_RGB,
                              big_state->texture_combine_rgb_op[2]));
        }

      /* For the Alpha component */
      n_alpha_func_args =
        _cogl_get_n_args_for_combine_func (big_state->texture_combine_alpha_func);

      translate_sources (pipeline,
                         n_alpha_func_args,
                         big_state->texture_combine_alpha_src,
                         sources);

      GE (ctx, glTexEnvi (GL_TEXTURE_ENV, GL_SRC0_ALPHA,
                          sources[0]));
      GE (ctx, glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA,
                          big_state->texture_combine_alpha_op[0]));
      if (n_alpha_func_args > 1)
        {
          GE (ctx, glTexEnvi (GL_TEXTURE_ENV, GL_SRC1_ALPHA,
                              sources[1]));
          GE (ctx, glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA,
                              big_state->texture_combine_alpha_op[1]));
        }
      if (n_alpha_func_args > 2)
        {
          GE (ctx, glTexEnvi (GL_TEXTURE_ENV, GL_SRC2_ALPHA,
                              sources[2]));
          GE (ctx, glTexEnvi (GL_TEXTURE_ENV, GL_OPERAND2_ALPHA,
                              big_state->texture_combine_alpha_op[2]));
        }
    }

  if (layers_difference & COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT)
    {
      CoglPipelineLayer *authority =
        _cogl_pipeline_layer_get_authority
        (layer, COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT);
      CoglPipelineLayerBigState *big_state = authority->big_state;

      GE (ctx, glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR,
                           big_state->texture_combine_constant));
    }

  return TRUE;
}

static CoglBool
get_highest_unit_index_cb (CoglPipelineLayer *layer,
                           void *user_data)
{
  int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
  int *highest_index = user_data;

  *highest_index = unit_index;

  return TRUE;
}

static CoglBool
_cogl_pipeline_fragend_fixed_end (CoglPipeline *pipeline,
                                  unsigned long pipelines_difference)
{
  int highest_unit_index = -1;
  int i;

  _COGL_GET_CONTEXT (ctx, FALSE);

  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         get_highest_unit_index_cb,
                                         &highest_unit_index);

  /* Disable additional texture units that may have previously been in use.. */
  for (i = highest_unit_index + 1; i < ctx->texture_units->len; i++)
    _cogl_disable_texture_unit (i);

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

          GE (ctx, glEnable (GL_FOG));

          GE (ctx, glFogfv (GL_FOG_COLOR, fogColor));

          if (ctx->driver == COGL_DRIVER_GLES1)
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
          /* TODO: support other modes for GLES2 */

          /* NB: GLES doesn't have glFogi */
          GE (ctx, glFogf (GL_FOG_MODE, gl_mode));
          GE (ctx, glHint (GL_FOG_HINT, GL_NICEST));

          GE (ctx, glFogf (GL_FOG_DENSITY, fog_state->density));
          GE (ctx, glFogf (GL_FOG_START, fog_state->z_near));
          GE (ctx, glFogf (GL_FOG_END, fog_state->z_far));
        }
      else
        GE (ctx, glDisable (GL_FOG));
    }

  return TRUE;
}

const CoglPipelineFragend _cogl_pipeline_fixed_fragend =
{
  _cogl_pipeline_fragend_fixed_start,
  _cogl_pipeline_fragend_fixed_add_layer,
  NULL, /* passthrough */
  _cogl_pipeline_fragend_fixed_end,
  NULL, /* pipeline_change_notify */
  NULL, /* pipeline_set_parent_notify */
  NULL, /* layer_change_notify */
};

#endif /* COGL_PIPELINE_FRAGEND_FIXED */

