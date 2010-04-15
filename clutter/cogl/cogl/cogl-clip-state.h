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

  gboolean stack_dirty;
  gboolean stencil_used;
};

void
_cogl_clip_state_init (CoglClipState *state);

void
_cogl_clip_state_destroy (CoglClipState *state);

void
_cogl_clip_state_dirty (CoglClipState *state);

void
_cogl_clip_state_flush (CoglClipState *clip_state);

/* TODO: we may want to make these two functions public because they
 * can be used to implement a better API than cogl_clip_stack_save()
 * and cogl_clip_stack_restore().
 */
/*
 * _cogl_get_clip_stack:
 *
 * Gets a handle to the current clip stack. This can be used to later
 * return to the same clip stack state with _cogl_set_clip_stack(). A
 * reference is not taken on the stack so if you want to keep it you
 * should call cogl_handle_ref() or _cogl_clip_stack_copy().
 *
 * Return value: a handle to the current clip stack.
 */
CoglHandle
_cogl_get_clip_stack (void);

/*
 * _cogl_set_clip_stack:
 * @handle: a handle to the replacement clip stack
 *
 * Replaces the current clip stack with @handle.
 *
 * Return value: a handle to the current clip stack.
 */
void
_cogl_set_clip_stack (CoglHandle handle);

#endif /* __COGL_CLIP_STATE_H */
