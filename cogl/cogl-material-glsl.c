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

#include "cogl-material-private.h"
#include "cogl-shader-private.h"

#ifdef COGL_MATERIAL_BACKEND_GLSL

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
 * GL/GLES compatability defines for material thingies:
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

typedef struct _CoglMaterialBackendGlslPrivate
{
  GlslProgramState *glsl_program_state;
} CoglMaterialBackendGlslPrivate;

const CoglMaterialBackend _cogl_material_glsl_backend;

static int
_cogl_material_backend_glsl_get_max_texture_units (void)
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
 * the same glsl program as the current material. This is a simple
 * mechanism for reducing the number of glsl programs we have to
 * generate.
 */
static CoglMaterial *
find_glsl_authority (CoglMaterial *material, CoglHandle user_program)
{
  /* Find the first material that modifies the user shader */
  return _cogl_material_get_authority (material,
                                       COGL_MATERIAL_STATE_USER_SHADER);
}

static CoglMaterialBackendGlslPrivate *
get_glsl_priv (CoglMaterial *material)
{
  if (!(material->backend_priv_set_mask & COGL_MATERIAL_BACKEND_GLSL_MASK))
    return NULL;

  return material->backend_privs[COGL_MATERIAL_BACKEND_GLSL];
}

static void
set_glsl_priv (CoglMaterial *material, CoglMaterialBackendGlslPrivate *priv)
{
  if (priv)
    {
      material->backend_privs[COGL_MATERIAL_BACKEND_GLSL] = priv;
      material->backend_priv_set_mask |= COGL_MATERIAL_BACKEND_GLSL_MASK;
    }
  else
    material->backend_priv_set_mask &= ~COGL_MATERIAL_BACKEND_GLSL_MASK;
}

static GlslProgramState *
get_glsl_program_state (CoglMaterial *material)
{
  CoglMaterialBackendGlslPrivate *priv = get_glsl_priv (material);
  if (!priv)
    return NULL;
  return priv->glsl_program_state;
}

static void
dirty_glsl_program_state (CoglMaterial *material)
{
  CoglMaterialBackendGlslPrivate *priv;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  priv = get_glsl_priv (material);
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
_cogl_material_backend_glsl_start (CoglMaterial *material,
                                   int n_layers,
                                   unsigned long materials_difference)
{
  CoglMaterialBackendGlslPrivate *priv;
  CoglMaterial *authority;
  CoglMaterialBackendGlslPrivate *authority_priv;
  CoglProgram *user_program;
  GLuint gl_program;
  GSList *l;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!cogl_features_available (COGL_FEATURE_SHADERS_GLSL))
    return FALSE;

  user_program = cogl_material_get_user_program (material);
  if (user_program == COGL_INVALID_HANDLE ||
      _cogl_program_get_language (user_program) != COGL_SHADER_LANGUAGE_GLSL)
    return FALSE; /* XXX: change me when we support code generation here */

  /* Now lookup our glsl backend private state (allocating if
   * necessary) */
  priv = get_glsl_priv (material);
  if (!priv)
    {
      priv = g_slice_new0 (CoglMaterialBackendGlslPrivate);
      set_glsl_priv (material, priv);
    }

  /* If we already have a valid GLSL program then we don't need to
     relink a new one */
  if (priv->glsl_program_state)
    {
      /* However if the program has changed since the last link then we do
         need to relink */
      if (priv->glsl_program_state->user_program_age == user_program->age)
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
       * the same program being generated as for this material).
       *
       * We always make sure to associate new programs with the
       * glsl-authority to maximize the chance that other materials can
       * share it.
       */
      authority = find_glsl_authority (material, user_program);
      authority_priv = get_glsl_priv (authority);
      if (!authority_priv)
        {
          authority_priv = g_slice_new0 (CoglMaterialBackendGlslPrivate);
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

          /* If the material isn't actually its own glsl-authority
           * then take a reference to the program state associated
           * with the glsl-authority... */
          if (authority != material)
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

  /* Add all of the shaders from the user program */
  for (l = user_program->attached_shaders; l; l = l->next)
    {
      CoglShader *shader = l->data;

      g_assert (shader->language == COGL_SHADER_LANGUAGE_GLSL);

      GE( glAttachShader (gl_program, shader->gl_handle) );
    }

  priv->glsl_program_state->gl_program = gl_program;
  priv->glsl_program_state->user_program_age = user_program->age;

  link_program (gl_program);

  return TRUE;
}

gboolean
_cogl_material_backend_glsl_add_layer (CoglMaterial *material,
                                       CoglMaterialLayer *layer,
                                       unsigned long layers_difference)
{
  return TRUE;
}

gboolean
_cogl_material_backend_glsl_passthrough (CoglMaterial *material)
{
  return TRUE;
}

gboolean
_cogl_material_backend_glsl_end (CoglMaterial *material,
                                 unsigned long materials_difference)
{
  GlslProgramState *glsl_program_state = get_glsl_program_state (material);
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
  _cogl_use_program (gl_program, COGL_MATERIAL_PROGRAM_TYPE_GLSL);
#endif

  _cogl_program_flush_uniforms (cogl_material_get_user_program (material),
                                gl_program, gl_program_changed);

  glsl_program_state->gl_program_changed = FALSE;

  return TRUE;
}

static void
_cogl_material_backend_glsl_pre_change_notify (CoglMaterial *material,
                                               CoglMaterialState change,
                                               const CoglColor *new_color)
{
  static const unsigned long glsl_op_changes =
    COGL_MATERIAL_STATE_USER_SHADER;

  if (!(change & glsl_op_changes))
    return;

  dirty_glsl_program_state (material);
}

static void
_cogl_material_backend_glsl_free_priv (CoglMaterial *material)
{
  CoglMaterialBackendGlslPrivate *priv = get_glsl_priv (material);
  if (priv)
    {
      if (priv->glsl_program_state)
        glsl_program_state_unref (priv->glsl_program_state);
      g_slice_free (CoglMaterialBackendGlslPrivate, priv);
      set_glsl_priv (material, NULL);
    }
}

const CoglMaterialBackend _cogl_material_glsl_backend =
{
  _cogl_material_backend_glsl_get_max_texture_units,
  _cogl_material_backend_glsl_start,
  _cogl_material_backend_glsl_add_layer,
  _cogl_material_backend_glsl_passthrough,
  _cogl_material_backend_glsl_end,
  _cogl_material_backend_glsl_pre_change_notify,
  NULL, /* material_set_parent_notify */
  NULL, /* layer_pre_change_notify */
  _cogl_material_backend_glsl_free_priv,
  NULL /* free_layer_priv */
};

#endif /* COGL_MATERIAL_BACKEND_GLSL */

