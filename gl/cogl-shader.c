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
#include "cogl-shader-private.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-handle.h"

#include <glib.h>

/* Expecting ARB functions not to be defined */
#define glCreateShaderObjectARB         ctx->pf_glCreateShaderObjectARB
#define glGetObjectParameterivARB       ctx->pf_glGetObjectParameterivARB
#define glGetInfoLogARB                 ctx->pf_glGetInfoLogARB
#define glCompileShaderARB              ctx->pf_glCompileShaderARB
#define glShaderSourceARB               ctx->pf_glShaderSourceARB
#define glDeleteObjectARB               ctx->pf_glDeleteObjectARB

static void _cogl_shader_free (CoglShader *shader);

COGL_HANDLE_DEFINE (Shader, shader, shader_handles);

static void
_cogl_shader_free (CoglShader *shader)
{
  /* Frees shader resources but its handle is not
     released! Do that separately before this! */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  glDeleteObjectARB (shader->gl_handle);
}

CoglHandle
cogl_create_shader (COGLenum shaderType)
{
  CoglShader *shader;

  _COGL_GET_CONTEXT (ctx, 0);

  shader = g_slice_new (CoglShader);
  shader->ref_count = 1;
  shader->gl_handle = glCreateShaderObjectARB (shaderType);

  COGL_HANDLE_DEBUG_NEW (shader, shader);

  return _cogl_shader_handle_new (shader);
}

void
cogl_shader_source (CoglHandle   handle,
                    const gchar *source)
{
  CoglShader *shader;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (!cogl_is_shader (handle))
    return;

  shader = _cogl_shader_pointer_from_handle (handle);

  glShaderSourceARB (shader->gl_handle, 1, &source, NULL);
}

void
cogl_shader_compile (CoglHandle handle)
{
  CoglShader *shader;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_shader (handle))
    return;

  shader = _cogl_shader_pointer_from_handle (handle);

  glCompileShaderARB (shader->gl_handle);
}

void
cogl_shader_get_info_log (CoglHandle  handle,
                          guint       size,
                          gchar      *buffer)
{
  CoglShader *shader;
  COGLint len;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_shader (handle))
    return;

  shader = _cogl_shader_pointer_from_handle (handle);

  glGetInfoLogARB (shader->gl_handle, size-1, &len, buffer);
  buffer[len]='\0';
}

void
cogl_shader_get_parameteriv (CoglHandle  handle,
                             COGLenum    pname,
                             COGLint    *dest)
{
  CoglShader *shader;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_shader (handle))
    return;

  shader = _cogl_shader_pointer_from_handle (handle);

  glGetObjectParameterivARB (shader->gl_handle, pname, dest);
}
