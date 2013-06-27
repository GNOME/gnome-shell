/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2014 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
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
