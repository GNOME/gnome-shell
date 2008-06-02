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

#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-handle.h"

#ifdef HAVE_COGL_GLES2

#include "cogl-shader.h"
#include "cogl-program.h"

static void _cogl_program_free (CoglProgram *program);

COGL_HANDLE_DEFINE (Program, program, program_handles);

static void
_cogl_program_free (CoglProgram *program)
{
  /* Frees program resources but its handle is not
     released! Do that separately before this! */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  glDeleteProgram (program->gl_handle);
}

CoglHandle
cogl_create_program (void)
{
  CoglProgram *program;
  _COGL_GET_CONTEXT (ctx, 0);

  program = g_slice_new (CoglProgram);
  program->ref_count = 1;
  program->gl_handle = glCreateProgram ();

  program->attached_vertex_shader = FALSE;
  program->attached_fragment_shader = FALSE;
  program->attached_fixed_vertex_shader = FALSE;
  program->attached_fixed_fragment_shader = FALSE;

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

  if (shader->type == GL_VERTEX_SHADER)
    {
      if (program->attached_fixed_vertex_shader)
	{
	  glDetachShader (program->gl_handle, ctx->gles2.vertex_shader);
	  program->attached_fixed_vertex_shader = FALSE;
	}
      program->attached_vertex_shader = TRUE;
    }
  else if (shader->type == GL_FRAGMENT_SHADER)
    {
      if (program->attached_fixed_fragment_shader)
	{
	  glDetachShader (program->gl_handle, ctx->gles2.fragment_shader);
	  program->attached_fixed_fragment_shader = FALSE;
	}
      program->attached_fragment_shader = TRUE;
    }

  glAttachShader (program->gl_handle, shader->gl_handle);
}

void
cogl_program_link (CoglHandle handle)
{
  CoglProgram *program;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (!cogl_is_program (handle))
    return;

  program = _cogl_program_pointer_from_handle (handle);

  if (!program->attached_vertex_shader
      && !program->attached_fixed_vertex_shader)
    {
      glAttachShader (program->gl_handle, ctx->gles2.vertex_shader);
      program->attached_fixed_vertex_shader = TRUE;
    }

  if (!program->attached_fragment_shader
      && !program->attached_fixed_fragment_shader)
    {
      glAttachShader (program->gl_handle, ctx->gles2.fragment_shader);
      program->attached_fixed_fragment_shader = TRUE;
    }

  /* Set the attributes so that the wrapper functions will still work */
  cogl_gles2_wrapper_bind_attributes (program->gl_handle);

  glLinkProgram (program->gl_handle);

  /* Retrieve the uniforms */
  cogl_gles2_wrapper_get_uniforms (program->gl_handle,
				   &program->uniforms);
}

void
cogl_program_use (CoglHandle handle)
{
  CoglProgram *program;
  GLuint gl_handle;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (handle != COGL_INVALID_HANDLE && !cogl_is_program (handle))
    return;

  if (handle == COGL_INVALID_HANDLE)
    {
      /* Go back to the fixed-functionality emulator program */
      gl_handle = ctx->gles2.program;
      ctx->gles2.uniforms = &ctx->gles2.fixed_uniforms;
    }
  else
    {
      program = _cogl_program_pointer_from_handle (handle);
      gl_handle = program->gl_handle;
      /* Use the uniforms in the program */
      ctx->gles2.uniforms = &program->uniforms;
    }  

  glUseProgram (gl_handle);

  /* Update all of the matrix attributes */
  cogl_gles2_wrapper_update_matrix (&ctx->gles2, GL_MODELVIEW);
  cogl_gles2_wrapper_update_matrix (&ctx->gles2, GL_PROJECTION);
  cogl_gles2_wrapper_update_matrix (&ctx->gles2, GL_TEXTURE);
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

  return glGetUniformLocation (program->gl_handle, uniform_name);
}

void
cogl_program_uniform_1f (COGLint uniform_no,
                         gfloat  value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  glUniform1f (uniform_no, value);
}

#else /* HAVE_COGL_GLES2 */

/* No support on regular OpenGL 1.1 */

CoglHandle
cogl_create_program (void)
{
  return COGL_INVALID_HANDLE;
}

gboolean
cogl_is_program (CoglHandle handle)
{
  return FALSE;
}

CoglHandle
cogl_program_ref (CoglHandle handle)
{
  return COGL_INVALID_HANDLE;
}

void
cogl_program_unref (CoglHandle handle)
{
}

void
cogl_program_attach_shader (CoglHandle program_handle,
                            CoglHandle shader_handle)
{
}

void
cogl_program_link (CoglHandle program_handle)
{
}

void
cogl_program_use (CoglHandle program_handle)
{
}

COGLint
cogl_program_get_uniform_location (CoglHandle   program_handle,
                                   const gchar *uniform_name)
{
  return 0;
}

void
cogl_program_uniform_1f (COGLint uniform_no,
                         gfloat  value)
{
}

#endif /* HAVE_COGL_GLES2 */
