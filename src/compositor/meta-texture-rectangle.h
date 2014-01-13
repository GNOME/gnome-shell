/*
 * texture rectangle
 *
 * A small utility function to help create a rectangle texture
 *
 * Authored By Neil Roberts  <neil@linux.intel.com>
 *
 * Copyright (C) 2011 Intel Corporation
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

#ifndef __META_TEXTURE_RECTANGLE_H__
#define __META_TEXTURE_RECTANGLE_H__

#include <cogl/cogl.h>

G_BEGIN_DECLS

CoglTexture *
meta_texture_rectangle_new (unsigned int width,
                            unsigned int height,
                            CoglPixelFormat format,
                            unsigned int rowstride,
                            const guint8 *data);

gboolean
meta_texture_rectangle_check (CoglTexture *texture);

G_END_DECLS

#endif /* __META_TEXTURE_RECTANGLE_H__ */
