/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * MutterTextureTower
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __MUTTER_TEXTURE_TOWER_H__
#define __MUTTER_TEXTURE_TOWER_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

/**
 * SECTION:MutterTextureTower
 * @short_description: mipmap emulation by creation of scaled down images
 *
 * A #MutterTextureTower is used to get good looking scaled down images when
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

typedef struct _MutterTextureTower MutterTextureTower;

MutterTextureTower *mutter_texture_tower_new               (void);
void                mutter_texture_tower_free              (MutterTextureTower *tower);
void                mutter_texture_tower_set_base_texture  (MutterTextureTower *tower,
                                                            CoglHandle          texture);
void                mutter_texture_tower_update_area       (MutterTextureTower *tower,
                                                            int                 x,
                                                            int                 y,
                                                            int                 width,
                                                            int                 height);
CoglHandle          mutter_texture_tower_get_paint_texture (MutterTextureTower *tower);

G_BEGIN_DECLS

#endif /* __MUTTER_TEXTURE_TOWER_H__ */
