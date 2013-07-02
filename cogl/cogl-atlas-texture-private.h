/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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

#ifndef _COGL_ATLAS_TEXTURE_PRIVATE_H_
#define _COGL_ATLAS_TEXTURE_PRIVATE_H_

#include "cogl-object-private.h"
#include "cogl-texture-private.h"
#include "cogl-rectangle-map.h"
#include "cogl-atlas.h"
#include "cogl-atlas-texture.h"

struct _CoglAtlasTexture
{
  CoglTexture           _parent;

  /* The format that the texture is in. This isn't necessarily the
     same format as the atlas texture because we can store
     pre-multiplied and non-pre-multiplied textures together */
  CoglPixelFormat       internal_format;

  /* The rectangle that was used to add this texture to the
     atlas. This includes the 1-pixel border */
  CoglRectangleMapEntry rectangle;

  /* The atlas that this texture is in. If the texture is no longer in
     an atlas then this will be NULL. A reference is taken on the
     atlas by the texture (but not vice versa so there is no cycle) */
  CoglAtlas            *atlas;

  /* Either a CoglSubTexture representing the atlas region for easy
   * rendering or if the texture has been migrated out of the atlas it
   * may be some other texture type such as CoglTexture2D */
  CoglTexture          *sub_texture;
};

CoglAtlasTexture *
_cogl_atlas_texture_new_from_bitmap (CoglBitmap *bmp,
                                     CoglBool can_convert_in_place);

void
_cogl_atlas_texture_add_reorganize_callback (CoglContext *ctx,
                                             GHookFunc callback,
                                             void *user_data);

void
_cogl_atlas_texture_remove_reorganize_callback (CoglContext *ctx,
                                                GHookFunc callback,
                                                void *user_data);

CoglBool
_cogl_is_atlas_texture (void *object);

#endif /* _COGL_ATLAS_TEXTURE_PRIVATE_H_ */
