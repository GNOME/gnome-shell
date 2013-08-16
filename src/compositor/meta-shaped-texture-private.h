/*
 * shaped texture
 *
 * An actor to draw a texture clipped to a list of rectangles
 *
 * Authored By Neil Roberts  <neil@linux.intel.com>
 *
 * Copyright (C) 2008 Intel Corporation
 *               2013 Red Hat, Inc.
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

#ifndef __META_SHAPED_TEXTURE_PRIVATE_H__
#define __META_SHAPED_TEXTURE_PRIVATE_H__

#include <meta/meta-shaped-texture.h>
#include "meta-wayland-private.h"

ClutterActor *meta_shaped_texture_new_with_xwindow (Window xwindow);
ClutterActor *meta_shaped_texture_new_with_wayland_surface  (MetaWaylandSurface *surface);
void meta_shaped_texture_set_wayland_surface                (MetaShapedTexture  *stex,
                                                             MetaWaylandSurface *surface);
MetaWaylandSurface *meta_shaped_texture_get_wayland_surface (MetaShapedTexture *stex);

void meta_shaped_texture_set_pixmap (MetaShapedTexture *stex,
                                     Pixmap             pixmap);
void meta_shaped_texture_attach_wayland_buffer (MetaShapedTexture  *stex,
                                                MetaWaylandBuffer  *buffer);

#endif
