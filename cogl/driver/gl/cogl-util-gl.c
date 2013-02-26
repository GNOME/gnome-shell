/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012, 2013 Intel Corporation.
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
 * Authors:
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-types.h"
#include "cogl-context-private.h"
#include "cogl-error-private.h"
#include "cogl-util-gl-private.h"

#ifdef COGL_GL_DEBUG
/* GL error to string conversion */
static const struct {
  GLuint error_code;
  const char *error_string;
} gl_errors[] = {
  { GL_NO_ERROR,          "No error" },
  { GL_INVALID_ENUM,      "Invalid enumeration value" },
  { GL_INVALID_VALUE,     "Invalid value" },
  { GL_INVALID_OPERATION, "Invalid operation" },
#ifdef HAVE_COGL_GL
  { GL_STACK_OVERFLOW,    "Stack overflow" },
  { GL_STACK_UNDERFLOW,   "Stack underflow" },
#endif
  { GL_OUT_OF_MEMORY,     "Out of memory" },

#ifdef GL_INVALID_FRAMEBUFFER_OPERATION_EXT
  { GL_INVALID_FRAMEBUFFER_OPERATION_EXT, "Invalid framebuffer operation" }
#endif
};

static const unsigned int n_gl_errors = G_N_ELEMENTS (gl_errors);

const char *
_cogl_gl_error_to_string (GLenum error_code)
{
  int i;

  for (i = 0; i < n_gl_errors; i++)
    {
      if (gl_errors[i].error_code == error_code)
        return gl_errors[i].error_string;
    }

  return "Unknown GL error";
}
#endif /* COGL_GL_DEBUG */

CoglBool
_cogl_gl_util_catch_out_of_memory (CoglContext *ctx, CoglError **error)
{
  GLenum gl_error;
  CoglBool out_of_memory = FALSE;

  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    {
      if (gl_error == GL_OUT_OF_MEMORY)
        out_of_memory = TRUE;
#ifdef COGL_GL_DEBUG
      else
        {
          g_warning ("%s: GL error (%d): %s\n",
                     G_STRLOC,
                     gl_error,
                     _cogl_gl_error_to_string (gl_error));
        }
#endif
    }

  if (out_of_memory)
    {
      _cogl_set_error (error, COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_NO_MEMORY,
                       "Out of memory");
      return TRUE;
    }

  return FALSE;
}

void
_cogl_gl_util_get_texture_target_string (CoglTextureType texture_type,
                                         const char **target_string_out,
                                         const char **swizzle_out)
{
  const char *target_string, *tex_coord_swizzle;

  switch (texture_type)
    {
#if 0 /* TODO */
    case COGL_TEXTURE_TYPE_1D:
      target_string = "1D";
      tex_coord_swizzle = "s";
      break;
#endif

    case COGL_TEXTURE_TYPE_2D:
      target_string = "2D";
      tex_coord_swizzle = "st";
      break;

    case COGL_TEXTURE_TYPE_3D:
      target_string = "3D";
      tex_coord_swizzle = "stp";
      break;

    case COGL_TEXTURE_TYPE_RECTANGLE:
      target_string = "2DRect";
      tex_coord_swizzle = "st";
      break;
    }

  if (target_string_out)
    *target_string_out = target_string;
  if (swizzle_out)
    *swizzle_out = tex_coord_swizzle;
}
