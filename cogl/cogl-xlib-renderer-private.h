/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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

int64_t
_cogl_xlib_renderer_get_dispatch_timeout (CoglRenderer *renderer);

CoglOutput *
_cogl_xlib_renderer_output_for_rectangle (CoglRenderer *renderer,
                                          int x,
                                          int y,
                                          int width,
                                          int height);

#endif /* __COGL_RENDERER_XLIB_PRIVATE_H */
