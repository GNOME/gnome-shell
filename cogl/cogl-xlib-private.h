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

void
_cogl_xlib_query_damage_extension (void);

int
_cogl_xlib_get_damage_base (void);

#endif /* __COGL_XLIB_PRIVATE_H */
