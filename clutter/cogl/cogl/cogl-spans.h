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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_SPANS_PRIVATE_H
#define __COGL_SPANS_PRIVATE_H

#include "cogl-handle.h"

typedef struct _CoglSpan
{
  int start;
  int size;
  int waste;
} CoglSpan;

typedef struct _CoglSpanIter
{
  int       index;
  GArray   *array;
  CoglSpan *span;
  float     pos;
  float     next_pos;
  float     origin;
  float     cover_start;
  float     cover_end;
  float     intersect_start;
  float     intersect_end;
  float     intersect_start_local;
  float     intersect_end_local;
  gboolean  intersects;
  gboolean  flipped;
} CoglSpanIter;

void
_cogl_span_iter_update (CoglSpanIter *iter);

void
_cogl_span_iter_begin (CoglSpanIter *iter,
                       GArray       *spans,
                       float         normalize_factor,
                       float         cover_start,
                       float         cover_end);

void
_cogl_span_iter_next (CoglSpanIter *iter);

gboolean
_cogl_span_iter_end (CoglSpanIter *iter);

#endif /* __COGL_SPANS_PRIVATE_H */
