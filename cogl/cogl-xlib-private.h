/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010,2011 Intel Corporation.
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
