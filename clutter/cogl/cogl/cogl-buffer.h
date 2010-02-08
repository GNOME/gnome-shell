/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C)2010 Intel Corporation.
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

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_BUFFER_H__
#define __COGL_BUFFER_H__

#include <glib.h>
#include <cogl/cogl-types.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-buffer
 * @short_description: Buffer creation and manipulation
 * @stability: Unstable
 *
 * COGL allows the creation and the manipulation of buffers. If the underlying
 * OpenGL implementation allows it, COGL will use Pixel Buffer Objects.
 */

/**
 * cogl_is_buffer:
 * @handle: a #CoglHandle to test
 *
 * Checks whether @handle is a buffer handle.
 *
 * Return value: %TRUE if the handle is a CoglBuffer, and %FALSE otherwise
 *
 * Since: 1.2
 * Stability: Unstable
 */
gboolean
cogl_is_buffer (CoglHandle handle);

/**
 * cogl_buffer_get_size:
 * @handle: a buffer handle
 *
 * Retrieves the size of buffer
 *
 * Return value: the size of the buffer in bytes
 *
 * Since: 1.2
 * Stability: Unstable
 */
guint
cogl_buffer_get_size (CoglHandle handle);

/**
 * CoglBufferUsageHint:
 * @COGL_BUFFER_USAGE_HINT_TEXTURE: the buffer will be used as a source data
 *   for a texture
 *
 * The usage hint on a buffer allows the user to give some clue on how the
 * buffer will be used.
 *
 * Since: 1.2
 * Stability: Unstable
 */
typedef enum { /*< prefix=COGL_BUFFER_USAGE_HINT >*/
 COGL_BUFFER_USAGE_HINT_TEXTURE,
} CoglBufferUsageHint;

/**
 * cogl_buffer_set_usage_hint:
 * @handle: a buffer handle
 * @hint: the new hint
 *
 * Set the usage hint on a buffer. See #CoglBufferUsageHint for a description
 * of the available hints.
 *
 * Since: 1.2
 * Stability: Unstable
 */
void
cogl_buffer_set_usage_hint (CoglHandle          handle,
                            CoglBufferUsageHint hint);

/**
 * cogl_buffer_get_usage_hint:
 * @handle: a buffer handle
 *
 * Return value: the #CoglBufferUsageHint currently used by the buffer
 *
 * Since: 1.2
 * Stability: Unstable
 */
CoglBufferUsageHint
cogl_buffer_get_usage_hint (CoglHandle handle);

/**
 * CoglBufferUpdateHint:
 * @COGL_BUFFER_UPDATE_HINT_STATIC: the buffer will not change over time
 * @COGL_BUFFER_UPDATE_HINT_DYNAMIC: the buffer will change from time to time
 * @COGL_BUFFER_UPDATE_HINT_STREAM: the buffer will be used once or a couple of
 *   times
 *
 * The update hint on a buffer allows the user to give some clue on how often
 * the buffer data is going to be updated.
 *
 * Since: 1.2
 * Stability: Unstable
 */
typedef enum { /*< prefix=COGL_BUFFER_UPDATE_HINT >*/
  COGL_BUFFER_UPDATE_HINT_STATIC,
  COGL_BUFFER_UPDATE_HINT_DYNAMIC,
  COGL_BUFFER_UPDATE_HINT_STREAM
} CoglBufferUpdateHint;

/**
 * cogl_buffer_set_update_hint:
 * @handle: a buffer handle
 * @hint: the new hint
 *
 * Set the update hint on a buffer. See #CoglBufferUpdateHint for a description
 * of the available hints.
 *
 * Since: 1.2
 * Stability: Unstable
 */
void
cogl_buffer_set_update_hint (CoglHandle          handle,
                             CoglBufferUpdateHint hint);

/**
 * cogl_buffer_get_update_hint:
 * @handle: a buffer handle
 *
 * Return value: the #CoglBufferUpdateHint currently used by the buffer
 *
 * Since: 1.2
 * Stability: Unstable
 */
CoglBufferUpdateHint
cogl_buffer_get_update_hint (CoglHandle handle);

/**
 * CoglBufferAccess:
 * @COGL_BUFFER_ACCESS_READ: the buffer will be read
 * @COGL_BUFFER_ACCESS_WRITE: the buffer will written to
 * @COGL_BUFFER_ACCESS_READ_WRITE: the buffer will be used for both reading and
 *   writing
 *
 * Since: 1.2
 * Stability: Unstable
 */
typedef enum { /*< prefix=COGL_BUFFER_ACCESS >*/
 COGL_BUFFER_ACCESS_READ       = 1 << 0,
 COGL_BUFFER_ACCESS_WRITE      = 1 << 1,
 COGL_BUFFER_ACCESS_READ_WRITE = COGL_BUFFER_ACCESS_READ |
                                 COGL_BUFFER_ACCESS_WRITE
} CoglBufferAccess;

/**
 * cogl_buffer_map:
 * @handle: a buffer handle
 * @access: how the mapped buffer will by use by the application
 *
 * Maps the buffer into the application address space for direct access.
 *
 * Return value: A pointer to the mapped memory or %NULL is the call fails
 *
 * Since: 1.2
 * Stability: Unstable
 */
guchar *
cogl_buffer_map (CoglHandle       handle,
                 CoglBufferAccess access);

/**
 * cogl_buffer_unmap:
 * @handle: a buffer handle
 *
 * Unmaps a buffer previously mapped by cogl_buffer_map().
 *
 * Since: 1.2
 * Stability: Unstable
 */
void
cogl_buffer_unmap (CoglHandle handle);

/**
 * cogl_buffer_set_data:
 * @handle: a buffer handle
 * @offset: destination offset (in bytes) in the buffer
 * @data: a pointer to the data to be copied into the buffer
 * @size: number of bytes to copy
 *
 * Updates part of the buffer with new data from @data. Where to put this new
 * data is controlled by @offset and @offset + @data should be less than the
 * buffer size.
 *
 * Return value: %TRUE is the operation succeeded, %FALSE otherwise
 *
 * Since: 1.2
 * Stability: Unstable
 */
gboolean
cogl_buffer_set_data (CoglHandle    handle,
                      gsize         offset,
                      const guchar *data,
                      gsize         size);

/* the functions above are experimental, the actual symbols are suffixed by
 * _EXP so we can ensure ABI compatibility and leave the cogl_buffer namespace
 * free for future use. A bunch of defines translates the symbols documented
 * above into the real symbols */

gboolean
cogl_is_buffer_EXP (CoglHandle handle);

guint
cogl_buffer_get_size_EXP (CoglHandle handle);

void
cogl_buffer_set_usage_hint_EXP (CoglHandle          handle,
                                CoglBufferUsageHint hint);

CoglBufferUsageHint
cogl_buffer_get_usage_hint_EXP (CoglHandle handle);

void
cogl_buffer_set_update_hint_EXP (CoglHandle          handle,
                                 CoglBufferUpdateHint hint);

CoglBufferUpdateHint
cogl_buffer_get_update_hint_EXP (CoglHandle handle);

guchar *
cogl_buffer_map_EXP (CoglHandle       handle,
                     CoglBufferAccess access);

void
cogl_buffer_unmap_EXP (CoglHandle handle);

gboolean
cogl_buffer_set_data_EXP (CoglHandle    handle,
                          gsize         offset,
                          const guchar *data,
                          gsize         size);

#define cogl_is_buffer  cogl_is_buffer_EXP
#define cogl_buffer_get_size cogl_buffer_get_size_EXP
#define cogl_buffer_set_usage_hint cogl_buffer_set_usage_hint_EXP
#define cogl_buffer_get_usage_hint cogl_buffer_get_usage_hint_EXP
#define cogl_buffer_set_update_hint cogl_buffer_set_update_hint_EXP
#define cogl_buffer_get_update_hint cogl_buffer_get_update_hint_EXP
#define cogl_buffer_map cogl_buffer_map_EXP
#define cogl_buffer_unmap cogl_buffer_unmap_EXP
#define cogl_buffer_set_data cogl_buffer_set_data_EXP

G_END_DECLS

#endif /* __COGL_BUFFER_H__ */
