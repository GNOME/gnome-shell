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

#ifndef __COGL_INDICES_PRIVATE_H
#define __COGL_INDICES_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-index-buffer-private.h"
#include "cogl-types.h"

struct _CoglIndices
{
  CoglObject _parent;

  CoglIndexBuffer *buffer;
  size_t offset;

  CoglIndicesType type;

  int immutable_ref;
};

CoglIndices *
_cogl_indices_immutable_ref (CoglIndices *indices);

void
_cogl_indices_immutable_unref (CoglIndices *indices);

#endif /* __COGL_INDICES_PRIVATE_H */

