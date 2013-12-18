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

#include <string.h>

#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-offscreen.h"

#ifdef COGL_PIPELINE_PROGEND_GLSL

#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-program-private.h"
#include "cogl-pipeline-fragend-glsl-private.h"
#include "cogl-pipeline-vertend-glsl-private.h"
#include "cogl-pipeline-cache.h"
#include "cogl-pipeline-state-private.h"
#include "cogl-attribute-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-pipeline-progend-glsl-private.h"

/* These are used to generalise updating some uniforms that are
   required when building for drivers missing some fixed function
   state that we use */

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

  /* This builtin is only necessary if the following private feature
   * is not implemented in the driver */
  CoglPrivateFeature feature_replacement;
} BuiltinUniformData;

static BuiltinUniformData builtin_uniforms[] =
  {
    { "cogl_point_size_in",
      cogl_pipeline_get_point_size, update_float_uniform,
      COGL_PIPELINE_STATE_POINT_SIZE,
      COGL_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM },
    { "_cogl_alpha_test_ref",
      cogl_pipeline_get_alpha_test_reference, update_float_uniform,
      COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE,
      COGL_PRIVATE_FEATURE_ALPHA_TEST }
  };

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

  unsigned long dirty_builtin_uniforms;
  GLint builtin_uniform_locations[G_N_ELEMENTS (builtin_uniforms)];

  GLint modelview_uniform;
  GLint projection_uniform;
  GLint mvp_uniform;

  CoglMatrixEntryCache projection_cache;
  CoglMatrixEntryCache modelview_cache;

  /* We need to track the last pipeline that the program was used with
   * so know if we need to update all of the uniforms */
  CoglPipeline *last_used_for_pipeline;

  /* Array of GL uniform locations indexed by Cogl's uniform
     location. We are careful only to allocated this array if a custom
     uniform is actually set */
  GArray *uniform_locations;

  /* Array of attribute locations. */
  GArray *attribute_locations;

  /* The 'flip' uniform is used to flip the geometry upside-down when
     the framebuffer requires it only when there are vertex
     snippets. Otherwise this is acheived using the projection
     matrix */
  GLint flip_uniform;
  int flushed_flip_state;

  UnitState *unit_state;

  CoglPipelineCacheEntry *cache_entry;
} CoglPipelineProgramState;

static CoglUserDataKey program_state_key;

static CoglPipelineProgramState *
get_program_state (CoglPipeline *pipeline)
{
  return cogl_object_get_user_data (COGL_OBJECT (pipeline), &program_state_key);
}

#define UNIFORM_LOCATION_UNKNOWN -2

#define ATTRIBUTE_LOCATION_UNKNOWN -2

/* Under GLES2 the vertex attribute API needs to query the attribute
   numbers because it can't used the fixed function API to set the
   builtin attributes. We cache the attributes here because the
   progend knows when the program is changed so it can clear the
   cache. This should always be called after the pipeline is flushed
   so they can assert that the gl program is valid */

/* All attributes names get internally mapped to a global set of
 * sequential indices when they are setup which we need to need to
 * then be able to map to a GL attribute location once we have
 * a linked GLSL program */

int
_cogl_pipeline_progend_glsl_get_attrib_location (CoglPipeline *pipeline,
                                                 int name_index)
{
  CoglPipelineProgramState *program_state = get_program_state (pipeline);
  int *locations;

  _COGL_GET_CONTEXT (ctx, -1);

  _COGL_RETURN_VAL_IF_FAIL (program_state != NULL, -1);
  _COGL_RETURN_VAL_IF_FAIL (program_state->program != 0, -1);

  if (G_UNLIKELY (program_state->attribute_locations == NULL))
    program_state->attribute_locations =
      g_array_new (FALSE, FALSE, sizeof (int));

  if (G_UNLIKELY (program_state->attribute_locations->len <= name_index))
    {
      int i = program_state->attribute_locations->len;
      g_array_set_size (program_state->attribute_locations, name_index + 1);
      for (; i < program_state->attribute_locations->len; i++)
        g_array_index (program_state->attribute_locations, int, i)
          = ATTRIBUTE_LOCATION_UNKNOWN;
    }

  locations = &g_array_index (program_state->attribute_locations, int, 0);

  if (locations[name_index] == ATTRIBUTE_LOCATION_UNKNOWN)
    {
      CoglAttributeNameState *name_state =
        g_array_index (ctx->attribute_name_index_map,
                       CoglAttributeNameState *, name_index);

      _COGL_RETURN_VAL_IF_FAIL (name_state != NULL, 0);

      GE_RET( locations[name_index],
              ctx, glGetAttribLocation (program_state->program,
                                        name_state->name) );
    }

  return locations[name_index];
}

static void
clear_attribute_cache (CoglPipelineProgramState *program_state)
{
  if (program_state->attribute_locations)
    {
      g_array_free (program_state->attribute_locations, TRUE);
      program_state->attribute_locations = NULL;
    }
}

static void
clear_flushed_matrix_stacks (CoglPipelineProgramState *program_state)
{
  _cogl_matrix_entry_cache_destroy (&program_state->projection_cache);
  _cogl_matrix_entry_cache_init (&program_state->projection_cache);
  _cogl_matrix_entry_cache_destroy (&program_state->modelview_cache);
  _cogl_matrix_entry_cache_init (&program_state->modelview_cache);
}

static CoglPipelineProgramState *
program_state_new (int n_layers,
                   CoglPipelineCacheEntry *cache_entry)
{
  CoglPipelineProgramState *program_state;

  program_state = g_slice_new (CoglPipelineProgramState);
  program_state->ref_count = 1;
  program_state->program = 0;
  program_state->unit_state = g_new (UnitState, n_layers);
  program_state->uniform_locations = NULL;
  program_state->attribute_locations = NULL;
  program_state->cache_entry = cache_entry;
  _cogl_matrix_entry_cache_init (&program_state->modelview_cache);
  _cogl_matrix_entry_cache_init (&program_state->projection_cache);

  return program_state;
}

static void
destroy_program_state (void *user_data,
                       void *instance)
{
  CoglPipelineProgramState *program_state = user_data;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* If the program state was last used for this pipeline then clear
     it so that if same address gets used again for a new pipeline
     then we won't think it's the same pipeline and avoid updating the
     uniforms */
  if (program_state->last_used_for_pipeline == instance)
    program_state->last_used_for_pipeline = NULL;

  if (program_state->cache_entry &&
      program_state->cache_entry->pipeline != instance)
    program_state->cache_entry->usage_count--;

  if (--program_state->ref_count == 0)
    {
      clear_attribute_cache (program_state);

      _cogl_matrix_entry_cache_destroy (&program_state->projection_cache);
      _cogl_matrix_entry_cache_destroy (&program_state->modelview_cache);

      if (program_state->program)
        GE( ctx, glDeleteProgram (program_state->program) );

      g_free (program_state->unit_state);

      if (program_state->uniform_locations)
        g_array_free (program_state->uniform_locations, TRUE);

      g_slice_free (CoglPipelineProgramState, program_state);
    }
}

static void
set_program_state (CoglPipeline *pipeline,
                  CoglPipelineProgramState *program_state)
{
  if (program_state)
    {
      program_state->ref_count++;

      /* If we're not setting the state on the template pipeline then
       * mark it as a usage of the pipeline cache entry */
      if (program_state->cache_entry &&
          program_state->cache_entry->pipeline != pipeline)
        program_state->cache_entry->usage_count++;
    }

  _cogl_object_set_user_data (COGL_OBJECT (pipeline),
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
  CoglBool update_all;
  CoglPipelineProgramState *program_state;
} UpdateUniformsState;

static CoglBool
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
                          "cogl_sampler%i", layer_index);

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
                          "_cogl_layer_constant_%i", layer_index);

  GE_RET( uniform_location,
          ctx, glGetUniformLocation (state->gl_program,
                                     ctx->codegen_source_buffer->str) );

  unit_state->combine_constant_uniform = uniform_location;

  g_string_set_size (ctx->codegen_source_buffer, 0);
  g_string_append_printf (ctx->codegen_source_buffer,
                          "cogl_texture_matrix[%i]", layer_index);

  GE_RET( uniform_location,
          ctx, glGetUniformLocation (state->gl_program,
                                     ctx->codegen_source_buffer->str) );

  unit_state->texture_matrix_uniform = uniform_location;

  state->unit++;

  return TRUE;
}

static CoglBool
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

  if (unit_state->texture_matrix_uniform != -1 &&
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

  return TRUE;
}

static void
update_builtin_uniforms (CoglContext *context,
                         CoglPipeline *pipeline,
                         GLuint gl_program,
                         CoglPipelineProgramState *program_state)
{
  int i;

  if (program_state->dirty_builtin_uniforms == 0)
    return;

  for (i = 0; i < G_N_ELEMENTS (builtin_uniforms); i++)
    if (!_cogl_has_private_feature (context,
                                    builtin_uniforms[i].feature_replacement) &&
        (program_state->dirty_builtin_uniforms & (1 << i)) &&
        program_state->builtin_uniform_locations[i] != -1)
      builtin_uniforms[i].update_func (pipeline,
                                       program_state
                                       ->builtin_uniform_locations[i],
                                       builtin_uniforms[i].getter_func);

  program_state->dirty_builtin_uniforms = 0;
}

typedef struct
{
  CoglPipelineProgramState *program_state;
  unsigned long *uniform_differences;
  int n_differences;
  CoglContext *ctx;
  const CoglBoxedValue *values;
  int value_index;
} FlushUniformsClosure;

static CoglBool
flush_uniform_cb (int uniform_num, void *user_data)
{
  FlushUniformsClosure *data = user_data;

  if (COGL_FLAGS_GET (data->uniform_differences, uniform_num))
    {
      GArray *uniform_locations;
      GLint uniform_location;

      if (data->program_state->uniform_locations == NULL)
        data->program_state->uniform_locations =
          g_array_new (FALSE, FALSE, sizeof (GLint));

      uniform_locations = data->program_state->uniform_locations;

      if (uniform_locations->len <= uniform_num)
        {
          unsigned int old_len = uniform_locations->len;

          g_array_set_size (uniform_locations, uniform_num + 1);

          while (old_len <= uniform_num)
            {
              g_array_index (uniform_locations, GLint, old_len) =
                UNIFORM_LOCATION_UNKNOWN;
              old_len++;
            }
        }

      uniform_location = g_array_index (uniform_locations, GLint, uniform_num);

      if (uniform_location == UNIFORM_LOCATION_UNKNOWN)
        {
          const char *uniform_name =
            g_ptr_array_index (data->ctx->uniform_names, uniform_num);

          uniform_location =
            data->ctx->glGetUniformLocation (data->program_state->program,
                                             uniform_name);
          g_array_index (uniform_locations, GLint, uniform_num) =
            uniform_location;
        }

      if (uniform_location != -1)
        _cogl_boxed_value_set_uniform (data->ctx,
                                       uniform_location,
                                       data->values + data->value_index);

      data->n_differences--;
      COGL_FLAGS_SET (data->uniform_differences, uniform_num, FALSE);
    }

  data->value_index++;

  return data->n_differences > 0;
}

static void
_cogl_pipeline_progend_glsl_flush_uniforms (CoglPipeline *pipeline,
                                            CoglPipelineProgramState *
                                                                  program_state,
                                            GLuint gl_program,
                                            CoglBool program_changed)
{
  CoglPipelineUniformsState *uniforms_state;
  FlushUniformsClosure data;
  int n_uniform_longs;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (pipeline->differences & COGL_PIPELINE_STATE_UNIFORMS)
    uniforms_state = &pipeline->big_state->uniforms_state;
  else
    uniforms_state = NULL;

  data.program_state = program_state;
  data.ctx = ctx;

  n_uniform_longs = COGL_FLAGS_N_LONGS_FOR_SIZE (ctx->n_uniform_names);

  data.uniform_differences = g_newa (unsigned long, n_uniform_longs);

  /* Try to find a common ancestor for the values that were already
     flushed on the pipeline that this program state was last used for
     so we can avoid flushing those */

  if (program_changed || program_state->last_used_for_pipeline == NULL)
    {
      if (program_changed)
        {
          /* The program has changed so all of the uniform locations
             are invalid */
          if (program_state->uniform_locations)
            g_array_set_size (program_state->uniform_locations, 0);
        }

      /* We need to flush everything so mark all of the uniforms as
         dirty */
      memset (data.uniform_differences, 0xff,
              n_uniform_longs * sizeof (unsigned long));
      data.n_differences = G_MAXINT;
    }
  else if (program_state->last_used_for_pipeline)
    {
      int i;

      memset (data.uniform_differences, 0,
              n_uniform_longs * sizeof (unsigned long));
      _cogl_pipeline_compare_uniform_differences
        (data.uniform_differences,
         program_state->last_used_for_pipeline,
         pipeline);

      /* We need to be sure to flush any uniforms that have changed
         since the last flush */
      if (uniforms_state)
        _cogl_bitmask_set_flags (&uniforms_state->changed_mask,
                                 data.uniform_differences);

      /* Count the number of differences. This is so we can stop early
         when we've flushed all of them */
      data.n_differences = 0;

      for (i = 0; i < n_uniform_longs; i++)
        data.n_differences +=
          _cogl_util_popcountl (data.uniform_differences[i]);
    }

  while (pipeline && data.n_differences > 0)
    {
      if (pipeline->differences & COGL_PIPELINE_STATE_UNIFORMS)
        {
          const CoglPipelineUniformsState *parent_uniforms_state =
            &pipeline->big_state->uniforms_state;

          data.values = parent_uniforms_state->override_values;
          data.value_index = 0;

          _cogl_bitmask_foreach (&parent_uniforms_state->override_mask,
                                 flush_uniform_cb,
                                 &data);
        }

      pipeline = _cogl_pipeline_get_parent (pipeline);
    }

  if (uniforms_state)
    _cogl_bitmask_clear_all (&uniforms_state->changed_mask);
}

static CoglBool
_cogl_pipeline_progend_glsl_start (CoglPipeline *pipeline)
{
  CoglHandle user_program;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_GLSL))
    return FALSE;

  user_program = cogl_pipeline_get_user_program (pipeline);
  if (user_program &&
      _cogl_program_get_language (user_program) != COGL_SHADER_LANGUAGE_GLSL)
    return FALSE;

  return TRUE;
}

static void
_cogl_pipeline_progend_glsl_end (CoglPipeline *pipeline,
                                 unsigned long pipelines_difference)
{
  CoglPipelineProgramState *program_state;
  GLuint gl_program;
  CoglBool program_changed = FALSE;
  UpdateUniformsState state;
  CoglProgram *user_program;
  CoglPipelineCacheEntry *cache_entry = NULL;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

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
         (_cogl_pipeline_get_state_for_vertex_codegen (ctx) |
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
              cache_entry =
                _cogl_pipeline_cache_get_combined_template (ctx->pipeline_cache,
                                                            authority);

              program_state = get_program_state (cache_entry->pipeline);
            }

          if (program_state)
            program_state->ref_count++;
          else
            program_state
              = program_state_new (cogl_pipeline_get_n_layers (authority),
                                   cache_entry);

          set_program_state (authority, program_state);

          program_state->ref_count--;

          if (cache_entry)
            set_program_state (cache_entry->pipeline, program_state);
        }

      if (authority != pipeline)
        set_program_state (pipeline, program_state);
    }

  /* If the program has changed since the last link then we do
   * need to relink */
  if (program_state->program && user_program &&
       user_program->age != program_state->user_program_age)
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
          for (l = user_program->attached_shaders; l; l = l->next)
            {
              CoglShader *shader = l->data;

              _cogl_shader_compile_real (shader, pipeline);

              g_assert (shader->language == COGL_SHADER_LANGUAGE_GLSL);

              GE( ctx, glAttachShader (program_state->program,
                                       shader->gl_handle) );
            }

          program_state->user_program_age = user_program->age;
        }

      /* Attach any shaders from the GLSL backends */
      if ((backend_shader = _cogl_pipeline_fragend_glsl_get_shader (pipeline)))
        GE( ctx, glAttachShader (program_state->program, backend_shader) );
      if ((backend_shader = _cogl_pipeline_vertend_glsl_get_shader (pipeline)))
        GE( ctx, glAttachShader (program_state->program, backend_shader) );

      /* XXX: OpenGL as a special case requires the vertex position to
       * be bound to generic attribute 0 so for simplicity we
       * unconditionally bind the cogl_position_in attribute here...
       */
      GE( ctx, glBindAttribLocation (program_state->program,
                                     0, "cogl_position_in"));

      link_program (program_state->program);

      program_changed = TRUE;
    }

  gl_program = program_state->program;

  _cogl_use_fragment_program (gl_program, COGL_PIPELINE_PROGRAM_TYPE_GLSL);
  _cogl_use_vertex_program (gl_program, COGL_PIPELINE_PROGRAM_TYPE_GLSL);

  state.unit = 0;
  state.gl_program = gl_program;
  state.program_state = program_state;

  if (program_changed)
    {
      cogl_pipeline_foreach_layer (pipeline,
                                   get_uniform_cb,
                                   &state);
      clear_attribute_cache (program_state);

      GE_RET (program_state->flip_uniform,
              ctx, glGetUniformLocation (gl_program, "_cogl_flip_vector"));
      program_state->flushed_flip_state = -1;
    }

  state.unit = 0;
  state.update_all = (program_changed ||
                      program_state->last_used_for_pipeline != pipeline);

  cogl_pipeline_foreach_layer (pipeline,
                               update_constants_cb,
                               &state);

  if (program_changed)
    {
      int i;

      clear_flushed_matrix_stacks (program_state);

      for (i = 0; i < G_N_ELEMENTS (builtin_uniforms); i++)
        if (!_cogl_has_private_feature
            (ctx, builtin_uniforms[i].feature_replacement))
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

  update_builtin_uniforms (ctx, pipeline, gl_program, program_state);

  _cogl_pipeline_progend_glsl_flush_uniforms (pipeline,
                                              program_state,
                                              gl_program,
                                              program_changed);

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

  if ((change & (_cogl_pipeline_get_state_for_vertex_codegen (ctx) |
                 _cogl_pipeline_get_state_for_fragment_codegen (ctx))))
    {
      dirty_program_state (pipeline);
    }
  else
    {
      int i;

      for (i = 0; i < G_N_ELEMENTS (builtin_uniforms); i++)
        if (!_cogl_has_private_feature
            (ctx, builtin_uniforms[i].feature_replacement) &&
            (change & builtin_uniforms[i].change))
          {
            CoglPipelineProgramState *program_state
              = get_program_state (pipeline);
            if (program_state)
              program_state->dirty_builtin_uniforms |= 1 << i;
            return;
          }
    }
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

  if ((change & (_cogl_pipeline_get_layer_state_for_fragment_codegen (ctx) |
                 COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN)))
    {
      dirty_program_state (owner);
    }
  else if (change & COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT)
    {
      CoglPipelineProgramState *program_state = get_program_state (owner);
      if (program_state)
        {
          int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
          program_state->unit_state[unit_index].dirty_combine_constant = TRUE;
        }
    }
  else if (change & COGL_PIPELINE_LAYER_STATE_USER_MATRIX)
    {
      CoglPipelineProgramState *program_state = get_program_state (owner);
      if (program_state)
        {
          int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
          program_state->unit_state[unit_index].dirty_texture_matrix = TRUE;
        }
    }
}

static void
_cogl_pipeline_progend_glsl_pre_paint (CoglPipeline *pipeline,
                                       CoglFramebuffer *framebuffer)
{
  CoglBool needs_flip;
  CoglMatrixEntry *projection_entry;
  CoglMatrixEntry *modelview_entry;
  CoglPipelineProgramState *program_state;
  CoglBool modelview_changed;
  CoglBool projection_changed;
  CoglBool need_modelview;
  CoglBool need_projection;
  CoglMatrix modelview, projection;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  program_state = get_program_state (pipeline);

  projection_entry = ctx->current_projection_entry;
  modelview_entry = ctx->current_modelview_entry;

  /* An initial pipeline is flushed while creating the context. At
     this point there are no matrices selected so we can't do
     anything */
  if (modelview_entry == NULL || projection_entry == NULL)
    return;

  needs_flip = cogl_is_offscreen (ctx->current_draw_buffer);

  projection_changed =
    _cogl_matrix_entry_cache_maybe_update (&program_state->projection_cache,
                                           projection_entry,
                                           (needs_flip &&
                                            program_state->flip_uniform ==
                                            -1));

  modelview_changed =
    _cogl_matrix_entry_cache_maybe_update (&program_state->modelview_cache,
                                           modelview_entry,
                                           /* never flip modelview */
                                           FALSE);

  if (modelview_changed || projection_changed)
    {
      if (program_state->mvp_uniform != -1)
        need_modelview = need_projection = TRUE;
      else
        {
          need_projection = (program_state->projection_uniform != -1 &&
                             projection_changed);
          need_modelview = (program_state->modelview_uniform != -1 &&
                            modelview_changed);
        }

      if (need_modelview)
        cogl_matrix_entry_get (modelview_entry, &modelview);
      if (need_projection)
        {
          if (needs_flip && program_state->flip_uniform == -1)
            {
              CoglMatrix tmp_matrix;
              cogl_matrix_entry_get (projection_entry, &tmp_matrix);
              cogl_matrix_multiply (&projection,
                                    &ctx->y_flip_matrix,
                                    &tmp_matrix);
            }
          else
            cogl_matrix_entry_get (projection_entry, &projection);
        }

      if (projection_changed && program_state->projection_uniform != -1)
        GE (ctx, glUniformMatrix4fv (program_state->projection_uniform,
                                     1, /* count */
                                     FALSE, /* transpose */
                                     cogl_matrix_get_array (&projection)));

      if (modelview_changed && program_state->modelview_uniform != -1)
        GE (ctx, glUniformMatrix4fv (program_state->modelview_uniform,
                                     1, /* count */
                                     FALSE, /* transpose */
                                     cogl_matrix_get_array (&modelview)));

      if (program_state->mvp_uniform != -1)
        {
          /* The journal usually uses an identity matrix for the
             modelview so we can optimise this common case by
             avoiding the matrix multiplication */
          if (cogl_matrix_entry_is_identity (modelview_entry))
            {
              GE (ctx,
                  glUniformMatrix4fv (program_state->mvp_uniform,
                                      1, /* count */
                                      FALSE, /* transpose */
                                      cogl_matrix_get_array (&projection)));
            }
          else
            {
              CoglMatrix combined;

              cogl_matrix_multiply (&combined,
                                    &projection,
                                    &modelview);
              GE (ctx,
                  glUniformMatrix4fv (program_state->mvp_uniform,
                                      1, /* count */
                                      FALSE, /* transpose */
                                      cogl_matrix_get_array (&combined)));
            }
        }
    }

  if (program_state->flip_uniform != -1
      && program_state->flushed_flip_state != needs_flip)
    {
      static const float do_flip[4] = { 1.0f, -1.0f, 1.0f, 1.0f };
      static const float dont_flip[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
      GE( ctx, glUniform4fv (program_state->flip_uniform,
                             1, /* count */
                             needs_flip ? do_flip : dont_flip) );
      program_state->flushed_flip_state = needs_flip;
    }
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

const CoglPipelineProgend _cogl_pipeline_glsl_progend =
  {
    COGL_PIPELINE_VERTEND_GLSL,
    COGL_PIPELINE_FRAGEND_GLSL,
    _cogl_pipeline_progend_glsl_start,
    _cogl_pipeline_progend_glsl_end,
    _cogl_pipeline_progend_glsl_pre_change_notify,
    _cogl_pipeline_progend_glsl_layer_pre_change_notify,
    _cogl_pipeline_progend_glsl_pre_paint
  };

#endif /* COGL_PIPELINE_PROGEND_GLSL */
