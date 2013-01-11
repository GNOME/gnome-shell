/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Collabora Ltd.
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

#ifndef __COGL_FENCE_PRIVATE_H__
#define __COGL_FENCE_PRIVATE_H__

#include "cogl-fence.h"
#include "cogl-queue.h"
#include "cogl-winsys-private.h"

COGL_TAILQ_HEAD (CoglFenceList, CoglFenceClosure);

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
  COGL_TAILQ_ENTRY (CoglFenceClosure) list;
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
