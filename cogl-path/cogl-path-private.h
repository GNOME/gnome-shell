/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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
