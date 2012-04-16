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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __COGL_RECTANGLE_MAP_H
#define __COGL_RECTANGLE_MAP_H

#include "cogl-types.h"

typedef struct _CoglRectangleMap      CoglRectangleMap;
typedef struct _CoglRectangleMapEntry CoglRectangleMapEntry;

typedef void (* CoglRectangleMapCallback) (const CoglRectangleMapEntry *entry,
                                           void *rectangle_data,
                                           void *user_data);

struct _CoglRectangleMapEntry
{
  unsigned int x, y;
  unsigned int width, height;
};

CoglRectangleMap *
_cogl_rectangle_map_new (unsigned int width,
                         unsigned int height,
                         GDestroyNotify value_destroy_func);

CoglBool
_cogl_rectangle_map_add (CoglRectangleMap *map,
                         unsigned int width,
                         unsigned int height,
                         void *data,
                         CoglRectangleMapEntry *rectangle);

void
_cogl_rectangle_map_remove (CoglRectangleMap *map,
                            const CoglRectangleMapEntry *rectangle);

unsigned int
_cogl_rectangle_map_get_width (CoglRectangleMap *map);

unsigned int
_cogl_rectangle_map_get_height (CoglRectangleMap *map);

unsigned int
_cogl_rectangle_map_get_remaining_space (CoglRectangleMap *map);

unsigned int
_cogl_rectangle_map_get_n_rectangles (CoglRectangleMap *map);

void
_cogl_rectangle_map_foreach (CoglRectangleMap *map,
                             CoglRectangleMapCallback callback,
                             void *data);

void
_cogl_rectangle_map_free (CoglRectangleMap *map);

#endif /* __COGL_RECTANGLE_MAP_H */
