/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011,2013 Intel Corporation.
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

#ifndef __COGL_ONSCREEN_PRIVATE_H
#define __COGL_ONSCREEN_PRIVATE_H

#include "cogl-onscreen.h"
#include "cogl-framebuffer-private.h"
#include "cogl-closure-list-private.h"
#include "cogl-list.h"

#include <glib.h>

#ifdef COGL_HAS_WIN32_SUPPORT
#include <windows.h>
#endif

typedef struct _CoglOnscreenEvent
{
  CoglList link;

  CoglOnscreen *onscreen;
  CoglFrameInfo *info;
  CoglFrameEvent type;
} CoglOnscreenEvent;

typedef struct _CoglOnscreenQueuedDirty
{
  CoglList link;

  CoglOnscreen *onscreen;
  CoglOnscreenDirtyInfo info;
} CoglOnscreenQueuedDirty;

struct _CoglOnscreen
{
  CoglFramebuffer  _parent;

#ifdef COGL_HAS_X11_SUPPORT
  uint32_t foreign_xid;
  CoglOnscreenX11MaskCallback foreign_update_mask_callback;
  void *foreign_update_mask_data;
#endif

#ifdef COGL_HAS_WIN32_SUPPORT
  HWND foreign_hwnd;
#endif

#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
  struct wl_surface *foreign_surface;
#endif

  CoglBool swap_throttled;

  CoglList frame_closures;

  CoglBool resizable;
  CoglList resize_closures;

  CoglList dirty_closures;

  int64_t frame_counter;
  int64_t swap_frame_counter; /* frame counter at last all to
                               * cogl_onscreen_swap_region() or
                               * cogl_onscreen_swap_buffers() */
  GQueue pending_frame_infos;

  void *winsys;
};

CoglOnscreen *
_cogl_onscreen_new (void);

void
_cogl_framebuffer_winsys_update_size (CoglFramebuffer *framebuffer,
                                      int width, int height);

void
_cogl_onscreen_queue_event (CoglOnscreen *onscreen,
                            CoglFrameEvent type,
                            CoglFrameInfo *info);

void
_cogl_onscreen_notify_frame_sync (CoglOnscreen *onscreen, CoglFrameInfo *info);

void
_cogl_onscreen_notify_complete (CoglOnscreen *onscreen, CoglFrameInfo *info);

void
_cogl_onscreen_notify_resize (CoglOnscreen *onscreen);

void
_cogl_onscreen_queue_dirty (CoglOnscreen *onscreen,
                            const CoglOnscreenDirtyInfo *info);


void
_cogl_onscreen_queue_full_dirty (CoglOnscreen *onscreen);

#endif /* __COGL_ONSCREEN_PRIVATE_H */
