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

#ifndef __COGL_CLIP_STATE_H
#define __COGL_CLIP_STATE_H

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

void
_cogl_clip_state_flush (CoglClipState *clip_state);

/*
 * _cogl_get_clip_stack:
 *
 * Gets a pointer to the current clip stack. This can be used to later
 * return to the same clip stack state with _cogl_set_clip_stack(). A
 * reference is not taken on the stack so if you want to keep it you
 * should call _cogl_clip_stack_ref().
 *
 * Return value: a pointer to the current clip stack.
 */
CoglClipStack *
_cogl_get_clip_stack (void);

/*
 * _cogl_set_clip_stack:
 * @stack: a pointer to the replacement clip stack
 *
 * Replaces the current clip stack with @stack.
 */
void
_cogl_set_clip_stack (CoglClipStack *stack);

#endif /* __COGL_CLIP_STATE_H */
