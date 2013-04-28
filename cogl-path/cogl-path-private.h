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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_PATH_PRIVATE_H
#define __COGL_PATH_PRIVATE_H

#include "cogl-object.h"
#include "cogl-attribute-private.h"

typedef struct _floatVec2
{
  float x;
  float y;
} floatVec2;

typedef struct _CoglPathNode
{
  float x;
  float y;
  unsigned int path_size;
} CoglPathNode;

typedef struct _CoglBezQuad
{
  floatVec2 p1;
  floatVec2 p2;
  floatVec2 p3;
} CoglBezQuad;

typedef struct _CoglBezCubic
{
  floatVec2 p1;
  floatVec2 p2;
  floatVec2 p3;
  floatVec2 p4;
} CoglBezCubic;

typedef struct _CoglPathData CoglPathData;

struct _CoglPath
{
  CoglObject _parent;

  CoglPathData *data;
};

#define COGL_PATH_N_ATTRIBUTES 2

struct _CoglPathData
{
  unsigned int         ref_count;

  CoglContext         *context;

  CoglPathFillRule     fill_rule;

  GArray              *path_nodes;

  floatVec2            path_start;
  floatVec2            path_pen;
  unsigned int         last_path;
  floatVec2            path_nodes_min;
  floatVec2            path_nodes_max;

  CoglAttributeBuffer *fill_attribute_buffer;
  CoglIndices         *fill_vbo_indices;
  unsigned int         fill_vbo_n_indices;
  CoglAttribute       *fill_attributes[COGL_PATH_N_ATTRIBUTES + 1];
  CoglPrimitive       *fill_primitive;

  CoglAttributeBuffer *stroke_attribute_buffer;
  CoglAttribute      **stroke_attributes;
  unsigned int         stroke_n_attributes;

  /* This is used as an optimisation for when the path contains a
     single contour specified using cogl2_path_rectangle. Cogl is more
     optimised to handle rectangles than paths so we can detect this
     case and divert to the journal or a rectangle clip. If it is TRUE
     then the entire path can be described by calling
     _cogl_path_get_bounds */
  CoglBool             is_rectangle;
};

void
_cogl_add_path_to_stencil_buffer (CoglPath  *path,
                                  CoglBool   merge,
                                  CoglBool   need_clear);

void
_cogl_path_get_bounds (CoglPath *path,
                       float *min_x,
                       float *min_y,
                       float *max_x,
                       float *max_y);

CoglBool
_cogl_path_is_rectangle (CoglPath *path);

#endif /* __COGL_PATH_PRIVATE_H */
