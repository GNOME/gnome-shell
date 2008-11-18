/*
 * Clutter COGL
 *
 * A basic GL/GLES Abstraction/Utility Layer
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2008 OpenedHand
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

#include <glib.h>

/* Expecting ARB functions not to be defined */
#define glCreateProgramObjectARB        ctx->pf_glCreateProgramObjectARB
#define glAttachObjectARB               ctx->pf_glAttachObjectARB
#define glUseProgramObjectARB           ctx->pf_glUseProgramObjectARB
#define glLinkProgramARB                ctx->pf_glLinkProgramARB
#define glGetUniformLocationARB         ctx->pf_glGetUniformLocationARB
#define glUniform1fARB                  ctx->pf_glUniform1fARB
#define glUniform2fARB                  ctx->pf_glUniform2fARB
#define glUniform3fARB                  ctx->pf_glUniform3fARB
#define glUniform4fARB                  ctx->pf_glUniform4fARB
#define glUniform1fvARB                 ctx->pf_glUniform1fvARB
#define glUniform2fvARB                 ctx->pf_glUniform2fvARB
#define glUniform3fvARB                 ctx->pf_glUniform3fvARB
#define glUniform4fvARB                 ctx->pf_glUniform4fvARB
#define glUniform1iARB                  ctx->pf_glUniform1iARB
#define glUniform2iARB                  ctx->pf_glUniform2iARB
#define glUniform3iARB                  ctx->pf_glUniform3iARB
#define glUniform4iARB                  ctx->pf_glUniform4iARB
#define glUniform1ivARB                 ctx->pf_glUniform1ivARB
#define glUniform2ivARB                 ctx->pf_glUniform2ivARB
#define glUniform3ivARB                 ctx->pf_glUniform3ivARB
#define glUniform4ivARB                 ctx->pf_glUniform4ivARB
#define glUniformMatrix2fvARB           ctx->pf_glUniformMatrix2fvARB
#define glUniformMatrix3fvARB           ctx->pf_glUniformMatrix3fvARB
#define glUniformMatrix4fvARB           ctx->pf_glUniformMatrix4fvARB
#define glDeleteObjectARB               ctx->pf_glDeleteObjectARB

static void _cogl_program_free (CoglProgram *program);

COGL_HANDLE_DEFINE (Program, program, program_handles);

static void
_cogl_program_free (CoglProgram *program)
{
  /* Frees program resources but its handle is not
     released! Do that separately before this! */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  glDeleteObjectARB (program->gl_handle);
}

CoglHandle
cogl_create_program (void)
{
  CoglProgram *program;
  _COGL_GET_CONTEXT (ctx, 0);

  program = g_slice_new (CoglProgram);
  program->ref_count = 1;
  program->gl_handle = glCreateProgramObjectARB ();

  COGL_HANDLE_DEBUG_NEW (program, program);

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

  glAttachObjectARB (program->gl_handle, shader->gl_handle);
}

void
cogl_program_link (CoglHandle handle)
{
  CoglProgram *program;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (!cogl_is_program (handle))
    return;

  program = _cogl_program_pointer_from_handle (handle);

  glLinkProgramARB (program->gl_handle);
}

void
cogl_program_use (CoglHandle handle)
{
  CoglProgram *program;
  GLhandleARB gl_handle;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (handle != COGL_INVALID_HANDLE && !cogl_is_program (handle))
    return;

  if (handle == COGL_INVALID_HANDLE)
    gl_handle = 0;
  else
    {
      program = _cogl_program_pointer_from_handle (handle);
      gl_handle = program->gl_handle;
    }

  glUseProgramObjectARB (gl_handle);
}

COGLint
cogl_program_get_uniform_location (CoglHandle   handle,
                                   const gchar *uniform_name)
{
  CoglProgram *program;
  _COGL_GET_CONTEXT (ctx, 0);
  
  if (!cogl_is_program (handle))
    return 0;

  program = _cogl_program_pointer_from_handle (handle);

  return glGetUniformLocationARB (program->gl_handle, uniform_name);
}

void
cogl_program_uniform_1f (COGLint uniform_no,
                         gfloat  value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  glUniform1fARB (uniform_no, value);
}

void
cogl_program_uniform_float (COGLint  uniform_no,
                            gint     size,
                            gint     count,
                            const GLfloat *value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  switch (size)
    {
    case 1:
      glUniform1fvARB (uniform_no, count, value);
      break;
    case 2:
      glUniform2fvARB (uniform_no, count, value);
      break;
    case 3:
      glUniform3fvARB (uniform_no, count, value);
      break;
    case 4:
      glUniform4fvARB (uniform_no, count, value);
      break;
    default:
      g_warning ("%s called with invalid size parameter", G_STRFUNC);
    }
}

void
cogl_program_uniform_int (COGLint  uniform_no,
                          gint     size,
                          gint     count,
                          const COGLint *value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  switch (size)
    {
    case 1:
      glUniform1ivARB (uniform_no, count, value);
      break;
    case 2:
      glUniform2ivARB (uniform_no, count, value);
      break;
    case 3:
      glUniform3ivARB (uniform_no, count, value);
      break;
    case 4:
      glUniform4ivARB (uniform_no, count, value);
      break;
    default:
      g_warning ("%s called with invalid size parameter", G_STRFUNC);
    }
}

void
cogl_program_uniform_matrix (COGLint   uniform_no,
                             gint      size,
                             gint      count,
                             gboolean  transpose,
                             const GLfloat  *value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  switch (size)
    {
    case 2 :
      glUniformMatrix2fvARB (uniform_no, count, transpose, value);
      break;
    case 3 :
      glUniformMatrix3fvARB (uniform_no, count, transpose, value);
      break;
    case 4 :
      glUniformMatrix4fvARB (uniform_no, count, transpose, value);
      break;
    default :
      g_warning ("%s called with invalid size parameter", G_STRFUNC);
    }
}
