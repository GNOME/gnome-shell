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
#include "cogl-context-private.h"
#include "cogl-handle.h"
#include "cogl-program-private.h"
#include "cogl-pipeline-fragend-glsl-private.h"
#include "cogl-pipeline-vertend-glsl-private.h"
#include "cogl-pipeline-cache.h"

#ifdef HAVE_COGL_GLES2

/* These are used to generalise updating some uniforms that are
   required when building for GLES2 */

typedef void (* UpdateUniformFunc) (CoglPipeline *pipeline,
                                    int uniform_location,
                                    void *getter_func);

static void update_float_uniform (CoglPipeline *pipeline,
                                  int uniform_location,
                                  void *getter_func);

typedef struct
{
  const char *uniform_name;
  void *getter_func;
  UpdateUniformFunc update_func;
  CoglPipelineState change;
} BuiltinUniformData;

static BuiltinUniformData builtin_uniforms[] =
  {
    { "cogl_point_size_in",
      cogl_pipeline_get_point_size, update_float_uniform,
      COGL_PIPELINE_STATE_POINT_SIZE },
    { "_cogl_alpha_test_ref",
      cogl_pipeline_get_alpha_test_reference, update_float_uniform,
      COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE }
  };

#endif /* HAVE_COGL_GLES2 */

const CoglPipelineProgend _cogl_pipeline_glsl_progend;

typedef struct _UnitState
{
  unsigned int dirty_combine_constant:1;
  unsigned int dirty_texture_matrix:1;

  GLint combine_constant_uniform;

  GLint texture_matrix_uniform;
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
  unsigned long dirty_builtin_uniforms;
  GLint builtin_uniform_locations[G_N_ELEMENTS (builtin_uniforms)];

  /* Under GLES2 we can't use the builtin functions to set attribute
     pointers such as the vertex position. Instead the vertex
     attribute code needs to query the attribute numbers from the
     progend backend */
  int position_attribute_location;
  int color_attribute_location;
  int normal_attribute_location;
  int tex_coord0_attribute_location;
  /* We only allocate this array if more than one tex coord attribute
     is requested because most pipelines will only use one layer */
  GArray *tex_coord_attribute_locations;

  GLint modelview_uniform;
  GLint projection_uniform;
  GLint mvp_uniform;

  CoglMatrixStack *flushed_modelview_stack;
  unsigned int flushed_modelview_stack_age;
  gboolean flushed_modelview_is_identity;
  CoglMatrixStack *flushed_projection_stack;
  unsigned int flushed_projection_stack_age;
#endif

  /* We need to track the last pipeline that the program was used with
   * so know if we need to update all of the uniforms */
  CoglPipeline *last_used_for_pipeline;

  UnitState *unit_state;
} CoglPipelineProgramState;

static CoglUserDataKey program_state_key;

static CoglPipelineProgramState *
get_program_state (CoglPipeline *pipeline)
{
  return cogl_object_get_user_data (COGL_OBJECT (pipeline), &program_state_key);
}

#ifdef HAVE_COGL_GLES2

#define ATTRIBUTE_LOCATION_UNKNOWN -2

/* Under GLES2 the vertex attribute API needs to query the attribute
   numbers because it can't used the fixed function API to set the
   builtin attributes. We cache the attributes here because the
   progend knows when the program is changed so it can clear the
   cache. This should always be called after the pipeline is flushed
   so they can assert that the gl program is valid */

int
_cogl_pipeline_progend_glsl_get_position_attribute (CoglPipeline *pipeline)
{
  CoglPipelineProgramState *program_state = get_program_state (pipeline);

  _COGL_GET_CONTEXT (ctx, -1);

  g_return_val_if_fail (program_state != NULL, -1);
  g_return_val_if_fail (program_state->program != 0, -1);

  if (program_state->position_attribute_location == ATTRIBUTE_LOCATION_UNKNOWN)
    GE_RET( program_state->position_attribute_location,
            ctx, glGetAttribLocation (program_state->program,
                                      "cogl_position_in") );

  return program_state->position_attribute_location;
}

int
_cogl_pipeline_progend_glsl_get_color_attribute (CoglPipeline *pipeline)
{
  CoglPipelineProgramState *program_state = get_program_state (pipeline);

  _COGL_GET_CONTEXT (ctx, -1);

  g_return_val_if_fail (program_state != NULL, -1);
  g_return_val_if_fail (program_state->program != 0, -1);

  if (program_state->color_attribute_location == ATTRIBUTE_LOCATION_UNKNOWN)
    GE_RET( program_state->color_attribute_location,
            ctx, glGetAttribLocation (program_state->program,
                                      "cogl_color_in") );

  return program_state->color_attribute_location;
}

int
_cogl_pipeline_progend_glsl_get_normal_attribute (CoglPipeline *pipeline)
{
  CoglPipelineProgramState *program_state = get_program_state (pipeline);

  _COGL_GET_CONTEXT (ctx, -1);

  g_return_val_if_fail (program_state != NULL, -1);
  g_return_val_if_fail (program_state->program != 0, -1);

  if (program_state->normal_attribute_location == ATTRIBUTE_LOCATION_UNKNOWN)
    GE_RET( program_state->normal_attribute_location,
            ctx, glGetAttribLocation (program_state->program,
                                      "cogl_normal_in") );

  return program_state->normal_attribute_location;
}

int
_cogl_pipeline_progend_glsl_get_tex_coord_attribute (CoglPipeline *pipeline,
                                                     int unit)
{
  CoglPipelineProgramState *program_state = get_program_state (pipeline);

  _COGL_GET_CONTEXT (ctx, -1);

  g_return_val_if_fail (program_state != NULL, -1);
  g_return_val_if_fail (program_state->program != 0, -1);

  if (unit == 0)
    {
      if (program_state->tex_coord0_attribute_location ==
          ATTRIBUTE_LOCATION_UNKNOWN)
        GE_RET( program_state->tex_coord0_attribute_location,
                ctx, glGetAttribLocation (program_state->program,
                                          "cogl_tex_coord0_in") );

      return program_state->tex_coord0_attribute_location;
    }
  else
    {
      char *name = g_strdup_printf ("cogl_tex_coord%i_in", unit);
      int *locations;

      if (program_state->tex_coord_attribute_locations == NULL)
        program_state->tex_coord_attribute_locations =
          g_array_new (FALSE, FALSE, sizeof (int));
      if (program_state->tex_coord_attribute_locations->len <= unit - 1)
        {
          int i = program_state->tex_coord_attribute_locations->len;
          g_array_set_size (program_state->tex_coord_attribute_locations, unit);
          for (; i < unit; i++)
            g_array_index (program_state->tex_coord_attribute_locations, int, i)
              = ATTRIBUTE_LOCATION_UNKNOWN;
        }

      locations = &g_array_index (program_state->tex_coord_attribute_locations,
                                  int, 0);

      if (locations[unit - 1] == ATTRIBUTE_LOCATION_UNKNOWN)
        GE_RET( locations[unit - 1],
                ctx, glGetAttribLocation (program_state->program, name) );

      g_free (name);

      return locations[unit - 1];
    }
}

static void
clear_attribute_cache (CoglPipelineProgramState *program_state)
{
  program_state->position_attribute_location = ATTRIBUTE_LOCATION_UNKNOWN;
  program_state->color_attribute_location = ATTRIBUTE_LOCATION_UNKNOWN;
  program_state->normal_attribute_location = ATTRIBUTE_LOCATION_UNKNOWN;
  program_state->tex_coord0_attribute_location = ATTRIBUTE_LOCATION_UNKNOWN;
  if (program_state->tex_coord_attribute_locations)
    {
      g_array_free (program_state->tex_coord_attribute_locations, TRUE);
      program_state->tex_coord_attribute_locations = NULL;
    }
}

static void
clear_flushed_matrix_stacks (CoglPipelineProgramState *program_state)
{
  if (program_state->flushed_modelview_stack)
    {
      cogl_object_unref (program_state->flushed_modelview_stack);
      program_state->flushed_modelview_stack = NULL;
    }
  if (program_state->flushed_projection_stack)
    {
      cogl_object_unref (program_state->flushed_projection_stack);
      program_state->flushed_projection_stack = NULL;
    }
  program_state->flushed_modelview_is_identity = FALSE;
}

#endif /* HAVE_COGL_GLES2 */

static CoglPipelineProgramState *
program_state_new (int n_layers)
{
  CoglPipelineProgramState *program_state;

  program_state = g_slice_new (CoglPipelineProgramState);
  program_state->ref_count = 1;
  program_state->program = 0;
  program_state->n_tex_coord_attribs = 0;
  program_state->unit_state = g_new (UnitState, n_layers);
#ifdef HAVE_COGL_GLES2
  program_state->tex_coord_attribute_locations = NULL;
  program_state->flushed_modelview_stack = NULL;
  program_state->flushed_modelview_is_identity = FALSE;
  program_state->flushed_projection_stack = NULL;
#endif

  return program_state;
}

static void
destroy_program_state (void *user_data)
{
  CoglPipelineProgramState *program_state = user_data;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (--program_state->ref_count == 0)
    {
#ifdef HAVE_COGL_GLES2
      if (ctx->driver == COGL_DRIVER_GLES2)
        {
          clear_attribute_cache (program_state);
          clear_flushed_matrix_stacks (program_state);
        }
#endif

      if (program_state->program)
        GE( ctx, glDeleteProgram (program_state->program) );

      g_free (program_state->unit_state);

      g_slice_free (CoglPipelineProgramState, program_state);
    }
}

static void
set_program_state (CoglPipeline *pipeline,
                  CoglPipelineProgramState *program_state)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &program_state_key,
                             program_state,
                             destroy_program_state);
}

static void
dirty_program_state (CoglPipeline *pipeline)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &program_state_key,
                             NULL,
                             NULL);
}

static void
link_program (GLint gl_program)
{
  GLint link_status;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  GE( ctx, glLinkProgram (gl_program) );

  GE( ctx, glGetProgramiv (gl_program, GL_LINK_STATUS, &link_status) );

  if (!link_status)
    {
      GLint log_length;
      GLsizei out_log_length;
      char *log;

      GE( ctx, glGetProgramiv (gl_program, GL_INFO_LOG_LENGTH, &log_length) );

      log = g_malloc (log_length);

      GE( ctx, glGetProgramInfoLog (gl_program, log_length,
                                    &out_log_length, log) );

      g_warning ("Failed to link GLSL program:\n%.*s\n",
                 log_length, log);

      g_free (log);
    }
}

typedef struct
{
  int unit;
  GLuint gl_program;
  gboolean update_all;
  CoglPipelineProgramState *program_state;
} UpdateUniformsState;

static gboolean
get_uniform_cb (CoglPipeline *pipeline,
                int layer_index,
                void *user_data)
{
  UpdateUniformsState *state = user_data;
  CoglPipelineProgramState *program_state = state->program_state;
  UnitState *unit_state = &program_state->unit_state[state->unit];
  GLint uniform_location;

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* We can reuse the source buffer to create the uniform name because
     the program has now been linked */
  g_string_set_size (ctx->codegen_source_buffer, 0);
  g_string_append_printf (ctx->codegen_source_buffer,
                          "_cogl_sampler_%i", state->unit);

  GE_RET( uniform_location,
          ctx, glGetUniformLocation (state->gl_program,
                                     ctx->codegen_source_buffer->str) );

  /* We can set the uniform immediately because the samplers are the
     unit index not the texture object number so it will never
     change. Unfortunately GL won't let us use a constant instead of a
     uniform */
  if (uniform_location != -1)
    GE( ctx, glUniform1i (uniform_location, state->unit) );

  g_string_set_size (ctx->codegen_source_buffer, 0);
  g_string_append_printf (ctx->codegen_source_buffer,
                          "_cogl_layer_constant_%i", state->unit);

  GE_RET( uniform_location,
          ctx, glGetUniformLocation (state->gl_program,
                                     ctx->codegen_source_buffer->str) );

  unit_state->combine_constant_uniform = uniform_location;

#ifdef HAVE_COGL_GLES2
  if (ctx->driver == COGL_DRIVER_GLES2)
    {
      g_string_set_size (ctx->codegen_source_buffer, 0);
      g_string_append_printf (ctx->codegen_source_buffer,
                              "cogl_texture_matrix[%i]", state->unit);

      GE_RET( uniform_location,
              ctx, glGetUniformLocation (state->gl_program,
                                         ctx->codegen_source_buffer->str) );

      unit_state->texture_matrix_uniform = uniform_location;
    }
#endif

  state->unit++;

  return TRUE;
}

static gboolean
update_constants_cb (CoglPipeline *pipeline,
                     int layer_index,
                     void *user_data)
{
  UpdateUniformsState *state = user_data;
  CoglPipelineProgramState *program_state = state->program_state;
  UnitState *unit_state = &program_state->unit_state[state->unit++];

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (unit_state->combine_constant_uniform != -1 &&
      (state->update_all || unit_state->dirty_combine_constant))
    {
      float constant[4];
      _cogl_pipeline_get_layer_combine_constant (pipeline,
                                                 layer_index,
                                                 constant);
      GE (ctx, glUniform4fv (unit_state->combine_constant_uniform,
                             1, constant));
      unit_state->dirty_combine_constant = FALSE;
    }

#ifdef HAVE_COGL_GLES2

  if (ctx->driver == COGL_DRIVER_GLES2 &&
      unit_state->texture_matrix_uniform != -1 &&
      (state->update_all || unit_state->dirty_texture_matrix))
    {
      const CoglMatrix *matrix;
      const float *array;

      matrix = _cogl_pipeline_get_layer_matrix (pipeline, layer_index);
      array = cogl_matrix_get_array (matrix);
      GE (ctx, glUniformMatrix4fv (unit_state->texture_matrix_uniform,
                                   1, FALSE, array));
      unit_state->dirty_texture_matrix = FALSE;
    }

#endif /* HAVE_COGL_GLES2 */

  return TRUE;
}

#ifdef HAVE_COGL_GLES2

static void
update_builtin_uniforms (CoglPipeline *pipeline,
                         GLuint gl_program,
                         CoglPipelineProgramState *program_state)
{
  int i;

  if (program_state->dirty_builtin_uniforms == 0)
    return;

  for (i = 0; i < G_N_ELEMENTS (builtin_uniforms); i++)
    if ((program_state->dirty_builtin_uniforms & (1 << i)) &&
        program_state->builtin_uniform_locations[i] != -1)
      builtin_uniforms[i].update_func (pipeline,
                                       program_state
                                       ->builtin_uniform_locations[i],
                                       builtin_uniforms[i].getter_func);

  program_state->dirty_builtin_uniforms = 0;
}

#endif /* HAVE_COGL_GLES2 */

static void
_cogl_pipeline_progend_glsl_end (CoglPipeline *pipeline,
                                 unsigned long pipelines_difference,
                                 int n_tex_coord_attribs)
{
  CoglPipelineProgramState *program_state;
  GLuint gl_program;
  gboolean program_changed = FALSE;
  UpdateUniformsState state;
  CoglProgram *user_program;
  CoglPipeline *template_pipeline = NULL;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* If neither of the glsl fragend or vertends are used then we don't
     need to do anything */
  if (pipeline->fragend != COGL_PIPELINE_FRAGEND_GLSL &&
      pipeline->vertend != COGL_PIPELINE_VERTEND_GLSL)
    return;

  program_state = get_program_state (pipeline);

  user_program = cogl_pipeline_get_user_program (pipeline);

  if (program_state == NULL)
    {
      CoglPipeline *authority;

      /* Get the authority for anything affecting program state. This
         should include both fragment codegen state and vertex codegen
         state */
      authority = _cogl_pipeline_find_equivalent_parent
        (pipeline,
         (COGL_PIPELINE_STATE_AFFECTS_VERTEX_CODEGEN |
          _cogl_pipeline_get_state_for_fragment_codegen (ctx)) &
         ~COGL_PIPELINE_STATE_LAYERS,
         _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx) |
         COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN);

      program_state = get_program_state (authority);

      if (program_state == NULL)
        {
          /* Check if there is already a similar cached pipeline whose
             program state we can share */
          if (G_LIKELY (!(COGL_DEBUG_ENABLED
                          (COGL_DEBUG_DISABLE_PROGRAM_CACHES))))
            {
              template_pipeline =
                _cogl_pipeline_cache_get_combined_template (ctx->pipeline_cache,
                                                            authority);

              program_state = get_program_state (template_pipeline);
            }

          if (program_state)
            program_state->ref_count++;
          else
            program_state
              = program_state_new (cogl_pipeline_get_n_layers (authority));

          set_program_state (authority, program_state);

          if (template_pipeline)
            {
              program_state->ref_count++;
              set_program_state (template_pipeline, program_state);
            }
        }

      if (authority != pipeline)
        {
          program_state->ref_count++;
          set_program_state (pipeline, program_state);
        }
    }

  /* If the program has changed since the last link then we do
   * need to relink
   *
   * Also if the number of texture coordinate attributes in use has
   * increased, then delete the program so we can prepend a new
   * _cogl_tex_coord[] varying array declaration. */
  if (program_state->program && user_program &&
      (user_program->age != program_state->user_program_age ||
       n_tex_coord_attribs > program_state->n_tex_coord_attribs))
    {
      GE( ctx, glDeleteProgram (program_state->program) );
      program_state->program = 0;
    }

  if (program_state->program == 0)
    {
      GLuint backend_shader;
      GSList *l;

      GE_RET( program_state->program, ctx, glCreateProgram () );

      /* Attach all of the shader from the user program */
      if (user_program)
        {
          if (program_state->n_tex_coord_attribs > n_tex_coord_attribs)
            n_tex_coord_attribs = program_state->n_tex_coord_attribs;

#ifdef HAVE_COGL_GLES2
          /* Find the largest count of texture coordinate attributes
           * associated with each of the shaders so we can ensure a consistent
           * _cogl_tex_coord[] array declaration across all of the shaders.*/
          if (ctx->driver == COGL_DRIVER_GLES2 &&
              user_program)
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

              GE( ctx, glAttachShader (program_state->program,
                                       shader->gl_handle) );
            }

          program_state->user_program_age = user_program->age;
        }

      /* Attach any shaders from the GLSL backends */
      if (pipeline->fragend == COGL_PIPELINE_FRAGEND_GLSL &&
          (backend_shader = _cogl_pipeline_fragend_glsl_get_shader (pipeline)))
        GE( ctx, glAttachShader (program_state->program, backend_shader) );
      if (pipeline->vertend == COGL_PIPELINE_VERTEND_GLSL &&
          (backend_shader = _cogl_pipeline_vertend_glsl_get_shader (pipeline)))
        GE( ctx, glAttachShader (program_state->program, backend_shader) );

      link_program (program_state->program);

      program_changed = TRUE;

      program_state->n_tex_coord_attribs = n_tex_coord_attribs;
    }

  gl_program = program_state->program;

  if (pipeline->fragend == COGL_PIPELINE_FRAGEND_GLSL)
    _cogl_use_fragment_program (gl_program, COGL_PIPELINE_PROGRAM_TYPE_GLSL);
  if (pipeline->vertend == COGL_PIPELINE_VERTEND_GLSL)
    _cogl_use_vertex_program (gl_program, COGL_PIPELINE_PROGRAM_TYPE_GLSL);

  state.unit = 0;
  state.gl_program = gl_program;
  state.program_state = program_state;

  if (program_changed)
    cogl_pipeline_foreach_layer (pipeline,
                                 get_uniform_cb,
                                 &state);

  state.unit = 0;
  state.update_all = (program_changed ||
                      program_state->last_used_for_pipeline != pipeline);

  cogl_pipeline_foreach_layer (pipeline,
                               update_constants_cb,
                               &state);

#ifdef HAVE_COGL_GLES2
  if (ctx->driver == COGL_DRIVER_GLES2)
    {
      if (program_changed)
        {
          int i;

          clear_attribute_cache (program_state);
          clear_flushed_matrix_stacks (program_state);

          for (i = 0; i < G_N_ELEMENTS (builtin_uniforms); i++)
            GE_RET( program_state->builtin_uniform_locations[i], ctx,
                    glGetUniformLocation (gl_program,
                                          builtin_uniforms[i].uniform_name) );

          GE_RET( program_state->modelview_uniform, ctx,
                  glGetUniformLocation (gl_program,
                                        "cogl_modelview_matrix") );

          GE_RET( program_state->projection_uniform, ctx,
                  glGetUniformLocation (gl_program,
                                        "cogl_projection_matrix") );

          GE_RET( program_state->mvp_uniform, ctx,
                  glGetUniformLocation (gl_program,
                                        "cogl_modelview_projection_matrix") );
        }
      if (program_changed ||
          program_state->last_used_for_pipeline != pipeline)
        program_state->dirty_builtin_uniforms = ~(unsigned long) 0;

      update_builtin_uniforms (pipeline, gl_program, program_state);
    }
#endif

  if (user_program)
    _cogl_program_flush_uniforms (user_program,
                                  gl_program,
                                  program_changed);

  /* We need to track the last pipeline that the program was used with
   * so know if we need to update all of the uniforms */
  program_state->last_used_for_pipeline = pipeline;
}

static void
_cogl_pipeline_progend_glsl_pre_change_notify (CoglPipeline *pipeline,
                                               CoglPipelineState change,
                                               const CoglColor *new_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if ((change & _cogl_pipeline_get_state_for_fragment_codegen (ctx)))
    dirty_program_state (pipeline);

#ifdef HAVE_COGL_GLES2
  else if (ctx->driver == COGL_DRIVER_GLES2)
    {
      int i;

      for (i = 0; i < G_N_ELEMENTS (builtin_uniforms); i++)
        if ((change & builtin_uniforms[i].change))
          {
            CoglPipelineProgramState *program_state
              = get_program_state (pipeline);
            if (program_state)
              program_state->dirty_builtin_uniforms |= 1 << i;
            return;
          }
    }
#endif /* HAVE_COGL_GLES2 */
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
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if ((change & _cogl_pipeline_get_state_for_fragment_codegen (ctx)))
    {
      dirty_program_state (owner);
      return;
    }

  if (change & COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT)
    {
      CoglPipelineProgramState *program_state = get_program_state (owner);
      if (program_state)
        {
          int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
          program_state->unit_state[unit_index].dirty_combine_constant = TRUE;
        }
    }

  if (change & COGL_PIPELINE_LAYER_STATE_USER_MATRIX)
    {
      CoglPipelineProgramState *program_state = get_program_state (owner);
      if (program_state)
        {
          int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
          program_state->unit_state[unit_index].dirty_texture_matrix = TRUE;
        }
    }
}

#ifdef HAVE_COGL_GLES2

static void
flush_modelview_cb (gboolean is_identity,
                    const CoglMatrix *matrix,
                    void *user_data)
{
  CoglPipelineProgramState *program_state = user_data;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  GE( ctx, glUniformMatrix4fv (program_state->modelview_uniform, 1, FALSE,
                               cogl_matrix_get_array (matrix)) );
}

static void
flush_projection_cb (gboolean is_identity,
                    const CoglMatrix *matrix,
                    void *user_data)
{
  CoglPipelineProgramState *program_state = user_data;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  GE( ctx, glUniformMatrix4fv (program_state->projection_uniform, 1, FALSE,
                               cogl_matrix_get_array (matrix)) );
}

typedef struct
{
  CoglPipelineProgramState *program_state;
  const CoglMatrix *projection_matrix;
} FlushCombinedData;

static void
flush_combined_step_two_cb (gboolean is_identity,
                            const CoglMatrix *matrix,
                            void *user_data)
{
  FlushCombinedData *data = user_data;
  CoglMatrix mvp_matrix;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* If the modelview is the identity then we can bypass the matrix
     multiplication */
  if (is_identity)
    {
      const float *array = cogl_matrix_get_array (data->projection_matrix);
      GE( ctx, glUniformMatrix4fv (data->program_state->mvp_uniform,
                                   1, FALSE, array ) );
    }
  else
    {
      cogl_matrix_multiply (&mvp_matrix,
                            data->projection_matrix,
                            matrix);

      GE( ctx, glUniformMatrix4fv (data->program_state->mvp_uniform, 1, FALSE,
                                   cogl_matrix_get_array (&mvp_matrix)) );
    }
}

static void
flush_combined_step_one_cb (gboolean is_identity,
                            const CoglMatrix *matrix,
                            void *user_data)
{
  FlushCombinedData data;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  data.program_state = user_data;
  data.projection_matrix = matrix;

  _cogl_matrix_stack_prepare_for_flush (ctx->flushed_modelview_stack,
                                        COGL_MATRIX_MODELVIEW,
                                        flush_combined_step_two_cb,
                                        &data);
}

static void
_cogl_pipeline_progend_glsl_pre_paint (CoglPipeline *pipeline)
{
  CoglPipelineProgramState *program_state = get_program_state (pipeline);
  gboolean modelview_changed;
  gboolean projection_changed;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->driver != COGL_DRIVER_GLES2)
    return;

  /* We only need to update the matrices if we're using the the GLSL
     vertend, but this is a requirement on GLES2 anyway */
  g_return_if_fail (pipeline->vertend == COGL_PIPELINE_VERTEND_GLSL);

  program_state = get_program_state (pipeline);

  /* An initial pipeline is flushed while creating the context. At
     this point there are no matrices flushed so we can't do
     anything */
  if (ctx->flushed_modelview_stack == NULL ||
      ctx->flushed_projection_stack == NULL)
    return;

  /* When flushing from the journal the modelview matrix is usually
     the identity matrix so it makes sense to optimise this case by
     specifically checking whether we already have the identity matrix
     which will catch a lot of common cases of redundant flushing */
  if (program_state->flushed_modelview_is_identity &&
      _cogl_matrix_stack_has_identity_flag (ctx->flushed_modelview_stack))
    modelview_changed = FALSE;
  else
    modelview_changed =
      program_state->flushed_modelview_stack != ctx->flushed_modelview_stack ||
      program_state->flushed_modelview_stack_age !=
      _cogl_matrix_stack_get_age (program_state->flushed_modelview_stack);

  projection_changed =
    program_state->flushed_projection_stack != ctx->flushed_projection_stack ||
    program_state->flushed_projection_stack_age !=
    _cogl_matrix_stack_get_age (program_state->flushed_projection_stack);

  if (modelview_changed)
    {
      cogl_object_ref (ctx->flushed_modelview_stack);
      if (program_state->flushed_modelview_stack)
        cogl_object_unref (program_state->flushed_modelview_stack);
      program_state->flushed_modelview_stack = ctx->flushed_modelview_stack;
      program_state->flushed_modelview_stack_age =
        _cogl_matrix_stack_get_age (ctx->flushed_modelview_stack);
      program_state->flushed_modelview_is_identity =
        _cogl_matrix_stack_has_identity_flag (ctx->flushed_modelview_stack);

      if (program_state->modelview_uniform != -1)
        _cogl_matrix_stack_prepare_for_flush (program_state
                                              ->flushed_modelview_stack,
                                              COGL_MATRIX_MODELVIEW,
                                              flush_modelview_cb,
                                              program_state);
    }

  if (projection_changed)
    {
      cogl_object_ref (ctx->flushed_projection_stack);
      if (program_state->flushed_projection_stack)
        cogl_object_unref (program_state->flushed_projection_stack);
      program_state->flushed_projection_stack = ctx->flushed_projection_stack;
      program_state->flushed_projection_stack_age =
        _cogl_matrix_stack_get_age (ctx->flushed_projection_stack);

      if (program_state->projection_uniform != -1)
        _cogl_matrix_stack_prepare_for_flush (program_state
                                              ->flushed_projection_stack,
                                              COGL_MATRIX_PROJECTION,
                                              flush_projection_cb,
                                              program_state);
    }

  if (program_state->mvp_uniform != -1 &&
      (modelview_changed || projection_changed))
    _cogl_matrix_stack_prepare_for_flush (ctx->flushed_projection_stack,
                                          COGL_MATRIX_PROJECTION,
                                          flush_combined_step_one_cb,
                                          program_state);
}

static void
update_float_uniform (CoglPipeline *pipeline,
                      int uniform_location,
                      void *getter_func)
{
  float (* float_getter_func) (CoglPipeline *) = getter_func;
  float value;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  value = float_getter_func (pipeline);
  GE( ctx, glUniform1f (uniform_location, value) );
}

#endif

const CoglPipelineProgend _cogl_pipeline_glsl_progend =
  {
    _cogl_pipeline_progend_glsl_end,
    _cogl_pipeline_progend_glsl_pre_change_notify,
    _cogl_pipeline_progend_glsl_layer_pre_change_notify,
#ifdef HAVE_COGL_GLES2
    _cogl_pipeline_progend_glsl_pre_paint
#else
    NULL
#endif
  };

#endif /* COGL_PIPELINE_PROGEND_GLSL */

