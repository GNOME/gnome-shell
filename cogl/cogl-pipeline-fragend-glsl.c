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
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-pipeline-private.h"
#include "cogl-shader-private.h"
#include "cogl-blend-string.h"

#ifdef COGL_PIPELINE_FRAGEND_GLSL

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl-handle.h"
#include "cogl-shader-private.h"
#include "cogl-program-private.h"
#include "cogl-pipeline-cache.h"

#include <glib.h>

/*
 * GL/GLES compatability defines for pipeline thingies:
 */

/* This might not be defined on GLES */
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D                           0x806F
#endif

const CoglPipelineFragend _cogl_pipeline_glsl_backend;

typedef struct _UnitState
{
  unsigned int sampled:1;
  unsigned int combine_constant_used:1;
} UnitState;

typedef struct
{
  int ref_count;

  GLuint gl_shader;
  GString *header, *source;
  UnitState *unit_state;

  /* Age of the user program that was current when the shader was
     generated. We need to keep track of this because if the user
     program changes then we may need to redecide whether to generate
     a shader at all */
  unsigned int user_program_age;
} CoglPipelineShaderState;

static CoglUserDataKey shader_state_key;

static CoglPipelineShaderState *
shader_state_new (int n_layers)
{
  CoglPipelineShaderState *shader_state;

  shader_state = g_slice_new0 (CoglPipelineShaderState);
  shader_state->ref_count = 1;
  shader_state->unit_state = g_new0 (UnitState, n_layers);

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

      g_free (shader_state->unit_state);

      g_slice_free (CoglPipelineShaderState, shader_state);
    }
}

static void
set_shader_state (CoglPipeline *pipeline, CoglPipelineShaderState *shader_state)
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
_cogl_pipeline_fragend_glsl_get_shader (CoglPipeline *pipeline)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);

  if (shader_state)
    return shader_state->gl_shader;
  else
    return 0;
}

static gboolean
_cogl_pipeline_fragend_glsl_start (CoglPipeline *pipeline,
                                   int n_layers,
                                   unsigned long pipelines_difference,
                                   int n_tex_coord_attribs)
{
  CoglPipelineShaderState *shader_state;
  CoglPipeline *authority;
  CoglPipeline *template_pipeline = NULL;
  CoglProgram *user_program;
  int i;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!cogl_features_available (COGL_FEATURE_SHADERS_GLSL))
    return FALSE;

  user_program = cogl_pipeline_get_user_program (pipeline);

  /* If the user fragment shader isn't GLSL then we should let
     another backend handle it */
  if (user_program &&
      _cogl_program_has_fragment_shader (user_program) &&
      _cogl_program_get_language (user_program) != COGL_SHADER_LANGUAGE_GLSL)
    return FALSE;

  /* Now lookup our glsl backend private state */
  shader_state = get_shader_state (pipeline);

  if (shader_state == NULL)
    {
      /* If we don't have an associated glsl shader yet then find the
       * glsl-authority (the oldest ancestor whose state will result in
       * the same shader being generated as for this pipeline).
       *
       * We always make sure to associate new shader with the
       * glsl-authority to maximize the chance that other pipelines can
       * share it.
       */
      authority = _cogl_pipeline_find_equivalent_parent
        (pipeline,
         _cogl_pipeline_get_state_for_fragment_codegen (ctx) &
         ~COGL_PIPELINE_STATE_LAYERS,
         _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx));

      shader_state = get_shader_state (authority);

      /* If we don't have an existing program associated with the
       * glsl-authority then start generating code for a new shader...
       */
      if (shader_state == NULL)
        {
          /* Check if there is already a similar cached pipeline whose
             shader state we can share */
          if (G_LIKELY (!(COGL_DEBUG_ENABLED
                          (COGL_DEBUG_DISABLE_PROGRAM_CACHES))))
            {
              template_pipeline =
                _cogl_pipeline_cache_get_fragment_template (ctx->pipeline_cache,
                                                            authority);

              shader_state = get_shader_state (template_pipeline);
            }

          if (shader_state)
            shader_state->ref_count++;
          else
            shader_state = shader_state_new (n_layers);

          set_shader_state (authority, shader_state);

          if (template_pipeline)
            {
              shader_state->ref_count++;
              set_shader_state (template_pipeline, shader_state);
            }
        }

      /* If the pipeline isn't actually its own glsl-authority
       * then take a reference to the program state associated
       * with the glsl-authority... */
      if (authority != pipeline)
        {
          shader_state->ref_count++;
          set_shader_state (pipeline, shader_state);
        }
    }

  if (shader_state->gl_shader)
    {
      /* If we already have a valid GLSL shader then we don't need to
         generate a new one. However if there's a user program and it
         has changed since the last link then we do need a new shader */
      if (user_program == NULL ||
          shader_state->user_program_age == user_program->age)
        return TRUE;

      /* We need to recreate the shader so destroy the existing one */
      GE( ctx, glDeleteShader (shader_state->gl_shader) );
      shader_state->gl_shader = 0;
    }

  /* If we make it here then we have a glsl_shader_state struct
     without a gl_shader either because this is the first time we've
     encountered it or because the user program has changed */

  if (user_program)
    shader_state->user_program_age = user_program->age;

  /* If the user program contains a fragment shader then we don't need
     to generate one */
  if (user_program &&
      _cogl_program_has_fragment_shader (user_program))
    return TRUE;

  /* We reuse two grow-only GStrings for code-gen. One string
     contains the uniform and attribute declarations while the
     other contains the main function. We need two strings
     because we need to dynamically declare attributes as the
     add_layer callback is invoked */
  g_string_set_size (ctx->codegen_header_buffer, 0);
  g_string_set_size (ctx->codegen_source_buffer, 0);
  shader_state->header = ctx->codegen_header_buffer;
  shader_state->source = ctx->codegen_source_buffer;

  g_string_append (shader_state->source,
                   "void\n"
                   "main ()\n"
                   "{\n");

  for (i = 0; i < n_layers; i++)
    {
      shader_state->unit_state[i].sampled = FALSE;
      shader_state->unit_state[i].combine_constant_used = FALSE;
    }

  return TRUE;
}

static void
add_constant_lookup (CoglPipelineShaderState *shader_state,
                     CoglPipeline *pipeline,
                     CoglPipelineLayer *layer,
                     const char *swizzle)
{
  int unit_index = _cogl_pipeline_layer_get_unit_index (layer);

  /* Create a sampler uniform for this layer if we haven't already */
  if (!shader_state->unit_state[unit_index].combine_constant_used)
    {
      g_string_append_printf (shader_state->header,
                              "uniform vec4 _cogl_layer_constant_%i;\n",
                              unit_index);
      shader_state->unit_state[unit_index].combine_constant_used = TRUE;
    }

  g_string_append_printf (shader_state->source,
                          "_cogl_layer_constant_%i.%s",
                          unit_index, swizzle);
}

static void
add_texture_lookup (CoglPipelineShaderState *shader_state,
                    CoglPipeline *pipeline,
                    CoglPipelineLayer *layer,
                    const char *swizzle)
{
  CoglHandle texture;
  int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
  const char *target_string, *tex_coord_swizzle;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_TEXTURING)))
    {
      g_string_append (shader_state->source,
                       "vec4 (1.0, 1.0, 1.0, 1.0).");
      g_string_append (shader_state->source, swizzle);

      return;
    }

  texture = _cogl_pipeline_layer_get_texture (layer);

  if (texture == COGL_INVALID_HANDLE)
    {
      target_string = "2D";
      tex_coord_swizzle = "st";
    }
  else
    {
      GLenum gl_target;

      cogl_texture_get_gl_texture (texture, NULL, &gl_target);
      switch (gl_target)
        {
#ifdef HAVE_COGL_GL
        case GL_TEXTURE_1D:
          target_string = "1D";
          tex_coord_swizzle = "s";
          break;
#endif

        case GL_TEXTURE_2D:
          target_string = "2D";
          tex_coord_swizzle = "st";
          break;

#ifdef GL_ARB_texture_rectangle
        case GL_TEXTURE_RECTANGLE_ARB:
          target_string = "2DRect";
          tex_coord_swizzle = "st";
          break;
#endif

        case GL_TEXTURE_3D:
          target_string = "3D";
          tex_coord_swizzle = "stp";
          break;

        default:
          g_assert_not_reached ();
        }
    }

  /* Create a sampler uniform for this layer if we haven't already */
  if (!shader_state->unit_state[unit_index].sampled)
    {
      g_string_append_printf (shader_state->header,
                              "uniform sampler%s _cogl_sampler_%i;\n",
                              target_string,
                              unit_index);
      shader_state->unit_state[unit_index].sampled = TRUE;
    }

  g_string_append_printf (shader_state->source,
                          "texture%s (_cogl_sampler_%i, ",
                          target_string, unit_index);

  /* If point sprite coord generation is being used then divert to the
     built-in varying var for that instead of the texture
     coordinates. We don't want to do this under GL because in that
     case we will instead use glTexEnv(GL_COORD_REPLACE) to replace
     the texture coords with the point sprite coords. Although GL also
     supports the gl_PointCoord variable, it requires GLSL 1.2 which
     would mean we would have to declare the GLSL version and check
     for it */
  if (ctx->driver == COGL_DRIVER_GLES2 &&
      cogl_pipeline_get_layer_point_sprite_coords_enabled (pipeline,
                                                           layer->index))
    g_string_append_printf (shader_state->source,
                            "gl_PointCoord.%s",
                            tex_coord_swizzle);
  else
    g_string_append_printf (shader_state->source,
                            "cogl_tex_coord_in[%d].%s",
                            unit_index, tex_coord_swizzle);

  g_string_append_printf (shader_state->source, ").%s", swizzle);
}

typedef struct
{
  int unit_index;
  CoglPipelineLayer *layer;
} FindPipelineLayerData;

static gboolean
find_pipeline_layer_cb (CoglPipelineLayer *layer,
                        void *user_data)
{
  FindPipelineLayerData *data = user_data;
  int unit_index;

  unit_index = _cogl_pipeline_layer_get_unit_index (layer);

  if (unit_index == data->unit_index)
    {
      data->layer = layer;
      return FALSE;
    }

  return TRUE;
}

static void
add_arg (CoglPipelineShaderState *shader_state,
         CoglPipeline *pipeline,
         CoglPipelineLayer *layer,
         CoglPipelineCombineSource src,
         CoglPipelineCombineOp operand,
         const char *swizzle)
{
  GString *shader_source = shader_state->source;
  char alpha_swizzle[5] = "aaaa";

  g_string_append_c (shader_source, '(');

  if (operand == COGL_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_COLOR ||
      operand == COGL_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_ALPHA)
    g_string_append_printf (shader_source,
                            "vec4(1.0, 1.0, 1.0, 1.0).%s - ",
                            swizzle);

  /* If the operand is reading from the alpha then replace the swizzle
     with the same number of copies of the alpha */
  if (operand == COGL_PIPELINE_COMBINE_OP_SRC_ALPHA ||
      operand == COGL_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_ALPHA)
    {
      alpha_swizzle[strlen (swizzle)] = '\0';
      swizzle = alpha_swizzle;
    }

  switch (src)
    {
    case COGL_PIPELINE_COMBINE_SOURCE_TEXTURE:
      add_texture_lookup (shader_state,
                          pipeline,
                          layer,
                          swizzle);
      break;

    case COGL_PIPELINE_COMBINE_SOURCE_CONSTANT:
      add_constant_lookup (shader_state,
                           pipeline,
                           layer,
                           swizzle);
      break;

    case COGL_PIPELINE_COMBINE_SOURCE_PREVIOUS:
      if (_cogl_pipeline_layer_get_unit_index (layer) > 0)
        {
          g_string_append_printf (shader_source, "cogl_color_out.%s", swizzle);
          break;
        }
      /* flow through */
    case COGL_PIPELINE_COMBINE_SOURCE_PRIMARY_COLOR:
      g_string_append_printf (shader_source, "cogl_color_in.%s", swizzle);
      break;

    default:
      if (src >= COGL_PIPELINE_COMBINE_SOURCE_TEXTURE0 &&
          src < COGL_PIPELINE_COMBINE_SOURCE_TEXTURE0 + 32)
        {
          FindPipelineLayerData data;

          data.unit_index = src - COGL_PIPELINE_COMBINE_SOURCE_TEXTURE0;
          data.layer = layer;

          _cogl_pipeline_foreach_layer_internal (pipeline,
                                                 find_pipeline_layer_cb,
                                                 &data);

          add_texture_lookup (shader_state,
                              pipeline,
                              data.layer,
                              swizzle);
        }
      break;
    }

  g_string_append_c (shader_source, ')');
}

static void
append_masked_combine (CoglPipeline *pipeline,
                       CoglPipelineLayer *layer,
                       const char *swizzle,
                       CoglPipelineCombineFunc function,
                       CoglPipelineCombineSource *src,
                       CoglPipelineCombineOp *op)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);
  GString *shader_source = shader_state->source;

  g_string_append_printf (shader_state->source,
                          "  cogl_color_out.%s = ", swizzle);

  switch (function)
    {
    case COGL_PIPELINE_COMBINE_FUNC_REPLACE:
      add_arg (shader_state, pipeline, layer,
               src[0], op[0], swizzle);
      break;

    case COGL_PIPELINE_COMBINE_FUNC_MODULATE:
      add_arg (shader_state, pipeline, layer,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " * ");
      add_arg (shader_state, pipeline, layer,
               src[1], op[1], swizzle);
      break;

    case COGL_PIPELINE_COMBINE_FUNC_ADD:
      add_arg (shader_state, pipeline, layer,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " + ");
      add_arg (shader_state, pipeline, layer,
               src[1], op[1], swizzle);
      break;

    case COGL_PIPELINE_COMBINE_FUNC_ADD_SIGNED:
      add_arg (shader_state, pipeline, layer,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " + ");
      add_arg (shader_state, pipeline, layer,
               src[1], op[1], swizzle);
      g_string_append_printf (shader_source,
                              " - vec4(0.5, 0.5, 0.5, 0.5).%s",
                              swizzle);
      break;

    case COGL_PIPELINE_COMBINE_FUNC_SUBTRACT:
      add_arg (shader_state, pipeline, layer,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " - ");
      add_arg (shader_state, pipeline, layer,
               src[1], op[1], swizzle);
      break;

    case COGL_PIPELINE_COMBINE_FUNC_INTERPOLATE:
      add_arg (shader_state, pipeline, layer,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " * ");
      add_arg (shader_state, pipeline, layer,
               src[2], op[2], swizzle);
      g_string_append (shader_source, " + ");
      add_arg (shader_state, pipeline, layer,
               src[1], op[1], swizzle);
      g_string_append_printf (shader_source,
                              " * (vec4(1.0, 1.0, 1.0, 1.0).%s - ",
                              swizzle);
      add_arg (shader_state, pipeline, layer,
               src[2], op[2], swizzle);
      g_string_append_c (shader_source, ')');
      break;

    case COGL_PIPELINE_COMBINE_FUNC_DOT3_RGB:
    case COGL_PIPELINE_COMBINE_FUNC_DOT3_RGBA:
      g_string_append (shader_source, "vec4(4.0 * ((");
      add_arg (shader_state, pipeline, layer,
               src[0], op[0], "r");
      g_string_append (shader_source, " - 0.5) * (");
      add_arg (shader_state, pipeline, layer,
               src[1], op[1], "r");
      g_string_append (shader_source, " - 0.5) + (");
      add_arg (shader_state, pipeline, layer,
               src[0], op[0], "g");
      g_string_append (shader_source, " - 0.5) * (");
      add_arg (shader_state, pipeline, layer,
               src[1], op[1], "g");
      g_string_append (shader_source, " - 0.5) + (");
      add_arg (shader_state, pipeline, layer,
               src[0], op[0], "b");
      g_string_append (shader_source, " - 0.5) * (");
      add_arg (shader_state, pipeline, layer,
               src[1], op[1], "b");
      g_string_append_printf (shader_source, " - 0.5))).%s", swizzle);
      break;
    }

  g_string_append_printf (shader_source, ";\n");
}

static gboolean
_cogl_pipeline_fragend_glsl_add_layer (CoglPipeline *pipeline,
                                        CoglPipelineLayer *layer,
                                        unsigned long layers_difference)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);
  CoglPipelineLayer *combine_authority =
    _cogl_pipeline_layer_get_authority (layer,
                                        COGL_PIPELINE_LAYER_STATE_COMBINE);
  CoglPipelineLayerBigState *big_state = combine_authority->big_state;

  if (!shader_state->source)
    return TRUE;

  if (!_cogl_pipeline_need_texture_combine_separate (combine_authority) ||
      /* GL_DOT3_RGBA Is a bit weird as a GL_COMBINE_RGB function
       * since if you use it, it overrides your ALPHA function...
       */
      big_state->texture_combine_rgb_func ==
      COGL_PIPELINE_COMBINE_FUNC_DOT3_RGBA)
    append_masked_combine (pipeline,
                           layer,
                           "rgba",
                           big_state->texture_combine_rgb_func,
                           big_state->texture_combine_rgb_src,
                           big_state->texture_combine_rgb_op);
  else
    {
      append_masked_combine (pipeline,
                             layer,
                             "rgb",
                             big_state->texture_combine_rgb_func,
                             big_state->texture_combine_rgb_src,
                             big_state->texture_combine_rgb_op);
      append_masked_combine (pipeline,
                             layer,
                             "a",
                             big_state->texture_combine_alpha_func,
                             big_state->texture_combine_alpha_src,
                             big_state->texture_combine_alpha_op);
    }

  return TRUE;
}

gboolean
_cogl_pipeline_fragend_glsl_passthrough (CoglPipeline *pipeline)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);

  if (!shader_state->source)
    return TRUE;

  g_string_append (shader_state->source,
                   "  cogl_color_out = cogl_color_in;\n");

  return TRUE;
}

/* GLES2 doesn't have alpha testing so we need to implement it in the
   shader */

#ifdef HAVE_COGL_GLES2

static void
add_alpha_test_snippet (CoglPipeline *pipeline,
                        CoglPipelineShaderState *shader_state)
{
  CoglPipelineAlphaFunc alpha_func;

  alpha_func = cogl_pipeline_get_alpha_test_function (pipeline);

  if (alpha_func == COGL_PIPELINE_ALPHA_FUNC_ALWAYS)
    /* Do nothing */
    return;

  if (alpha_func == COGL_PIPELINE_ALPHA_FUNC_NEVER)
    {
      /* Always discard the fragment */
      g_string_append (shader_state->source,
                       "  discard;\n");
      return;
    }

  /* For all of the other alpha functions we need a uniform for the
     reference */

  g_string_append (shader_state->header,
                   "uniform float _cogl_alpha_test_ref;\n");

  g_string_append (shader_state->source,
                   "  if (cogl_color_out.a ");

  switch (alpha_func)
    {
    case COGL_PIPELINE_ALPHA_FUNC_LESS:
      g_string_append (shader_state->source, ">=");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_EQUAL:
      g_string_append (shader_state->source, "!=");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_LEQUAL:
      g_string_append (shader_state->source, ">");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_GREATER:
      g_string_append (shader_state->source, "<=");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_NOTEQUAL:
      g_string_append (shader_state->source, "==");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_GEQUAL:
      g_string_append (shader_state->source, "< ");
      break;

    case COGL_PIPELINE_ALPHA_FUNC_ALWAYS:
    case COGL_PIPELINE_ALPHA_FUNC_NEVER:
      g_assert_not_reached ();
      break;
    }

  g_string_append (shader_state->source,
                   " _cogl_alpha_test_ref)\n    discard;\n");
}

#endif /*  HAVE_COGL_GLES2 */

gboolean
_cogl_pipeline_fragend_glsl_end (CoglPipeline *pipeline,
                                 unsigned long pipelines_difference)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (shader_state->source)
    {
      const char *source_strings[2];
      GLint lengths[2];
      GLint compile_status;
      GLuint shader;
      int n_tex_coord_attribs = 0;
      int i, n_layers;

      COGL_STATIC_COUNTER (fragend_glsl_compile_counter,
                           "glsl fragment compile counter",
                           "Increments each time a new GLSL "
                           "fragment shader is compiled",
                           0 /* no application private data */);
      COGL_COUNTER_INC (_cogl_uprof_context, fragend_glsl_compile_counter);

#ifdef HAVE_COGL_GLES2
      if (ctx->driver == COGL_DRIVER_GLES2)
        add_alpha_test_snippet (pipeline, shader_state);
#endif

      g_string_append (shader_state->source, "}\n");

      GE_RET( shader, ctx, glCreateShader (GL_FRAGMENT_SHADER) );

      lengths[0] = shader_state->header->len;
      source_strings[0] = shader_state->header->str;
      lengths[1] = shader_state->source->len;
      source_strings[1] = shader_state->source->str;

      /* Find the highest texture unit that is sampled to pass as the
         number of texture coordinate attributes */
      n_layers = cogl_pipeline_get_n_layers (pipeline);
      for (i = 0; i < n_layers; i++)
        if (shader_state->unit_state[i].sampled)
          n_tex_coord_attribs = i + 1;

      _cogl_shader_set_source_with_boilerplate (shader, GL_FRAGMENT_SHADER,
                                                n_tex_coord_attribs,
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

  return TRUE;
}

static void
_cogl_pipeline_fragend_glsl_pre_change_notify (CoglPipeline *pipeline,
                                               CoglPipelineState change,
                                               const CoglColor *new_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if ((change & _cogl_pipeline_get_state_for_fragment_codegen (ctx)))
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
_cogl_pipeline_fragend_glsl_layer_pre_change_notify (
                                                CoglPipeline *owner,
                                                CoglPipelineLayer *layer,
                                                CoglPipelineLayerState change)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if ((change & _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx)))
    {
      dirty_shader_state (owner);
      return;
    }

  /* TODO: we could be saving snippets of texture combine code along
   * with each layer and then when a layer changes we would just free
   * the snippet. */
}

const CoglPipelineFragend _cogl_pipeline_glsl_fragend =
{
  _cogl_pipeline_fragend_glsl_start,
  _cogl_pipeline_fragend_glsl_add_layer,
  _cogl_pipeline_fragend_glsl_passthrough,
  _cogl_pipeline_fragend_glsl_end,
  _cogl_pipeline_fragend_glsl_pre_change_notify,
  NULL, /* pipeline_set_parent_notify */
  _cogl_pipeline_fragend_glsl_layer_pre_change_notify
};

#endif /* COGL_PIPELINE_FRAGEND_GLSL */

