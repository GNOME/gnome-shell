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

#ifndef __COGL_DISPLAY_PRIVATE_H
#define __COGL_DISPLAY_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-display.h"
#include "cogl-renderer.h"
#include "cogl-onscreen-template.h"

struct _CoglDisplay
{
  CoglObject _parent;

  CoglBool setup;
  CoglRenderer *renderer;
  CoglOnscreenTemplate *onscreen_template;

#ifdef COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT
  struct wl_display *wayland_compositor_display;
#endif

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  gdl_plane_id_t gdl_plane;
#endif

  void *winsys;
};

#endif /* __COGL_DISPLAY_PRIVATE_H */
