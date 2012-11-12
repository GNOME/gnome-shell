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
#include "cogl-context.h"
#include "cogl-output.h"

typedef struct _CoglXlibRenderer
{
  CoglX11Renderer _parent;

  Display *xdpy;

  /* Current top of the XError trap state stack. The actual memory for
     these is expected to be allocated on the stack by the caller */
  CoglXlibTrapState *trap_state;

  /* A poll FD for handling event retrieval within Cogl */
  CoglPollFD poll_fd;

  unsigned long outputs_update_serial;
} CoglXlibRenderer;

CoglBool
_cogl_xlib_renderer_connect (CoglRenderer *renderer, CoglError **error);

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

CoglXlibRenderer *
_cogl_xlib_renderer_get_data (CoglRenderer *renderer);

void
_cogl_xlib_renderer_poll_get_info (CoglRenderer *renderer,
                                   CoglPollFD **poll_fds,
                                   int *n_poll_fds,
                                   int64_t *timeout);

void
_cogl_xlib_renderer_poll_dispatch (CoglRenderer *renderer,
                                   const CoglPollFD *poll_fds,
                                   int n_poll_fds);

CoglOutput *
_cogl_xlib_renderer_output_for_rectangle (CoglRenderer *renderer,
                                          int x,
                                          int y,
                                          int width,
                                          int height);

#endif /* __COGL_RENDERER_XLIB_PRIVATE_H */
