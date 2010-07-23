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
#include "cogl-shader-private.h"

#ifdef COGL_PIPELINE_BACKEND_GLSL

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-shader-private.h"
#include "cogl-program-private.h"

#ifndef HAVE_COGL_GLES2

#define glCreateProgram     ctx->drv.pf_glCreateProgram
#define glAttachShader      ctx->drv.pf_glAttachShader
#define glUseProgram        ctx->drv.pf_glUseProgram
#define glLinkProgram       ctx->drv.pf_glLinkProgram
#define glDeleteProgram     ctx->drv.pf_glDeleteProgram
#define glGetProgramInfoLog ctx->drv.pf_glGetProgramInfoLog
#define glGetProgramiv      ctx->drv.pf_glGetProgramiv

#endif /* HAVE_COGL_GLES2 */

#include <glib.h>

/*
 * GL/GLES compatability defines for pipeline thingies:
 */

#ifdef HAVE_COGL_GLES2
#include "../gles/cogl-gles2-wrapper.h"
#endif

typedef struct _GlslProgramState
{
  int ref_count;

  /* Age of the user program that was current when the gl_program was
     linked. This is used to detect when we need to relink a new
     program */
  unsigned int user_program_age;
  GLuint gl_program;

#ifdef HAVE_COGL_GLES2
  /* To allow writing shaders that are portable between GLES 2 and
   * OpenGL Cogl prepends a number of boilerplate #defines and
   * declarations to user shaders. One of those declarations is an
   * array of texture coordinate varyings, but to know how to emit the
   * declaration we need to know how many texture coordinate
   * attributes are in use.  The boilerplate also needs to be changed
   * if this increases. */
  int n_tex_coord_attribs;
#endif

  /* This is set to TRUE if the program has changed since we last
     flushed the uniforms */
  gboolean gl_program_changed;

#ifdef HAVE_COGL_GLES2
  /* The GLES2 generated program that was generated from the user
     program. This is used to detect when the GLES2 backend generates
     a different program which would mean we need to flush all of the
     custom uniforms. This is a massive hack but it can go away once
     this GLSL backend starts generating its own shaders */
  GLuint gles2_program;
#endif
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

      g_slice_free (GlslProgramState, state);
    }
}

/* This tries to find the oldest ancestor whos state would generate
 * the same glsl program as the current pipeline. This is a simple
 * mechanism for reducing the number of glsl programs we have to
 * generate.
 */
static CoglPipeline *
find_glsl_authority (CoglPipeline *pipeline, CoglHandle user_program)
{
  /* Find the first pipeline that modifies the user shader */
  return _cogl_pipeline_get_authority (pipeline,
                                       COGL_PIPELINE_STATE_USER_SHADER);
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
  GLuint gl_program;
  GSList *l;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!cogl_features_available (COGL_FEATURE_SHADERS_GLSL))
    return FALSE;

  user_program = cogl_pipeline_get_user_program (pipeline);
  if (user_program == COGL_INVALID_HANDLE ||
      _cogl_program_get_language (user_program) != COGL_SHADER_LANGUAGE_GLSL)
    return FALSE; /* XXX: change me when we support code generation here */

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
      if (priv->glsl_program_state->user_program_age == user_program->age
#ifdef HAVE_COGL_GLES2
          && priv->glsl_program_state->n_tex_coord_attribs >=
             n_tex_coord_attribs
#endif
         )
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
      authority = find_glsl_authority (pipeline, user_program);
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

          /* If the pipeline isn't actually its own glsl-authority
           * then take a reference to the program state associated
           * with the glsl-authority... */
          if (authority != pipeline)
            priv->glsl_program_state =
              glsl_program_state_ref (authority_priv->glsl_program_state);
        }
    }

  /* If we make it here then we have a glsl_program_state struct
     without a gl_program either because this is the first time we've
     encountered it or because the user program has changed since it
     was last linked */

  priv->glsl_program_state->gl_program_changed = TRUE;

  GE_RET( gl_program, glCreateProgram () );

#ifdef HAVE_COGL_GLES2
  /* Find the largest count of texture coordinate attributes
   * associated with each of the shaders so we can ensure a consistent
   * _cogl_tex_coord[] array declaration across all of the shaders.*/
  for (l = user_program->attached_shaders; l; l = l->next)
    {
      CoglShader *shader = l->data;
      n_tex_coord_attribs = MAX (shader->n_tex_coord_attribs,
                                 n_tex_coord_attribs);
    }
#endif

  /* Add all of the shaders from the user program */
  for (l = user_program->attached_shaders; l; l = l->next)
    {
      CoglShader *shader = l->data;

      g_assert (shader->language == COGL_SHADER_LANGUAGE_GLSL);

      _cogl_shader_compile_real (shader, n_tex_coord_attribs);

      GE( glAttachShader (gl_program, shader->gl_handle) );
    }

  priv->glsl_program_state->gl_program = gl_program;
  priv->glsl_program_state->user_program_age = user_program->age;
#ifdef HAVE_COGL_GLES2
  priv->glsl_program_state->n_tex_coord_attribs = n_tex_coord_attribs;
#endif

  link_program (gl_program);

  return TRUE;
}

gboolean
_cogl_pipeline_backend_glsl_add_layer (CoglPipeline *pipeline,
                                       CoglPipelineLayer *layer,
                                       unsigned long layers_difference)
{
  return TRUE;
}

gboolean
_cogl_pipeline_backend_glsl_passthrough (CoglPipeline *pipeline)
{
  return TRUE;
}

gboolean
_cogl_pipeline_backend_glsl_end (CoglPipeline *pipeline,
                                 unsigned long pipelines_difference)
{
  GlslProgramState *glsl_program_state = get_glsl_program_state (pipeline);
  GLuint gl_program;
  gboolean gl_program_changed;

  gl_program = glsl_program_state->gl_program;
  gl_program_changed = glsl_program_state->gl_program_changed;

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

  _cogl_program_flush_uniforms (cogl_pipeline_get_user_program (pipeline),
                                gl_program, gl_program_changed);

  glsl_program_state->gl_program_changed = FALSE;

  return TRUE;
}

static void
_cogl_pipeline_backend_glsl_pre_change_notify (CoglPipeline *pipeline,
                                               CoglPipelineState change,
                                               const CoglColor *new_color)
{
  static const unsigned long glsl_op_changes =
    COGL_PIPELINE_STATE_USER_SHADER;

  if (!(change & glsl_op_changes))
    return;

  dirty_glsl_program_state (pipeline);
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
  NULL, /* layer_pre_change_notify */
  _cogl_pipeline_backend_glsl_free_priv,
  NULL /* free_layer_priv */
};

#endif /* COGL_PIPELINE_BACKEND_GLSL */

