/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __COGL_PRIMITIVES_H
#define __COGL_PRIMITIVES_H

typedef struct _floatVec2     floatVec2;
typedef struct _CoglBezQuad   CoglBezQuad;
typedef struct _CoglBezCubic  CoglBezCubic;
typedef struct _CoglPathNode  CoglPathNode;

struct _floatVec2
{
  float x;
  float y;
};

struct _CoglPathNode
{
  float x;
  float y;
  unsigned int path_size;
};

struct _CoglBezQuad
{
  floatVec2 p1;
  floatVec2 p2;
  floatVec2 p3;
};

struct _CoglBezCubic
{
  floatVec2 p1;
  floatVec2 p2;
  floatVec2 p3;
  floatVec2 p4;
};

void
_cogl_journal_flush (void);

#endif /* __COGL_PRIMITIVES_H */
