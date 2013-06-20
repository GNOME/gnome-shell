/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010,2013 Intel Corporation.
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

#include <test-fixtures/test-unit.h>

#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"

#ifdef COGL_PIPELINE_VERTEND_GLSL

#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-program-private.h"
#include "cogl-pipeline-vertend-glsl-private.h"
#include "cogl-pipeline-state-private.h"
#include "cogl-glsl-shader-private.h"

const CoglPipelineVertend _cogl_pipeline_glsl_vertend;

typedef struct
{
  unsigned int ref_count;

  GLuint gl_shader;
  GString *header, *source;

} CoglPipelineShaderState;

static CoglUserDataKey shader_state_key;

static CoglPipelineShaderState *
shader_state_new (void)
{
  CoglPipelineShaderState *shader_state;

  shader_state = g_slice_new0 (CoglPipelineShaderState);
  shader_state->ref_count = 1;

  return shader_state;
}

static CoglPipelineShaderState *
get_shader_state (CoglPipeline *pipeline)
{
  return cogl_object_get_user_data (COGL_OBJECT (pipeline), &shader_state_key);
}

static void
destroy_shader_state (void *user_data)
{
  CoglPipelineShaderState *shader_state = user_data;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (--shader_state->ref_count == 0)
    {
      if (shader_state->gl_shader)
        GE( ctx, glDeleteShader (shader_state->gl_shader) );

      g_slice_free (CoglPipelineShaderState, shader_state);
    }
}

static void
set_shader_state (CoglPipeline *pipeline,
                  CoglPipelineShaderState *shader_state)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &shader_state_key,
                             shader_state,
                             destroy_shader_state);
}

static void
dirty_shader_state (CoglPipeline *pipeline)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &shader_state_key,
                             NULL,
                             NULL);
}

GLuint
_cogl_pipeline_vertend_glsl_get_shader (CoglPipeline *pipeline)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);

  if (shader_state)
    return shader_state->gl_shader;
  else
    return 0;
}

static CoglPipelineSnippetList *
get_vertex_snippets (CoglPipeline *pipeline)
{
  pipeline =
    _cogl_pipeline_get_authority (pipeline,
                                  COGL_PIPELINE_STATE_VERTEX_SNIPPETS);

  return &pipeline->big_state->vertex_snippets;
}

static CoglPipelineSnippetList *
get_layer_vertex_snippets (CoglPipelineLayer *layer)
{
  unsigned long state = COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS;
  layer = _cogl_pipeline_layer_get_authority (layer, state);

  return &layer->big_state->vertex_snippets;
}

static CoglBool
add_layer_declaration_cb (CoglPipelineLayer *layer,
                          void *user_data)
{
  CoglPipelineShaderState *shader_state = user_data;
  CoglTextureType texture_type =
    _cogl_pipeline_layer_get_texture_type (layer);
  const char *target_string;

  _cogl_gl_util_get_texture_target_string (texture_type, &target_string, NULL);

  g_string_append_printf (shader_state->header,
                          "uniform sampler%s cogl_sampler%i;\n",
                          target_string,
                          layer->index);

  return TRUE;
}

static void
add_layer_declarations (CoglPipeline *pipeline,
                        CoglPipelineShaderState *shader_state)
{
  /* We always emit sampler uniforms in case there will be custom
   * layer snippets that want to sample arbitrary layers. */

  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         add_layer_declaration_cb,
                                         shader_state);
}

static void
add_global_declarations (CoglPipeline *pipeline,
                         CoglPipelineShaderState *shader_state)
{
  CoglSnippetHook hook = COGL_SNIPPET_HOOK_VERTEX_GLOBALS;
  CoglPipelineSnippetList *snippets = get_vertex_snippets (pipeline);

  /* Add the global data hooks. All of the code in these snippets is
   * always added and only the declarations data is used */

  _cogl_pipeline_snippet_generate_declarations (shader_state->header,
                                                hook,
                                                snippets);
}

static void
_cogl_pipeline_vertend_glsl_start (CoglPipeline *pipeline,
                                   int n_layers,
                                   unsigned long pipelines_difference)
{
  CoglPipelineShaderState *shader_state;
  CoglPipeline *template_pipeline = NULL;
  CoglProgram *user_program = cogl_pipeline_get_user_program (pipeline);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Now lookup our glsl backend private state (allocating if
   * necessary) */
  shader_state = get_shader_state (pipeline);

  if (shader_state == NULL)
    {
      CoglPipeline *authority;

      /* Get the authority for anything affecting vertex shader
         state */
      authority = _cogl_pipeline_find_equivalent_parent
        (pipeline,
         _cogl_pipeline_get_state_for_vertex_codegen (ctx) &
         ~COGL_PIPELINE_STATE_LAYERS,
         COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN);

      shader_state = get_shader_state (authority);

      if (shader_state == NULL)
        {
          /* Check if there is already a similar cached pipeline whose
             shader state we can share */
          if (G_LIKELY (!(COGL_DEBUG_ENABLED
                          (COGL_DEBUG_DISABLE_PROGRAM_CACHES))))
            {
              template_pipeline =
                _cogl_pipeline_cache_get_vertex_template (ctx->pipeline_cache,
                                                          authority);

              shader_state = get_shader_state (template_pipeline);
            }

          if (shader_state)
            shader_state->ref_count++;
          else
            shader_state = shader_state_new ();

          set_shader_state (authority, shader_state);

          if (template_pipeline)
            {
              shader_state->ref_count++;
              set_shader_state (template_pipeline, shader_state);
            }
        }

      if (authority != pipeline)
        {
          shader_state->ref_count++;
          set_shader_state (pipeline, shader_state);
        }
    }

  if (user_program)
    {
      /* If the user program contains a vertex shader then we don't need
         to generate one */
      if (_cogl_program_has_vertex_shader (user_program))
        {
          if (shader_state->gl_shader)
            {
              GE( ctx, glDeleteShader (shader_state->gl_shader) );
              shader_state->gl_shader = 0;
            }
          return;
        }
    }

  if (shader_state->gl_shader)
    return;

  /* If we make it here then we have a shader_state struct without a gl_shader
     either because this is the first time we've encountered it or
     because the user program has changed */

  /* We reuse two grow-only GStrings for code-gen. One string
     contains the uniform and attribute declarations while the
     other contains the main function. We need two strings
     because we need to dynamically declare attributes as the
     add_layer callback is invoked */
  g_string_set_size (ctx->codegen_header_buffer, 0);
  g_string_set_size (ctx->codegen_source_buffer, 0);
  shader_state->header = ctx->codegen_header_buffer;
  shader_state->source = ctx->codegen_source_buffer;

  add_layer_declarations (pipeline, shader_state);
  add_global_declarations (pipeline, shader_state);

  g_string_append (shader_state->source,
                   "void\n"
                   "cogl_generated_source ()\n"
                   "{\n");

  if (cogl_pipeline_get_per_vertex_point_size (pipeline))
    g_string_append (shader_state->header,
                     "attribute float cogl_point_size_in;\n");
  else if (!(ctx->private_feature_flags &
            COGL_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM))
    {
      /* There is no builtin uniform for the point size on GLES2 so we
         need to copy it from the custom uniform in the vertex shader
         if we're not using per-vertex point sizes, however we'll only
         do this if the point-size is non-zero. Toggle the point size
         between zero and non-zero causes a state change which
         generates a new program */
      if (cogl_pipeline_get_point_size (pipeline) > 0.0f)
        {
          g_string_append (shader_state->header,
                           "uniform float cogl_point_size_in;\n");
          g_string_append (shader_state->source,
                           "  cogl_point_size_out = cogl_point_size_in;\n");
        }
    }
}

static CoglBool
_cogl_pipeline_vertend_glsl_add_layer (CoglPipeline *pipeline,
                                       CoglPipelineLayer *layer,
                                       unsigned long layers_difference,
                                       CoglFramebuffer *framebuffer)
{
  CoglPipelineShaderState *shader_state;
  CoglPipelineSnippetData snippet_data;
  int layer_index = layer->index;

  _COGL_GET_CONTEXT (ctx, FALSE);

  shader_state = get_shader_state (pipeline);

  if (shader_state->source == NULL)
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

  g_string_append_printf (shader_state->header,
                          "vec4\n"
                          "cogl_real_transform_layer%i (mat4 matrix, "
                          "vec4 tex_coord)\n"
                          "{\n"
                          "  return matrix * tex_coord;\n"
                          "}\n",
                          layer_index);

  /* Wrap the layer code in any snippets that have been hooked */
  memset (&snippet_data, 0, sizeof (snippet_data));
  snippet_data.snippets = get_layer_vertex_snippets (layer);
  snippet_data.hook = COGL_SNIPPET_HOOK_TEXTURE_COORD_TRANSFORM;
  snippet_data.chain_function = g_strdup_printf ("cogl_real_transform_layer%i",
                                                 layer_index);
  snippet_data.final_name = g_strdup_printf ("cogl_transform_layer%i",
                                             layer_index);
  snippet_data.function_prefix = g_strdup_printf ("cogl_transform_layer%i",
                                                  layer_index);
  snippet_data.return_type = "vec4";
  snippet_data.return_variable = "cogl_tex_coord";
  snippet_data.return_variable_is_argument = TRUE;
  snippet_data.arguments = "cogl_matrix, cogl_tex_coord";
  snippet_data.argument_declarations = "mat4 cogl_matrix, vec4 cogl_tex_coord";
  snippet_data.source_buf = shader_state->header;

  _cogl_pipeline_snippet_generate_code (&snippet_data);

  g_free ((char *) snippet_data.chain_function);
  g_free ((char *) snippet_data.final_name);
  g_free ((char *) snippet_data.function_prefix);

  g_string_append_printf (shader_state->source,
                          "  cogl_tex_coord%i_out = "
                          "cogl_transform_layer%i (cogl_texture_matrix%i,\n"
                          "                           "
                          "                        cogl_tex_coord%i_in);\n",
                          layer_index,
                          layer_index,
                          layer_index,
                          layer_index);

  return TRUE;
}

static CoglBool
_cogl_pipeline_vertend_glsl_end (CoglPipeline *pipeline,
                                 unsigned long pipelines_difference)
{
  CoglPipelineShaderState *shader_state;

  _COGL_GET_CONTEXT (ctx, FALSE);

  shader_state = get_shader_state (pipeline);

  if (shader_state->source)
    {
      const char *source_strings[2];
      GLint lengths[2];
      GLint compile_status;
      GLuint shader;
      CoglPipelineSnippetData snippet_data;
      CoglPipelineSnippetList *vertex_snippets;
      CoglBool has_per_vertex_point_size =
        cogl_pipeline_get_per_vertex_point_size (pipeline);

      COGL_STATIC_COUNTER (vertend_glsl_compile_counter,
                           "glsl vertex compile counter",
                           "Increments each time a new GLSL "
                           "vertex shader is compiled",
                           0 /* no application private data */);
      COGL_COUNTER_INC (_cogl_uprof_context, vertend_glsl_compile_counter);

      g_string_append (shader_state->header,
                       "void\n"
                       "cogl_real_vertex_transform ()\n"
                       "{\n"
                       "  cogl_position_out = "
                       "cogl_modelview_projection_matrix * "
                       "cogl_position_in;\n"
                       "}\n");

      g_string_append (shader_state->source,
                       "  cogl_vertex_transform ();\n");

      if (has_per_vertex_point_size)
        {
          g_string_append (shader_state->header,
                           "void\n"
                           "cogl_real_point_size_calculation ()\n"
                           "{\n"
                           "  cogl_point_size_out = cogl_point_size_in;\n"
                           "}\n");
          g_string_append (shader_state->source,
                           "  cogl_point_size_calculation ();\n");
        }

      g_string_append (shader_state->source,
                       "  cogl_color_out = cogl_color_in;\n"
                       "}\n");

      vertex_snippets = get_vertex_snippets (pipeline);

      /* Add hooks for the vertex transform part */
      memset (&snippet_data, 0, sizeof (snippet_data));
      snippet_data.snippets = vertex_snippets;
      snippet_data.hook = COGL_SNIPPET_HOOK_VERTEX_TRANSFORM;
      snippet_data.chain_function = "cogl_real_vertex_transform";
      snippet_data.final_name = "cogl_vertex_transform";
      snippet_data.function_prefix = "cogl_vertex_transform";
      snippet_data.source_buf = shader_state->header;
      _cogl_pipeline_snippet_generate_code (&snippet_data);

      /* Add hooks for the point size calculation part */
      if (has_per_vertex_point_size)
        {
          memset (&snippet_data, 0, sizeof (snippet_data));
          snippet_data.snippets = vertex_snippets;
          snippet_data.hook = COGL_SNIPPET_HOOK_POINT_SIZE;
          snippet_data.chain_function = "cogl_real_point_size_calculation";
          snippet_data.final_name = "cogl_point_size_calculation";
          snippet_data.function_prefix = "cogl_point_size_calculation";
          snippet_data.source_buf = shader_state->header;
          _cogl_pipeline_snippet_generate_code (&snippet_data);
        }

      /* Add all of the hooks for vertex processing */
      memset (&snippet_data, 0, sizeof (snippet_data));
      snippet_data.snippets = vertex_snippets;
      snippet_data.hook = COGL_SNIPPET_HOOK_VERTEX;
      snippet_data.chain_function = "cogl_generated_source";
      snippet_data.final_name = "cogl_vertex_hook";
      snippet_data.function_prefix = "cogl_vertex_hook";
      snippet_data.source_buf = shader_state->source;
      _cogl_pipeline_snippet_generate_code (&snippet_data);

      g_string_append (shader_state->source,
                       "void\n"
                       "main ()\n"
                       "{\n"
                       "  cogl_vertex_hook ();\n");

      /* If there are any snippets then we can't rely on the
         projection matrix to flip the rendering for offscreen buffers
         so we'll need to flip it using an extra statement and a
         uniform */
      if (_cogl_pipeline_has_vertex_snippets (pipeline))
        {
          g_string_append (shader_state->header,
                           "uniform vec4 _cogl_flip_vector;\n");
          g_string_append (shader_state->source,
                           "  cogl_position_out *= _cogl_flip_vector;\n");
        }

      g_string_append (shader_state->source,
                       "}\n");

      GE_RET( shader, ctx, glCreateShader (GL_VERTEX_SHADER) );

      lengths[0] = shader_state->header->len;
      source_strings[0] = shader_state->header->str;
      lengths[1] = shader_state->source->len;
      source_strings[1] = shader_state->source->str;

      _cogl_glsl_shader_set_source_with_boilerplate (ctx,
                                                     NULL,
                                                     shader, GL_VERTEX_SHADER,
                                                     pipeline,
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

      shader_state->header = NULL;
      shader_state->source = NULL;
      shader_state->gl_shader = shader;
    }

#ifdef HAVE_COGL_GL
  if ((ctx->private_feature_flags &
       COGL_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM) &&
      (pipelines_difference & COGL_PIPELINE_STATE_POINT_SIZE))
    {
      CoglPipeline *authority =
        _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_POINT_SIZE);

      if (authority->big_state->point_size > 0.0f)
        GE( ctx, glPointSize (authority->big_state->point_size) );
    }
#endif /* HAVE_COGL_GL */

  return TRUE;
}

static void
_cogl_pipeline_vertend_glsl_pre_change_notify (CoglPipeline *pipeline,
                                               CoglPipelineState change,
                                               const CoglColor *new_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if ((change & _cogl_pipeline_get_state_for_vertex_codegen (ctx)))
    dirty_shader_state (pipeline);
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
  CoglPipelineShaderState *shader_state;

  shader_state = get_shader_state (owner);
  if (!shader_state)
    return;

  if ((change & COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN))
    {
      dirty_shader_state (owner);
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

UNIT_TEST (check_point_size_shader,
           0 /* no requirements */,
           0 /* no failure cases */)
{
  CoglPipeline *pipelines[4];
  CoglPipelineShaderState *shader_states[G_N_ELEMENTS (pipelines)];
  int i;

  /* Default pipeline with zero point size */
  pipelines[0] = cogl_pipeline_new (test_ctx);

  /* Point size 1 */
  pipelines[1] = cogl_pipeline_new (test_ctx);
  cogl_pipeline_set_point_size (pipelines[1], 1.0f);

  /* Point size 2 */
  pipelines[2] = cogl_pipeline_new (test_ctx);
  cogl_pipeline_set_point_size (pipelines[2], 2.0f);

  /* Same as the first pipeline, but reached by restoring the old
   * state from a copy */
  pipelines[3] = cogl_pipeline_copy (pipelines[1]);
  cogl_pipeline_set_point_size (pipelines[3], 0.0f);

  /* Draw something with all of the pipelines to make sure their state
   * is flushed */
  for (i = 0; i < G_N_ELEMENTS (pipelines); i++)
    cogl_framebuffer_draw_rectangle (test_fb,
                                     pipelines[i],
                                     0.0f, 0.0f,
                                     10.0f, 10.0f);
  cogl_framebuffer_finish (test_fb);

  /* Get all of the shader states. These might be NULL if the driver
   * is not using GLSL */
  for (i = 0; i < G_N_ELEMENTS (pipelines); i++)
    shader_states[i] = get_shader_state (pipelines[i]);

  /* If the first two pipelines are using GLSL then they should have
   * the same shader unless there is no builtin uniform for the point
   * size */
  if (shader_states[0])
    {
      if ((test_ctx->private_feature_flags &
           COGL_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM))
        g_assert (shader_states[0] == shader_states[1]);
      else
        g_assert (shader_states[0] != shader_states[1]);
    }

  /* The second and third pipelines should always have the same shader
   * state because only toggling between zero and non-zero should
   * change the shader */
  g_assert (shader_states[1] == shader_states[2]);

  /* The fourth pipeline should be exactly the same as the first */
  g_assert (shader_states[0] == shader_states[3]);
}

#endif /* COGL_PIPELINE_VERTEND_GLSL */
