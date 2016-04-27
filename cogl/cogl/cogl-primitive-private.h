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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_PRIMITIVE_PRIVATE_H
#define __COGL_PRIMITIVE_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-attribute-buffer-private.h"
#include "cogl-attribute-private.h"
#include "cogl-framebuffer.h"

struct _CoglPrimitive
{
  CoglObject _parent;

  CoglIndices *indices;
  CoglVerticesMode mode;
  int first_vertex;
  int n_vertices;

  int immutable_ref;

  CoglAttribute **attributes;
  int n_attributes;

  int n_embedded_attributes;
  CoglAttribute *embedded_attribute;
};

CoglPrimitive *
_cogl_primitive_immutable_ref (CoglPrimitive *primitive);

void
_cogl_primitive_immutable_unref (CoglPrimitive *primitive);

void
_cogl_primitive_draw (CoglPrimitive *primitive,
                      CoglFramebuffer *framebuffer,
                      CoglPipeline *pipeline,
                      CoglDrawFlags flags);

#endif /* __COGL_PRIMITIVE_PRIVATE_H */

