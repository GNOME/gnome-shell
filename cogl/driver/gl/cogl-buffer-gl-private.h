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

#ifndef _COGL_BUFFER_GL_PRIVATE_H_
#define _COGL_BUFFER_GL_PRIVATE_H_

#include "cogl-types.h"
#include "cogl-context.h"
#include "cogl-buffer.h"
#include "cogl-buffer-private.h"

void
_cogl_buffer_gl_create (CoglBuffer *buffer);

void
_cogl_buffer_gl_destroy (CoglBuffer *buffer);

void *
_cogl_buffer_gl_map (CoglBuffer *buffer,
                     CoglBufferAccess  access,
                     CoglBufferMapHint hints);

void
_cogl_buffer_gl_unmap (CoglBuffer *buffer);

CoglBool
_cogl_buffer_gl_set_data (CoglBuffer *buffer,
                          unsigned int offset,
                          const void *data,
                          unsigned int size);

void *
_cogl_buffer_gl_bind (CoglBuffer *buffer, CoglBufferBindTarget target);

void
_cogl_buffer_gl_unbind (CoglBuffer *buffer);

#endif /* _COGL_BUFFER_GL_PRIVATE_H_ */
