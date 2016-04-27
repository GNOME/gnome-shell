/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Collabora Ltd.
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

#ifndef __COGL_FENCE_PRIVATE_H__
#define __COGL_FENCE_PRIVATE_H__

#include "cogl-fence.h"
#include "cogl-list.h"
#include "cogl-winsys-private.h"

typedef enum
{
  FENCE_TYPE_PENDING,
#ifdef GL_ARB_sync
  FENCE_TYPE_GL_ARB,
#endif
  FENCE_TYPE_WINSYS,
  FENCE_TYPE_ERROR
} CoglFenceType;

struct _CoglFenceClosure
{
  CoglList link;
  CoglFramebuffer *framebuffer;

  CoglFenceType type;
  void *fence_obj;

  CoglFenceCallback callback;
  void *user_data;
};

void
_cogl_fence_submit (CoglFenceClosure *fence);

void
_cogl_fence_cancel_fences_for_framebuffer (CoglFramebuffer *framebuffer);

#endif /* __COGL_FENCE_PRIVATE_H__ */
