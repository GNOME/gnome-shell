/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2012 Intel Corporation.
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
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_OFFSCREEN_H__
#define __COGL_OFFSCREEN_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-texture.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-offscreen
 * @short_description: Functions for creating and manipulating offscreen
 *                     framebuffers.
 *
 * Cogl allows creating and operating on offscreen framebuffers.
 */

typedef struct _CoglOffscreen CoglOffscreen;

#define COGL_OFFSCREEN(X) ((CoglOffscreen *)X)

/* Offscreen api */

/**
 * cogl_offscreen_new_with_texture:
 * @texture: A #CoglTexture pointer
 *
 * This creates an offscreen framebuffer object using the given
 * @texture as the primary color buffer. It doesn't just initialize
 * the contents of the offscreen buffer with the @texture; they are
 * tightly bound so that drawing to the offscreen buffer effectively
 * updates the contents of the given texture. You don't need to
 * destroy the offscreen buffer before you can use the @texture again.
 *
 * <note>This api only works with low-level #CoglTexture types such as
 * #CoglTexture2D, #CoglTexture3D and #CoglTextureRectangle, and not
 * with meta-texture types such as #CoglTexture2DSliced.</note>
 *
 * The storage for the framebuffer is actually allocated lazily
 * so this function will never return %NULL to indicate a runtime
 * error. This means it is still possible to configure the framebuffer
 * before it is really allocated.
 *
 * Simple applications without full error handling can simply rely on
 * Cogl to lazily allocate the storage of framebuffers but you should
 * be aware that if Cogl encounters an error (such as running out of
 * GPU memory) then your application will simply abort with an error
 * message. If you need to be able to catch such exceptions at runtime
 * then you can explicitly allocate your framebuffer when you have
 * finished configuring it by calling cogl_framebuffer_allocate() and
 * passing in a #CoglError argument to catch any exceptions.
 *
 * Return value: (transfer full): a newly instantiated #CoglOffscreen
 *   framebuffer.
 */
CoglOffscreen *
cogl_offscreen_new_with_texture (CoglTexture *texture);

/**
 * cogl_offscreen_new_to_texture:
 * @texture: A #CoglTexture pointer
 *
 * This creates an offscreen buffer object using the given @texture as the
 * primary color buffer. It doesn't just initialize the contents of the
 * offscreen buffer with the @texture; they are tightly bound so that
 * drawing to the offscreen buffer effectivly updates the contents of the
 * given texture. You don't need to destroy the offscreen buffer before
 * you can use the @texture again.
 *
 * <note>This only works with low-level #CoglTexture types such as
 * #CoglTexture2D, #CoglTexture3D and #CoglTextureRectangle, and not
 * with meta-texture types such as #CoglTexture2DSliced.</note>
 *
 * Return value: (transfer full): a newly instantiated #CoglOffscreen
 *   framebuffer or %NULL if it wasn't possible to create the
 *   buffer.
 * Deprecated: 1.16: Use cogl_offscreen_new_with_texture instead.
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_offscreen_new_with_texture)
CoglOffscreen *
cogl_offscreen_new_to_texture (CoglTexture *texture);

/**
 * cogl_is_offscreen:
 * @object: A pointer to a #CoglObject
 *
 * Determines whether the given #CoglObject references an offscreen
 * framebuffer object.
 *
 * Returns: %TRUE if @object is a #CoglOffscreen framebuffer,
 *          %FALSE otherwise
 */
CoglBool
cogl_is_offscreen (void *object);

/**
 * cogl_offscreen_ref:
 * @offscreen: A pointer to a #CoglOffscreen framebuffer
 *
 * Increments the reference count on the @offscreen framebuffer.
 *
 * Return value: (transfer none): For convenience it returns the
 *                                given @offscreen
 *
 * Deprecated: 1.2: cogl_object_ref() should be used in new code.
 */
COGL_DEPRECATED_FOR (cogl_object_ref)
void *
cogl_offscreen_ref (void *offscreen);

/**
 * cogl_offscreen_unref:
 * @offscreen: A pointer to a #CoglOffscreen framebuffer
 *
 * Decreases the reference count for the @offscreen buffer and frees it when
 * the count reaches 0.
 *
 * Deprecated: 1.2: cogl_object_unref() should be used in new code.
 */
COGL_DEPRECATED_FOR (cogl_object_unref)
void
cogl_offscreen_unref (void *offscreen);

COGL_END_DECLS

#endif /* __COGL_OFFSCREEN_H__ */
