/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __COGL_FRAMEBUFFER_PRIVATE_H
#define __COGL_FRAMEBUFFER_PRIVATE_H

#include "cogl-handle.h"
#include "cogl-matrix-stack.h"
#include "cogl-clip-stack.h"

typedef enum _CoglFramebufferType {
  COGL_FRAMEBUFFER_TYPE_ONSCREEN,
  COGL_FRAMEBUFFER_TYPE_OFFSCREEN
} CoglFramebufferType;

typedef struct
{
  CoglHandleObject    _parent;
  CoglFramebufferType  type;
  int                 width;
  int                 height;

  CoglMatrixStack    *modelview_stack;
  CoglMatrixStack    *projection_stack;
  int                 viewport_x;
  int                 viewport_y;
  int                 viewport_width;
  int                 viewport_height;

  CoglClipStackState  clip_state;
} CoglFramebuffer;

#define COGL_FRAMEBUFFER(X) ((CoglFramebuffer *)(X))

typedef struct _CoglOffscreen
{
  CoglFramebuffer  _parent;
  GLuint          fbo_handle;
  GSList          *renderbuffers;
  CoglHandle      texture;
} CoglOffscreen;

#define COGL_OFFSCREEN(X) ((CoglOffscreen *)(X))

typedef struct _CoglOnscreen
{
  CoglFramebuffer  _parent;
} CoglOnscreen;

#define COGL_ONSCREEN(X) ((CoglOnscreen *)(X))

void
_cogl_framebuffer_state_init (void);
int
_cogl_framebuffer_get_width (CoglHandle handle);
int
_cogl_framebuffer_get_height (CoglHandle handle);
CoglClipStackState *
_cogl_framebuffer_get_clip_state (CoglHandle handle);
void
_cogl_framebuffer_set_viewport (CoglHandle handle,
                                int x,
                                int y,
                                int width,
                                int height);
int
_cogl_framebuffer_get_viewport_x (CoglHandle handle);
int
_cogl_framebuffer_get_viewport_y (CoglHandle handle);
int
_cogl_framebuffer_get_viewport_width (CoglHandle handle);
int
_cogl_framebuffer_get_viewport_height (CoglHandle handle);
void
_cogl_framebuffer_get_viewport4fv (CoglHandle handle, int *viewport);
CoglMatrixStack *
_cogl_framebuffer_get_modelview_stack (CoglHandle handle);
CoglMatrixStack *
_cogl_framebuffer_get_projection_stack (CoglHandle handle);

typedef enum _CoglFramebufferFlushFlags
{
  /* XXX: When using this, that imples you are going to manually load the
   * modelview matrix (via glLoadMatrix). _cogl_matrix_stack_flush_to_gl wont
   * be called for framebuffer->modelview_stack, and the modelview_stack will
   * also be marked as dirty. */
  COGL_FRAMEBUFFER_FLUSH_SKIP_MODELVIEW =     1L<<0,
} CoglFramebufferFlushFlags;

void
_cogl_framebuffer_flush_state (CoglHandle handle,
                               CoglFramebufferFlushFlags flags);

CoglHandle
_cogl_onscreen_new (void);

CoglHandle
_cogl_get_framebuffer (void);
GSList *
_cogl_create_framebuffer_stack (void);
void
_cogl_free_framebuffer_stack (GSList *stack);

#endif /* __COGL_FRAMEBUFFER_PRIVATE_H */

