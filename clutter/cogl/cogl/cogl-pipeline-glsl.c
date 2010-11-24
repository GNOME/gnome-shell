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

#include <string.h>

#include "cogl-pipeline-private.h"
#include "cogl-shader-private.h"
#include "cogl-blend-string.h"

#ifdef COGL_PIPELINE_BACKEND_GLSL

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-shader-private.h"
#include "cogl-program-private.h"

#ifndef HAVE_COGL_GLES2

#define glCreateProgram      ctx->drv.pf_glCreateProgram
#define glAttachShader       ctx->drv.pf_glAttachShader
#define glUseProgram         ctx->drv.pf_glUseProgram
#define glLinkProgram        ctx->drv.pf_glLinkProgram
#define glDeleteProgram      ctx->drv.pf_glDeleteProgram
#define glGetProgramInfoLog  ctx->drv.pf_glGetProgramInfoLog
#define glGetProgramiv       ctx->drv.pf_glGetProgramiv
#define glCreateShader       ctx->drv.pf_glCreateShader
#define glGetShaderiv        ctx->drv.pf_glGetShaderiv
#define glGetShaderInfoLog   ctx->drv.pf_glGetShaderInfoLog
#define glCompileShader      ctx->drv.pf_glCompileShader
#define glShaderSource       ctx->drv.pf_glShaderSource
#define glDeleteShader       ctx->drv.pf_glDeleteShader
#define glGetUniformLocation ctx->drv.pf_glGetUniformLocation
#define glUniform1i          ctx->drv.pf_glUniform1i
#define glUniform1f          ctx->drv.pf_glUniform1f
#define glUniform4fv         ctx->drv.pf_glUniform4fv

#endif /* HAVE_COGL_GLES2 */

#include <glib.h>

/*
 * GL/GLES compatability defines for pipeline thingies:
 */

#ifdef HAVE_COGL_GLES2
#include "../gles/cogl-gles2-wrapper.h"
#endif

/* This might not be defined on GLES */
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D                           0x806F
#endif

typedef struct _UnitState
{
  unsigned int sampled:1;
  unsigned int combine_constant_used:1;
  unsigned int dirty_combine_constant:1;

  GLint combine_constant_uniform;
} UnitState;

typedef struct _GlslProgramState
{
  int ref_count;

  /* Age of the user program that was current when the gl_program was
     linked. This is used to detect when we need to relink a new
     program */
  unsigned int user_program_age;
  GLuint gl_program;
  GString *header, *source;
  UnitState *unit_state;

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
  gboolean alpha_test_reference_used;
  gboolean dirty_alpha_test_reference;
  GLint alpha_test_reference_uniform;
#endif

  /* We need to track the last pipeline that the program was used with
   * so know if we need to update all of the uniforms */
  CoglPipeline *last_used_for_pipeline;
} GlslProgramState;

typedef struct _CoglPipelineBackendGlslPrivate
{
  GlslProgramState *glsl_program_state;
} CoglPipelineBackendGlslPrivate;

const CoglPipelineBackend _cogl_pipeline_glsl_backend;

static int
_cogl_pipeline_backend_glsl_get_max_texture_units (void)
{
  return _cogl_get_max_texture_image_units ();
}

static GlslProgramState *
glsl_program_state_new (int n_layers)
{
  GlslProgramState *state = g_slice_new0 (GlslProgramState);

  state->ref_count = 1;
  state->unit_state = g_new0 (UnitState, n_layers);

  return state;
}

static GlslProgramState *
glsl_program_state_ref (GlslProgramState *state)
{
  state->ref_count++;
  return state;
}

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

void
glsl_program_state_unref (GlslProgramState *state)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (state->ref_count > 0);

  state->ref_count--;
  if (state->ref_count == 0)
    {
      if (state->gl_program)
        {
          delete_program (state->gl_program);
          state->gl_program = 0;
        }

      g_free (state->unit_state);

      g_slice_free (GlslProgramState, state);
    }
}

static CoglPipelineBackendGlslPrivate *
get_glsl_priv (CoglPipeline *pipeline)
{
  if (!(pipeline->backend_priv_set_mask & COGL_PIPELINE_BACKEND_GLSL_MASK))
    return NULL;

  return pipeline->backend_privs[COGL_PIPELINE_BACKEND_GLSL];
}

static void
set_glsl_priv (CoglPipeline *pipeline, CoglPipelineBackendGlslPrivate *priv)
{
  if (priv)
    {
      pipeline->backend_privs[COGL_PIPELINE_BACKEND_GLSL] = priv;
      pipeline->backend_priv_set_mask |= COGL_PIPELINE_BACKEND_GLSL_MASK;
    }
  else
    pipeline->backend_priv_set_mask &= ~COGL_PIPELINE_BACKEND_GLSL_MASK;
}

static GlslProgramState *
get_glsl_program_state (CoglPipeline *pipeline)
{
  CoglPipelineBackendGlslPrivate *priv = get_glsl_priv (pipeline);
  if (!priv)
    return NULL;
  return priv->glsl_program_state;
}

static void
dirty_glsl_program_state (CoglPipeline *pipeline)
{
  CoglPipelineBackendGlslPrivate *priv;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  priv = get_glsl_priv (pipeline);
  if (!priv)
    return;

  if (priv->glsl_program_state)
    {
      glsl_program_state_unref (priv->glsl_program_state);
      priv->glsl_program_state = NULL;
    }
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

static gboolean
_cogl_pipeline_backend_glsl_start (CoglPipeline *pipeline,
                                   int n_layers,
                                   unsigned long pipelines_difference,
                                   int n_tex_coord_attribs)
{
  CoglPipelineBackendGlslPrivate *priv;
  CoglPipeline *authority;
  CoglPipelineBackendGlslPrivate *authority_priv;
  CoglProgram *user_program;
  GSList *l;
  int i;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!cogl_features_available (COGL_FEATURE_SHADERS_GLSL))
    return FALSE;

  user_program = cogl_pipeline_get_user_program (pipeline);
  if (user_program &&
      _cogl_program_get_language (user_program) != COGL_SHADER_LANGUAGE_GLSL)
    return FALSE;

  /* Now lookup our glsl backend private state (allocating if
   * necessary) */
  priv = get_glsl_priv (pipeline);
  if (!priv)
    {
      priv = g_slice_new0 (CoglPipelineBackendGlslPrivate);
      set_glsl_priv (pipeline, priv);
    }

  /* If we already have a valid GLSL program then we don't need to
     relink a new one */
  if (priv->glsl_program_state)
    {
      /* However if the program has changed since the last link then we do
       * need to relink
       *
       * Also if the number of texture coordinate attributes in use has
       * increased, then delete the program so we can prepend a new
       * _cogl_tex_coord[] varying array declaration. */
      if (user_program == NULL ||
          (priv->glsl_program_state->user_program_age == user_program->age
#ifdef HAVE_COGL_GLES2
           && (priv->glsl_program_state->n_tex_coord_attribs >=
               n_tex_coord_attribs)
#endif
           ))
        return TRUE;

      /* Destroy the existing program. We can't just dirty the whole
         glsl state because otherwise if we are not the authority on
         the user program then we'll just find the same state again */
      delete_program (priv->glsl_program_state->gl_program);
      priv->glsl_program_state->gl_program = 0;
    }
  else
    {
      /* If we don't have an associated glsl program yet then find the
       * glsl-authority (the oldest ancestor whose state will result in
       * the same program being generated as for this pipeline).
       *
       * We always make sure to associate new programs with the
       * glsl-authority to maximize the chance that other pipelines can
       * share it.
       */
      authority =
        _cogl_pipeline_find_codegen_authority (pipeline, user_program);
      authority_priv = get_glsl_priv (authority);
      if (!authority_priv)
        {
          authority_priv = g_slice_new0 (CoglPipelineBackendGlslPrivate);
          set_glsl_priv (authority, authority_priv);
        }

      /* If we don't have an existing program associated with the
       * glsl-authority then start generating code for a new program...
       */
      if (!authority_priv->glsl_program_state)
        {
          GlslProgramState *glsl_program_state =
            glsl_program_state_new (n_layers);
          authority_priv->glsl_program_state = glsl_program_state;
        }

      /* If the pipeline isn't actually its own glsl-authority
       * then take a reference to the program state associated
       * with the glsl-authority... */
      if (authority != pipeline)
        priv->glsl_program_state =
          glsl_program_state_ref (authority_priv->glsl_program_state);
    }

  /* If we make it here then we have a glsl_program_state struct
     without a gl_program either because this is the first time we've
     encountered it or because the user program has changed since it
     was last linked */

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

  priv->glsl_program_state->n_tex_coord_attribs = n_tex_coord_attribs;

  /* Check whether the user program contains a fragment
     shader. Otherwise we need to generate one */
  if (user_program)
    for (l = user_program->attached_shaders; l; l = l->next)
      {
        CoglShader *shader = l->data;

        if (shader->type == COGL_SHADER_TYPE_FRAGMENT)
          goto no_fragment_shader_needed;
      }

  /* We reuse two grow-only GStrings for code-gen. One string
     contains the uniform and attribute declarations while the
     other contains the main function. We need two strings
     because we need to dynamically declare attributes as the
     add_layer callback is invoked */
  g_string_set_size (ctx->fragment_header_buffer, 0);
  g_string_set_size (ctx->fragment_source_buffer, 0);
  priv->glsl_program_state->header = ctx->fragment_header_buffer;
  priv->glsl_program_state->source = ctx->fragment_source_buffer;

  g_string_append (priv->glsl_program_state->source,
                   "void\n"
                   "main ()\n"
                   "{\n");

#ifdef HAVE_COGL_GLES2
  priv->glsl_program_state->alpha_test_reference_uniform = -1;
  priv->glsl_program_state->alpha_test_reference_used = FALSE;
  priv->glsl_program_state->dirty_alpha_test_reference = FALSE;
#endif

  for (i = 0; i < n_layers; i++)
    {
      priv->glsl_program_state->unit_state[i].sampled = FALSE;
      priv->glsl_program_state->unit_state[i].combine_constant_used = FALSE;
      priv->glsl_program_state->unit_state[i].dirty_combine_constant = FALSE;
    }

 no_fragment_shader_needed:

  return TRUE;
}

static void
add_constant_lookup (GlslProgramState *glsl_program_state,
                     CoglPipeline *pipeline,
                     CoglPipelineLayer *layer,
                     const char *swizzle)
{
  int unit_index = _cogl_pipeline_layer_get_unit_index (layer);

  /* Create a sampler uniform for this layer if we haven't already */
  if (!glsl_program_state->unit_state[unit_index].combine_constant_used)
    {
      g_string_append_printf (glsl_program_state->header,
                              "uniform vec4 _cogl_layer_constant_%i;\n",
                              unit_index);
      glsl_program_state->unit_state[unit_index].combine_constant_used = TRUE;
      glsl_program_state->unit_state[unit_index].dirty_combine_constant = TRUE;
    }

  g_string_append_printf (glsl_program_state->source,
                          "_cogl_layer_constant_%i.%s",
                          unit_index, swizzle);
}

static void
add_texture_lookup (GlslProgramState *glsl_program_state,
                    CoglPipeline *pipeline,
                    CoglPipelineLayer *layer,
                    const char *swizzle)
{
  CoglHandle texture;
  int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
  const char *target_string, *tex_coord_swizzle;

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
#ifndef HAVE_COGL_GLES2
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
  if (!glsl_program_state->unit_state[unit_index].sampled)
    {
      g_string_append_printf (glsl_program_state->header,
                              "uniform sampler%s _cogl_sampler_%i;\n",
                              target_string,
                              unit_index);
      glsl_program_state->unit_state[unit_index].sampled = TRUE;
    }

  g_string_append_printf (glsl_program_state->source,
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
#ifdef HAVE_COGL_GLES2
  if (cogl_pipeline_get_layer_point_sprite_coords_enabled (pipeline,
                                                           layer->index))
    g_string_append_printf (glsl_program_state->source,
                            "gl_PointCoord.%s",
                            tex_coord_swizzle);
  else
#endif
    g_string_append_printf (glsl_program_state->source,
                            "cogl_tex_coord_in[%d].%s",
                            unit_index, tex_coord_swizzle);

  g_string_append_printf (glsl_program_state->source, ").%s", swizzle);
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
add_arg (GlslProgramState *glsl_program_state,
         CoglPipeline *pipeline,
         CoglPipelineLayer *layer,
         GLint src,
         GLenum operand,
         const char *swizzle)
{
  GString *shader_source = glsl_program_state->source;
  char alpha_swizzle[5] = "aaaa";

  g_string_append_c (shader_source, '(');

  if (operand == GL_ONE_MINUS_SRC_COLOR || operand == GL_ONE_MINUS_SRC_ALPHA)
    g_string_append_printf (shader_source,
                            "vec4(1.0, 1.0, 1.0, 1.0).%s - ",
                            swizzle);

  /* If the operand is reading from the alpha then replace the swizzle
     with the same number of copies of the alpha */
  if (operand == GL_SRC_ALPHA || operand == GL_ONE_MINUS_SRC_ALPHA)
    {
      alpha_swizzle[strlen (swizzle)] = '\0';
      swizzle = alpha_swizzle;
    }

  switch (src)
    {
    case GL_TEXTURE:
      add_texture_lookup (glsl_program_state,
                          pipeline,
                          layer,
                          swizzle);
      break;

    case GL_CONSTANT:
      add_constant_lookup (glsl_program_state,
                           pipeline,
                           layer,
                           swizzle);
      break;

    case GL_PREVIOUS:
      if (_cogl_pipeline_layer_get_unit_index (layer) > 0)
        {
          g_string_append_printf (shader_source, "cogl_color_out.%s", swizzle);
          break;
        }
      /* flow through */
    case GL_PRIMARY_COLOR:
      g_string_append_printf (shader_source, "cogl_color_in.%s", swizzle);
      break;

    default:
      if (src >= GL_TEXTURE0 && src < GL_TEXTURE0 + 32)
        {
          FindPipelineLayerData data;

          data.unit_index = src - GL_TEXTURE0;
          data.layer = layer;

          _cogl_pipeline_foreach_layer_internal (pipeline,
                                                 find_pipeline_layer_cb,
                                                 &data);

          add_texture_lookup (glsl_program_state,
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
                       GLint function,
                       GLint *src,
                       GLint *op)
{
  GlslProgramState *glsl_program_state = get_glsl_program_state (pipeline);
  GString *shader_source = glsl_program_state->source;

  g_string_append_printf (glsl_program_state->source,
                          "  cogl_color_out.%s = ", swizzle);

  switch (function)
    {
    case GL_REPLACE:
      add_arg (glsl_program_state, pipeline, layer,
               src[0], op[0], swizzle);
      break;

    case GL_MODULATE:
      add_arg (glsl_program_state, pipeline, layer,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " * ");
      add_arg (glsl_program_state, pipeline, layer,
               src[1], op[1], swizzle);
      break;

    case GL_ADD:
      add_arg (glsl_program_state, pipeline, layer,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " + ");
      add_arg (glsl_program_state, pipeline, layer,
               src[1], op[1], swizzle);
      break;

    case GL_ADD_SIGNED:
      add_arg (glsl_program_state, pipeline, layer,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " + ");
      add_arg (glsl_program_state, pipeline, layer,
               src[1], op[1], swizzle);
      g_string_append_printf (shader_source,
                              " - vec4(0.5, 0.5, 0.5, 0.5).%s",
                              swizzle);
      break;

    case GL_SUBTRACT:
      add_arg (glsl_program_state, pipeline, layer,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " - ");
      add_arg (glsl_program_state, pipeline, layer,
               src[1], op[1], swizzle);
      break;

    case GL_INTERPOLATE:
      add_arg (glsl_program_state, pipeline, layer,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " * ");
      add_arg (glsl_program_state, pipeline, layer,
               src[2], op[2], swizzle);
      g_string_append (shader_source, " + ");
      add_arg (glsl_program_state, pipeline, layer,
               src[1], op[1], swizzle);
      g_string_append_printf (shader_source,
                              " * (vec4(1.0, 1.0, 1.0, 1.0).%s - ",
                              swizzle);
      add_arg (glsl_program_state, pipeline, layer,
               src[2], op[2], swizzle);
      g_string_append_c (shader_source, ')');
      break;

    case GL_DOT3_RGB:
    case GL_DOT3_RGBA:
      g_string_append (shader_source, "vec4(4 * ((");
      add_arg (glsl_program_state, pipeline, layer,
               src[0], op[0], "r");
      g_string_append (shader_source, " - 0.5) * (");
      add_arg (glsl_program_state, pipeline, layer,
               src[1], op[1], "r");
      g_string_append (shader_source, " - 0.5) + (");
      add_arg (glsl_program_state, pipeline, layer,
               src[0], op[0], "g");
      g_string_append (shader_source, " - 0.5) * (");
      add_arg (glsl_program_state, pipeline, layer,
               src[1], op[1], "g");
      g_string_append (shader_source, " - 0.5) + (");
      add_arg (glsl_program_state, pipeline, layer,
               src[0], op[0], "b");
      g_string_append (shader_source, " - 0.5) * (");
      add_arg (glsl_program_state, pipeline, layer,
               src[1], op[1], "b");
      g_string_append_printf (shader_source, " - 0.5))).%s", swizzle);
      break;
    }

  g_string_append_printf (shader_source, ";\n");
}

static gboolean
_cogl_pipeline_backend_glsl_add_layer (CoglPipeline *pipeline,
                                        CoglPipelineLayer *layer,
                                        unsigned long layers_difference)
{
  GlslProgramState *glsl_program_state = get_glsl_program_state (pipeline);
  CoglPipelineLayer *combine_authority =
    _cogl_pipeline_layer_get_authority (layer,
                                        COGL_PIPELINE_LAYER_STATE_COMBINE);
  CoglPipelineLayerBigState *big_state = combine_authority->big_state;

  if (!glsl_program_state->source)
    return TRUE;

  if (!_cogl_pipeline_need_texture_combine_separate (combine_authority) ||
      /* GL_DOT3_RGBA Is a bit weird as a GL_COMBINE_RGB function
       * since if you use it, it overrides your ALPHA function...
       */
      big_state->texture_combine_rgb_func == GL_DOT3_RGBA)
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
_cogl_pipeline_backend_glsl_passthrough (CoglPipeline *pipeline)
{
  GlslProgramState *glsl_program_state = get_glsl_program_state (pipeline);

  if (!glsl_program_state->source)
    return TRUE;

  g_string_append (glsl_program_state->source,
                   "  cogl_color_out = cogl_color_in;\n");

  return TRUE;
}

typedef struct
{
  int unit;
  GLuint gl_program;
  gboolean update_all;
  GlslProgramState *glsl_program_state;
} UpdateUniformsState;

static gboolean
get_uniform_cb (CoglPipeline *pipeline,
                int layer_index,
                void *user_data)
{
  UpdateUniformsState *state = user_data;
  GlslProgramState *glsl_program_state = state->glsl_program_state;
  UnitState *unit_state = &glsl_program_state->unit_state[state->unit];
  GLint uniform_location;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (unit_state->sampled)
    {
      /* We can reuse the source buffer to create the uniform name because
         the program has now been linked */
      g_string_set_size (ctx->fragment_source_buffer, 0);
      g_string_append_printf (ctx->fragment_source_buffer,
                              "_cogl_sampler_%i", state->unit);

      GE_RET( uniform_location,
              glGetUniformLocation (state->gl_program,
                                    ctx->fragment_source_buffer->str) );

      g_return_val_if_fail (uniform_location != -1, TRUE);

      /* We can set the uniform immediately because the samplers are
         the unit index not the texture object number so it will never
         change. Unfortunately GL won't let us use a constant instead
         of a uniform */
      GE( glUniform1i (uniform_location, state->unit) );
    }

  if (unit_state->combine_constant_used)
    {
      g_string_set_size (ctx->fragment_source_buffer, 0);
      g_string_append_printf (ctx->fragment_source_buffer,
                              "_cogl_layer_constant_%i", state->unit);

      GE_RET( uniform_location,
              glGetUniformLocation (state->gl_program,
                                    ctx->fragment_source_buffer->str) );

      g_return_val_if_fail (uniform_location != -1, TRUE);

      unit_state->combine_constant_uniform = uniform_location;
    }

  state->unit++;

  return TRUE;
}

static gboolean
update_constants_cb (CoglPipeline *pipeline,
                     int layer_index,
                     void *user_data)
{
  UpdateUniformsState *state = user_data;
  GlslProgramState *glsl_program_state = state->glsl_program_state;
  UnitState *unit_state = &glsl_program_state->unit_state[state->unit++];

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (unit_state->combine_constant_used &&
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

/* GLES2 doesn't have alpha testing so we need to implement it in the
   shader */

#ifdef HAVE_COGL_GLES2

static void
add_alpha_test_snippet (CoglPipeline *pipeline,
                        GlslProgramState *glsl_program_state)
{
  CoglPipelineAlphaFunc alpha_func;

  alpha_func = cogl_pipeline_get_alpha_test_function (pipeline);

  if (alpha_func == COGL_PIPELINE_ALPHA_FUNC_ALWAYS)
    /* Do nothing */
    return;

  if (alpha_func == COGL_PIPELINE_ALPHA_FUNC_NEVER)
    {
      /* Always discard the fragment */
      g_string_append (glsl_program_state->source,
                       "  discard;\n");
      return;
    }

  /* For all of the other alpha functions we need a uniform for the
     reference */

  glsl_program_state->alpha_test_reference_used = TRUE;
  glsl_program_state->dirty_alpha_test_reference = TRUE;

  g_string_append (glsl_program_state->header,
                   "uniform float _cogl_alpha_test_ref;\n");

  g_string_append (glsl_program_state->source,
                   "  if (cogl_color_out.a ");

  switch (alpha_func)
    {
    case COGL_PIPELINE_ALPHA_FUNC_LESS:
      g_string_append (glsl_program_state->source, ">=");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_EQUAL:
      g_string_append (glsl_program_state->source, "!=");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_LEQUAL:
      g_string_append (glsl_program_state->source, ">");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_GREATER:
      g_string_append (glsl_program_state->source, "<=");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_NOTEQUAL:
      g_string_append (glsl_program_state->source, "==");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_GEQUAL:
      g_string_append (glsl_program_state->source, "< ");
      break;

    case COGL_PIPELINE_ALPHA_FUNC_ALWAYS:
    case COGL_PIPELINE_ALPHA_FUNC_NEVER:
      g_assert_not_reached ();
      break;
    }

  g_string_append (glsl_program_state->source,
                   " _cogl_alpha_test_ref)\n    discard;\n");
}

static void
update_alpha_test_reference (CoglPipeline *pipeline,
                             GLuint gl_program,
                             GlslProgramState *glsl_program_state)
{
  float alpha_reference;

  if (glsl_program_state->dirty_alpha_test_reference)
    {
      if (glsl_program_state->alpha_test_reference_uniform == -1)
        {
          GE_RET( glsl_program_state->alpha_test_reference_uniform,
                  glGetUniformLocation (gl_program,
                                        "_cogl_alpha_test_ref") );
          g_return_if_fail (glsl_program_state->
                            alpha_test_reference_uniform != -1);
        }

      alpha_reference = cogl_pipeline_get_alpha_test_reference (pipeline);

      GE( glUniform1f (glsl_program_state->alpha_test_reference_uniform,
                       alpha_reference) );

      glsl_program_state->dirty_alpha_test_reference = FALSE;
    }
}

#endif /*  HAVE_COGL_GLES2 */

gboolean
_cogl_pipeline_backend_glsl_end (CoglPipeline *pipeline,
                                 unsigned long pipelines_difference)
{
  GlslProgramState *glsl_program_state = get_glsl_program_state (pipeline);
  CoglProgram *user_program;
  GLuint gl_program;
  gboolean gl_program_changed = FALSE;
  UpdateUniformsState state;

  _COGL_GET_CONTEXT (ctx, FALSE);

  gl_program = glsl_program_state->gl_program;
  user_program = cogl_pipeline_get_user_program (pipeline);

  if (gl_program == 0)
    {
      gl_program_changed = TRUE;

      GE_RET( gl_program, glCreateProgram () );

      if (user_program)
        {
          GSList *l;

          /* Add all of the shaders from the user program */
          for (l = user_program->attached_shaders; l; l = l->next)
            {
              CoglShader *shader = l->data;

              g_assert (shader->language == COGL_SHADER_LANGUAGE_GLSL);

              _cogl_shader_compile_real (shader,
                                         glsl_program_state->
                                         n_tex_coord_attribs);

              GE( glAttachShader (gl_program, shader->gl_handle) );
            }

          glsl_program_state->user_program_age = user_program->age;
        }

      if (glsl_program_state->source)
        {
          const GLchar *source_strings[2];
          GLint lengths[2];
          GLint compile_status;
          GLuint shader;

          COGL_STATIC_COUNTER (backend_glsl_compile_counter,
                               "glsl compile counter",
                               "Increments each time a new GLSL "
                               "program is compiled",
                               0 /* no application private data */);
          COGL_COUNTER_INC (_cogl_uprof_context, backend_glsl_compile_counter);

#ifdef HAVE_COGL_GLES2
          add_alpha_test_snippet (pipeline, glsl_program_state);
#endif

          g_string_append (glsl_program_state->source, "}\n");

          if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_SHOW_SOURCE))
            g_message ("pipeline program:\n%s%s",
                       glsl_program_state->header->str,
                       glsl_program_state->source->str);

          GE_RET( shader, glCreateShader (GL_FRAGMENT_SHADER) );

          lengths[0] = glsl_program_state->header->len;
          source_strings[0] = glsl_program_state->header->str;
          lengths[1] = glsl_program_state->source->len;
          source_strings[1] = glsl_program_state->source->str;

          _cogl_shader_set_source_with_boilerplate (shader, GL_FRAGMENT_SHADER,
                                                    glsl_program_state->
                                                    n_tex_coord_attribs,
                                                    2, /* count */
                                                    source_strings, lengths);

          GE( glCompileShader (shader) );
          GE( glGetShaderiv (shader, GL_COMPILE_STATUS, &compile_status) );

          if (!compile_status)
            {
              GLint len = 0;
              char *shader_log;

              GE( glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &len) );
              shader_log = g_alloca (len);
              GE( glGetShaderInfoLog (shader, len, &len, shader_log) );
              g_warning ("Shader compilation failed:\n%s", shader_log);
            }

          GE( glAttachShader (gl_program, shader) );

          /* We can delete the shader now, but it won't actually be
             destroyed until the program is also desroyed */
          GE( glDeleteShader (shader) );

          glsl_program_state->header = NULL;
          glsl_program_state->source = NULL;
        }

      link_program (gl_program);

      glsl_program_state->gl_program = gl_program;
    }

#ifdef HAVE_COGL_GLES2
  /* This function is a massive hack to get the GLES2 backend to
     work. It should only be neccessary until we move the GLSL
     generation into this file instead of the GLES2 driver backend */
  gl_program = _cogl_gles2_use_program (gl_program);
  /* We need to detect when the GLES2 backend gives us a different
     program from last time */
  if (gl_program != glsl_program_state->gles2_program)
    {
      glsl_program_state->gles2_program = gl_program;
      gl_program_changed = TRUE;
    }
#else
  _cogl_use_program (gl_program, COGL_PIPELINE_PROGRAM_TYPE_GLSL);
#endif

  state.unit = 0;
  state.gl_program = gl_program;
  state.glsl_program_state = glsl_program_state;

  if (gl_program_changed)
    cogl_pipeline_foreach_layer (pipeline,
                                 get_uniform_cb,
                                 &state);

  state.unit = 0;
  state.update_all = (gl_program_changed ||
                      glsl_program_state->last_used_for_pipeline != pipeline);

  cogl_pipeline_foreach_layer (pipeline,
                               update_constants_cb,
                               &state);

#ifdef HAVE_COGL_GLES2
  if (glsl_program_state->alpha_test_reference_used)
    {
      if (gl_program_changed)
        glsl_program_state->alpha_test_reference_uniform = -1;
      if (gl_program_changed ||
          glsl_program_state->last_used_for_pipeline != pipeline)
        glsl_program_state->dirty_alpha_test_reference = TRUE;

      update_alpha_test_reference (pipeline, gl_program, glsl_program_state);
    }
#endif

  if (user_program)
    _cogl_program_flush_uniforms (user_program,
                                  gl_program,
                                  gl_program_changed);

  /* We need to track the last pipeline that the program was used with
   * so know if we need to update all of the uniforms */
  glsl_program_state->last_used_for_pipeline = pipeline;

  return TRUE;
}

static void
_cogl_pipeline_backend_glsl_pre_change_notify (CoglPipeline *pipeline,
                                               CoglPipelineState change,
                                               const CoglColor *new_color)
{
  static const unsigned long fragment_op_changes =
    COGL_PIPELINE_STATE_LAYERS |
#ifdef COGL_HAS_GLES2
    COGL_PIPELINE_STATE_ALPHA_FUNC |
#endif
    COGL_PIPELINE_STATE_USER_SHADER;
    /* TODO: COGL_PIPELINE_STATE_FOG */

  if ((change & fragment_op_changes))
    dirty_glsl_program_state (pipeline);
#ifdef COGL_HAS_GLES2
  else if ((change & COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE))
    {
      GlslProgramState *glsl_program_state =
        get_glsl_program_state (pipeline);
      glsl_program_state->dirty_alpha_test_reference = TRUE;
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
_cogl_pipeline_backend_glsl_layer_pre_change_notify (
                                                CoglPipeline *owner,
                                                CoglPipelineLayer *layer,
                                                CoglPipelineLayerState change)
{
  CoglPipelineBackendGlslPrivate *priv;
  static const unsigned long not_fragment_op_changes =
    COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT |
    COGL_PIPELINE_LAYER_STATE_TEXTURE;

  priv = get_glsl_priv (owner);
  if (!priv)
    return;

  if (!(change & not_fragment_op_changes))
    {
      dirty_glsl_program_state (owner);
      return;
    }

  if (change & COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT)
    {
      GlslProgramState *glsl_program_state =
        get_glsl_program_state (owner);
      int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
      glsl_program_state->unit_state[unit_index].dirty_combine_constant = TRUE;
    }

  /* TODO: we could be saving snippets of texture combine code along
   * with each layer and then when a layer changes we would just free
   * the snippet. */
  return;
}

static void
_cogl_pipeline_backend_glsl_free_priv (CoglPipeline *pipeline)
{
  CoglPipelineBackendGlslPrivate *priv = get_glsl_priv (pipeline);
  if (priv)
    {
      if (priv->glsl_program_state)
        glsl_program_state_unref (priv->glsl_program_state);
      g_slice_free (CoglPipelineBackendGlslPrivate, priv);
      set_glsl_priv (pipeline, NULL);
    }
}

const CoglPipelineBackend _cogl_pipeline_glsl_backend =
{
  _cogl_pipeline_backend_glsl_get_max_texture_units,
  _cogl_pipeline_backend_glsl_start,
  _cogl_pipeline_backend_glsl_add_layer,
  _cogl_pipeline_backend_glsl_passthrough,
  _cogl_pipeline_backend_glsl_end,
  _cogl_pipeline_backend_glsl_pre_change_notify,
  NULL, /* pipeline_set_parent_notify */
  _cogl_pipeline_backend_glsl_layer_pre_change_notify,
  _cogl_pipeline_backend_glsl_free_priv,
  NULL /* free_layer_priv */
};

#endif /* COGL_PIPELINE_BACKEND_GLSL */

