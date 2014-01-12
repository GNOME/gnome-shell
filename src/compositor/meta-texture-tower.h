/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * MetaTextureTower
 *
 * Mipmap emulation by creation of scaled down images
 *
 * Copyright (C) 2009 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __META_TEXTURE_TOWER_H__
#define __META_TEXTURE_TOWER_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

/**
 * SECTION:MetaTextureTower
 * @short_description: mipmap emulation by creation of scaled down images
 *
 * A #MetaTextureTower is used to get good looking scaled down images when
 * we can't use the GL drivers mipmap support. There are two separate reasons
 *
 *  - Some cards (including radeon cards <= r5xx) only support
 *    TEXTURE_RECTANGLE_ARB and not NPOT textures. Rectangular textures
 *    are defined not to support mipmapping.
 *  - Even when NPOT textures are available, the combination of NPOT
 *    textures, texture_from_pixmap, and mipmapping doesn't typically
 *    work, since the X server doesn't allocate pixmaps in the right
 *    layout for mipmapping.
 *
 * So, what we do is create the "mipmap" levels ourselves by successive
 * power-of-two scaledowns, and when rendering pick the single texture
 * that best matches the scale we are rendering at. (Since we aren't
 * typically using perspective transforms, we'll frequently have a single
 * scale for the entire texture.)
 */

typedef struct _MetaTextureTower MetaTextureTower;

MetaTextureTower *meta_texture_tower_new               (void);
void              meta_texture_tower_free              (MetaTextureTower *tower);
void              meta_texture_tower_set_base_texture  (MetaTextureTower *tower,
                                                        CoglTexture      *texture);
void              meta_texture_tower_update_area       (MetaTextureTower *tower,
                                                        int               x,
                                                        int               y,
                                                        int               width,
                                                        int               height);
CoglTexture      *meta_texture_tower_get_paint_texture (MetaTextureTower *tower);

G_BEGIN_DECLS

#endif /* __META_TEXTURE_TOWER_H__ */
