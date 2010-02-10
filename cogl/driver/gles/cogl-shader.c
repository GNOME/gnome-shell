/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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

COGL_HANDLE_DEFINE (Shader, shader);

static void
_cogl_shader_free (CoglShader *shader)
{
  /* Frees shader resources but its handle is not
     released! Do that separately before this! */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  glDeleteShader (shader->gl_handle);
}

CoglHandle
cogl_create_shader (CoglShaderType type)
{
  CoglShader *shader;
  GLenum gl_type;

  if (type == COGL_SHADER_TYPE_VERTEX)
    gl_type = GL_VERTEX_SHADER;
  else if (type == COGL_SHADER_TYPE_FRAGMENT)
    gl_type = GL_FRAGMENT_SHADER;
  else
    {
      g_warning ("Unexpected shader type (0x%08lX) given to "
                 "cogl_create_shader", (unsigned long) type);
      return COGL_INVALID_HANDLE;
    }

  shader = g_slice_new (CoglShader);
  shader->gl_handle = glCreateShader (gl_type);

  return _cogl_shader_handle_new (shader);
}

void
cogl_shader_source (CoglHandle   handle,
                    const char  *source)
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

char *
cogl_shader_get_info_log (CoglHandle handle)
{
  CoglShader *shader;
  char buffer[512];
  int len = 0;
  _COGL_GET_CONTEXT (ctx, NULL);

  if (!cogl_is_shader (handle))
    return NULL;

  shader = _cogl_shader_pointer_from_handle (handle);

  glGetShaderInfoLog (shader->gl_handle, 511, &len, buffer);
  buffer[len] = '\0';

  return g_strdup (buffer);
}

CoglShaderType
cogl_shader_get_type (CoglHandle  handle)
{
  GLint type;
  CoglShader *shader;

  if (!cogl_is_shader (handle))
    {
      g_warning ("Non shader handle type passed to cogl_shader_get_type");
      return COGL_SHADER_TYPE_VERTEX;
    }

  shader = _cogl_shader_pointer_from_handle (handle);

  GE (glGetShaderiv (shader->gl_handle, GL_SHADER_TYPE, &type));
  if (type == GL_VERTEX_SHADER)
    return COGL_SHADER_TYPE_VERTEX;
  else if (type == GL_FRAGMENT_SHADER)
    return COGL_SHADER_TYPE_VERTEX;
  else
    {
      g_warning ("Unexpected shader type 0x%08lX", (unsigned long)type);
      return COGL_SHADER_TYPE_VERTEX;
    }
}

gboolean
cogl_shader_is_compiled (CoglHandle handle)
{
  GLint status;
  CoglShader *shader;

  if (!cogl_is_shader (handle))
    return FALSE;

  shader = _cogl_shader_pointer_from_handle (handle);

  GE (glGetShaderiv (shader->gl_handle, GL_COMPILE_STATUS, &status));
  if (status == GL_TRUE)
    return TRUE;
  else
    return FALSE;
}

#else /* HAVE_COGL_GLES2 */

/* No support on regular OpenGL 1.1 */

CoglHandle
cogl_create_shader (CoglShaderType type)
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
                    const char   *source)
{
}

void
cogl_shader_compile (CoglHandle shader_handle)
{
}

char *
cogl_shader_get_info_log (CoglHandle handle)
{
  return NULL;
}

CoglShaderType
cogl_shader_get_type (CoglHandle  handle)
{
  return COGL_SHADER_TYPE_VERTEX;
}

gboolean
cogl_shader_is_compiled (CoglHandle handle)
{
  return FALSE;
}

#endif /* HAVE_COGL_GLES2 */
