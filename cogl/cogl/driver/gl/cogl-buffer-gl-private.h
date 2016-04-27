/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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
_cogl_buffer_gl_map_range (CoglBuffer *buffer,
                           size_t offset,
                           size_t size,
                           CoglBufferAccess access,
                           CoglBufferMapHint hints,
                           CoglError **error);

void
_cogl_buffer_gl_unmap (CoglBuffer *buffer);

CoglBool
_cogl_buffer_gl_set_data (CoglBuffer *buffer,
                          unsigned int offset,
                          const void *data,
                          unsigned int size,
                          CoglError **error);

void *
_cogl_buffer_gl_bind (CoglBuffer *buffer,
                      CoglBufferBindTarget target,
                      CoglError **error);

void
_cogl_buffer_gl_unbind (CoglBuffer *buffer);

#endif /* _COGL_BUFFER_GL_PRIVATE_H_ */
