/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2016 Red Hat Inc.
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

#ifndef __COGL_MUTTER_H___
#define __COGL_MUTTER_H___

#include "cogl-mutter-config.h"
#include "cogl-defines.h"

#include <cogl/cogl-texture.h>
#include <cogl/cogl-meta-texture.h>
#include <cogl/cogl-error-private.h>
#include <cogl/cogl-frame-info-private.h>
#include <cogl/cogl-renderer-private.h>
#include <cogl/winsys/cogl-winsys-egl-private.h>
#include <cogl/winsys/cogl-winsys-private.h>

void cogl_renderer_set_custom_winsys (CoglRenderer          *renderer,
                                      CoglWinsysVtableGetter winsys_vtable_getter);

#endif /* __COGL_MUTTER_H___ */
