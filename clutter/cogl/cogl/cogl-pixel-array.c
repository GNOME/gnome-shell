/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *
 * Authors:
 *   Damien Lespiau <damien.lespiau@intel.com>
 *   Robert Bragg <robert@linux.intel.com>
 */

/* For an overview of the functionality implemented here, please see
 * cogl-pixel-array.h, which contains the gtk-doc section overview for the
 * Pixel Buffers API.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-context.h"
#include "cogl-object.h"
#include "cogl-pixel-array-private.h"
#include "cogl-pixel-array.h"

/*
 * GL/GLES compatibility defines for the buffer API:
 */

#if defined (HAVE_COGL_GL)

#define glGenBuffers ctx->drv.pf_glGenBuffers
#define glBindBuffer ctx->drv.pf_glBindBuffer
#define glBufferData ctx->drv.pf_glBufferData
#define glBufferSubData ctx->drv.pf_glBufferSubData
#define glGetBufferSubData ctx->drv.pf_glGetBufferSubData
#define glDeleteBuffers ctx->drv.pf_glDeleteBuffers
#define glMapBuffer ctx->drv.pf_glMapBuffer
#define glUnmapBuffer ctx->drv.pf_glUnmapBuffer

#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER GL_PIXEL_UNPACK_BUFFER_ARB
#endif

#ifndef GL_PIXEL_PACK_BUFFER
#define GL_PIXEL_PACK_BUFFER GL_PIXEL_PACK_BUFFER_ARB
#endif

#elif defined (HAVE_COGL_GLES2)

#include "../gles/cogl-gles2-wrapper.h"

#endif

static void
_cogl_pixel_array_free (CoglPixelArray *buffer);

#if !defined (COGL_HAS_GLES)
static const CoglBufferVtable
cogl_pixel_array_vtable;
#endif
static const CoglBufferVtable
cogl_malloc_pixel_array_vtable;

COGL_BUFFER_DEFINE (PixelArray, pixel_array)

CoglPixelArray *
cogl_pixel_array_new (unsigned int size)
{
  CoglPixelArray *pixel_array = g_slice_new0 (CoglPixelArray);
  CoglBuffer *buffer = COGL_BUFFER (pixel_array);

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  /* parent's constructor */
  _cogl_buffer_initialize (buffer,
                           size,
                           COGL_BUFFER_USAGE_HINT_TEXTURE,
                           COGL_BUFFER_UPDATE_HINT_STATIC);

/* malloc version only for GLES */
#if !defined (COGL_HAS_GLES)
  if (cogl_features_available (COGL_FEATURE_PBOS))
    {
      /* PBOS */
      buffer->vtable = &cogl_pixel_array_vtable;

      GE( glGenBuffers (1, &buffer->gl_handle) );
      COGL_BUFFER_SET_FLAG (buffer, BUFFER_OBJECT);
    }
  else
#endif
    {
      /* malloc fallback subclass */
      buffer->vtable = &cogl_malloc_pixel_array_vtable;

      /* create the buffer here as there's no point for a lazy allocation in
       * the malloc case */
      buffer->data = g_malloc (size);
    }

  pixel_array->flags = COGL_PIXEL_ARRAY_FLAG_NONE;

  /* return COGL_INVALID_HANDLE; */
  return _cogl_pixel_array_object_new (pixel_array);
}

CoglPixelArray *
cogl_pixel_array_new_for_size (unsigned int    width,
                               unsigned int    height,
                               CoglPixelFormat format,
                               unsigned int   *rowstride)
{
  CoglPixelArray *buffer;
  CoglPixelArray *pixel_array;
  unsigned int stride;

  /* creating a buffer to store "any" format does not make sense */
  if (G_UNLIKELY (format == COGL_PIXEL_FORMAT_ANY))
    return COGL_INVALID_HANDLE;

  /* for now we fallback to cogl_pixel_array_new, later, we could ask
   * libdrm a tiled buffer for instance */
  stride = width * _cogl_get_format_bpp (format);
  if (rowstride)
    *rowstride = stride;

  buffer = cogl_pixel_array_new (height * stride);
  if (G_UNLIKELY (buffer == COGL_INVALID_HANDLE))
    return COGL_INVALID_HANDLE;

  pixel_array = COGL_PIXEL_ARRAY (buffer);
  pixel_array->width = width;
  pixel_array->height = height;
  pixel_array->format = format;
  pixel_array->stride = stride;

  return buffer;
}

static void
_cogl_pixel_array_free (CoglPixelArray *buffer)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* parent's destructor */
  _cogl_buffer_fini (COGL_BUFFER (buffer));

  GE( glDeleteBuffers (1, &(COGL_BUFFER (buffer)->gl_handle)) );

  g_slice_free (CoglPixelArray, buffer);
}

#if !defined (COGL_HAS_GLES)
static guint8 *
_cogl_pixel_array_map (CoglBuffer       *buffer,
                       CoglBufferAccess  access)
{
  CoglPixelArray *pixel_array = COGL_PIXEL_ARRAY (buffer);
  GLenum gl_target;
  guint8 *data;

  _COGL_GET_CONTEXT (ctx, NULL);

  /* we determine the target lazily, on the first map */
  gl_target = GL_PIXEL_UNPACK_BUFFER;
  pixel_array->gl_target = gl_target;

  _cogl_buffer_bind (buffer, gl_target);

  /* create an empty store if we don't have one yet. creating the store
   * lazily allows the user of the CoglBuffer to set a hint before the
   * store is created. */
  if (!COGL_PIXEL_ARRAY_FLAG_IS_SET (pixel_array, STORE_CREATED))
    {
      GE( glBufferData (gl_target,
                        buffer->size,
                        NULL,
                        _cogl_buffer_hints_to_gl_enum (buffer->usage_hint,
                                                       buffer->update_hint)) );
      COGL_PIXEL_ARRAY_SET_FLAG (pixel_array, STORE_CREATED);
    }

  GE_RET( data, glMapBuffer (gl_target,
                             _cogl_buffer_access_to_gl_enum (access)) );
  if (data)
    COGL_BUFFER_SET_FLAG (buffer, MAPPED);

  _cogl_buffer_bind (NULL, gl_target);

  return data;
}

static void
_cogl_pixel_array_unmap (CoglBuffer *buffer)
{
  CoglPixelArray *pixel_array = COGL_PIXEL_ARRAY (buffer);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_buffer_bind (buffer, pixel_array->gl_target);

  GE( glUnmapBuffer (pixel_array->gl_target) );
  COGL_BUFFER_CLEAR_FLAG (buffer, MAPPED);

  _cogl_buffer_bind (NULL, pixel_array->gl_target);
}

static gboolean
_cogl_pixel_array_set_data (CoglBuffer   *buffer,
                            unsigned int  offset,
                            const guint8 *data,
                            unsigned int  size)
{
  CoglPixelArray *pixel_array = COGL_PIXEL_ARRAY (buffer);

  _COGL_GET_CONTEXT (ctx, FALSE);

  pixel_array->gl_target = GL_PIXEL_UNPACK_BUFFER;

  _cogl_buffer_bind (buffer, pixel_array->gl_target);

  /* create an empty store if we don't have one yet. creating the store
   * lazily allows the user of the CoglBuffer to set a hint before the
   * store is created. */
  if (!COGL_PIXEL_ARRAY_FLAG_IS_SET (pixel_array, STORE_CREATED))
    {
      GE( glBufferData (pixel_array->gl_target,
                        buffer->size,
                        NULL,
                        _cogl_buffer_hints_to_gl_enum (buffer->usage_hint,
                                                       buffer->update_hint)) );
      COGL_PIXEL_ARRAY_SET_FLAG (pixel_array, STORE_CREATED);
    }

  GE( glBufferSubData (pixel_array->gl_target, offset, size, data) );

  _cogl_buffer_bind (NULL, pixel_array->gl_target);

  return TRUE;
}

#if 0
gboolean
cogl_pixel_array_set_region (CoglPixelArray *buffer,
                             guint8          *data,
                             unsigned int     src_width,
                             unsigned int     src_height,
                             unsigned int     src_rowstride,
                             unsigned int     dst_x,
                             unsigned int     dst_y)
{
  if (!cogl_is_pixel_array (buffer))
    return FALSE;

  return TRUE;
}
#endif

static const CoglBufferVtable cogl_pixel_array_vtable =
{
  _cogl_pixel_array_map,
  _cogl_pixel_array_unmap,
  _cogl_pixel_array_set_data,
};
#endif

/*
 * Fallback path, buffer->data points to a malloc'ed buffer.
 */

static guint8 *
_cogl_malloc_pixel_array_map (CoglBuffer       *buffer,
                              CoglBufferAccess  access)
{
  COGL_BUFFER_SET_FLAG (buffer, MAPPED);
  return buffer->data;
}

static void
_cogl_malloc_pixel_array_unmap (CoglBuffer *buffer)
{
  COGL_BUFFER_CLEAR_FLAG (buffer, MAPPED);
}

static gboolean
_cogl_malloc_pixel_array_set_data (CoglBuffer   *buffer,
                                   unsigned int  offset,
                                   const guint8 *data,
                                   unsigned int  size)
{
  memcpy (buffer->data + offset, data, size);
  return TRUE;
}

static const CoglBufferVtable cogl_malloc_pixel_array_vtable =
{
  _cogl_malloc_pixel_array_map,
  _cogl_malloc_pixel_array_unmap,
  _cogl_malloc_pixel_array_set_data,
};
