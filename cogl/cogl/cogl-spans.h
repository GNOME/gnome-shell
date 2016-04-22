/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __COGL_SPANS_PRIVATE_H
#define __COGL_SPANS_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-pipeline-layer-state.h"

typedef struct _CoglSpan
{
  float start;
  float size;
  float waste;
} CoglSpan;

typedef struct _CoglSpanIter
{
  int index;
  const CoglSpan *spans;
  int n_spans;
  const CoglSpan *span;
  float pos;
  float next_pos;
  float origin;
  float cover_start;
  float cover_end;
  float intersect_start;
  float intersect_end;
  CoglBool intersects;
  CoglBool flipped;
  CoglPipelineWrapMode wrap_mode;
  int mirror_direction;
} CoglSpanIter;

void
_cogl_span_iter_update (CoglSpanIter *iter);

void
_cogl_span_iter_begin (CoglSpanIter *iter,
                       const CoglSpan *spans,
                       int n_spans,
                       float normalize_factor,
                       float cover_start,
                       float cover_end,
                       CoglPipelineWrapMode wrap_mode);

void
_cogl_span_iter_next (CoglSpanIter *iter);

CoglBool
_cogl_span_iter_end (CoglSpanIter *iter);

#endif /* __COGL_SPANS_PRIVATE_H */
