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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_INDICES_H__
#define __COGL_INDICES_H__

#include <cogl/cogl-index-buffer.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-index-range
 * @short_description: Fuctions for declaring a range of vertex indices
 *   stored in a #CoglIndexBuffer.
 *
 * FIXME
 */

typedef struct _CoglIndices CoglIndices;

CoglIndices *
cogl_indices_new (CoglIndicesType type,
                  const void *indices_data,
                  int n_indices);

CoglIndices *
cogl_indices_new_for_buffer (CoglIndicesType type,
                             CoglIndexBuffer *buffer,
                             gsize offset);

CoglIndexBuffer *
cogl_indices_get_buffer (CoglIndices *indices);

CoglIndicesType
cogl_indices_get_type (CoglIndices *indices);

gsize
cogl_indices_get_offset (CoglIndices *indices);

void
cogl_indices_set_offset (CoglIndices *indices,
                         gsize offset);

CoglIndices *
cogl_get_rectangle_indices (int n_rectangles);

G_END_DECLS

#endif /* __COGL_INDICES_H__ */

