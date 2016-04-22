/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2014 Intel Corporation.
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
 */

#ifndef __COGL_TEXTURE_DEPRECATED_H__
#define __COGL_TEXTURE_DEPRECATED_H__

/**
 * cogl_texture_get_format:
 * @texture: a #CoglTexture pointer.
 *
 * Queries the #CoglPixelFormat of a cogl texture.
 *
 * Return value: the #CoglPixelFormat of the GPU side texture
 * Deprecated: 1.18: This api is misleading
 */
COGL_DEPRECATED_IN_1_18
CoglPixelFormat
cogl_texture_get_format (CoglTexture *texture);

/**
 * cogl_texture_get_rowstride:
 * @texture a #CoglTexture pointer.
 *
 * Determines the bytes-per-pixel for the #CoglPixelFormat retrieved
 * from cogl_texture_get_format() and multiplies that by the texture's
 * width.
 *
 * <note>It's very unlikely that anyone would need to use this API to
 * query the internal rowstride of a #CoglTexture which can just be
 * considered an implementation detail. Actually it's not even useful
 * internally since underlying drivers are free to use a different
 * format</note>
 *
 * <note>This API is only here for backwards compatibility and
 * shouldn't be used in new code. In particular please don't be
 * mislead to pass the returned value to cogl_texture_get_data() for
 * the rowstride, since you should be passing the rowstride you desire
 * for your destination buffer not the rowstride of the source
 * texture.</note>
 *
 * Return value: The bytes-per-pixel for the current format
 *               multiplied by the texture's width
 *
 * Deprecated: 1.10: There's no replacement for the API but there's
 *                   also no known need for API either. It was just
 *                   a mistake that it was ever published.
 */
COGL_DEPRECATED_IN_1_10
unsigned int
cogl_texture_get_rowstride (CoglTexture *texture);

/**
 * cogl_texture_ref: (skip)
 * @texture: a #CoglTexture.
 *
 * Increment the reference count for a cogl texture.
 *
 * Deprecated: 1.2: Use cogl_object_ref() instead
 *
 * Return value: the @texture pointer.
 */
COGL_DEPRECATED_FOR (cogl_object_ref)
void *
cogl_texture_ref (void *texture);

/**
 * cogl_texture_unref: (skip)
 * @texture: a #CoglTexture.
 *
 * Decrement the reference count for a cogl texture.
 *
 * Deprecated: 1.2: Use cogl_object_unref() instead
 */
COGL_DEPRECATED_FOR (cogl_object_unref)
void
cogl_texture_unref (void *texture);

#endif /* __COGL_TEXTURE_DEPRECATED_H__ */
