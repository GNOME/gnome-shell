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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_VERTEX_ATTRIBUTE_PRIVATE_H
#define __COGL_VERTEX_ATTRIBUTE_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-vertex-attribute.h"

typedef enum
{
  COGL_VERTEX_ATTRIBUTE_NAME_ID_POSITION_ARRAY,
  COGL_VERTEX_ATTRIBUTE_NAME_ID_COLOR_ARRAY,
  COGL_VERTEX_ATTRIBUTE_NAME_ID_TEXTURE_COORD_ARRAY,
  COGL_VERTEX_ATTRIBUTE_NAME_ID_NORMAL_ARRAY,
  COGL_VERTEX_ATTRIBUTE_NAME_ID_CUSTOM_ARRAY
} CoglVertexAttributeNameID;

struct _CoglVertexAttribute
{
  CoglObject _parent;

  CoglVertexArray *array;
  char *name;
  CoglVertexAttributeNameID name_id;
  gsize stride;
  gsize offset;
  int n_components;
  CoglVertexAttributeType type;
  gboolean normalized;
  unsigned int texture_unit;

  int immutable_ref;
};

CoglVertexAttribute *
_cogl_vertex_attribute_immutable_ref (CoglVertexAttribute *vertex_attribute);

void
_cogl_vertex_attribute_immutable_unref (CoglVertexAttribute *vertex_attribute);

void
_cogl_draw_vertex_attributes_array (CoglVerticesMode mode,
                                    int first_vertex,
                                    int n_vertices,
                                    CoglVertexAttribute **attributes);

void
_cogl_draw_indexed_vertex_attributes_array (CoglVerticesMode mode,
                                            int first_vertex,
                                            int n_vertices,
                                            CoglIndices *indices,
                                            CoglVertexAttribute **attributes);

#endif /* __COGL_VERTEX_ATTRIBUTE_PRIVATE_H */

