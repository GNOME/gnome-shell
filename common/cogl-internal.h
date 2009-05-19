/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __COGL_INTERNAL_H
#define __COGL_INTERNAL_H

#include "cogl-debug.h"

#ifdef HAVE_COGL_GLES2
typedef enum {
  COGL_BOXED_NONE,
  COGL_BOXED_INT,
  COGL_BOXED_FLOAT,
  COGL_BOXED_MATRIX
} CoglBoxedType;

typedef struct _CoglBoxedValue
{
  CoglBoxedType type;
  int size, count;
  gboolean transpose;

  union {
    gfloat float_value[4];
    gint int_value[4];
    gfloat matrix[16];
    gfloat *float_array;
    gint *int_array;
    gpointer array;
  } v;
} CoglBoxedValue;
#endif

#ifdef COGL_GL_DEBUG

const gchar *cogl_gl_error_to_string (GLenum error_code);

#define GE(x...)                        G_STMT_START {  \
  GLenum __err;                                         \
  (x);                                                  \
  while ((__err = glGetError ()) != GL_NO_ERROR)        \
    {                                                   \
      g_warning ("%s: GL error (%d): %s\n",             \
                 G_STRLOC,                              \
                 cogl_gl_error_to_string (__err));      \
    }                                   } G_STMT_END

#else /* !COGL_GL_DEBUG */

#define GE(x) (x)

#endif /* COGL_GL_DEBUG */

#define COGL_ENABLE_BLEND             (1<<1)
#define COGL_ENABLE_ALPHA_TEST        (1<<2)
#define COGL_ENABLE_VERTEX_ARRAY      (1<<3)
#define COGL_ENABLE_COLOR_ARRAY       (1<<4)
#define COGL_ENABLE_BACKFACE_CULLING  (1<<5)

void    _cogl_features_init (void);
gint    _cogl_get_format_bpp (CoglPixelFormat format);

void    cogl_enable (gulong flags);
gulong  cogl_get_enable (void);

#endif /* __COGL_INTERNAL_H */
