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

#ifndef _COGL_UTIL_GL_PRIVATE_H_

#include "cogl-types.h"
#include "cogl-context.h"
#include "cogl-gl-header.h"

#ifdef COGL_GL_DEBUG

const char *
_cogl_gl_error_to_string (GLenum error_code);

#define GE(ctx, x)                      G_STMT_START {  \
  GLenum __err;                                         \
  (ctx)->x;                                             \
  while ((__err = (ctx)->glGetError ()) != GL_NO_ERROR) \
    {                                                   \
      g_warning ("%s: GL error (%d): %s\n",             \
                 G_STRLOC,                              \
                 __err,                                 \
                 _cogl_gl_error_to_string (__err));     \
    }                                   } G_STMT_END

#define GE_RET(ret, ctx, x)             G_STMT_START {  \
  GLenum __err;                                         \
  ret = (ctx)->x;                                       \
  while ((__err = (ctx)->glGetError ()) != GL_NO_ERROR) \
    {                                                   \
      g_warning ("%s: GL error (%d): %s\n",             \
                 G_STRLOC,                              \
                 __err,                                 \
                 _cogl_gl_error_to_string (__err));     \
    }                                   } G_STMT_END

#else /* !COGL_GL_DEBUG */

#define GE(ctx, x) ((ctx)->x)
#define GE_RET(ret, ctx, x) (ret = ((ctx)->x))

#endif /* COGL_GL_DEBUG */

CoglBool
_cogl_gl_util_catch_out_of_memory (CoglContext *ctx, CoglError **error);

#endif /* _COGL_UTIL_GL_PRIVATE_H_ */
