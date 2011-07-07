/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"

#ifdef COGL_PIPELINE_VERTEND_GLSL

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl-handle.h"
#include "cogl-program-private.h"
#include "cogl-pipeline-vertend-glsl-private.h"

const CoglPipelineVertend _cogl_pipeline_glsl_vertend;

typedef struct
{
  unsigned int ref_count;

  GLuint gl_shader;
  GString *header, *source;

  /* Age of the user program that was current when the shader was
     generated. We need to keep track of this because if the user
     program changes then we may need to redecide whether to generate
     a shader at all */
  unsigned int user_program_age;
} CoglPipelineVertendPrivate;

static CoglUserDataKey glsl_priv_key;

static CoglPipelineVertendPrivate *
get_glsl_priv (CoglPipeline *pipeline)
{
  return cogl_object_get_user_data (COGL_OBJECT (pipeline), &glsl_priv_key);
}

static void
destroy_glsl_priv (void *user_data)
{
  CoglPipelineVertendPrivate *priv = user_data;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (--priv->ref_count == 0)
    {
      if (priv->gl_shader)
        GE( ctx, glDeleteShader (priv->gl_shader) );

      g_slice_free (CoglPipelineVertendPrivate, priv);
    }
}

static void
set_glsl_priv (CoglPipeline *pipeline, CoglPipelineVertendPrivate *priv)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &glsl_priv_key,
                             priv,
                             destroy_glsl_priv);
}

static void
dirty_glsl_shader_state (CoglPipeline *pipeline)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &glsl_priv_key,
                             NULL,
                             destroy_glsl_priv);
}

GLuint
_cogl_pipeline_vertend_glsl_get_shader (CoglPipeline *pipeline)
{
  CoglPipelineVertendPrivate *priv = get_glsl_priv (pipeline);

  if (priv)
    return priv->gl_shader;
  else
    return 0;
}

static gboolean
_cogl_pipeline_vertend_glsl_start (CoglPipeline *pipeline,
                                   int n_layers,
                                   unsigned long pipelines_difference)
{
  CoglPipelineVertendPrivate *priv;
  CoglProgram *user_program;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!cogl_features_available (COGL_FEATURE_SHADERS_GLSL))
    return FALSE;

  user_program = cogl_pipeline_get_user_program (pipeline);

  /* If the user program has a vertex shader that isn't GLSL then the
     appropriate vertend for that language should handle it */
  if (user_program &&
      _cogl_program_has_vertex_shader (user_program) &&
      _cogl_program_get_language (user_program) != COGL_SHADER_LANGUAGE_GLSL)
    return FALSE;

  /* Now lookup our glsl backend private state (allocating if
   * necessary) */
  priv = get_glsl_priv (pipeline);

  if (priv == NULL)
    {
      CoglPipeline *authority;

      /* Get the authority for anything affecting vertex shader
         state */
      authority = _cogl_pipeline_find_equivalent_parent
        (pipeline,
         COGL_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN &
         ~COGL_PIPELINE_STATE_LAYERS,
         COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN);

      priv = get_glsl_priv (authority);

      if (priv == NULL)
        {
          priv = g_slice_new0 (CoglPipelineVertendPrivate);
          priv->ref_count = 1;
          set_glsl_priv (authority, priv);
        }

      if (authority != pipeline)
        {
          priv->ref_count++;
          set_glsl_priv (pipeline, priv);
        }
    }

  if (priv->gl_shader)
    {
      /* If we already have a valid GLSL shader then we don't need to
         generate a new one. However if there's a user program and it
         has changed since the last link then we do need a new shader */
      if (user_program == NULL ||
          priv->user_program_age == user_program->age)
        return TRUE;

      /* We need to recreate the shader so destroy the existing one */
      GE( ctx, glDeleteShader (priv->gl_shader) );
      priv->gl_shader = 0;
    }

  /* If we make it here then we have a priv struct without a gl_shader
     either because this is the first time we've encountered it or
     because the user program has changed */

  if (user_program)
    priv->user_program_age = user_program->age;

  /* If the user program contains a vertex shader then we don't need
     to generate one */
  if (user_program &&
      _cogl_program_has_vertex_shader (user_program))
    return TRUE;

  /* We reuse two grow-only GStrings for code-gen. One string
     contains the uniform and attribute declarations while the
     other contains the main function. We need two strings
     because we need to dynamically declare attributes as the
     add_layer callback is invoked */
  g_string_set_size (ctx->codegen_header_buffer, 0);
  g_string_set_size (ctx->codegen_source_buffer, 0);
  priv->header = ctx->codegen_header_buffer;
  priv->source = ctx->codegen_source_buffer;

  g_string_append (priv->source,
                   "void\n"
                   "main ()\n"
                   "{\n");

  if (ctx->driver == COGL_DRIVER_GLES2)
    /* There is no builtin uniform for the pointsize on GLES2 so we need
       to copy it from the custom uniform in the vertex shader */
    g_string_append (priv->source,
                     "  cogl_point_size_out = cogl_point_size_in;\n");
  /* On regular OpenGL we'll just flush the point size builtin */
  else if (pipelines_difference & COGL_PIPELINE_STATE_POINT_SIZE)
    {
      CoglPipeline *authority =
        _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_POINT_SIZE);

      if (ctx->point_size_cache != authority->big_state->point_size)
        {
          GE( ctx, glPointSize (authority->big_state->point_size) );
          ctx->point_size_cache = authority->big_state->point_size;
        }
    }

  return TRUE;
}

static gboolean
_cogl_pipeline_vertend_glsl_add_layer (CoglPipeline *pipeline,
                                       CoglPipelineLayer *layer,
                                       unsigned long layers_difference)
{
  CoglPipelineVertendPrivate *priv;
  int unit_index;

  _COGL_GET_CONTEXT (ctx, FALSE);

  priv = get_glsl_priv (pipeline);

  unit_index = _cogl_pipeline_layer_get_unit_index (layer);

  if (ctx->driver != COGL_DRIVER_GLES2)
    {
      /* We are using the fixed function uniforms for the user matrices
         and the only way to set them is with the fixed function API so we
         still need to flush them here */
      if (layers_difference & COGL_PIPELINE_LAYER_STATE_USER_MATRIX)
        {
          CoglPipelineLayerState state = COGL_PIPELINE_LAYER_STATE_USER_MATRIX;
          CoglPipelineLayer *authority =
            _cogl_pipeline_layer_get_authority (layer, state);
          CoglTextureUnit *unit = _cogl_get_texture_unit (unit_index);

          _cogl_matrix_stack_set (unit->matrix_stack,
                                  &authority->big_state->matrix);

          _cogl_set_active_texture_unit (unit_index);

          _cogl_matrix_stack_flush_to_gl (unit->matrix_stack,
                                          COGL_MATRIX_TEXTURE);
        }
    }

  if (priv->source == NULL)
    return TRUE;

  /* Transform the texture coordinates by the layer's user matrix.
   *
   * FIXME: this should avoid doing the transform if there is no user
   * matrix set. This might need a separate layer state flag for
   * whether there is a user matrix
   *
   * FIXME: we could be more clever here and try to detect if the
   * fragment program is going to use the texture coordinates and
   * avoid setting them if not
   */

  g_string_append_printf (priv->source,
                          "  cogl_tex_coord_out[%i] = "
                          "cogl_texture_matrix[%i] * cogl_tex_coord%i_in;\n",
                          unit_index, unit_index, unit_index);

  return TRUE;
}

static gboolean
_cogl_pipeline_vertend_glsl_end (CoglPipeline *pipeline,
                                 unsigned long pipelines_difference)
{
  CoglPipelineVertendPrivate *priv;

  _COGL_GET_CONTEXT (ctx, FALSE);

  priv = get_glsl_priv (pipeline);

  if (priv->source)
    {
      const char *source_strings[2];
      GLint lengths[2];
      GLint compile_status;
      GLuint shader;
      int n_layers;

      COGL_STATIC_COUNTER (vertend_glsl_compile_counter,
                           "glsl vertex compile counter",
                           "Increments each time a new GLSL "
                           "vertex shader is compiled",
                           0 /* no application private data */);
      COGL_COUNTER_INC (_cogl_uprof_context, vertend_glsl_compile_counter);

      g_string_append (priv->source,
                       "  cogl_position_out = "
                       "cogl_modelview_projection_matrix * "
                       "cogl_position_in;\n"
                       "  cogl_color_out = cogl_color_in;\n"
                       "}\n");

      GE_RET( shader, ctx, glCreateShader (GL_VERTEX_SHADER) );

      lengths[0] = priv->header->len;
      source_strings[0] = priv->header->str;
      lengths[1] = priv->source->len;
      source_strings[1] = priv->source->str;

      n_layers = cogl_pipeline_get_n_layers (pipeline);

      _cogl_shader_set_source_with_boilerplate (shader, GL_VERTEX_SHADER,
                                                n_layers,
                                                2, /* count */
                                                source_strings, lengths);

      GE( ctx, glCompileShader (shader) );
      GE( ctx, glGetShaderiv (shader, GL_COMPILE_STATUS, &compile_status) );

      if (!compile_status)
        {
          GLint len = 0;
          char *shader_log;

          GE( ctx, glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &len) );
          shader_log = g_alloca (len);
          GE( ctx, glGetShaderInfoLog (shader, len, &len, shader_log) );
          g_warning ("Shader compilation failed:\n%s", shader_log);
        }

      priv->header = NULL;
      priv->source = NULL;
      priv->gl_shader = shader;
    }

  return TRUE;
}

static void
_cogl_pipeline_vertend_glsl_pre_change_notify (CoglPipeline *pipeline,
                                               CoglPipelineState change,
                                               const CoglColor *new_color)
{
  if ((change & COGL_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN))
    dirty_glsl_shader_state (pipeline);
}

/* NB: layers are considered immutable once they have any dependants
 * so although multiple pipelines can end up depending on a single
 * static layer, we can guarantee that if a layer is being *changed*
 * then it can only have one pipeline depending on it.
 *
 * XXX: Don't forget this is *pre* change, we can't read the new value
 * yet!
 */
static void
_cogl_pipeline_vertend_glsl_layer_pre_change_notify (
                                                CoglPipeline *owner,
                                                CoglPipelineLayer *layer,
                                                CoglPipelineLayerState change)
{
  CoglPipelineVertendPrivate *priv;

  priv = get_glsl_priv (owner);
  if (!priv)
    return;

  if ((change & COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN))
    {
      dirty_glsl_shader_state (owner);
      return;
    }

  /* TODO: we could be saving snippets of texture combine code along
   * with each layer and then when a layer changes we would just free
   * the snippet. */
}

const CoglPipelineVertend _cogl_pipeline_glsl_vertend =
  {
    _cogl_pipeline_vertend_glsl_start,
    _cogl_pipeline_vertend_glsl_add_layer,
    _cogl_pipeline_vertend_glsl_end,
    _cogl_pipeline_vertend_glsl_pre_change_notify,
    _cogl_pipeline_vertend_glsl_layer_pre_change_notify
  };

#endif /* COGL_PIPELINE_VERTEND_GLSL */
