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
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-program.h"
#include "cogl-shader-private.h"
#include "cogl-internal.h"
#include "cogl-handle.h"
#include "cogl-context.h"
#include "cogl-journal-private.h"
#include "cogl-material-opengl-private.h"

#include <glib.h>

#include <string.h>

#define glCreateProgram              ctx->drv.pf_glCreateProgram
#define glAttachShader               ctx->drv.pf_glAttachShader
#define glUseProgram                 ctx->drv.pf_glUseProgram
#define glLinkProgram                ctx->drv.pf_glLinkProgram
#define glGetUniformLocation         ctx->drv.pf_glGetUniformLocation
#define glUniform1f                  ctx->drv.pf_glUniform1f
#define glUniform2f                  ctx->drv.pf_glUniform2f
#define glUniform3f                  ctx->drv.pf_glUniform3f
#define glUniform4f                  ctx->drv.pf_glUniform4f
#define glUniform1fv                 ctx->drv.pf_glUniform1fv
#define glUniform2fv                 ctx->drv.pf_glUniform2fv
#define glUniform3fv                 ctx->drv.pf_glUniform3fv
#define glUniform4fv                 ctx->drv.pf_glUniform4fv
#define glUniform1i                  ctx->drv.pf_glUniform1i
#define glUniform2i                  ctx->drv.pf_glUniform2i
#define glUniform3i                  ctx->drv.pf_glUniform3i
#define glUniform4i                  ctx->drv.pf_glUniform4i
#define glUniform1iv                 ctx->drv.pf_glUniform1iv
#define glUniform2iv                 ctx->drv.pf_glUniform2iv
#define glUniform3iv                 ctx->drv.pf_glUniform3iv
#define glUniform4iv                 ctx->drv.pf_glUniform4iv
#define glUniformMatrix2fv           ctx->drv.pf_glUniformMatrix2fv
#define glUniformMatrix3fv           ctx->drv.pf_glUniformMatrix3fv
#define glUniformMatrix4fv           ctx->drv.pf_glUniformMatrix4fv
#define glDeleteProgram              ctx->drv.pf_glDeleteProgram

#define glProgramString ctx->drv.pf_glProgramString
#define glBindProgram ctx->drv.pf_glBindProgram
#define glDeletePrograms ctx->drv.pf_glDeletePrograms
#define glGenPrograms ctx->drv.pf_glGenPrograms
#define glProgramLocalParameter4fv ctx->drv.pf_glProgramLocalParameter4fv


static void _cogl_program_free (CoglProgram *program);

COGL_HANDLE_DEFINE (Program, program);
COGL_OBJECT_DEFINE_DEPRECATED_REF_COUNTING (program);

static void
_cogl_program_free (CoglProgram *program)
{
  /* Frees program resources but its handle is not
     released! Do that separately before this! */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (program->gl_handle)
    {
      if (program->language == COGL_SHADER_LANGUAGE_ARBFP)
        GE (glDeletePrograms (1, &program->gl_handle));
      else
        GE (glDeleteProgram (program->gl_handle));
    }

  g_slice_free (CoglProgram, program);
}

CoglHandle
cogl_create_program (void)
{
  CoglProgram *program;
  _COGL_GET_CONTEXT (ctx, NULL);

  program = g_slice_new0 (CoglProgram);

  return _cogl_program_handle_new (program);
}

void
cogl_program_attach_shader (CoglHandle program_handle,
                            CoglHandle shader_handle)
{
  CoglProgram *program;
  CoglShader *shader;
  CoglShaderLanguage language;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (cogl_is_program (program_handle));
  g_return_if_fail (cogl_is_shader (shader_handle));

  program = _cogl_program_pointer_from_handle (program_handle);
  shader = _cogl_shader_pointer_from_handle (shader_handle);

  language = shader->language;

  /* We only allow attaching one ARBfp shader to a program */
  if (language == COGL_SHADER_LANGUAGE_ARBFP)
    g_return_if_fail (program->gl_handle == 0);

  program->language = language;

  if (language == COGL_SHADER_LANGUAGE_ARBFP)
    {
#ifdef COGL_GL_DEBUG
      GLenum gl_error;
#endif

      GE (glGenPrograms (1, &program->gl_handle));

      GE (glBindProgram (GL_FRAGMENT_PROGRAM_ARB, program->gl_handle));

#ifdef COGL_GL_DEBUG
      while ((gl_error = glGetError ()) != GL_NO_ERROR)
        ;
#endif
      glProgramString (GL_FRAGMENT_PROGRAM_ARB,
                       GL_PROGRAM_FORMAT_ASCII_ARB,
                       strlen (shader->arbfp_source),
                       shader->arbfp_source);
#ifdef COGL_GL_DEBUG
      gl_error = glGetError ();
      if (gl_error != GL_NO_ERROR)
        {
          g_warning ("%s: GL error (%d): Failed to compile ARBfp:\n%s\n%s",
                     G_STRLOC,
                     gl_error,
                     shader->arbfp_source,
                     glGetString (GL_PROGRAM_ERROR_STRING_ARB));
        }
#endif
    }
  else
    {
      if (!program->gl_handle)
        program->gl_handle = glCreateProgram ();
      GE (glAttachShader (program->gl_handle, shader->gl_handle));
    }

  /* NB: There is no separation between shader objects and program
   * objects for ARBfp */
}

void
cogl_program_link (CoglHandle handle)
{
  CoglProgram *program;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_program (handle))
    return;

  program = _cogl_program_pointer_from_handle (handle);

  if (program->language == COGL_SHADER_LANGUAGE_GLSL &&
      program->gl_handle)
    GE (glLinkProgram (program->gl_handle));

  program->is_linked = TRUE;
}

void
cogl_program_use (CoglHandle handle)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (handle == COGL_INVALID_HANDLE ||
                    cogl_is_program (handle));

  if (handle != COGL_INVALID_HANDLE)
    {
      CoglProgram *program = handle;
      g_return_if_fail (program->is_linked);
    }

  if (ctx->current_program == 0 && handle != 0)
    ctx->legacy_state_set++;
  else if (handle == 0 && ctx->current_program != 0)
    ctx->legacy_state_set--;

  if (handle != COGL_INVALID_HANDLE)
    cogl_handle_ref (handle);
  if (ctx->current_program != COGL_INVALID_HANDLE)
    cogl_handle_unref (ctx->current_program);
  ctx->current_program = handle;
}

/* ARBfp local parameters can be referenced like:
 *
 * "program.local[5]"
 *                ^14char offset (after whitespace is stripped)
 */
static int
get_local_param_index (const char *uniform_name)
{
  char *input = g_strdup (uniform_name);
  int i;
  char *p = input;
  char *endptr;
  int _index;

  for (i = 0; input[i] != '\0'; i++)
    if (input[i] != '_' && input[i] != '\t')
      *p++ = input[i];
  input[i] = '\0';

  g_return_val_if_fail (strncmp ("program.local[", input, 14) == 0, -1);

  _index = g_ascii_strtoull (input + 14, &endptr, 10);
  g_return_val_if_fail (endptr != input + 14, -1);
  g_return_val_if_fail (*endptr == ']', -1);

  g_return_val_if_fail (_index >= 0 &&
                        _index < COGL_PROGRAM_MAX_ARBFP_LOCAL_PARAMS, -1);

  g_free (input);

  return _index;
}

int
cogl_program_get_uniform_location (CoglHandle   handle,
                                   const char *uniform_name)
{
  CoglProgram *program;
  _COGL_GET_CONTEXT (ctx, 0);

  if (!cogl_is_program (handle))
    return 0;

  program = _cogl_program_pointer_from_handle (handle);

  if (program->language == COGL_SHADER_LANGUAGE_ARBFP)
    return get_local_param_index (uniform_name);
  else
    return glGetUniformLocation (program->gl_handle, uniform_name);
}

void
cogl_program_set_uniform_1f (CoglHandle handle,
                             int uniform_location,
                             float value)
{
  CoglProgram *program = handle;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (cogl_is_program (handle));
  g_return_if_fail (program->language != COGL_SHADER_LANGUAGE_ARBFP);

  _cogl_gl_use_program_wrapper (program);

  GE (glUniform1f (uniform_location, value));
}

void
cogl_program_uniform_1f (int uniform_location,
                         float value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  cogl_program_set_uniform_1f (ctx->current_program,
                               uniform_location, value);
}

void
cogl_program_set_uniform_1i (CoglHandle handle,
                             int uniform_location,
                             int value)
{
  CoglProgram *program = handle;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (cogl_is_program (handle));
  g_return_if_fail (program->language != COGL_SHADER_LANGUAGE_ARBFP);

  _cogl_gl_use_program_wrapper (program);

  GE (glUniform1i (uniform_location, value));
}

void
cogl_program_uniform_1i (int uniform_location,
                         int value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  cogl_program_set_uniform_1i (ctx->current_program, uniform_location, value);
}

void
cogl_program_set_uniform_float (CoglHandle handle,
                                int uniform_location,
                                int n_components,
                                int count,
                                const float *value)
{
  CoglProgram *program = handle;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (cogl_is_program (handle));

  if (program->language == COGL_SHADER_LANGUAGE_ARBFP)
    {
      unsigned int _index = uniform_location;
      unsigned int index_end = _index + count;
      int i;
      int j;

      g_return_if_fail (n_components == 4);

      GE (glBindProgram (GL_FRAGMENT_PROGRAM_ARB, program->gl_handle));

      for (i = _index; i < index_end; i++)
        for (j = 0; j < 4; j++)
          program->arbfp_local_params[i][j] = *(value++);

      for (i = _index; i < index_end; i++)
        GE (glProgramLocalParameter4fv (GL_FRAGMENT_PROGRAM_ARB,
                                        i,
                                        &program->arbfp_local_params[i][0]));
    }
  else
    {
      _cogl_gl_use_program_wrapper (program);

      switch (n_components)
        {
        case 1:
          GE (glUniform1fv (uniform_location, count, value));
          break;
        case 2:
          GE (glUniform2fv (uniform_location, count, value));
          break;
        case 3:
          GE (glUniform3fv (uniform_location, count, value));
          break;
        case 4:
          GE (glUniform4fv (uniform_location, count, value));
          break;
        default:
          g_warning ("%s called with invalid size parameter", G_STRFUNC);
        }
    }
}

void
cogl_program_uniform_float (int uniform_location,
                            int n_components,
                            int count,
                            const float *value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  cogl_program_set_uniform_float (ctx->current_program,
                                  uniform_location,
                                  n_components, count, value);
}

void
cogl_program_set_uniform_int (CoglHandle handle,
                              int uniform_location,
                              int n_components,
                              int count,
                              const int *value)
{
  CoglProgram *program = handle;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (cogl_is_program (handle));

  _cogl_gl_use_program_wrapper (program);

  switch (n_components)
    {
    case 1:
      glUniform1iv (uniform_location, count, value);
      break;
    case 2:
      glUniform2iv (uniform_location, count, value);
      break;
    case 3:
      glUniform3iv (uniform_location, count, value);
      break;
    case 4:
      glUniform4iv (uniform_location, count, value);
      break;
    default:
      g_warning ("%s called with invalid size parameter", G_STRFUNC);
    }
}

void
cogl_program_uniform_int (int uniform_location,
                          int n_components,
                          int count,
                          const int *value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  cogl_program_set_uniform_int (ctx->current_program,
                                uniform_location, n_components, count, value);
}

void
cogl_program_set_uniform_matrix (CoglHandle handle,
                                 int uniform_location,
                                 int n_components,
                                 int count,
                                 gboolean transpose,
                                 const float*value)
{
  CoglProgram *program = handle;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (cogl_is_program (handle));
  g_return_if_fail (program->language != COGL_SHADER_LANGUAGE_ARBFP);

  _cogl_gl_use_program_wrapper (program);

  switch (n_components)
    {
    case 2 :
      GE (glUniformMatrix2fv (uniform_location, count, transpose, value));
      break;
    case 3 :
      GE (glUniformMatrix3fv (uniform_location, count, transpose, value));
      break;
    case 4 :
      GE (glUniformMatrix4fv (uniform_location, count, transpose, value));
      break;
    default :
      g_warning ("%s called with invalid size parameter", G_STRFUNC);
    }
}

void
cogl_program_uniform_matrix (int uniform_location,
                             int dimensions,
                             int count,
                             gboolean  transpose,
                             const float *value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  cogl_program_set_uniform_matrix (ctx->current_program,
                                   uniform_location, dimensions,
                                   count, transpose, value);
}

CoglShaderLanguage
_cogl_program_get_language (CoglHandle handle)
{
  CoglProgram *program = handle;
  return program->language;
}

