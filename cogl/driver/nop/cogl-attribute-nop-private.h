/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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

#ifndef _COGL_ATTRIBUTE_NOP_PRIVATE_H_
#define _COGL_ATTRIBUTE_NOP_PRIVATE_H_

#include "cogl-types.h"
#include "cogl-context-private.h"

void
_cogl_nop_flush_attributes_state (CoglFramebuffer *framebuffer,
                                  CoglPipeline *pipeline,
                                  CoglFlushLayerState *layers_state,
                                  CoglDrawFlags flags,
                                  CoglAttribute **attributes,
                                  int n_attributes);

#endif /* _COGL_ATTRIBUTE_NOP_PRIVATE_H_ */
