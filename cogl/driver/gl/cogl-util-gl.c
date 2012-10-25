/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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
#include "cogl-internal.h"
#include "cogl-util-gl-private.h"

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
