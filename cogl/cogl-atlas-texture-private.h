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

#ifndef __COGL_ATLAS_TEXTURE_H
#define __COGL_ATLAS_TEXTURE_H

#include "cogl-handle.h"
#include "cogl-texture-private.h"
#include "cogl-atlas.h"

#define COGL_ATLAS_TEXTURE(tex) ((CoglAtlasTexture *) tex)

typedef struct _CoglAtlasTexture CoglAtlasTexture;

struct _CoglAtlasTexture
{
  CoglTexture        _parent;

  /* The format that the texture is in. This isn't necessarily the
     same format as the atlas texture because we can store
     pre-multiplied and non-pre-multiplied textures together */
  CoglPixelFormat    format;

  /* The rectangle that was used to add this texture to the
     atlas. This includes the 1-pixel border */
  CoglAtlasRectangle rectangle;

  /* The texture might need to be migrated out in which case this will
     be set to TRUE and sub_texture will actually be a real texture */
  gboolean           in_atlas;

  /* A CoglSubTexture representing the region for easy rendering */
  CoglHandle         sub_texture;
};

GQuark
_cogl_handle_atlas_texture_get_type (void);

CoglHandle
_cogl_atlas_texture_new_from_bitmap (CoglHandle       bmp_handle,
                                     CoglTextureFlags flags,
                                     CoglPixelFormat  internal_format);

#endif /* __COGL_ATLAS_TEXTURE_H */
