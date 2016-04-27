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

#ifndef __COGL_INDEX_BUFFER_H__
#define __COGL_INDEX_BUFFER_H__

#include <cogl/cogl-context.h>

#ifdef COGL_HAS_GTYPE_SUPPORT
#include <glib-object.h>
#endif

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-index-buffer
 * @short_description: Functions for creating and manipulating vertex
 * indices.
 *
 * FIXME
 */

#define COGL_INDEX_BUFFER(buffer) ((CoglIndexBuffer*) buffer)

typedef struct _CoglIndexBuffer	      CoglIndexBuffer;

#ifdef COGL_HAS_GTYPE_SUPPORT
/**
 * cogl_index_buffer_get_gtype:
 *
 * Returns: a #GType that can be used with the GLib type system.
 */
GType cogl_index_buffer_get_gtype (void);
#endif

/**
 * cogl_index_buffer_new:
 * @context: A #CoglContext
 * @bytes: The number of bytes to allocate for vertex attribute data.
 *
 * Declares a new #CoglIndexBuffer of @size bytes to contain vertex
 * indices. Once declared, data can be set using
 * cogl_buffer_set_data() or by mapping it into the application's
 * address space using cogl_buffer_map().
 *
 * Return value: (transfer full): A newly allocated #CoglIndexBuffer
 *
 * Since: 1.4
 * Stability: Unstable
 */
CoglIndexBuffer *
cogl_index_buffer_new (CoglContext *context,
                       size_t bytes);

/**
 * cogl_is_index_buffer:
 * @object: A #CoglObject
 *
 * Gets whether the given object references a #CoglIndexBuffer.
 *
 * Returns: %TRUE if the @object references a #CoglIndexBuffer,
 *   %FALSE otherwise
 *
 * Since: 1.4
 * Stability: Unstable
 */
CoglBool
cogl_is_index_buffer (void *object);

COGL_END_DECLS

#endif /* __COGL_INDEX_BUFFER_H__ */

