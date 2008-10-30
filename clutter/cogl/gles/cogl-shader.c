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

#ifdef HAVE_COGL_GLES2

static void _cogl_shader_free (CoglShader *shader);

COGL_HANDLE_DEFINE (Shader, shader, shader_handles);

static void
_cogl_shader_free (CoglShader *shader)
{
  /* Frees shader resources but its handle is not
     released! Do that separately before this! */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  glDeleteShader (shader->gl_handle);
}

CoglHandle
cogl_create_shader (COGLenum shaderType)
{
  CoglShader *shader;

  _COGL_GET_CONTEXT (ctx, 0);

  shader = g_slice_new (CoglShader);
  shader->ref_count = 1;
  shader->gl_handle = glCreateShader (shaderType);
  shader->type = shaderType;

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

  glShaderSource (shader->gl_handle, 1, &source, NULL);
}

void
cogl_shader_compile (CoglHandle handle)
{
  CoglShader *shader;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_shader (handle))
    return;

  shader = _cogl_shader_pointer_from_handle (handle);

  glCompileShader (shader->gl_handle);
}

void
cogl_shader_get_info_log (CoglHandle  handle,
                          guint       size,
                          gchar      *buffer)
{
  CoglShader *shader;
  COGLint len = 0;
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_shader (handle))
    return;

  shader = _cogl_shader_pointer_from_handle (handle);

  glGetShaderInfoLog (shader->gl_handle, size - 1, &len, buffer);
  buffer[len] = '\0';
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

  glGetShaderiv (shader->gl_handle, pname, dest);
}

#else /* HAVE_COGL_GLES2 */

/* No support on regular OpenGL 1.1 */

CoglHandle
cogl_create_shader (COGLenum shaderType)
{
  return COGL_INVALID_HANDLE;
}

gboolean
cogl_is_shader (CoglHandle handle)
{
  return FALSE;
}

CoglHandle
cogl_shader_ref (CoglHandle handle)
{
  return COGL_INVALID_HANDLE;
}

void
cogl_shader_unref (CoglHandle handle)
{
}

void
cogl_shader_source (CoglHandle  shader,
                    const gchar   *source)
{
}

void
cogl_shader_compile (CoglHandle shader_handle)
{
}

void
cogl_shader_get_info_log (CoglHandle  handle,
                          guint       size,
                          gchar      *buffer)
{
}

void
cogl_shader_get_parameteriv (CoglHandle  handle,
                             COGLenum    pname,
                             COGLint    *dest)
{
}

#endif /* HAVE_COGL_GLES2 */
