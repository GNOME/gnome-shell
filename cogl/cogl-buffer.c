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
 * cogl-buffer.h, which contains the gtk-doc section overview for the
 * Pixel Buffers API.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-pixel-buffer-private.h"

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

/* XXX:
 * The CoglObject macros don't support any form of inheritance, so for
 * now we implement the CoglObject support for the CoglBuffer
 * abstract class manually.
 */

static GSList *_cogl_buffer_types;

void
_cogl_buffer_register_buffer_type (const CoglObjectClass *klass)
{
  _cogl_buffer_types = g_slist_prepend (_cogl_buffer_types, (void *) klass);
}

CoglBool
cogl_is_buffer (void *object)
{
  const CoglObject *obj = object;
  GSList *l;

  if (object == NULL)
    return FALSE;

  for (l = _cogl_buffer_types; l; l = l->next)
    if (l->data == obj->klass)
      return TRUE;

  return FALSE;
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

static GLenum
_cogl_buffer_hints_to_gl_enum (CoglBuffer *buffer)
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

static void
bo_recreate_store (CoglBuffer *buffer)
{
  GLenum gl_target;
  GLenum gl_enum;

  /* This assumes the buffer is already bound */

  gl_target = convert_bind_target_to_gl_target (buffer->last_target);
  gl_enum = _cogl_buffer_hints_to_gl_enum (buffer);

  GE( buffer->context, glBufferData (gl_target,
                                     buffer->size,
                                     NULL,
                                     gl_enum) );
  buffer->store_created = TRUE;
}

static void *
bo_map (CoglBuffer       *buffer,
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
    bo_recreate_store (buffer);

  GE_RET( data, ctx, glMapBuffer (gl_target,
                                  _cogl_buffer_access_to_gl_enum (access)) );
  if (data)
    buffer->flags |= COGL_BUFFER_FLAG_MAPPED;

  _cogl_buffer_unbind (buffer);

  return data;
}

static void
bo_unmap (CoglBuffer *buffer)
{
  CoglContext *ctx = buffer->context;

  _cogl_buffer_bind_no_create (buffer, buffer->last_target);

  GE( ctx, glUnmapBuffer (convert_bind_target_to_gl_target
                          (buffer->last_target)) );
  buffer->flags &= ~COGL_BUFFER_FLAG_MAPPED;

  _cogl_buffer_unbind (buffer);
}

static CoglBool
bo_set_data (CoglBuffer   *buffer,
             unsigned int  offset,
             const void   *data,
             unsigned int  size)
{
  CoglBufferBindTarget target;
  GLenum gl_target;
  CoglContext *ctx = buffer->context;

  target = buffer->last_target;
  _cogl_buffer_bind (buffer, target);

  gl_target = convert_bind_target_to_gl_target (target);

  GE( ctx, glBufferSubData (gl_target, offset, size, data) );

  _cogl_buffer_unbind (buffer);

  return TRUE;
}

/*
 * Fallback path, buffer->data points to a malloc'ed buffer.
 */

static void *
malloc_map (CoglBuffer       *buffer,
            CoglBufferAccess  access,
            CoglBufferMapHint hints)
{
  buffer->flags |= COGL_BUFFER_FLAG_MAPPED;
  return buffer->data;
}

static void
malloc_unmap (CoglBuffer *buffer)
{
  buffer->flags &= ~COGL_BUFFER_FLAG_MAPPED;
}

static CoglBool
malloc_set_data (CoglBuffer   *buffer,
                 unsigned int  offset,
                 const void   *data,
                 unsigned int  size)
{
  memcpy (buffer->data + offset, data, size);
  return TRUE;
}

void
_cogl_buffer_initialize (CoglBuffer           *buffer,
                         CoglContext          *context,
                         unsigned int          size,
                         CoglBool              use_malloc,
                         CoglBufferBindTarget  default_target,
                         CoglBufferUsageHint   usage_hint,
                         CoglBufferUpdateHint  update_hint)
{
  buffer->context       = cogl_object_ref (context);
  buffer->flags         = COGL_BUFFER_FLAG_NONE;
  buffer->store_created = FALSE;
  buffer->size          = size;
  buffer->last_target   = default_target;
  buffer->usage_hint    = usage_hint;
  buffer->update_hint   = update_hint;
  buffer->data          = NULL;
  buffer->immutable_ref = 0;

  if (use_malloc)
    {
      buffer->vtable.map = malloc_map;
      buffer->vtable.unmap = malloc_unmap;
      buffer->vtable.set_data = malloc_set_data;

      buffer->data = g_malloc (size);
    }
  else
    {
      buffer->vtable.map = bo_map;
      buffer->vtable.unmap = bo_unmap;
      buffer->vtable.set_data = bo_set_data;

      GE( context, glGenBuffers (1, &buffer->gl_handle) );
      buffer->flags |= COGL_BUFFER_FLAG_BUFFER_OBJECT;
    }
}

void
_cogl_buffer_fini (CoglBuffer *buffer)
{
  _COGL_RETURN_IF_FAIL (!(buffer->flags & COGL_BUFFER_FLAG_MAPPED));
  _COGL_RETURN_IF_FAIL (buffer->immutable_ref == 0);

  if (buffer->flags & COGL_BUFFER_FLAG_BUFFER_OBJECT)
    GE( buffer->context, glDeleteBuffers (1, &buffer->gl_handle) );
  else
    g_free (buffer->data);

  cogl_object_unref (buffer->context);
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

void *
_cogl_buffer_bind (CoglBuffer *buffer, CoglBufferBindTarget target)
{
  void *ret;

  ret = _cogl_buffer_bind_no_create (buffer, target);

  /* create an empty store if we don't have one yet. creating the store
   * lazily allows the user of the CoglBuffer to set a hint before the
   * store is created. */
  if ((buffer->flags & COGL_BUFFER_FLAG_BUFFER_OBJECT) &&
      !buffer->store_created)
    bo_recreate_store (buffer);

  return ret;
}

void
_cogl_buffer_unbind (CoglBuffer *buffer)
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

unsigned int
cogl_buffer_get_size (CoglBuffer *buffer)
{
  if (!cogl_is_buffer (buffer))
    return 0;

  return COGL_BUFFER (buffer)->size;
}

void
cogl_buffer_set_update_hint (CoglBuffer *buffer,
                             CoglBufferUpdateHint hint)
{
  if (!cogl_is_buffer (buffer))
    return;

  if (G_UNLIKELY (hint > COGL_BUFFER_UPDATE_HINT_STREAM))
    hint = COGL_BUFFER_UPDATE_HINT_STATIC;

  buffer->update_hint = hint;
}

CoglBufferUpdateHint
cogl_buffer_get_update_hint (CoglBuffer *buffer)
{
  if (!cogl_is_buffer (buffer))
    return FALSE;

  return buffer->update_hint;
}

static void
warn_about_midscene_changes (void)
{
  static CoglBool seen = FALSE;
  if (!seen)
    {
      g_warning ("Mid-scene modification of buffers has "
                 "undefined results\n");
      seen = TRUE;
    }
}

void *
cogl_buffer_map (CoglBuffer        *buffer,
                 CoglBufferAccess   access,
                 CoglBufferMapHint  hints)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_buffer (buffer), NULL);

  if (G_UNLIKELY (buffer->immutable_ref))
    warn_about_midscene_changes ();

  if (buffer->flags & COGL_BUFFER_FLAG_MAPPED)
    return buffer->data;

  buffer->data = buffer->vtable.map (buffer, access, hints);
  return buffer->data;
}

void
cogl_buffer_unmap (CoglBuffer *buffer)
{
  if (!cogl_is_buffer (buffer))
    return;

  if (!(buffer->flags & COGL_BUFFER_FLAG_MAPPED))
    return;

  buffer->vtable.unmap (buffer);
}

void *
_cogl_buffer_map_for_fill_or_fallback (CoglBuffer *buffer)
{
  CoglContext *ctx = buffer->context;
  void *ret;

  _COGL_RETURN_VAL_IF_FAIL (!ctx->buffer_map_fallback_in_use, NULL);

  ctx->buffer_map_fallback_in_use = TRUE;

  ret = cogl_buffer_map (buffer,
                         COGL_BUFFER_ACCESS_WRITE,
                         COGL_BUFFER_MAP_HINT_DISCARD);

  if (ret)
    return ret;
  else
    {
      /* If the map fails then we'll use a temporary buffer to fill
         the data and then upload it using cogl_buffer_set_data when
         the buffer is unmapped. The temporary buffer is shared to
         avoid reallocating it every time */
      g_byte_array_set_size (ctx->buffer_map_fallback_array, buffer->size);

      buffer->flags |= COGL_BUFFER_FLAG_MAPPED_FALLBACK;

      return ctx->buffer_map_fallback_array->data;
    }
}

void
_cogl_buffer_unmap_for_fill_or_fallback (CoglBuffer *buffer)
{
  CoglContext *ctx = buffer->context;

  _COGL_RETURN_IF_FAIL (ctx->buffer_map_fallback_in_use);

  ctx->buffer_map_fallback_in_use = FALSE;

  if ((buffer->flags & COGL_BUFFER_FLAG_MAPPED_FALLBACK))
    {
      cogl_buffer_set_data (buffer, 0,
                            ctx->buffer_map_fallback_array->data,
                            buffer->size);
      buffer->flags &= ~COGL_BUFFER_FLAG_MAPPED_FALLBACK;
    }
  else
    cogl_buffer_unmap (buffer);
}

CoglBool
cogl_buffer_set_data (CoglBuffer *buffer,
                      size_t offset,
                      const void *data,
                      size_t size)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_buffer (buffer), FALSE);
  _COGL_RETURN_VAL_IF_FAIL ((offset + size) <= buffer->size, FALSE);

  if (G_UNLIKELY (buffer->immutable_ref))
    warn_about_midscene_changes ();

  return buffer->vtable.set_data (buffer, offset, data, size);
}

CoglBuffer *
_cogl_buffer_immutable_ref (CoglBuffer *buffer)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_buffer (buffer), NULL);

  buffer->immutable_ref++;
  return buffer;
}

void
_cogl_buffer_immutable_unref (CoglBuffer *buffer)
{
  _COGL_RETURN_IF_FAIL (cogl_is_buffer (buffer));
  _COGL_RETURN_IF_FAIL (buffer->immutable_ref > 0);

  buffer->immutable_ref--;
}

