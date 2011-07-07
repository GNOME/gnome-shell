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

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-handle.h"
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
 * The CoglHandle macros don't support any form of inheritance, so for
 * now we implement the CoglObject support for the CoglBuffer
 * abstract class manually.
 */

void
_cogl_buffer_register_buffer_type (GQuark type)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  ctx->buffer_types = g_slist_prepend (ctx->buffer_types,
                                       GINT_TO_POINTER (type));
}

gboolean
cogl_is_buffer (const void *object)
{
  const CoglHandleObject *obj = object;
  GSList *l;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (object == NULL)
    return FALSE;

  for (l = ctx->buffer_types; l; l = l->next)
    if (GPOINTER_TO_INT (l->data) == obj->klass->type)
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
bo_map (CoglBuffer       *buffer,
        CoglBufferAccess  access,
        CoglBufferMapHint hints)
{
  guint8 *data;
  CoglBufferBindTarget target;
  GLenum gl_target;

  _COGL_GET_CONTEXT (ctx, NULL);

  if ((access & COGL_BUFFER_ACCESS_READ) &&
      !cogl_features_available (COGL_FEATURE_MAP_BUFFER_FOR_READ))
    return NULL;
  if ((access & COGL_BUFFER_ACCESS_WRITE) &&
      !cogl_features_available (COGL_FEATURE_MAP_BUFFER_FOR_WRITE))
    return NULL;

  target = buffer->last_target;
  _cogl_buffer_bind (buffer, target);

  gl_target = convert_bind_target_to_gl_target (target);

  /* create an empty store if we don't have one yet. creating the store
   * lazily allows the user of the CoglBuffer to set a hint before the
   * store is created. */
  if (!buffer->store_created || (hints & COGL_BUFFER_MAP_HINT_DISCARD))
    {
      GLenum gl_enum;

      gl_enum = _cogl_buffer_hints_to_gl_enum (buffer->usage_hint,
                                               buffer->update_hint);


      GE( ctx, glBufferData (gl_target,
                             buffer->size,
                             NULL,
                             gl_enum) );
      buffer->store_created = TRUE;
    }

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
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_buffer_bind (buffer, buffer->last_target);

  GE( ctx, glUnmapBuffer (convert_bind_target_to_gl_target
                          (buffer->last_target)) );
  buffer->flags &= ~COGL_BUFFER_FLAG_MAPPED;

  _cogl_buffer_unbind (buffer);
}

static gboolean
bo_set_data (CoglBuffer   *buffer,
             unsigned int  offset,
             const void   *data,
             unsigned int  size)
{
  CoglBufferBindTarget target;
  GLenum gl_target;

  _COGL_GET_CONTEXT (ctx, FALSE);

  target = buffer->last_target;
  _cogl_buffer_bind (buffer, target);

  gl_target = convert_bind_target_to_gl_target (target);

  /* create an empty store if we don't have one yet. creating the store
   * lazily allows the user of the CoglBuffer to set a hint before the
   * store is created. */
  if (!buffer->store_created)
    {
      GLenum gl_enum = _cogl_buffer_hints_to_gl_enum (buffer->usage_hint,
                                                      buffer->update_hint);
      GE( ctx, glBufferData (gl_target,
                             buffer->size,
                             NULL,
                             gl_enum) );
      buffer->store_created = TRUE;
    }

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

static gboolean
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
                         unsigned int          size,
                         gboolean              use_malloc,
                         CoglBufferBindTarget  default_target,
                         CoglBufferUsageHint   usage_hint,
                         CoglBufferUpdateHint  update_hint)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

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

      GE( ctx, glGenBuffers (1, &buffer->gl_handle) );
      buffer->flags |= COGL_BUFFER_FLAG_BUFFER_OBJECT;
    }
}

void
_cogl_buffer_fini (CoglBuffer *buffer)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (!(buffer->flags & COGL_BUFFER_FLAG_MAPPED));
  g_return_if_fail (buffer->immutable_ref == 0);

  if (buffer->flags & COGL_BUFFER_FLAG_BUFFER_OBJECT)
    GE( ctx, glDeleteBuffers (1, &buffer->gl_handle) );
  else
    g_free (buffer->data);
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

GLenum
_cogl_buffer_hints_to_gl_enum (CoglBufferUsageHint  usage_hint,
                               CoglBufferUpdateHint update_hint)
{
  _COGL_GET_CONTEXT (ctx, 0);

  /* usage hint is always TEXTURE for now */
  if (update_hint == COGL_BUFFER_UPDATE_HINT_STATIC)
    return GL_STATIC_DRAW;
  if (update_hint == COGL_BUFFER_UPDATE_HINT_DYNAMIC)
    return GL_DYNAMIC_DRAW;
  /* OpenGL ES 1.1 and 2 only know about STATIC_DRAW and DYNAMIC_DRAW */
#ifdef HAVE_COGL_GL
  if (update_hint == COGL_BUFFER_UPDATE_HINT_STREAM)
    return GL_STREAM_DRAW;
#endif

  return GL_STATIC_DRAW;
}

void *
_cogl_buffer_bind (CoglBuffer *buffer, CoglBufferBindTarget target)
{
  _COGL_GET_CONTEXT (ctx, NULL);

  g_return_val_if_fail (buffer != NULL, NULL);

  /* Don't allow binding the buffer to multiple targets at the same time */
  g_return_val_if_fail (ctx->current_buffer[buffer->last_target] != buffer,
                        NULL);

  /* Don't allow nesting binds to the same target */
  g_return_val_if_fail (ctx->current_buffer[target] == NULL, NULL);

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

void
_cogl_buffer_unbind (CoglBuffer *buffer)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (buffer != NULL);

  /* the unbind should pair up with a previous bind */
  g_return_if_fail (ctx->current_buffer[buffer->last_target] == buffer);

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
  static gboolean seen = FALSE;
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
  g_return_val_if_fail (cogl_is_buffer (buffer), NULL);

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
  void *ret;

  _COGL_GET_CONTEXT (ctx, NULL);

  g_return_val_if_fail (!ctx->buffer_map_fallback_in_use, NULL);

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
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (ctx->buffer_map_fallback_in_use);

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

gboolean
cogl_buffer_set_data (CoglBuffer   *buffer,
                      gsize         offset,
                      const void   *data,
                      gsize         size)
{
  g_return_val_if_fail (cogl_is_buffer (buffer), FALSE);
  g_return_val_if_fail ((offset + size) <= buffer->size, FALSE);

  if (G_UNLIKELY (buffer->immutable_ref))
    warn_about_midscene_changes ();

  return buffer->vtable.set_data (buffer, offset, data, size);
}

CoglBuffer *
_cogl_buffer_immutable_ref (CoglBuffer *buffer)
{
  g_return_val_if_fail (cogl_is_buffer (buffer), NULL);

  buffer->immutable_ref++;
  return buffer;
}

void
_cogl_buffer_immutable_unref (CoglBuffer *buffer)
{
  g_return_if_fail (cogl_is_buffer (buffer));
  g_return_if_fail (buffer->immutable_ref > 0);

  buffer->immutable_ref--;
}

