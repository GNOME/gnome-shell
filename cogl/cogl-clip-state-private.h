/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#ifndef __COGL_CLIP_STATE_PRIVATE_H
#define __COGL_CLIP_STATE_PRIVATE_H

#include "cogl-clip-stack.h"

typedef struct _CoglClipState CoglClipState;

struct _CoglClipState
{
  /* Stack of CoglClipStacks */
  GSList *stacks;
};

void
_cogl_clip_state_init (CoglClipState *state);

void
_cogl_clip_state_destroy (CoglClipState *state);

CoglClipStack *
_cogl_clip_state_get_stack (CoglClipState *clip_state);

void
_cogl_clip_state_set_stack (CoglClipState *clip_state,
                            CoglClipStack *clip_stack);

void
_cogl_clip_state_save_clip_stack (CoglClipState *clip_state);

void
_cogl_clip_state_restore_clip_stack (CoglClipState *clip_state);

#endif /* __COGL_CLIP_STATE_PRIVATE_H */
