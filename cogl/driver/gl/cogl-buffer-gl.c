/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010,2011,2012 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-buffer-gl-private.h"

/*
 * GL/GLES compatibility defines for the buffer API:
 */

#ifndef GL_PIXEL_PACK_BUFFER
#define GL_PIXEL_PACK_BUFFER 0x88EB
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_READ_ONLY
#define GL_READ_ONLY 0x88B8
#endif
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif
#ifndef GL_READ_WRITE
#define GL_READ_WRITE 0x88BA
#endif


void
_cogl_buffer_gl_create (CoglBuffer *buffer)
{
  CoglContext *ctx = buffer->context;

  GE (ctx, glGenBuffers (1, &buffer->gl_handle));
}

void
_cogl_buffer_gl_destroy (CoglBuffer *buffer)
{
  GE( buffer->context, glDeleteBuffers (1, &buffer->gl_handle) );
}

static GLenum
update_hints_to_gl_enum (CoglBuffer *buffer)
{
  /* usage hint is always DRAW for now */
  switch (buffer->update_hint)
    {
    case COGL_BUFFER_UPDATE_HINT_STATIC:
      return GL_STATIC_DRAW;
    case COGL_BUFFER_UPDATE_HINT_DYNAMIC:
      return GL_DYNAMIC_DRAW;

    case COGL_BUFFER_UPDATE_HINT_STREAM:
      /* OpenGL ES 1.1 only knows about STATIC_DRAW and DYNAMIC_DRAW */
#if defined(HAVE_COGL_GL) || defined(HAVE_COGL_GLES2)
      if (buffer->context->driver != COGL_DRIVER_GLES1)
        return GL_STREAM_DRAW;
#else
      return GL_DYNAMIC_DRAW;
#endif
    }

  g_assert_not_reached ();
}

static GLenum
convert_bind_target_to_gl_target (CoglBufferBindTarget target)
{
  switch (target)
    {
      case COGL_BUFFER_BIND_TARGET_PIXEL_PACK:
        return GL_PIXEL_PACK_BUFFER;
      case COGL_BUFFER_BIND_TARGET_PIXEL_UNPACK:
        return GL_PIXEL_UNPACK_BUFFER;
      case COGL_BUFFER_BIND_TARGET_ATTRIBUTE_BUFFER:
        return GL_ARRAY_BUFFER;
      case COGL_BUFFER_BIND_TARGET_INDEX_BUFFER:
        return GL_ELEMENT_ARRAY_BUFFER;
      default:
        g_return_val_if_reached (COGL_BUFFER_BIND_TARGET_PIXEL_UNPACK);
    }
}

static void
recreate_store (CoglBuffer *buffer)
{
  GLenum gl_target;
  GLenum gl_enum;

  /* This assumes the buffer is already bound */

  gl_target = convert_bind_target_to_gl_target (buffer->last_target);
  gl_enum = update_hints_to_gl_enum (buffer);

  GE( buffer->context, glBufferData (gl_target,
                                     buffer->size,
                                     NULL,
                                     gl_enum) );
  buffer->store_created = TRUE;
}

GLenum
_cogl_buffer_access_to_gl_enum (CoglBufferAccess access)
{
  if ((access & COGL_BUFFER_ACCESS_READ_WRITE) == COGL_BUFFER_ACCESS_READ_WRITE)
    return GL_READ_WRITE;
  else if (access & COGL_BUFFER_ACCESS_WRITE)
    return GL_WRITE_ONLY;
  else
    return GL_READ_ONLY;
}

static void *
_cogl_buffer_bind_no_create (CoglBuffer *buffer,
                             CoglBufferBindTarget target)
{
  CoglContext *ctx = buffer->context;

  _COGL_RETURN_VAL_IF_FAIL (buffer != NULL, NULL);

  /* Don't allow binding the buffer to multiple targets at the same time */
  _COGL_RETURN_VAL_IF_FAIL (ctx->current_buffer[buffer->last_target] != buffer,
                            NULL);

  /* Don't allow nesting binds to the same target */
  _COGL_RETURN_VAL_IF_FAIL (ctx->current_buffer[target] == NULL, NULL);

  buffer->last_target = target;
  ctx->current_buffer[target] = buffer;

  if (buffer->flags & COGL_BUFFER_FLAG_BUFFER_OBJECT)
    {
      GLenum gl_target = convert_bind_target_to_gl_target (buffer->last_target);
      GE( ctx, glBindBuffer (gl_target, buffer->gl_handle) );
      return NULL;
    }
  else
    return buffer->data;
}

void *
_cogl_buffer_gl_map (CoglBuffer *buffer,
                     CoglBufferAccess  access,
                     CoglBufferMapHint hints)
{
  uint8_t *data;
  CoglBufferBindTarget target;
  GLenum gl_target;
  CoglContext *ctx = buffer->context;

  if ((access & COGL_BUFFER_ACCESS_READ) &&
      !cogl_has_feature (ctx, COGL_FEATURE_ID_MAP_BUFFER_FOR_READ))
    return NULL;
  if ((access & COGL_BUFFER_ACCESS_WRITE) &&
      !cogl_has_feature (ctx, COGL_FEATURE_ID_MAP_BUFFER_FOR_WRITE))
    return NULL;

  target = buffer->last_target;
  _cogl_buffer_bind_no_create (buffer, target);

  gl_target = convert_bind_target_to_gl_target (target);

  /* create an empty store if we don't have one yet. creating the store
   * lazily allows the user of the CoglBuffer to set a hint before the
   * store is created. */
  if (!buffer->store_created || (hints & COGL_BUFFER_MAP_HINT_DISCARD))
    recreate_store (buffer);

  GE_RET( data, ctx, glMapBuffer (gl_target,
                                  _cogl_buffer_access_to_gl_enum (access)) );
  if (data)
    buffer->flags |= COGL_BUFFER_FLAG_MAPPED;

  _cogl_buffer_gl_unbind (buffer);

  return data;
}

void
_cogl_buffer_gl_unmap (CoglBuffer *buffer)
{
  CoglContext *ctx = buffer->context;

  _cogl_buffer_bind_no_create (buffer, buffer->last_target);

  GE( ctx, glUnmapBuffer (convert_bind_target_to_gl_target
                          (buffer->last_target)) );
  buffer->flags &= ~COGL_BUFFER_FLAG_MAPPED;

  _cogl_buffer_gl_unbind (buffer);
}

CoglBool
_cogl_buffer_gl_set_data (CoglBuffer *buffer,
                          unsigned int offset,
                          const void *data,
                          unsigned int size)
{
  CoglBufferBindTarget target;
  GLenum gl_target;
  CoglContext *ctx = buffer->context;

  target = buffer->last_target;
  _cogl_buffer_gl_bind (buffer, target);

  gl_target = convert_bind_target_to_gl_target (target);

  GE( ctx, glBufferSubData (gl_target, offset, size, data) );

  _cogl_buffer_gl_unbind (buffer);

  return TRUE;
}

void *
_cogl_buffer_gl_bind (CoglBuffer *buffer, CoglBufferBindTarget target)
{
  void *ret;

  ret = _cogl_buffer_bind_no_create (buffer, target);

  /* create an empty store if we don't have one yet. creating the store
   * lazily allows the user of the CoglBuffer to set a hint before the
   * store is created. */
  if ((buffer->flags & COGL_BUFFER_FLAG_BUFFER_OBJECT) &&
      !buffer->store_created)
    recreate_store (buffer);

  return ret;
}

void
_cogl_buffer_gl_unbind (CoglBuffer *buffer)
{
  CoglContext *ctx = buffer->context;

  _COGL_RETURN_IF_FAIL (buffer != NULL);

  /* the unbind should pair up with a previous bind */
  _COGL_RETURN_IF_FAIL (ctx->current_buffer[buffer->last_target] == buffer);

  if (buffer->flags & COGL_BUFFER_FLAG_BUFFER_OBJECT)
    {
      GLenum gl_target = convert_bind_target_to_gl_target (buffer->last_target);
      GE( ctx, glBindBuffer (gl_target, 0) );
    }

  ctx->current_buffer[buffer->last_target] = NULL;
}
