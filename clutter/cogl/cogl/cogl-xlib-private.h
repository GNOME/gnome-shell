/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010,2011 Intel Corporation.
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

#ifndef __COGL_XLIB_PRIVATE_H
#define __COGL_XLIB_PRIVATE_H

#include "cogl/cogl.h"

#include <X11/Xlib.h>

typedef struct _CoglXlibTrapState CoglXlibTrapState;

struct _CoglXlibTrapState
{
  /* These values are intended to be internal to
   * _cogl_xlib_{un,}trap_errors but they need to be in the header so
   * that the struct can be allocated on the stack */
  int (* old_error_handler) (Display *, XErrorEvent *);
  int trapped_error_code;
  CoglXlibTrapState *old_state;
};

typedef struct _CoglXlibFilterClosure
{
  CoglXlibFilterFunc func;
  void *data;
} CoglXlibFilterClosure;

void
_cogl_xlib_query_damage_extension (void);

int
_cogl_xlib_get_damage_base (void);

void
_cogl_xlib_trap_errors (CoglXlibTrapState *state);

int
_cogl_xlib_untrap_errors (CoglXlibTrapState *state);

/*
 * _cogl_xlib_add_filter:
 *
 * Adds a callback function that will receive all X11 events. The
 * function can stop further processing of the event by return
 * %COGL_XLIB_FILTER_REMOVE.
 */
void
_cogl_xlib_add_filter (CoglXlibFilterFunc func,
                       void *data);

/*
 * _cogl_xlib_remove_filter:
 *
 * Removes a callback that was previously added with
 * _cogl_xlib_add_filter().
 */
void
_cogl_xlib_remove_filter (CoglXlibFilterFunc func,
                          void *data);

#endif /* __COGL_XLIB_PRIVATE_H */
