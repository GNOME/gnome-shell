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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
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

static void _cogl_program_free (CoglProgram *program);

COGL_HANDLE_DEFINE (Program, program);
COGL_OBJECT_DEFINE_DEPRECATED_REF_COUNTING (program);

static void
_cogl_program_free (CoglProgram *program)
{
  /* Frees program resources but its handle is not
     released! Do that separately before this! */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  GE (glDeleteProgram (program->gl_handle));

  g_slice_free (CoglProgram, program);
}

CoglHandle
cogl_create_program (void)
{
  CoglProgram *program;
  _COGL_GET_CONTEXT (ctx, NULL);

  program = g_slice_new (CoglProgram);
  program->gl_handle = glCreateProgram ();

  return _cogl_program_handle_new (program);
}

void
cogl_program_attach_shader (CoglHandle program_handle,
                            CoglHandle shader_handle)
{
  CoglProgram *program;
  CoglShader *shader;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_program (program_handle) || !cogl_is_shader (shader_handle))
    return;

  program = _cogl_program_pointer_from_handle (program_handle);
  shader = _cogl_shader_pointer_from_handle (shader_handle);

  GE (glAttachShader (program->gl_handle, shader->gl_handle));
}

void
cogl_program_link (CoglHandle handle)
{
  CoglProgram *program;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_program (handle))
    return;

  program = _cogl_program_pointer_from_handle (handle);

  GE (glLinkProgram (program->gl_handle));
}

void
cogl_program_use (CoglHandle handle)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (handle == COGL_INVALID_HANDLE ||
                    cogl_is_program (handle));

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

int
cogl_program_get_uniform_location (CoglHandle   handle,
                                   const char *uniform_name)
{
  CoglProgram *program;
  _COGL_GET_CONTEXT (ctx, 0);

  if (!cogl_is_program (handle))
    return 0;

  program = _cogl_program_pointer_from_handle (handle);

  return glGetUniformLocation (program->gl_handle, uniform_name);
}

void
cogl_program_uniform_1f (int uniform_no,
                         float  value)
{
  CoglProgram *program;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  program = ctx->current_program;

  g_return_if_fail (program != NULL);

  _cogl_gl_use_program_wrapper (program->gl_handle);

  GE (glUniform1f (uniform_no, value));
}

void
cogl_program_uniform_1i (int uniform_no,
                         int    value)
{
  CoglProgram *program;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  program = ctx->current_program;

  g_return_if_fail (program != NULL);

  _cogl_gl_use_program_wrapper (program->gl_handle);

  GE (glUniform1i (uniform_no, value));
}

void
cogl_program_uniform_float (int  uniform_no,
                            int     size,
                            int     count,
                            const GLfloat *value)
{
  CoglProgram *program;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  program = ctx->current_program;

  g_return_if_fail (program != NULL);

  _cogl_gl_use_program_wrapper (program->gl_handle);

  switch (size)
    {
    case 1:
      GE (glUniform1fv (uniform_no, count, value));
      break;
    case 2:
      GE (glUniform2fv (uniform_no, count, value));
      break;
    case 3:
      GE (glUniform3fv (uniform_no, count, value));
      break;
    case 4:
      GE (glUniform4fv (uniform_no, count, value));
      break;
    default:
      g_warning ("%s called with invalid size parameter", G_STRFUNC);
    }
}

void
cogl_program_uniform_int (int  uniform_no,
                          int     size,
                          int     count,
                          const int *value)
{
  CoglProgram *program;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  program = ctx->current_program;

  g_return_if_fail (program != NULL);

  _cogl_gl_use_program_wrapper (program->gl_handle);

  switch (size)
    {
    case 1:
      glUniform1iv (uniform_no, count, value);
      break;
    case 2:
      glUniform2iv (uniform_no, count, value);
      break;
    case 3:
      glUniform3iv (uniform_no, count, value);
      break;
    case 4:
      glUniform4iv (uniform_no, count, value);
      break;
    default:
      g_warning ("%s called with invalid size parameter", G_STRFUNC);
    }
}

void
cogl_program_uniform_matrix (int   uniform_no,
                             int      size,
                             int      count,
                             gboolean  transpose,
                             const GLfloat  *value)
{
  CoglProgram *program;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  program = ctx->current_program;

  g_return_if_fail (program != NULL);

  _cogl_gl_use_program_wrapper (program->gl_handle);

  switch (size)
    {
    case 2 :
      GE (glUniformMatrix2fv (uniform_no, count, transpose, value));
      break;
    case 3 :
      GE (glUniformMatrix3fv (uniform_no, count, transpose, value));
      break;
    case 4 :
      GE (glUniformMatrix4fv (uniform_no, count, transpose, value));
      break;
    default :
      g_warning ("%s called with invalid size parameter", G_STRFUNC);
    }
}
