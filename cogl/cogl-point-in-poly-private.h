/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 */

#ifndef __COGL_POINT_INT_POLYGON_PRIVATE_H
#define __COGL_POINT_INT_POLYGON_PRIVATE_H

#include <glib.h>

COGL_BEGIN_DECLS

int
_cogl_util_point_in_screen_poly (float point_x,
                                 float point_y,
                                 void *vertices,
                                 size_t stride,
                                 int n_vertices);

COGL_END_DECLS

#endif /* __COGL_POINT_INT_POLYGON_PRIVATE_H */

