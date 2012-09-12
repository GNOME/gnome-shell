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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-types.h"
#include "cogl-framebuffer.h"
#include "cogl-attribute.h"
#include "cogl-attribute-private.h"
#include "cogl-attribute-nop-private.h"

void
_cogl_nop_flush_attributes_state (CoglFramebuffer *framebuffer,
                                  CoglPipeline *pipeline,
                                  CoglFlushLayerState *layers_state,
                                  CoglDrawFlags flags,
                                  CoglAttribute **attributes,
                                  int n_attributes)
{
}
