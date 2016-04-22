/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-shader-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-glsl-shader-private.h"
#include "cogl-glsl-shader-boilerplate.h"

#include <glib.h>

#include <string.h>

static void _cogl_shader_free (CoglShader *shader);

COGL_HANDLE_DEFINE (Shader, shader);
COGL_OBJECT_DEFINE_DEPRECATED_REF_COUNTING (shader);

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif

static void
_cogl_shader_free (CoglShader *shader)
{
  /* Frees shader resources but its handle is not
     released! Do that separately before this! */
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

#ifdef HAVE_COGL_GL
  if (shader->language == COGL_SHADER_LANGUAGE_ARBFP)
    {
      if (shader->gl_handle)
        GE (ctx, glDeletePrograms (1, &shader->gl_handle));
    }
  else
#endif
    if (shader->gl_handle)
      GE (ctx, glDeleteShader (shader->gl_handle));

  g_slice_free (CoglShader, shader);
}

CoglHandle
cogl_create_shader (CoglShaderType type)
{
  CoglShader *shader;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  switch (type)
    {
    case COGL_SHADER_TYPE_VERTEX:
    case COGL_SHADER_TYPE_FRAGMENT:
      break;
    default:
      g_warning ("Unexpected shader type (0x%08lX) given to "
                 "cogl_create_shader", (unsigned long) type);
      return COGL_INVALID_HANDLE;
    }

  shader = g_slice_new (CoglShader);
  shader->language = COGL_SHADER_LANGUAGE_GLSL;
  shader->gl_handle = 0;
  shader->compilation_pipeline = NULL;
  shader->type = type;

  return _cogl_shader_handle_new (shader);
}

static void
delete_shader (CoglShader *shader)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

#ifdef HAVE_COGL_GL
  if (shader->language == COGL_SHADER_LANGUAGE_ARBFP)
    {
      if (shader->gl_handle)
        GE (ctx, glDeletePrograms (1, &shader->gl_handle));
    }
  else
#endif
    {
      if (shader->gl_handle)
        GE (ctx, glDeleteShader (shader->gl_handle));
    }

  shader->gl_handle = 0;

  if (shader->compilation_pipeline)
    {
      cogl_object_unref (shader->compilation_pipeline);
      shader->compilation_pipeline = NULL;
    }
}

void
cogl_shader_source (CoglHandle   handle,
                    const char  *source)
{
  CoglShader *shader;
  CoglShaderLanguage language;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_shader (handle))
    return;

  shader = handle;

#ifdef HAVE_COGL_GL
  if (strncmp (source, "!!ARBfp1.0", 10) == 0)
    language = COGL_SHADER_LANGUAGE_ARBFP;
  else
#endif
    language = COGL_SHADER_LANGUAGE_GLSL;

  /* Delete the old object if the language is changing... */
  if (G_UNLIKELY (language != shader->language) &&
      shader->gl_handle)
    delete_shader (shader);

  shader->source = g_strdup (source);

  shader->language = language;
}

void
cogl_shader_compile (CoglHandle handle)
{
  CoglShader *shader;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_shader (handle))
    return;

#ifdef HAVE_COGL_GL
  shader = handle;
  if (shader->language == COGL_SHADER_LANGUAGE_ARBFP)
    _cogl_shader_compile_real (handle, NULL);
#endif

  /* XXX: For GLSL we don't actually compile anything until the shader
   * gets used so we have an opportunity to add some boilerplate to
   * the shader.
   *
   * At the end of the day this is obviously a badly designed API
   * given that we are having to lie to the user. It was a mistake to
   * so thinly wrap the OpenGL shader API and the current plan is to
   * replace it with a pipeline snippets API. */
}

void
_cogl_shader_compile_real (CoglHandle handle,
                           CoglPipeline *pipeline)
{
  CoglShader *shader = handle;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

#ifdef HAVE_COGL_GL
  if (shader->language == COGL_SHADER_LANGUAGE_ARBFP)
    {
#ifdef COGL_GL_DEBUG
      GLenum gl_error;
#endif

      if (shader->gl_handle)
        return;

      GE (ctx, glGenPrograms (1, &shader->gl_handle));

      GE (ctx, glBindProgram (GL_FRAGMENT_PROGRAM_ARB, shader->gl_handle));

      if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_SHOW_SOURCE)))
        g_message ("user ARBfp program:\n%s", shader->source);

#ifdef COGL_GL_DEBUG
      while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
        ;
#endif
      ctx->glProgramString (GL_FRAGMENT_PROGRAM_ARB,
                            GL_PROGRAM_FORMAT_ASCII_ARB,
                            strlen (shader->source),
                            shader->source);
#ifdef COGL_GL_DEBUG
      gl_error = ctx->glGetError ();
      if (gl_error != GL_NO_ERROR)
        {
          g_warning ("%s: GL error (%d): Failed to compile ARBfp:\n%s\n%s",
                     G_STRLOC,
                     gl_error,
                     shader->source,
                     ctx->glGetString (GL_PROGRAM_ERROR_STRING_ARB));
        }
#endif
    }
  else
#endif
    {
      GLenum gl_type;
      GLint status;

      if (shader->gl_handle)
        {
          CoglPipeline *prev = shader->compilation_pipeline;

          /* XXX: currently the only things that will affect the
           * boilerplate for user shaders, apart from driver features,
           * are the pipeline layer-indices and texture-unit-indices
           */
          if (pipeline == prev ||
              _cogl_pipeline_layer_and_unit_numbers_equal (prev, pipeline))
            return;
        }

      if (shader->gl_handle)
        delete_shader (shader);

      switch (shader->type)
        {
        case COGL_SHADER_TYPE_VERTEX:
          gl_type = GL_VERTEX_SHADER;
          break;
        case COGL_SHADER_TYPE_FRAGMENT:
          gl_type = GL_FRAGMENT_SHADER;
          break;
        default:
          g_assert_not_reached ();
          break;
        }

      shader->gl_handle = ctx->glCreateShader (gl_type);

      _cogl_glsl_shader_set_source_with_boilerplate (ctx,
                                                     shader->gl_handle,
                                                     gl_type,
                                                     pipeline,
                                                     1,
                                                     (const char **)
                                                      &shader->source,
                                                     NULL);

      GE (ctx, glCompileShader (shader->gl_handle));

      shader->compilation_pipeline = cogl_object_ref (pipeline);

      GE (ctx, glGetShaderiv (shader->gl_handle, GL_COMPILE_STATUS, &status));
      if (!status)
        {
          char buffer[512];
          int len = 0;

          ctx->glGetShaderInfoLog (shader->gl_handle, 511, &len, buffer);
          buffer[len] = '\0';

          g_warning ("Failed to compile GLSL program:\n"
                     "src:\n%s\n"
                     "error:\n%s\n",
                     shader->source,
                     buffer);
        }
    }
}

char *
cogl_shader_get_info_log (CoglHandle handle)
{
  if (!cogl_is_shader (handle))
    return NULL;

  /* XXX: This API doesn't really do anything!
   *
   * This API is purely for compatibility
   *
   * The reason we don't do anything is because a shader needs to
   * be associated with a CoglPipeline for Cogl to be able to
   * compile and link anything.
   *
   * The way this API was originally designed as a very thin wrapper
   * over the GL api was a mistake and it's now very difficult to
   * make the API work in a meaningful way given how the rest of Cogl
   * has evolved.
   *
   * The CoglShader API is mostly deprecated by CoglSnippets and so
   * these days we do the bare minimum to support the existing users
   * of it until they are able to migrate to the snippets api.
   */

  return g_strdup ("");
}

CoglShaderType
cogl_shader_get_type (CoglHandle  handle)
{
  CoglShader *shader;

  _COGL_GET_CONTEXT (ctx, COGL_SHADER_TYPE_VERTEX);

  if (!cogl_is_shader (handle))
    {
      g_warning ("Non shader handle type passed to cogl_shader_get_type");
      return COGL_SHADER_TYPE_VERTEX;
    }

  shader = handle;
  return shader->type;
}

CoglBool
cogl_shader_is_compiled (CoglHandle handle)
{
#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES2)
  if (!cogl_is_shader (handle))
    return FALSE;

  /* XXX: This API doesn't really do anything!
   *
   * This API is purely for compatibility and blatantly lies to the
   * user about whether their shader has been compiled.
   *
   * I suppose we could say we're stretching the definition of
   * "compile" and are deferring any related errors to be "linker"
   * errors.
   *
   * The reason we don't do anything is because a shader needs to
   * be associated with a CoglPipeline for Cogl to be able to
   * compile and link anything.
   *
   * The CoglShader API is mostly deprecated by CoglSnippets and so
   * these days we do the bare minimum to support the existing users
   * of it until they are able to migrate to the snippets api.
   */

  return TRUE;

#else
  return FALSE;
#endif
}
