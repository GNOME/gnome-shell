/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 * Authors:
 *  Tomeu Vizoso <tomeu.vizoso@collabora.com>
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#ifndef __COGL_GLES2_CONTEXT_PRIVATE_H
#define __COGL_GLES2_CONTEXT_PRIVATE_H

#include <glib.h>

#include "cogl-object-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-queue.h"

typedef struct _CoglGLES2Offscreen CoglGLES2Offscreen;

COGL_LIST_HEAD (CoglGLES2OffscreenList, CoglGLES2Offscreen);

struct _CoglGLES2Offscreen
{
  COGL_LIST_ENTRY (CoglGLES2Offscreen) list_node;
  CoglOffscreen *original_offscreen;
  CoglGLFramebuffer gl_framebuffer;
};

struct _CoglGLES2Context
{
  CoglObject _parent;

  CoglContext *context;

  /* This is set to FALSE until the first time the GLES2 context is
   * bound to something. We need to keep track of this so we can set
   * the viewport and scissor the first time it is bound. */
  CoglBool has_been_bound;

  CoglFramebuffer *read_buffer;
  CoglGLES2Offscreen *gles2_read_buffer;
  CoglFramebuffer *write_buffer;
  CoglGLES2Offscreen *gles2_write_buffer;

  GLuint current_fbo_handle;

  CoglGLES2OffscreenList foreign_offscreens;

  CoglGLES2Vtable *vtable;

  void *winsys;
};

#endif /* __COGL_GLES2_CONTEXT_PRIVATE_H */
