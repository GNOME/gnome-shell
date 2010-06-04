/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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

#ifndef __COGL_XLIB_H
#define __COGL_XLIB_H

#include "cogl.h"
#include "cogl-context-winsys.h"

#include <X11/Xlib.h>

typedef struct _CoglXlibFilterClosure CoglXlibFilterClosure;

struct _CoglXlibFilterClosure
{
  CoglXlibFilterFunc func;
  gpointer data;
};

/*
 * _cogl_xlib_trap_errors:
 * @state: A temporary place to store data for the trap.
 *
 * Traps every X error until _cogl_xlib_untrap_errors() called. You
 * should allocate an uninitialised CoglXlibTrapState struct on the
 * stack to pass to this function. The same pointer should later be
 * passed to _cogl_xlib_untrap_errors(). Calls to
 * _cogl_xlib_trap_errors() can be nested as long as
 * _cogl_xlib_untrap_errors() is called with the corresponding state
 * pointers in reverse order.
 */
void
_cogl_xlib_trap_errors (CoglXlibTrapState *state);

/*
 * _cogl_xlib_untrap_errors:
 * @state: The state that was passed to _cogl_xlib_trap_errors().
 *
 * Removes the X error trap and returns the current status.
 *
 * Return value: the trapped error code, or 0 for success
 */
int
_cogl_xlib_untrap_errors (CoglXlibTrapState *state);

#endif /* __COGL_XLIB_H */
