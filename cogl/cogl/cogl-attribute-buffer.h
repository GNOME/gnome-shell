/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_ATTRIBUTE_BUFFER_H__
#define __COGL_ATTRIBUTE_BUFFER_H__

/* We forward declare the CoglAttributeBuffer type here to avoid some circular
 * dependency issues with the following headers.
 */
typedef struct _CoglAttributeBuffer CoglAttributeBuffer;

#include <cogl/cogl-context.h>

#ifdef COGL_HAS_GTYPE_SUPPORT
#include <glib-object.h>
#endif

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-attribute-buffer
 * @short_description: Functions for creating and manipulating attribute
 *   buffers
 *
 * FIXME
 */

#define COGL_ATTRIBUTE_BUFFER(buffer) ((CoglAttributeBuffer *)(buffer))

#ifdef COGL_HAS_GTYPE_SUPPORT
/**
 * cogl_attribute_buffer_get_gtype:
 *
 * Returns: a #GType that can be used with the GLib type system.
 */
GType cogl_attribute_buffer_get_gtype (void);
#endif

/**
 * cogl_attribute_buffer_new_with_size:
 * @context: A #CoglContext
 * @bytes: The number of bytes to allocate for vertex attribute data.
 *
 * Describes a new #CoglAttributeBuffer of @size bytes to contain
 * arrays of vertex attribute data. Afterwards data can be set using
 * cogl_buffer_set_data() or by mapping it into the application's
 * address space using cogl_buffer_map().
 *
 * The underlying storage of this buffer isn't allocated by this
 * function so that you have an opportunity to use the
 * cogl_buffer_set_update_hint() and cogl_buffer_set_usage_hint()
 * functions which may influence how the storage is allocated. The
 * storage will be allocated once you upload data to the buffer.
 *
 * Note: You can assume this function always succeeds and won't return
 * %NULL
 *
 * Return value: (transfer full): A newly allocated #CoglAttributeBuffer. Never %NULL.
 *
 * Stability: Unstable
 */
CoglAttributeBuffer *
cogl_attribute_buffer_new_with_size (CoglContext *context,
                                     size_t bytes);

/**
 * cogl_attribute_buffer_new:
 * @context: A #CoglContext
 * @bytes: The number of bytes to allocate for vertex attribute data.
 * @data: (array length=bytes): An optional pointer to vertex data to
 *        upload immediately.
 *
 * Describes a new #CoglAttributeBuffer of @size bytes to contain
 * arrays of vertex attribute data and also uploads @size bytes read
 * from @data to the new buffer.
 *
 * You should never pass a %NULL data pointer.
 *
 * <note>This function does not report out-of-memory errors back to
 * the caller by returning %NULL and so you can assume this function
 * always succeeds.</note>
 *
 * <note>In the unlikely case that there is an out of memory problem
 * then Cogl will abort the application with a message. If your
 * application needs to gracefully handle out-of-memory errors then
 * you can use cogl_attribute_buffer_new_with_size() and then
 * explicitly catch errors with cogl_buffer_set_data() or
 * cogl_buffer_map().</note>
 *
 * Return value: (transfer full): A newly allocated #CoglAttributeBuffer (never %NULL)
 *
 * Since: 1.4
 * Stability: Unstable
 */
CoglAttributeBuffer *
cogl_attribute_buffer_new (CoglContext *context,
                           size_t bytes,
                           const void *data);

/**
 * cogl_is_attribute_buffer:
 * @object: A #CoglObject
 *
 * Gets whether the given object references a #CoglAttributeBuffer.
 *
 * Returns: %TRUE if @object references a #CoglAttributeBuffer,
 *   %FALSE otherwise
 *
 * Since: 1.4
 * Stability: Unstable
 */
CoglBool
cogl_is_attribute_buffer (void *object);

COGL_END_DECLS

#endif /* __COGL_ATTRIBUTE_BUFFER_H__ */

