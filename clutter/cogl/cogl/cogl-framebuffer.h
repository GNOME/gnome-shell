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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_FRAMEBUFFER_H
#define __COGL_FRAMEBUFFER_H

#include <glib.h>


G_BEGIN_DECLS

#ifdef COGL_ENABLE_EXPERIMENTAL_API
#define cogl_onscreen_new cogl_onscreen_new_EXP

#define COGL_FRAMEBUFFER(X) ((CoglFramebuffer *)(X))

#define cogl_framebuffer_allocate cogl_framebuffer_allocate_EXP
gboolean
cogl_framebuffer_allocate (CoglFramebuffer *framebuffer,
                           GError **error);

#define cogl_framebuffer_swap_buffers cogl_framebuffer_swap_buffers_EXP
void
cogl_framebuffer_swap_buffers (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_swap_region cogl_framebuffer_swap_region_EXP
void
cogl_framebuffer_swap_region (CoglFramebuffer *framebuffer,
                              int *rectangles,
                              int n_rectangles);


typedef void (*CoglSwapBuffersNotify) (CoglFramebuffer *framebuffer,
                                       void *user_data);

#define cogl_framebuffer_add_swap_buffers_callback \
  cogl_framebuffer_add_swap_buffers_callback_EXP
unsigned int
cogl_framebuffer_add_swap_buffers_callback (CoglFramebuffer *framebuffer,
                                            CoglSwapBuffersNotify callback,
                                            void *user_data);

#define cogl_framebuffer_remove_swap_buffers_callback \
  cogl_framebuffer_remove_swap_buffers_callback_EXP
void
cogl_framebuffer_remove_swap_buffers_callback (CoglFramebuffer *framebuffer,
                                               unsigned int id);


typedef struct _CoglOnscreen CoglOnscreen;
#define COGL_ONSCREEN(X) ((CoglOnscreen *)(X))

CoglOnscreen *
cogl_onscreen_new (CoglContext *context, int width, int height);

#ifdef COGL_HAS_X11
#define cogl_onscreen_x11_set_foreign_window_xid \
  cogl_onscreen_x11_set_foreign_window_xid_EXP
void
cogl_onscreen_x11_set_foreign_window_xid (CoglOnscreen *onscreen,
                                          guint32 xid);

#define cogl_onscreen_x11_get_window_xid cogl_onscreen_x11_get_window_xid_EXP
guint32
cogl_onscreen_x11_get_window_xid (CoglOnscreen *onscreen);

#define cogl_onscreen_x11_get_visual_xid cogl_onscreen_x11_get_visual_xid_EXP
guint32
cogl_onscreen_x11_get_visual_xid (CoglOnscreen *onscreen);
#endif /* COGL_HAS_X11 */

void
cogl_onscreen_set_swap_throttled (CoglOnscreen *onscreen,
                                  gboolean throttled);

#define cogl_get_draw_framebuffer cogl_get_draw_framebuffer_EXP
CoglFramebuffer *
cogl_get_draw_framebuffer (void);

#endif /* COGL_ENABLE_EXPERIMENTAL_API */

G_END_DECLS

#endif /* __COGL_FRAMEBUFFER_H */

