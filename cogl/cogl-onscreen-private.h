/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011,2013 Intel Corporation.
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
