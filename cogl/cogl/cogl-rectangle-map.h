/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009 Intel Corporation.
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
 */

#ifndef __COGL_RECTANGLE_MAP_H
#define __COGL_RECTANGLE_MAP_H

#include <glib.h>
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
