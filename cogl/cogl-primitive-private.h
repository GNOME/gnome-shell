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

