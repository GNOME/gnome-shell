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
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-pixel-buffer-private.h"

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
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER GL_ARRAY_BUFFER_ARB
#endif

#elif defined (HAVE_COGL_GLES2)

#include "../gles/cogl-gles2-wrapper.h"

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

void
_cogl_buffer_initialize (CoglBuffer           *buffer,
                         unsigned int          size,
                         CoglBufferUsageHint   usage_hint,
                         CoglBufferUpdateHint  update_hint)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  buffer->flags       = COGL_BUFFER_FLAG_NONE;
  buffer->size        = size;
  buffer->usage_hint  = usage_hint;
  buffer->update_hint = update_hint;
  buffer->data        = NULL;
}

void
_cogl_buffer_fini (CoglBuffer *buffer)
{
  if (COGL_BUFFER_FLAG_IS_SET (buffer, MAPPED))
    cogl_buffer_unmap (buffer);
}

/* OpenGL ES 1.1 and 2 have a GL_OES_mapbuffer extension that is able to map
 * VBOs for write only, we don't support that in CoglBuffer */
#if defined (COGL_HAS_GLES)
GLenum
_cogl_buffer_access_to_gl_enum (CoglBufferAccess access)
{
  return 0;
}
#else
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
#endif

/* OpenGL ES 1.1 and 2 only know about STATIC_DRAW and DYNAMIC_DRAW */
#if defined (COGL_HAS_GLES)
GLenum
_cogl_buffer_hints_to_gl_enum (CoglBufferUsageHint usage_hint,
                               CoglBufferUpdateHint update_hint)
{
  /* usage hint is always TEXTURE for now */
  if (update_hint == COGL_BUFFER_UPDATE_HINT_STATIC)
      return GL_STATIC_DRAW;
  return GL_DYNAMIC_DRAW;
}
#else
GLenum
_cogl_buffer_hints_to_gl_enum (CoglBufferUsageHint  usage_hint,
                               CoglBufferUpdateHint update_hint)
{
  /* usage hint is always TEXTURE for now */
  if (update_hint == COGL_BUFFER_UPDATE_HINT_STATIC)
    return GL_STATIC_DRAW;
  if (update_hint == COGL_BUFFER_UPDATE_HINT_DYNAMIC)
    return GL_DYNAMIC_DRAW;
  if (update_hint == COGL_BUFFER_UPDATE_HINT_STREAM)
    return GL_STREAM_DRAW;

  return GL_STATIC_DRAW;
}
#endif

void
_cogl_buffer_bind (CoglBuffer *buffer,
                   GLenum      target)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Don't bind again an already bound pbo */
  if (ctx->current_pbo == buffer)
    return;

  if (buffer && COGL_BUFFER_FLAG_IS_SET (buffer, BUFFER_OBJECT))
    {
      GE( glBindBuffer (target, buffer->gl_handle) );
    }
  else if (buffer == NULL &&
           ctx->current_pbo &&
           COGL_BUFFER_FLAG_IS_SET (ctx->current_pbo, BUFFER_OBJECT))
    {
      GE( glBindBuffer (target, 0) );
    }

  ctx->current_pbo = buffer;
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

guint8 *
cogl_buffer_map (CoglBuffer      *buffer,
                 CoglBufferAccess access)
{
  if (!cogl_is_buffer (buffer))
    return NULL;

  if (COGL_BUFFER_FLAG_IS_SET (buffer, MAPPED))
    return buffer->data;

  buffer->data = buffer->vtable->map (buffer, access);
  return buffer->data;
}

void
cogl_buffer_unmap (CoglBuffer *buffer)
{
  if (!cogl_is_buffer (buffer))
    return;

  if (!COGL_BUFFER_FLAG_IS_SET (buffer, MAPPED))
    return;

  buffer->vtable->unmap (buffer);
}

gboolean
cogl_buffer_set_data (CoglBuffer   *buffer,
                      gsize         offset,
                      const guint8 *data,
                      gsize         size)
{
  if (!cogl_is_buffer (buffer))
    return FALSE;

  if (G_UNLIKELY((offset + size) > buffer->size))
    return FALSE;

  return buffer->vtable->set_data (buffer, offset, data, size);
}
