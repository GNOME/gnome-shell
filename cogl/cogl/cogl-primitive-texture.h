/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_PRIMITIVE_TEXTURE_H__
#define __COGL_PRIMITIVE_TEXTURE_H__

#include "cogl-types.h"

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-primitive-texture
 * @short_description: Interface for low-level textures like
 *                     #CoglTexture2D and #CoglTexture3D.
 *
 * A #CoglPrimitiveTexture is a texture that is directly represented
 * by a single texture on the GPU. For example these could be a
 * #CoglTexture2D, #CoglTexture3D or #CoglTextureRectangle. This is
 * opposed to high level meta textures which may be composed of
 * multiple primitive textures or a sub-region of another texture such
 * as #CoglAtlasTexture and #CoglTexture2DSliced.
 *
 * A texture that implements this interface can be directly used with
 * the low level cogl_primitive_draw() API. Other types of textures
 * need to be first resolved to primitive textures using the
 * #CoglMetaTexture interface.
 *
 * <note>Most developers won't need to use this interface directly but
 * still it is worth understanding the distinction between high-level
 * and primitive textures because you may find other references in the
 * documentation that detail limitations of using
 * primitive textures.</note>
 */

#ifdef __COGL_H_INSIDE__
/* For the public C api we typedef interface types as void to avoid needing
 * lots of casting in code and instead we will rely on runtime type checking
 * for these objects. */
typedef void CoglPrimitiveTexture;
#else
typedef struct _CoglPrimitiveTexture CoglPrimitiveTexture;
#define COGL_PRIMITIVE_TEXTURE(X) ((CoglPrimitiveTexture *)X)
#endif

/**
 * cogl_is_primitive_texture:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a primitive texture object.
 *
 * Return value: %TRUE if the pointer references a primitive texture, and
 *   %FALSE otherwise
 * Since: 2.0
 * Stability: unstable
 */
CoglBool
cogl_is_primitive_texture (void *object);

/**
 * cogl_primitive_texture_set_auto_mipmap:
 * @primitive_texture: A #CoglPrimitiveTexture
 * @value: The new value for whether to auto mipmap
 *
 * Sets whether the texture will automatically update the smaller
 * mipmap levels after any part of level 0 is updated. The update will
 * only occur whenever the texture is used for drawing with a texture
 * filter that requires the lower mipmap levels. An application should
 * disable this if it wants to upload its own data for the other
 * levels. By default auto mipmapping is enabled.
 *
 * Since: 2.0
 * Stability: unstable
 */
void
cogl_primitive_texture_set_auto_mipmap (CoglPrimitiveTexture *primitive_texture,
                                        CoglBool value);

COGL_END_DECLS

#endif /* __COGL_PRIMITIVE_TEXTURE_H__ */
