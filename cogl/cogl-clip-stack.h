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

#ifndef __COGL_CLIP_STACK_H
#define __COGL_CLIP_STACK_H

typedef struct _CoglClipStackState CoglClipStackState;

struct _CoglClipStackState
{
  /* Stack of stacks */
  GSList *stacks;

  gboolean stack_dirty;
  gboolean stencil_used;
};

void
_cogl_clip_stack_state_init (CoglClipStackState *state);

void
_cogl_clip_stack_state_destroy (CoglClipStackState *state);

void
_cogl_clip_stack_state_dirty (CoglClipStackState *state);

void
_cogl_flush_clip_state (CoglClipStackState *clip_state);

#endif /* __COGL_CLIP_STACK_H */
