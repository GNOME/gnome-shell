/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010,2011,2012,2013 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
#include "cogl-error-private.h"
#include "cogl-util-gl-private.h"

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
#ifndef GL_MAP_READ_BIT
#define GL_MAP_READ_BIT 0x0001
#endif
#ifndef GL_MAP_WRITE_BIT
#define GL_MAP_WRITE_BIT 0x0002
#endif
#ifndef GL_MAP_INVALIDATE_RANGE_BIT
#define GL_MAP_INVALIDATE_RANGE_BIT 0x0004
#endif
#ifndef GL_MAP_INVALIDATE_BUFFER_BIT
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
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

static CoglBool
recreate_store (CoglBuffer *buffer,
                CoglError **error)
{
  CoglContext *ctx = buffer->context;
  GLenum gl_target;
  GLenum gl_enum;
  GLenum gl_error;

  /* This assumes the buffer is already bound */

  gl_target = convert_bind_target_to_gl_target (buffer->last_target);
  gl_enum = update_hints_to_gl_enum (buffer);

  /* Clear any GL errors */
  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;

  ctx->glBufferData (gl_target,
                     buffer->size,
                     NULL,
                     gl_enum);

  if (_cogl_gl_util_catch_out_of_memory (ctx, error))
    return FALSE;

  buffer->store_created = TRUE;
  return TRUE;
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
_cogl_buffer_gl_map_range (CoglBuffer *buffer,
                           size_t offset,
                           size_t size,
                           CoglBufferAccess access,
                           CoglBufferMapHint hints,
                           CoglError **error)
{
  uint8_t *data;
  CoglBufferBindTarget target;
  GLenum gl_target;
  CoglContext *ctx = buffer->context;
  GLenum gl_error;

  if (((access & COGL_BUFFER_ACCESS_READ) &&
       !cogl_has_feature (ctx, COGL_FEATURE_ID_MAP_BUFFER_FOR_READ)) ||
      ((access & COGL_BUFFER_ACCESS_WRITE) &&
       !cogl_has_feature (ctx, COGL_FEATURE_ID_MAP_BUFFER_FOR_WRITE)))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "Tried to map a buffer with unsupported access mode");
      return NULL;
    }

  target = buffer->last_target;
  _cogl_buffer_bind_no_create (buffer, target);

  gl_target = convert_bind_target_to_gl_target (target);

  if ((hints & COGL_BUFFER_MAP_HINT_DISCARD_RANGE) &&
      offset == 0 && size >= buffer->size)
    hints |= COGL_BUFFER_MAP_HINT_DISCARD;

  /* If the map buffer range extension is supported then we will
   * always use it even if we are mapping the full range because the
   * normal mapping function doesn't support passing the discard
   * hints */
  if (ctx->glMapBufferRange)
    {
      GLbitfield gl_access = 0;
      CoglBool should_recreate_store = !buffer->store_created;

      if ((access & COGL_BUFFER_ACCESS_READ))
        gl_access |= GL_MAP_READ_BIT;
      if ((access & COGL_BUFFER_ACCESS_WRITE))
        gl_access |= GL_MAP_WRITE_BIT;

      if ((hints & COGL_BUFFER_MAP_HINT_DISCARD))
        {
          /* glMapBufferRange generates an error if you pass the
           * discard hint along with asking for read access. However
           * it can make sense to ask for both if write access is also
           * requested so that the application can immediately read
           * back what it just wrote. To work around the restriction
           * in GL we just recreate the buffer storage in that case
           * which is an alternative way to indicate that the buffer
           * contents can be discarded. */
          if ((access & COGL_BUFFER_ACCESS_READ))
            should_recreate_store = TRUE;
          else
            gl_access |= GL_MAP_INVALIDATE_BUFFER_BIT;
        }
      else if ((hints & COGL_BUFFER_MAP_HINT_DISCARD_RANGE) &&
               !(access & COGL_BUFFER_ACCESS_READ))
        gl_access |= GL_MAP_INVALIDATE_RANGE_BIT;

      if (should_recreate_store)
        {
          if (!recreate_store (buffer, error))
            {
              _cogl_buffer_gl_unbind (buffer);
              return NULL;
            }
        }

      /* Clear any GL errors */
      while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
        ;

      data = ctx->glMapBufferRange (gl_target,
                                    offset,
                                    size,
                                    gl_access);

      if (_cogl_gl_util_catch_out_of_memory (ctx, error))
        {
          _cogl_buffer_gl_unbind (buffer);
          return NULL;
        }

      _COGL_RETURN_VAL_IF_FAIL (data != NULL, NULL);
    }
  else
    {
      /* create an empty store if we don't have one yet. creating the store
       * lazily allows the user of the CoglBuffer to set a hint before the
       * store is created. */
      if (!buffer->store_created ||
          (hints & COGL_BUFFER_MAP_HINT_DISCARD))
        {
          if (!recreate_store (buffer, error))
            {
              _cogl_buffer_gl_unbind (buffer);
              return NULL;
            }
        }

      /* Clear any GL errors */
      while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
        ;

      data = ctx->glMapBuffer (gl_target,
                               _cogl_buffer_access_to_gl_enum (access));

      if (_cogl_gl_util_catch_out_of_memory (ctx, error))
        {
          _cogl_buffer_gl_unbind (buffer);
          return NULL;
        }

      _COGL_RETURN_VAL_IF_FAIL (data != NULL, NULL);

      data += offset;
    }

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
                          unsigned int size,
                          CoglError **error)
{
  CoglBufferBindTarget target;
  GLenum gl_target;
  CoglContext *ctx = buffer->context;
  GLenum gl_error;
  CoglBool status = TRUE;
  CoglError *internal_error = NULL;

  target = buffer->last_target;

  _cogl_buffer_gl_bind (buffer, target, &internal_error);

  /* NB: _cogl_buffer_gl_bind() may return NULL in non-error
   * conditions so we have to explicity check internal_error
   * to see if an exception was thrown.
   */
  if (internal_error)
    {
      _cogl_propagate_error (error, internal_error);
      return FALSE;
    }

  gl_target = convert_bind_target_to_gl_target (target);

  /* Clear any GL errors */
  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;

  ctx->glBufferSubData (gl_target, offset, size, data);

  if (_cogl_gl_util_catch_out_of_memory (ctx, error))
    status = FALSE;

  _cogl_buffer_gl_unbind (buffer);

  return status;
}

void *
_cogl_buffer_gl_bind (CoglBuffer *buffer,
                      CoglBufferBindTarget target,
                      CoglError **error)
{
  void *ret;

  ret = _cogl_buffer_bind_no_create (buffer, target);

  /* create an empty store if we don't have one yet. creating the store
   * lazily allows the user of the CoglBuffer to set a hint before the
   * store is created. */
  if ((buffer->flags & COGL_BUFFER_FLAG_BUFFER_OBJECT) &&
      !buffer->store_created)
    {
      if (!recreate_store (buffer, error))
        {
          _cogl_buffer_gl_unbind (buffer);
          return NULL;
        }
    }

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
