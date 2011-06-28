/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_RENDERER_XLIB_PRIVATE_H
#define __COGL_RENDERER_XLIB_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-xlib-private.h"
#include "cogl-x11-renderer-private.h"

typedef struct _CoglXlibRenderer
{
  CoglX11Renderer _parent;

  Display *xdpy;

  /* Current top of the XError trap state stack. The actual memory for
     these is expected to be allocated on the stack by the caller */
  CoglXlibTrapState *trap_state;
} CoglXlibRenderer;

gboolean
_cogl_xlib_renderer_connect (CoglRenderer *renderer, GError **error);

void
_cogl_xlib_renderer_disconnect (CoglRenderer *renderer);

/*
 * cogl_xlib_renderer_trap_errors:
 * @state: A temporary place to store data for the trap.
 *
 * Traps every X error until _cogl_xlib_renderer_untrap_errors()
 * called. You should allocate an uninitialised CoglXlibTrapState
 * struct on the stack to pass to this function. The same pointer
 * should later be passed to _cogl_xlib_renderer_untrap_errors().
 *
 * Calls to _cogl_xlib_renderer_trap_errors() can be nested as long as
 * _cogl_xlib_renderer_untrap_errors() is called with the
 * corresponding state pointers in reverse order.
 */
void
_cogl_xlib_renderer_trap_errors (CoglRenderer *renderer,
                                 CoglXlibTrapState *state);

/*
 * cogl_xlib_renderer_untrap_errors:
 * @state: The state that was passed to _cogl_xlib_renderer_trap_errors().
 *
 * Removes the X error trap and returns the current status.
 *
 * Return value: the trapped error code, or 0 for success
 */
int
_cogl_xlib_renderer_untrap_errors (CoglRenderer *renderer,
                                   CoglXlibTrapState *state);

#endif /* __COGL_RENDERER_XLIB_PRIVATE_H */
