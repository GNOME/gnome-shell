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

#include <string.h>

#include "cogl-shader-private.h"
#include "cogl-program.h"

static void _cogl_program_free (CoglProgram *program);

COGL_HANDLE_DEFINE (Program, program, program_handles);

static void
_cogl_program_free (CoglProgram *program)
{
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Unref all of the attached shaders */
  g_slist_foreach (program->attached_shaders, (GFunc) cogl_shader_unref, NULL);
  /* Destroy the list */
  g_slist_free (program->attached_shaders);

  _cogl_gles2_clear_cache_for_program ((CoglHandle) program);

  if (ctx->gles2.settings.user_program == (CoglHandle) program)
    {
      ctx->gles2.settings.user_program = COGL_INVALID_HANDLE;
      ctx->gles2.settings_dirty = TRUE;
    }

  for (i = 0; i < COGL_GLES2_NUM_CUSTOM_UNIFORMS; i++)
    if (program->custom_uniform_names[i])
      g_free (program->custom_uniform_names[i]);
}

CoglHandle
cogl_create_program (void)
{
  CoglProgram *program;

  program = g_slice_new (CoglProgram);
  program->ref_count = 1;
  program->attached_shaders = NULL;
  memset (program->custom_uniform_names, 0,
	  COGL_GLES2_NUM_CUSTOM_UNIFORMS * sizeof (char *));

  COGL_HANDLE_DEBUG_NEW (program, program);

  return _cogl_program_handle_new (program);
}

void
cogl_program_attach_shader (CoglHandle program_handle,
                            CoglHandle shader_handle)
{
  CoglProgram *program;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  if (!cogl_is_program (program_handle) || !cogl_is_shader (shader_handle))
    return;

  program = _cogl_program_pointer_from_handle (program_handle);
  program->attached_shaders
    = g_slist_prepend (program->attached_shaders,
		       cogl_shader_ref (shader_handle));

  /* Whenever the shader changes we will need to relink the program
     with the fixed functionality shaders so we should forget the
     cached programs */
  _cogl_gles2_clear_cache_for_program (program);
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
  
  if (handle != COGL_INVALID_HANDLE && !cogl_is_program (handle))
    return;

  ctx->gles2.settings.user_program = handle;
  ctx->gles2.settings_dirty = TRUE;
}

COGLint
cogl_program_get_uniform_location (CoglHandle   handle,
                                   const gchar *uniform_name)
{
  int i;
  CoglProgram *program;

  if (!cogl_is_program (handle))
    return -1;

  program = _cogl_program_pointer_from_handle (handle);

  /* We can't just ask the GL program object for the uniform location
     directly because it will change every time the program is linked
     with a new fixed functionality shader. Instead we make our own
     mapping of uniform numbers and cache the names */
  for (i = 0; program->custom_uniform_names[i]
	 && i < COGL_GLES2_NUM_CUSTOM_UNIFORMS; i++)
    if (!strcmp (program->custom_uniform_names[i], uniform_name))
      return i;

  if (i < COGL_GLES2_NUM_CUSTOM_UNIFORMS)
    {
      program->custom_uniform_names[i] = g_strdup (uniform_name);
      return i;
    }
  else
    /* We've run out of space for new uniform names so just pretend it
       isn't there */
    return -1;
}

void
cogl_program_uniform_1f (COGLint uniform_no,
                         gfloat  value)
{
  cogl_program_uniform_float (uniform_no, 1, 1, &value);
}

void
cogl_program_uniform_1i (COGLint uniform_no,
                         gint    value)
{
  cogl_program_uniform_int (uniform_no, 1, 1, &value);
}

static void
cogl_program_uniform_x (COGLint uniform_no,
			gint size,
			gint count,
			CoglBoxedType type,
			size_t value_size,
			gconstpointer value)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (uniform_no >= 0 && uniform_no < COGL_GLES2_NUM_CUSTOM_UNIFORMS
      && size >= 1 && size <= 4 && count >= 1)
    {
      CoglBoxedValue *bv = ctx->gles2.custom_uniforms + uniform_no;

      if (count == 1)
	{
	  if (bv->count > 1)
	    g_free (bv->v.array);

	  memcpy (bv->v.float_value, value, value_size);
	}
      else
	{
	  if (bv->count > 1)
	    {
	      if (bv->count != count || bv->size != size || bv->type != type)
		{
		  g_free (bv->v.array);
		  bv->v.array = g_malloc (count * value_size);
		}
	    }
	  else
	    bv->v.array = g_malloc (count * value_size);

	  memcpy (bv->v.array, value, count * value_size);
	}

      bv->type = type;
      bv->size = size;
      bv->count = count;

      ctx->gles2.dirty_custom_uniforms |= 1 << uniform_no;
    }
}

void
cogl_program_uniform_float (COGLint  uniform_no,
                            gint     size,
                            gint     count,
                            const GLfloat *value)
{
  cogl_program_uniform_x (uniform_no, size, count, COGL_BOXED_FLOAT,
			  sizeof (float) * size, value);
}

void
cogl_program_uniform_int (COGLint  uniform_no,
			  gint   size,
			  gint   count,
			  const GLint *value)
{
  cogl_program_uniform_x (uniform_no, size, count, COGL_BOXED_INT,
			  sizeof (gint) * size, value);
}

void
cogl_program_uniform_matrix (COGLint   uniform_no,
                             gint      size,
                             gint      count,
                             gboolean  transpose,
                             const GLfloat  *value)
{
  CoglBoxedValue *bv;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  bv = ctx->gles2.custom_uniforms + uniform_no;

  cogl_program_uniform_x (uniform_no, size, count, COGL_BOXED_MATRIX,
			  sizeof (float) * size * size, value);

  bv->transpose = transpose;
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

void
cogl_program_uniform_float (COGLint  uniform_no,
                            gint     size,
                            gint     count,
                            const GLfloat *value)
{
}

void
cogl_program_uniform_int (COGLint  uniform_no,
                          gint     size,
                          gint     count,
                          const COGLint *value)
{
}

void
cogl_program_uniform_matrix (COGLint   uniform_no,
                             gint      size,
                             gint      count,
                             gboolean  transpose,
                             const GLfloat  *value)
{
}


#endif /* HAVE_COGL_GLES2 */
