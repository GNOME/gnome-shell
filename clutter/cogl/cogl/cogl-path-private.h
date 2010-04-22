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

#include "cogl-handle.h"

#define COGL_PATH(tex) ((CoglPath *)(tex))

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

typedef struct _CoglPath CoglPath;
typedef struct _CoglPathData CoglPathData;

struct _CoglPath
{
  CoglHandleObject  _parent;

  CoglPathData     *data;
};

struct _CoglPathData
{
  unsigned int      ref_count;

  GArray           *path_nodes;

  floatVec2         path_start;
  floatVec2         path_pen;
  unsigned int      last_path;
  floatVec2         path_nodes_min;
  floatVec2         path_nodes_max;
};

/* This is an internal version of cogl_path_new that doesn't affect
   the current path and just creates a new handle */
CoglHandle
_cogl_path_new (void);

void
_cogl_add_path_to_stencil_buffer (CoglHandle path,
                                  gboolean   merge,
                                  gboolean   need_clear);

#endif /* __COGL_PATH_PRIVATE_H */
