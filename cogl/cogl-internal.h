/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#ifndef __COGL_INTERNAL_H
#define __COGL_INTERNAL_H

#include "cogl.h"
#include "cogl-matrix-stack.h"
#include "cogl-bitmask.h"

#ifdef COGL_HAS_XLIB_SUPPORT
#include <X11/Xlib.h>
#endif

typedef enum
{
  COGL_FRONT_WINDING_CLOCKWISE,
  COGL_FRONT_WINDING_COUNTER_CLOCKWISE
} CoglFrontWinding;

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
    float float_value[4];
    int int_value[4];
    float matrix[16];
    float *float_array;
    int *int_array;
    gpointer array;
  } v;
} CoglBoxedValue;
#endif

#ifdef COGL_GL_DEBUG

const char *
cogl_gl_error_to_string (GLenum error_code);

#define GE(x)                           G_STMT_START {  \
  GLenum __err;                                         \
  (x);                                                  \
  while ((__err = glGetError ()) != GL_NO_ERROR)        \
    {                                                   \
      g_warning ("%s: GL error (%d): %s\n",             \
                 G_STRLOC,                              \
                 __err,                                 \
                 cogl_gl_error_to_string (__err));      \
    }                                   } G_STMT_END

#define GE_RET(ret, x)                  G_STMT_START {  \
  GLenum __err;                                         \
  ret = (x);                                            \
  while ((__err = glGetError ()) != GL_NO_ERROR)        \
    {                                                   \
      g_warning ("%s: GL error (%d): %s\n",             \
                 G_STRLOC,                              \
                 __err,                                 \
                 cogl_gl_error_to_string (__err));      \
    }                                   } G_STMT_END

#else /* !COGL_GL_DEBUG */

#define GE(x) (x)
#define GE_RET(ret, x)  (ret = (x))

#endif /* COGL_GL_DEBUG */

#define COGL_ENABLE_BLEND             (1<<1)
#define COGL_ENABLE_ALPHA_TEST        (1<<2)
#define COGL_ENABLE_VERTEX_ARRAY      (1<<3)
#define COGL_ENABLE_COLOR_ARRAY       (1<<4)
#define COGL_ENABLE_BACKFACE_CULLING  (1<<5)

void
_cogl_features_init (void);

int
_cogl_get_format_bpp (CoglPixelFormat format);

void
_cogl_enable (unsigned long flags);

unsigned long
_cogl_get_enable (void);

typedef struct _CoglTextureUnit
{
  int              index;
  CoglMatrixStack *matrix_stack;
} CoglTextureUnit;

CoglTextureUnit *
_cogl_get_texture_unit (int index_);

void
_cogl_destroy_texture_units (void);

unsigned int
_cogl_get_max_texture_image_units (void);

void
_cogl_flush_face_winding (void);

/* Disables the texcoord arrays that don't have a corresponding bit
   set in the mask */
void
_cogl_disable_other_texcoord_arrays (const CoglBitmask *mask);

#ifdef COGL_HAS_XLIB_SUPPORT

/*
 * CoglX11FilterReturn:
 * @COGL_XLIB_FILTER_CONTINUE: The event was not handled, continues the
 *                            processing
 * @COGL_XLIB_FILTER_REMOVE: Remove the event, stops the processing
 *
 * Return values for the #CoglX11FilterFunc function.
 */
typedef enum _CoglXlibFilterReturn {
  COGL_XLIB_FILTER_CONTINUE,
  COGL_XLIB_FILTER_REMOVE
} CoglXlibFilterReturn;

/*
 * cogl_xlib_handle_event:
 * @xevent: pointer to XEvent structure
 *
 * This function processes a single X event; it can be used to hook
 * into external X event retrieval (for example that done by Clutter
 * or GDK).
 *
 * Return value: #CoglXlibFilterReturn. %COGL_XLIB_FILTER_REMOVE
 * indicates that Cogl has internally handled the event and the
 * caller should do no further processing. %COGL_XLIB_FILTER_CONTINUE
 * indicates that Cogl is either not interested in the event,
 * or has used the event to update internal state without taking
 * any exclusive action.
 */
CoglXlibFilterReturn
_cogl_xlib_handle_event (XEvent *xevent);

#endif /* COGL_HAS_XLIB_SUPPORT */

#endif /* __COGL_INTERNAL_H */
