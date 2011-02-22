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
    void *array;
  } v;
} CoglBoxedValue;

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

#define COGL_ENABLE_ALPHA_TEST        (1<<1)
#define COGL_ENABLE_VERTEX_ARRAY      (1<<2)
#define COGL_ENABLE_COLOR_ARRAY       (1<<3)
#define COGL_ENABLE_BACKFACE_CULLING  (1<<4)

int
_cogl_get_format_bpp (CoglPixelFormat format);

void
_cogl_enable (unsigned long flags);

unsigned long
_cogl_get_enable (void);

void
_cogl_flush_face_winding (void);

void
_cogl_transform_point (const CoglMatrix *matrix_mv,
                       const CoglMatrix *matrix_p,
                       const float *viewport,
                       float *x,
                       float *y);

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
 * CoglXlibFilterFunc:
 *
 * A callback function that can be registered with
 * _cogl_xlib_add_filter. The function should return
 * %COGL_XLIB_FILTER_REMOVE if it wants to prevent further processing
 * or %COGL_XLIB_FILTER_CONTINUE otherwise.
 */
typedef CoglXlibFilterReturn (* CoglXlibFilterFunc) (XEvent *xevent,
                                                     gpointer data);

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

/*
 * _cogl_xlib_get_display:
 *
 * Return value: the Xlib display that will be used by the Xlib winsys
 * backend. The display needs to be set with _cogl_xlib_set_display()
 * before this function is called.
 */
Display *
_cogl_xlib_get_display (void);

/*
 * cogl_xlib_set_display:
 *
 * Sets the Xlib display that Cogl will use for the Xlib winsys
 * backend. This function should eventually go away when Cogl gains a
 * more complete winsys abstraction.
 */
void
_cogl_xlib_set_display (Display *display);

/*
 * _cogl_xlib_add_filter:
 *
 * Adds a callback function that will receive all X11 events. The
 * function can stop further processing of the event by return
 * %COGL_XLIB_FILTER_REMOVE.
 */
void
_cogl_xlib_add_filter (CoglXlibFilterFunc func,
                       gpointer data);

/*
 * _cogl_xlib_remove_filter:
 *
 * Removes a callback that was previously added with
 * _cogl_xlib_add_filter().
 */
void
_cogl_xlib_remove_filter (CoglXlibFilterFunc func,
                          gpointer data);

#endif /* COGL_HAS_XLIB_SUPPORT */

typedef enum _CoglFeatureFlagsPrivate
{
  COGL_FEATURE_PRIVATE_PLACE_HOLDER = (1 << 0)
} CoglFeatureFlagsPrivate;

gboolean
_cogl_features_available_private (CoglFeatureFlagsPrivate features);

#endif /* __COGL_INTERNAL_H */
