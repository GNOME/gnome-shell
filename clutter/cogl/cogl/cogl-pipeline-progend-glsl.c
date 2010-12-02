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

#ifdef COGL_PIPELINE_PROGEND_GLSL

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-program-private.h"
#include "cogl-pipeline-fragend-glsl-private.h"

#ifndef HAVE_COGL_GLES2

#define glCreateProgram      ctx->drv.pf_glCreateProgram
#define glAttachShader       ctx->drv.pf_glAttachShader
#define glUseProgram         ctx->drv.pf_glUseProgram
#define glLinkProgram        ctx->drv.pf_glLinkProgram
#define glDeleteProgram      ctx->drv.pf_glDeleteProgram
#define glGetProgramInfoLog  ctx->drv.pf_glGetProgramInfoLog
#define glGetProgramiv       ctx->drv.pf_glGetProgramiv
#define glGetUniformLocation ctx->drv.pf_glGetUniformLocation
#define glUniform1i          ctx->drv.pf_glUniform1i
#define glUniform1f          ctx->drv.pf_glUniform1f
#define glUniform4fv         ctx->drv.pf_glUniform4fv

#endif /* HAVE_COGL_GLES2 */

const CoglPipelineProgend _cogl_pipeline_glsl_progend;

typedef struct _UnitState
{
  unsigned int dirty_combine_constant:1;

  GLint combine_constant_uniform;
} UnitState;

typedef struct
{
  unsigned int ref_count;

  /* Age that the user program had last time we generated a GL
     program. If it's different then we need to relink the program */
  unsigned int user_program_age;

  GLuint program;

  /* To allow writing shaders that are portable between GLES 2 and
   * OpenGL Cogl prepends a number of boilerplate #defines and
   * declarations to user shaders. One of those declarations is an
   * array of texture coordinate varyings, but to know how to emit the
   * declaration we need to know how many texture coordinate
   * attributes are in use.  The boilerplate also needs to be changed
   * if this increases. */
  int n_tex_coord_attribs;

#ifdef HAVE_COGL_GLES2
  /* The GLES2 generated program that was generated from the user
     program. This is used to detect when the GLES2 backend generates
     a different program which would mean we need to flush all of the
     custom uniforms. This is a massive hack but it can go away once
     this GLSL backend starts generating its own shaders */
  GLuint gles2_program;

  /* Under GLES2 the alpha test is implemented in the shader. We need
     a uniform for the reference value */
  gboolean dirty_alpha_test_reference;
  GLint alpha_test_reference_uniform;
#endif

  /* We need to track the last pipeline that the program was used with
   * so know if we need to update all of the uniforms */
  CoglPipeline *last_used_for_pipeline;

  UnitState *unit_state;
} CoglPipelineProgendPrivate;

static CoglUserDataKey glsl_priv_key;

static void
delete_program (GLuint program)
{
#ifdef HAVE_COGL_GLES2
  /* This hack can go away once this GLSL backend replaces the GLES2
     wrapper */
  _cogl_gles2_clear_cache_for_program (program);
#else
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
#endif

  GE (glDeleteProgram (program));
}

static CoglPipelineProgendPrivate *
get_glsl_priv (CoglPipeline *pipeline)
{
  return cogl_object_get_user_data (COGL_OBJECT (pipeline), &glsl_priv_key);
}

static void
destroy_glsl_priv (void *user_data)
{
  CoglPipelineProgendPrivate *priv = user_data;

  if (--priv->ref_count == 0)
    {
      if (priv->program)
        delete_program (priv->program);

      g_free (priv->unit_state);

      g_slice_free (CoglPipelineProgendPrivate, priv);
    }
}

static void
set_glsl_priv (CoglPipeline *pipeline, CoglPipelineProgendPrivate *priv)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &glsl_priv_key,
                             priv,
                             destroy_glsl_priv);
}

static void
dirty_glsl_program_state (CoglPipeline *pipeline)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &glsl_priv_key,
                             NULL,
                             destroy_glsl_priv);
}

static void
link_program (GLint gl_program)
{
  /* On GLES2 we'll let the backend link the program. This hack can go
     away once this backend replaces the GLES2 wrapper */
#ifndef HAVE_COGL_GLES2

  GLint link_status;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  GE( glLinkProgram (gl_program) );

  GE( glGetProgramiv (gl_program, GL_LINK_STATUS, &link_status) );

  if (!link_status)
    {
      GLint log_length;
      GLsizei out_log_length;
      char *log;

      GE( glGetProgramiv (gl_program, GL_INFO_LOG_LENGTH, &log_length) );

      log = g_malloc (log_length);

      GE( glGetProgramInfoLog (gl_program, log_length,
                               &out_log_length, log) );

      g_warning ("Failed to link GLSL program:\n%.*s\n",
                 log_length, log);

      g_free (log);
    }

#endif /* HAVE_COGL_GLES2 */
}

typedef struct
{
  int unit;
  GLuint gl_program;
  gboolean update_all;
  CoglPipelineProgendPrivate *priv;
} UpdateUniformsState;

static gboolean
get_uniform_cb (CoglPipeline *pipeline,
                int layer_index,
                void *user_data)
{
  UpdateUniformsState *state = user_data;
  CoglPipelineProgendPrivate *priv = state->priv;
  UnitState *unit_state = &priv->unit_state[state->unit];
  GLint uniform_location;

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* We can reuse the source buffer to create the uniform name because
     the program has now been linked */
  g_string_set_size (ctx->fragment_source_buffer, 0);
  g_string_append_printf (ctx->fragment_source_buffer,
                          "_cogl_sampler_%i", state->unit);

  GE_RET( uniform_location,
          glGetUniformLocation (state->gl_program,
                                ctx->fragment_source_buffer->str) );

  /* We can set the uniform immediately because the samplers are the
     unit index not the texture object number so it will never
     change. Unfortunately GL won't let us use a constant instead of a
     uniform */
  if (uniform_location != -1)
    GE( glUniform1i (uniform_location, state->unit) );

  g_string_set_size (ctx->fragment_source_buffer, 0);
  g_string_append_printf (ctx->fragment_source_buffer,
                          "_cogl_layer_constant_%i", state->unit);

  GE_RET( uniform_location,
          glGetUniformLocation (state->gl_program,
                                ctx->fragment_source_buffer->str) );

  unit_state->combine_constant_uniform = uniform_location;

  state->unit++;

  return TRUE;
}

static gboolean
update_constants_cb (CoglPipeline *pipeline,
                     int layer_index,
                     void *user_data)
{
  UpdateUniformsState *state = user_data;
  CoglPipelineProgendPrivate *priv = state->priv;
  UnitState *unit_state = &priv->unit_state[state->unit++];

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (unit_state->combine_constant_uniform != -1 &&
      (state->update_all || unit_state->dirty_combine_constant))
    {
      float constant[4];
      _cogl_pipeline_get_layer_combine_constant (pipeline,
                                                 layer_index,
                                                 constant);
      GE (glUniform4fv (unit_state->combine_constant_uniform,
                        1, constant));
      unit_state->dirty_combine_constant = FALSE;
    }
  return TRUE;
}

#ifdef HAVE_COGL_GLES2

static void
update_alpha_test_reference (CoglPipeline *pipeline,
                             GLuint gl_program,
                             CoglPipelineProgendPrivate *priv)
{
  float alpha_reference;

  if (priv->dirty_alpha_test_reference &&
      priv->alpha_test_reference_uniform != -1)
    {
      alpha_reference = cogl_pipeline_get_alpha_test_reference (pipeline);

      GE( glUniform1f (priv->alpha_test_reference_uniform,
                       alpha_reference) );

      priv->dirty_alpha_test_reference = FALSE;
    }
}

#endif /* HAVE_COGL_GLES2 */

static void
_cogl_pipeline_progend_glsl_end (CoglPipeline *pipeline,
                                 unsigned long pipelines_difference,
                                 int n_tex_coord_attribs)
{
  CoglPipelineProgendPrivate *priv;
  GLuint gl_program;
  gboolean program_changed = FALSE;
  UpdateUniformsState state;
  CoglProgram *user_program;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* If neither of the glsl fragend or vertends are used then we don't
     need to do anything */
  if (pipeline->fragend != COGL_PIPELINE_FRAGEND_GLSL)
    return;

  priv = get_glsl_priv (pipeline);

  user_program = cogl_pipeline_get_user_program (pipeline);

  if (priv == NULL)
    {
      CoglPipeline *authority;

      /* Get the authority for anything affecting program state. This
         should include both fragment codegen state and vertex codegen
         state */
      authority = _cogl_pipeline_find_equivalent_parent
        (pipeline,
         COGL_PIPELINE_STATE_AFFECTS_FRAGMENT_CODEGEN &
         ~COGL_PIPELINE_STATE_LAYERS,
         COGL_PIPELINE_LAYER_STATE_AFFECTS_FRAGMENT_CODEGEN,
         COGL_PIPELINE_FIND_EQUIVALENT_COMPARE_TEXTURE_TARGET);

      priv = get_glsl_priv (authority);

      if (priv == NULL)
        {
          priv = g_slice_new (CoglPipelineProgendPrivate);
          priv->ref_count = 1;
          priv->program = 0;
          priv->n_tex_coord_attribs = 0;
          priv->unit_state = g_new (UnitState,
                                    cogl_pipeline_get_n_layers (pipeline));
#ifdef HAVE_COGL_GLES2
          priv->gles2_program = 0;
#endif
          set_glsl_priv (authority, priv);
        }

      if (authority != pipeline)
        {
          priv->ref_count++;
          set_glsl_priv (pipeline, priv);
        }
    }

  /* If the program has changed since the last link then we do
   * need to relink
   *
   * Also if the number of texture coordinate attributes in use has
   * increased, then delete the program so we can prepend a new
   * _cogl_tex_coord[] varying array declaration. */
  if (priv->program && user_program &&
      (user_program->age != priv->user_program_age ||
       n_tex_coord_attribs > priv->n_tex_coord_attribs))
    {
      delete_program (priv->program);
      priv->program = 0;
    }

  if (priv->program == 0)
    {
      GLuint backend_shader;
      GSList *l;

      GE_RET( priv->program, glCreateProgram () );

      /* Attach all of the shader from the user program */
      if (user_program)
        {
          if (priv->n_tex_coord_attribs > n_tex_coord_attribs)
            n_tex_coord_attribs = priv->n_tex_coord_attribs;

#ifdef HAVE_COGL_GLES2
          /* Find the largest count of texture coordinate attributes
           * associated with each of the shaders so we can ensure a consistent
           * _cogl_tex_coord[] array declaration across all of the shaders.*/
          if (user_program)
            for (l = user_program->attached_shaders; l; l = l->next)
              {
                CoglShader *shader = l->data;
                n_tex_coord_attribs = MAX (shader->n_tex_coord_attribs,
                                           n_tex_coord_attribs);
              }
#endif

          for (l = user_program->attached_shaders; l; l = l->next)
            {
              CoglShader *shader = l->data;

              _cogl_shader_compile_real (shader, n_tex_coord_attribs);

              g_assert (shader->language == COGL_SHADER_LANGUAGE_GLSL);

              GE( glAttachShader (priv->program,
                                  shader->gl_handle) );
            }

          priv->user_program_age = user_program->age;
        }

      /* Attach any shaders from the GLSL backends */
      if (pipeline->fragend == COGL_PIPELINE_FRAGEND_GLSL &&
          (backend_shader = _cogl_pipeline_fragend_glsl_get_shader (pipeline)))
        GE( glAttachShader (priv->program, backend_shader) );

      link_program (priv->program);

      program_changed = TRUE;

      priv->n_tex_coord_attribs = n_tex_coord_attribs;
    }

  gl_program = priv->program;

#ifdef HAVE_COGL_GLES2
  /* This function is a massive hack to get the GLES2 backend to
     work. It should only be neccessary until we move the GLSL vertex
     shader generation into a vertend instead of the GLES2 driver
     backend */
  gl_program = _cogl_gles2_use_program (gl_program);
  /* We need to detect when the GLES2 backend gives us a different
     program from last time */
  if (gl_program != priv->gles2_program)
    {
      priv->gles2_program = gl_program;
      program_changed = TRUE;
    }
#else
  _cogl_use_program (gl_program, COGL_PIPELINE_PROGRAM_TYPE_GLSL);
#endif

  state.unit = 0;
  state.gl_program = gl_program;
  state.priv = priv;

  if (program_changed)
    cogl_pipeline_foreach_layer (pipeline,
                                 get_uniform_cb,
                                 &state);

  state.unit = 0;
  state.update_all = (program_changed ||
                      priv->last_used_for_pipeline != pipeline);

  cogl_pipeline_foreach_layer (pipeline,
                               update_constants_cb,
                               &state);

#ifdef HAVE_COGL_GLES2
  if (program_changed)
    GE_RET( priv->alpha_test_reference_uniform,
            glGetUniformLocation (gl_program,
                                  "_cogl_alpha_test_ref") );
  if (program_changed ||
      priv->last_used_for_pipeline != pipeline)
    priv->dirty_alpha_test_reference = TRUE;

  update_alpha_test_reference (pipeline, gl_program, priv);
#endif

  if (user_program)
    _cogl_program_flush_uniforms (user_program,
                                  gl_program,
                                  program_changed);

  /* We need to track the last pipeline that the program was used with
   * so know if we need to update all of the uniforms */
  priv->last_used_for_pipeline = pipeline;
}

static void
_cogl_pipeline_progend_glsl_pre_change_notify (CoglPipeline *pipeline,
                                               CoglPipelineState change,
                                               const CoglColor *new_color)
{
  if ((change & COGL_PIPELINE_STATE_AFFECTS_FRAGMENT_CODEGEN))
    dirty_glsl_program_state (pipeline);
#ifdef COGL_HAS_GLES2
  else if ((change & COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE))
    {
      CoglPipelineProgendPrivate *priv = get_glsl_priv (pipeline);
      if (priv)
        priv->dirty_alpha_test_reference = TRUE;
    }
#endif
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
_cogl_pipeline_progend_glsl_layer_pre_change_notify (
                                                CoglPipeline *owner,
                                                CoglPipelineLayer *layer,
                                                CoglPipelineLayerState change)
{
  if ((change & COGL_PIPELINE_LAYER_STATE_AFFECTS_FRAGMENT_CODEGEN))
    {
      dirty_glsl_program_state (owner);
      return;
    }

  if (change & COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT)
    {
      CoglPipelineProgendPrivate *priv = get_glsl_priv (owner);
      if (priv)
        {
          int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
          priv->unit_state[unit_index].dirty_combine_constant = TRUE;
        }
    }
}

const CoglPipelineProgend _cogl_pipeline_glsl_progend =
  {
    _cogl_pipeline_progend_glsl_end,
    _cogl_pipeline_progend_glsl_pre_change_notify,
    _cogl_pipeline_progend_glsl_layer_pre_change_notify
  };

#endif /* COGL_PIPELINE_PROGEND_GLSL */

