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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_SUB_TEXTURE_H
#define __COGL_SUB_TEXTURE_H

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-sub-texture
 * @short_description: Functions for creating and manipulating
 *                     sub-textures.
 *
 * These functions allow high-level textures to be created that
 * represent a sub-region of another texture. For example these
 * can be used to implement custom texture atlasing schemes.
 */


#define COGL_SUB_TEXTURE(tex) ((CoglSubTexture *) tex)
typedef struct _CoglSubTexture CoglSubTexture;

/**
 * cogl_sub_texture_new:
 * @ctx: A #CoglContext pointer
 * @parent_texture: The full texture containing a sub-region you want
 *                  to make a #CoglSubTexture from.
 * @sub_x: The top-left x coordinate of the parent region to make
 *         a texture from.
 * @sub_y: The top-left y coordinate of the parent region to make
 *         a texture from.
 * @sub_width: The width of the parent region to make a texture from.
 * @sub_height: The height of the parent region to make a texture
 *              from.
 *
 * Creates a high-level #CoglSubTexture representing a sub-region of
 * any other #CoglTexture. The sub-region must strictly lye within the
 * bounds of the @parent_texture. The returned texture implements the
 * #CoglMetaTexture interface because it's not a low level texture
 * that hardware can understand natively.
 *
 * <note>Remember: Unless you are using high level drawing APIs such
 * as cogl_rectangle() or other APIs documented to understand the
 * #CoglMetaTexture interface then you need to use the
 * #CoglMetaTexture interface to resolve a #CoglSubTexture into a
 * low-level texture before drawing.</note>
 *
 * Return value: (transfer full): A newly allocated #CoglSubTexture
 *          representing a sub-region of @parent_texture.
 *
 * Since: 1.10
 * Stability: unstable
 */
CoglSubTexture *
cogl_sub_texture_new (CoglContext *ctx,
                      CoglTexture *parent_texture,
                      int sub_x,
                      int sub_y,
                      int sub_width,
                      int sub_height);

/**
 * cogl_sub_texture_get_parent:
 * @sub_texture: A pointer to a #CoglSubTexture
 *
 * Retrieves the parent texture that @sub_texture derives its content
 * from.  This is the texture that was passed to
 * cogl_sub_texture_new() as the parent_texture argument.
 *
 * Return value: (transfer none): The parent texture that @sub_texture
 *               derives its content from.
 * Since: 1.10
 * Stability: unstable
 */
CoglTexture *
cogl_sub_texture_get_parent (CoglSubTexture *sub_texture);

/**
 * cogl_is_sub_texture:
 * @object: a #CoglObject
 *
 * Checks whether @object is a #CoglSubTexture.
 *
 * Return value: %TRUE if the passed @object represents a
 *               #CoglSubTexture and %FALSE otherwise.
 *
 * Since: 1.10
 * Stability: unstable
 */
CoglBool
cogl_is_sub_texture (void *object);

COGL_END_DECLS

#endif /* __COGL_SUB_TEXTURE_H */
