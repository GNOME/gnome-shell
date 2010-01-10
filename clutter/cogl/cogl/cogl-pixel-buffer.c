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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Damien Lespiau <damien.lespiau@intel.com>
 */

/* For an overview of the functionality implemented here, please see
 * cogl-pixel-buffer.h, which contains the gtk-doc section overview for the
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
#include "cogl-handle.h"
#include "cogl-pixel-buffer-private.h"
#include "cogl-pixel-buffer.h"

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
#define glActiveTexture ctx->drv.pf_glActiveTexture
#define glClientActiveTexture ctx->drv.pf_glClientActiveTexture

#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER GL_PIXEL_UNPACK_BUFFER_ARB
#endif

#ifndef GL_PIXEL_PACK_BUFFER
#define GL_PIXEL_PACK_BUFFER GL_PIXEL_PACK_BUFFER_ARB
#endif

#elif defined (HAVE_COGL_GLES2)

#include "../gles/cogl-gles2-wrapper.h"

#endif

static void _cogl_pixel_buffer_free (CoglPixelBuffer *buffer);

static const CoglBufferVtable cogl_pixel_buffer_vtable;

/* we don't want to use the stock COGL_HANDLE_DEFINE * for 2 reasons:
 *   - it defines already deprecated symbols
 *   - we want to suffix the public symbols by _EXP */

#define COGL_HANDLE_DEFINE_EXP(TypeName, type_name)             \
								\
static CoglHandleClass _cogl_##type_name##_class;               \
								\
GQuark                                                          \
_cogl_handle_##type_name##_get_type (void)                      \
{                                                               \
  static GQuark type = 0;                                       \
  if (!type)                                                    \
    type = g_quark_from_static_string ("Cogl"#TypeName);        \
  return type;                                                  \
}                                                               \
								\
static CoglHandle						\
_cogl_##type_name##_handle_new (Cogl##TypeName *new_obj)	\
{				                                \
  CoglHandleObject *obj = (CoglHandleObject *)&new_obj->_parent;\
  obj->ref_count = 1;                                           \
								\
  obj->klass = &_cogl_##type_name##_class;                      \
  if (!obj->klass->type)                                        \
    {                                                           \
      obj->klass->type = _cogl_handle_##type_name##_get_type ();\
      obj->klass->virt_free = _cogl_##type_name##_free;         \
    }                                                           \
								\
  _COGL_HANDLE_DEBUG_NEW (TypeName, obj);                       \
  return (CoglHandle) new_obj;			                \
}								\
								\
Cogl##TypeName *						\
_cogl_##type_name##_pointer_from_handle (CoglHandle handle)	\
{								\
  return (Cogl##TypeName *) handle;				\
}								\
								\
gboolean							\
cogl_is_##type_name##_EXP (CoglHandle handle)		        \
{                                                               \
  CoglHandleObject *obj = (CoglHandleObject *)handle;           \
                                                                \
  if (handle == COGL_INVALID_HANDLE)                            \
    return FALSE;                                               \
                                                                \
  return (obj->klass->type ==                                   \
          _cogl_handle_##type_name##_get_type ());              \
}

COGL_HANDLE_DEFINE_EXP(PixelBuffer, pixel_buffer)

CoglHandle
cogl_pixel_buffer_new_EXP (guint size)
{
  CoglPixelBuffer *pixel_buffer = g_slice_new0 (CoglPixelBuffer);
  CoglBuffer *buffer = COGL_BUFFER (pixel_buffer);

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  /* parent's constructor */
  _cogl_buffer_initialize (buffer,
                           size,
                           COGL_BUFFER_USAGE_HINT_DRAW,
                           COGL_BUFFER_UPDATE_HINT_STATIC);
  buffer->vtable = &cogl_pixel_buffer_vtable;

  GE( glGenBuffers (1, &buffer->gl_handle) );
  COGL_BUFFER_SET_FLAG (buffer, BUFFER_OBJECT);

  pixel_buffer->flags = COGL_PIXEL_BUFFER_FLAG_NONE;

  /* return COGL_INVALID_HANDLE; */
  return _cogl_pixel_buffer_handle_new (pixel_buffer);
}

CoglHandle
cogl_pixel_buffer_new_for_size_EXP (guint            width,
                                    guint            height,
                                    CoglPixelFormat  format,
                                    guint           *rowstride)
{
  CoglHandle buffer;
  CoglPixelBuffer *pixel_buffer;
  guint stride;

  /* creating a buffer to store "any" format does not make sense */
  if (G_UNLIKELY (format == COGL_PIXEL_FORMAT_ANY))
    return COGL_INVALID_HANDLE;

  /* for now we fallback to cogl_pixel_buffer_new_EXP, later, we could ask
   * libdrm a tiled buffer for instance */
  stride = width * _cogl_get_format_bpp (format);
  if (rowstride)
    *rowstride = stride;

  buffer = cogl_pixel_buffer_new_EXP (height * stride);
  if (G_UNLIKELY (buffer == COGL_INVALID_HANDLE))
    return COGL_INVALID_HANDLE;

  pixel_buffer = COGL_PIXEL_BUFFER (buffer);
  pixel_buffer->width = width;
  pixel_buffer->height = height;
  pixel_buffer->format = format;
  pixel_buffer->stride = stride;

  return buffer;
}

static void
_cogl_pixel_buffer_free (CoglPixelBuffer *buffer)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* parent's destructor */
  _cogl_buffer_fini (COGL_BUFFER (buffer));

  GE( glDeleteBuffers (1, &(COGL_BUFFER (buffer)->gl_handle)) );

  g_slice_free (CoglPixelBuffer, buffer);
}

static guchar *
_cogl_pixel_buffer_map (CoglBuffer       *buffer,
                        CoglBufferAccess  access)
{
  CoglPixelBuffer *pixel_buffer = COGL_PIXEL_BUFFER (buffer);
  GLenum gl_target;
  guchar *data;

  _COGL_GET_CONTEXT (ctx, NULL);

  /* we determine the target lazily, on the first map */
  gl_target = GL_PIXEL_UNPACK_BUFFER;
  pixel_buffer->gl_target = gl_target;

  _cogl_buffer_bind (buffer, gl_target);

  /* create an empty store if we don't have one yet. creating the store
   * lazily allows the user of the CoglBuffer to set a hint before the
   * store is created. */
  if (!COGL_PIXEL_BUFFER_FLAG_IS_SET (pixel_buffer, STORE_CREATED))
    {
      GE( glBufferData (gl_target,
                        buffer->size,
                        NULL,
                        _cogl_buffer_hints_to_gl_enum (buffer->usage_hint,
                                                       buffer->update_hint)) );
      COGL_PIXEL_BUFFER_SET_FLAG (pixel_buffer, STORE_CREATED);
    }

  GE_RET( data, glMapBuffer (gl_target,
                             _cogl_buffer_access_to_gl_enum (access)) );
  if (data)
    COGL_BUFFER_SET_FLAG (buffer, MAPPED);

  _cogl_buffer_bind (NULL, gl_target);

  return data;
}

static void
_cogl_pixel_buffer_unmap (CoglBuffer *buffer)
{
  CoglPixelBuffer *pixel_buffer = COGL_PIXEL_BUFFER (buffer);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_buffer_bind (buffer, pixel_buffer->gl_target);

  GE( glUnmapBuffer (pixel_buffer->gl_target) );
  COGL_BUFFER_CLEAR_FLAG (buffer, MAPPED);

  _cogl_buffer_bind (NULL, pixel_buffer->gl_target);
}

static gboolean
_cogl_pixel_buffer_set_data (CoglBuffer   *buffer,
                             guint         offset,
                             const guchar *data,
                             guint         size)
{
  CoglPixelBuffer *pixel_buffer = COGL_PIXEL_BUFFER (buffer);

  _COGL_GET_CONTEXT (ctx, FALSE);

  pixel_buffer->gl_target = GL_PIXEL_UNPACK_BUFFER;

  _cogl_buffer_bind (buffer, pixel_buffer->gl_target);

  /* create an empty store if we don't have one yet. creating the store
   * lazily allows the user of the CoglBuffer to set a hint before the
   * store is created. */
  if (!COGL_PIXEL_BUFFER_FLAG_IS_SET (pixel_buffer, STORE_CREATED))
    {
      GE( glBufferData (pixel_buffer->gl_target,
                        buffer->size,
                        NULL,
                        _cogl_buffer_hints_to_gl_enum (buffer->usage_hint,
                                                       buffer->update_hint)) );
      COGL_PIXEL_BUFFER_SET_FLAG (pixel_buffer, STORE_CREATED);
    }

  GE( glBufferSubData (pixel_buffer->gl_target, offset, size, data) );

  _cogl_buffer_bind (NULL, pixel_buffer->gl_target);

  return TRUE;
}

#if 0
gboolean
cogl_pixel_buffer_set_region_EXP (CoglHandle  buffer,
                                  guchar     *data,
                                  guint       src_width,
                                  guint       src_height,
                                  guint       src_rowstride,
                                  guint       dst_x,
                                  guint       dst_y)
{
  if (!cogl_is_pixel_buffer (buffer))
    return FALSE;

  return TRUE;
}
#endif

static const CoglBufferVtable cogl_pixel_buffer_vtable =
{
  _cogl_pixel_buffer_map,
  _cogl_pixel_buffer_unmap,
  _cogl_pixel_buffer_set_data,
};
