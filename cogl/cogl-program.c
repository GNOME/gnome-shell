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

#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl-handle.h"

#include "cogl-shader-private.h"
#include "cogl-program-private.h"

#include <string.h>

static void _cogl_program_free (CoglProgram *program);

COGL_HANDLE_DEFINE (Program, program);
COGL_OBJECT_DEFINE_DEPRECATED_REF_COUNTING (program);

/* A CoglProgram is effectively just a list of shaders that will be
   used together and a set of values for the custom uniforms. No
   actual GL program is created - instead this is the responsibility
   of the GLSL material backend. The uniform values are collected in
   an array and then flushed whenever the material backend requests
   it. */

static void
_cogl_program_free (CoglProgram *program)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Unref all of the attached shaders */
  g_slist_foreach (program->attached_shaders, (GFunc) cogl_handle_unref, NULL);
  /* Destroy the list */
  g_slist_free (program->attached_shaders);

  for (i = 0; i < program->custom_uniforms->len; i++)
    {
      CoglProgramUniform *uniform =
        &g_array_index (program->custom_uniforms, CoglProgramUniform, i);

      g_free (uniform->name);

      if (uniform->value.count > 1)
        g_free (uniform->value.v.array);
    }

  g_array_free (program->custom_uniforms, TRUE);

  g_slice_free (CoglProgram, program);
}

CoglHandle
cogl_create_program (void)
{
  CoglProgram *program;

  program = g_slice_new0 (CoglProgram);

  program->custom_uniforms =
    g_array_new (FALSE, FALSE, sizeof (CoglProgramUniform));
  program->age = 0;

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

  /* Only one shader is allowed if the type is ARBfp */
  if (shader->language == COGL_SHADER_LANGUAGE_ARBFP)
    g_return_if_fail (program->attached_shaders == NULL);
  else if (shader->language == COGL_SHADER_LANGUAGE_GLSL)
    g_return_if_fail (_cogl_program_get_language (program) ==
                      COGL_SHADER_LANGUAGE_GLSL);

  program->attached_shaders
    = g_slist_prepend (program->attached_shaders,
                       cogl_handle_ref (shader_handle));

  program->age++;
}

void
cogl_program_link (CoglHandle handle)
{
  /* There's no point in linking the program here because it will have
     to be relinked with a different fixed functionality shader
     whenever the settings change */
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
cogl_program_get_uniform_location (CoglHandle handle,
                                   const char *uniform_name)
{
  int i;
  CoglProgram *program;
  CoglProgramUniform *uniform;

  if (!cogl_is_program (handle))
    return -1;

  program = _cogl_program_pointer_from_handle (handle);

  /* We can't just ask the GL program object for the uniform location
     directly because it will change every time the program is linked
     with a different shader. Instead we make our own mapping of
     uniform numbers and cache the names */
  for (i = 0; i < program->custom_uniforms->len; i++)
    {
      uniform = &g_array_index (program->custom_uniforms,
                                CoglProgramUniform, i);

      if (!strcmp (uniform->name, uniform_name))
        return i;
    }

  /* Create a new uniform with the given name */
  g_array_set_size (program->custom_uniforms,
                    program->custom_uniforms->len + 1);
  uniform = &g_array_index (program->custom_uniforms,
                            CoglProgramUniform,
                            program->custom_uniforms->len - 1);

  uniform->name = g_strdup (uniform_name);
  memset (&uniform->value, 0, sizeof (CoglBoxedValue));
  uniform->dirty = TRUE;
  uniform->location_valid = FALSE;

  return program->custom_uniforms->len - 1;
}

static void
cogl_program_uniform_x (CoglHandle handle,
                        int uniform_no,
                        int size,
                        int count,
                        CoglBoxedType type,
                        gsize value_size,
                        gconstpointer value,
                        gboolean transpose)
{
  CoglProgram *program = handle;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (cogl_is_program (handle));
  g_return_if_fail (program != NULL);

  if (uniform_no >= 0 && uniform_no < program->custom_uniforms->len &&
      size >= 1 && size <= 4 && count >= 1)
    {
      CoglProgramUniform *uniform =
        &g_array_index (program->custom_uniforms,
                        CoglProgramUniform, uniform_no);

      if (count == 1)
        {
          if (uniform->value.count > 1)
            g_free (uniform->value.v.array);

          memcpy (uniform->value.v.float_value, value, value_size);
        }
      else
        {
          if (uniform->value.count > 1)
            {
              if (uniform->value.count != count ||
                  uniform->value.size != size ||
                  uniform->value.type != type)
                {
                  g_free (uniform->value.v.array);
                  uniform->value.v.array = g_malloc (count * value_size);
                }
            }
          else
            uniform->value.v.array = g_malloc (count * value_size);

          memcpy (uniform->value.v.array, value, count * value_size);
        }

      uniform->value.type = type;
      uniform->value.size = size;
      uniform->value.count = count;
      uniform->dirty = TRUE;
    }
}

void
cogl_program_uniform_1f (int uniform_no,
                         float  value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  cogl_program_uniform_x (ctx->current_program,
                          uniform_no, 1, 1, COGL_BOXED_FLOAT,
                          sizeof (float), &value, FALSE);
}

void
cogl_program_set_uniform_1f (CoglHandle handle,
                             int uniform_location,
                             float value)
{
  cogl_program_uniform_x (handle,
                          uniform_location, 1, 1, COGL_BOXED_FLOAT,
                          sizeof (float), &value, FALSE);
}

void
cogl_program_uniform_1i (int uniform_no,
                         int value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  cogl_program_uniform_x (ctx->current_program,
                          uniform_no, 1, 1, COGL_BOXED_INT,
                          sizeof (int), &value, FALSE);
}

void
cogl_program_set_uniform_1i (CoglHandle handle,
                             int uniform_location,
                             int value)
{
  cogl_program_uniform_x (handle,
                          uniform_location, 1, 1, COGL_BOXED_INT,
                          sizeof (int), &value, FALSE);
}

void
cogl_program_uniform_float (int uniform_no,
                            int size,
                            int count,
                            const GLfloat *value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  cogl_program_uniform_x (ctx->current_program,
                          uniform_no, size, count, COGL_BOXED_FLOAT,
                          sizeof (float) * size, value, FALSE);
}

void
cogl_program_set_uniform_float (CoglHandle handle,
                                int uniform_location,
                                int n_components,
                                int count,
                                const float *value)
{
  cogl_program_uniform_x (handle,
                          uniform_location, n_components, count,
                          COGL_BOXED_FLOAT,
                          sizeof (float) * n_components, value, FALSE);
}

void
cogl_program_uniform_int (int uniform_no,
                          int size,
                          int count,
                          const GLint *value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  cogl_program_uniform_x (ctx->current_program,
                          uniform_no, size, count, COGL_BOXED_INT,
                          sizeof (int) * size, value, FALSE);
}

void
cogl_program_set_uniform_int (CoglHandle handle,
                              int uniform_location,
                              int n_components,
                              int count,
                              const int *value)
{
  cogl_program_uniform_x (handle,
                          uniform_location, n_components, count,
                          COGL_BOXED_INT,
                          sizeof (int) * n_components, value, FALSE);
}

void
cogl_program_set_uniform_matrix (CoglHandle handle,
                                 int uniform_location,
                                 int dimensions,
                                 int count,
                                 gboolean transpose,
                                 const float *value)
{
  g_return_if_fail (cogl_is_program (handle));

  cogl_program_uniform_x (handle,
                          uniform_location, dimensions, count,
                          COGL_BOXED_MATRIX,
                          sizeof (float) * dimensions * dimensions,
                          value,
                          transpose);
}

void
cogl_program_uniform_matrix (int uniform_no,
                             int size,
                             int count,
                             gboolean transpose,
                             const float *value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  cogl_program_set_uniform_matrix (ctx->current_program,
                                   uniform_no, size, count, transpose, value);
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

  g_return_val_if_fail (_index >= 0, -1);

  g_free (input);

  return _index;
}

static void
_cogl_program_flush_uniform_glsl (GLint location,
                                  CoglBoxedValue *value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  switch (value->type)
    {
    case COGL_BOXED_NONE:
      break;

    case COGL_BOXED_INT:
      {
        int *ptr;

        if (value->count == 1)
          ptr = value->v.int_value;
        else
          ptr = value->v.int_array;

        switch (value->size)
          {
          case 1: ctx->glUniform1iv (location, value->count, ptr); break;
          case 2: ctx->glUniform2iv (location, value->count, ptr); break;
          case 3: ctx->glUniform3iv (location, value->count, ptr); break;
          case 4: ctx->glUniform4iv (location, value->count, ptr); break;
          }
      }
      break;

    case COGL_BOXED_FLOAT:
      {
        float *ptr;

        if (value->count == 1)
          ptr = value->v.float_value;
        else
          ptr = value->v.float_array;

        switch (value->size)
          {
          case 1: ctx->glUniform1fv (location, value->count, ptr); break;
          case 2: ctx->glUniform2fv (location, value->count, ptr); break;
          case 3: ctx->glUniform3fv (location, value->count, ptr); break;
          case 4: ctx->glUniform4fv (location, value->count, ptr); break;
          }
      }
      break;

    case COGL_BOXED_MATRIX:
      {
        float *ptr;

        if (value->count == 1)
          ptr = value->v.matrix;
        else
          ptr = value->v.float_array;

        switch (value->size)
          {
          case 2:
            ctx->glUniformMatrix2fv (location, value->count,
                                     value->transpose, ptr);
            break;
          case 3:
            ctx->glUniformMatrix3fv (location, value->count,
                                     value->transpose, ptr);
            break;
          case 4:
            ctx->glUniformMatrix4fv (location, value->count,
                                     value->transpose, ptr);
            break;
          }
      }
      break;
    }
}

#ifdef HAVE_COGL_GL

static void
_cogl_program_flush_uniform_arbfp (GLint location,
                                   CoglBoxedValue *value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (value->type != COGL_BOXED_NONE)
    {
      g_return_if_fail (value->type == COGL_BOXED_FLOAT);
      g_return_if_fail (value->size == 4);
      g_return_if_fail (value->count == 1);

      GE( ctx, glProgramLocalParameter4fv (GL_FRAGMENT_PROGRAM_ARB, location,
                                           value->v.float_value) );
    }
}

#endif /* HAVE_COGL_GL */

void
_cogl_program_flush_uniforms (CoglProgram *program,
                              GLuint gl_program,
                              gboolean gl_program_changed)
{
  CoglProgramUniform *uniform;
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (ctx->driver != COGL_DRIVER_GLES1);

  for (i = 0; i < program->custom_uniforms->len; i++)
    {
      uniform = &g_array_index (program->custom_uniforms,
                                CoglProgramUniform, i);

      if (gl_program_changed || uniform->dirty)
        {
          if (gl_program_changed || !uniform->location_valid)
            {
              if (_cogl_program_get_language (program) ==
                  COGL_SHADER_LANGUAGE_GLSL)
                uniform->location =
                  ctx->glGetUniformLocation (gl_program, uniform->name);
              else
                uniform->location =
                  get_local_param_index (uniform->name);

              uniform->location_valid = TRUE;
            }

          /* If the uniform isn't really in the program then there's
             no need to actually set it */
          if (uniform->location != -1)
            {
              switch (_cogl_program_get_language (program))
                {
                case COGL_SHADER_LANGUAGE_GLSL:
                  _cogl_program_flush_uniform_glsl (uniform->location,
                                                    &uniform->value);
                  break;

                case COGL_SHADER_LANGUAGE_ARBFP:
#ifdef HAVE_COGL_GL
                  _cogl_program_flush_uniform_arbfp (uniform->location,
                                                     &uniform->value);
#endif
                  break;
                }
            }

          uniform->dirty = FALSE;
        }
    }
}

CoglShaderLanguage
_cogl_program_get_language (CoglHandle handle)
{
  CoglProgram *program = handle;

  /* Use the language of the first shader */

  if (program->attached_shaders)
    {
      CoglShader *shader = program->attached_shaders->data;
      return shader->language;
    }
  else
    return COGL_SHADER_LANGUAGE_GLSL;
}

static gboolean
_cogl_program_has_shader_type (CoglProgram *program,
                               CoglShaderType type)
{
  GSList *l;

  for (l = program->attached_shaders; l; l = l->next)
    {
      CoglShader *shader = l->data;

      if (shader->type == type)
        return TRUE;
    }

  return FALSE;
}

gboolean
_cogl_program_has_fragment_shader (CoglHandle handle)
{
  return _cogl_program_has_shader_type (handle, COGL_SHADER_TYPE_FRAGMENT);
}

gboolean
_cogl_program_has_vertex_shader (CoglHandle handle)
{
  return _cogl_program_has_shader_type (handle, COGL_SHADER_TYPE_VERTEX);
}
