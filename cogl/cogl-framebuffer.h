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

#ifdef COGL_HAS_WIN32_SUPPORT
#include <windows.h>
#endif /* COGL_HAS_WIN32_SUPPORT */

#if COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
#include <wayland-client.h>
#endif /* COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT */

G_BEGIN_DECLS

#ifdef COGL_ENABLE_EXPERIMENTAL_API
#define cogl_onscreen_new cogl_onscreen_new_EXP

#define COGL_FRAMEBUFFER(X) ((CoglFramebuffer *)(X))

#define cogl_framebuffer_allocate cogl_framebuffer_allocate_EXP
gboolean
cogl_framebuffer_allocate (CoglFramebuffer *framebuffer,
                           GError **error);

#define cogl_framebuffer_get_width cogl_framebuffer_get_width_EXP
int
cogl_framebuffer_get_width (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_get_height cogl_framebuffer_get_height_EXP
int
cogl_framebuffer_get_height (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_set_viewport cogl_framebuffer_set_viewport_EXP
void
cogl_framebuffer_set_viewport (CoglFramebuffer *framebuffer,
                               float x,
                               float y,
                               float width,
                               float height);

#define cogl_framebuffer_get_viewport_x cogl_framebuffer_get_viewport_x_EXP
float
cogl_framebuffer_get_viewport_x (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_get_viewport_y cogl_framebuffer_get_viewport_y_EXP
float
cogl_framebuffer_get_viewport_y (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_get_viewport_width cogl_framebuffer_get_viewport_width_EXP
float
cogl_framebuffer_get_viewport_width (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_get_viewport_height cogl_framebuffer_get_viewport_height_EXP
float
cogl_framebuffer_get_viewport_height (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_get_viewport4fv cogl_framebuffer_get_viewport4fv_EXP
void
cogl_framebuffer_get_viewport4fv (CoglFramebuffer *framebuffer,
                                  float *viewport);

/**
 * cogl_framebuffer_get_red_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of red bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 1.8
 * Stability: unstable
 */
#define cogl_framebuffer_get_red_bits cogl_framebuffer_get_red_bits_EXP
int
cogl_framebuffer_get_red_bits (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_green_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of green bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 1.8
 * Stability: unstable
 */
#define cogl_framebuffer_get_green_bits cogl_framebuffer_get_green_bits_EXP
int
cogl_framebuffer_get_green_bits (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_blue_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of blue bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 1.8
 * Stability: unstable
 */
#define cogl_framebuffer_get_blue_bits cogl_framebuffer_get_blue_bits_EXP
int
cogl_framebuffer_get_blue_bits (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_alpha_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of alpha bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 1.8
 * Stability: unstable
 */
#define cogl_framebuffer_get_blue_bits cogl_framebuffer_get_blue_bits_EXP
int
cogl_framebuffer_get_alpha_bits (CoglFramebuffer *framebuffer);

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
typedef void (*CoglOnscreenX11MaskCallback) (CoglOnscreen *onscreen,
                                             guint32 event_mask,
                                             void *user_data);

/**
 * cogl_onscreen_x11_set_foreign_window_xid:
 * @onscreen: The unallocated framebuffer to associated with an X
 *            window.
 * @xid: The XID of an existing X window
 * @update: A callback that notifies of updates to what Cogl requires
 *          to be in the core X protocol event mask.
 *
 * Ideally we would recommend that you let Cogl be responsible for
 * creating any X window required to back an onscreen framebuffer but
 * if you really need to target a window created manually this
 * function can be called before @onscreen has been allocated to set a
 * foreign XID for your existing X window.
 *
 * Since Cogl needs, for example, to track changes to the size of an X
 * window it requires that certain events be selected for via the core
 * X protocol. This requirement may also be changed asynchronously so
 * you must pass in an @update callback to inform you of Cogl's
 * required event mask.
 *
 * For example if you are using Xlib you could use this API roughly
 * as follows:
 * [{
 * static void
 * my_update_cogl_x11_event_mask (CoglOnscreen *onscreen,
 *                                guint32 event_mask,
 *                                void *user_data)
 * {
 *   XSetWindowAttributes attrs;
 *   MyData *data = user_data;
 *   attrs.event_mask = event_mask | data->my_event_mask;
 *   XChangeWindowAttributes (data->xdpy,
 *                            data->xwin,
 *                            CWEventMask,
 *                            &attrs);
 * }
 *
 * {
 *   *snip*
 *   cogl_onscreen_x11_set_foreign_window_xid (onscreen,
 *                                             data->xwin,
 *                                             my_update_cogl_x11_event_mask,
 *                                             data);
 *   *snip*
 * }
 * }]
 *
 * Since: 2.0
 * Stability: Unstable
 */
#define cogl_onscreen_x11_set_foreign_window_xid \
  cogl_onscreen_x11_set_foreign_window_xid_EXP
void
cogl_onscreen_x11_set_foreign_window_xid (CoglOnscreen *onscreen,
                                          guint32 xid,
                                          CoglOnscreenX11MaskCallback update,
                                          void *user_data);

#define cogl_onscreen_x11_get_window_xid cogl_onscreen_x11_get_window_xid_EXP
guint32
cogl_onscreen_x11_get_window_xid (CoglOnscreen *onscreen);

#define cogl_onscreen_x11_get_visual_xid cogl_onscreen_x11_get_visual_xid_EXP
guint32
cogl_onscreen_x11_get_visual_xid (CoglOnscreen *onscreen);
#endif /* COGL_HAS_X11 */

#ifdef COGL_HAS_WIN32_SUPPORT
#define cogl_onscreen_win32_set_foreign_window \
  cogl_onscreen_win32_set_foreign_window_EXP
void
cogl_onscreen_win32_set_foreign_window (CoglOnscreen *onscreen,
                                        HWND hwnd);

#define cogl_onscreen_win32_get_window cogl_onscreen_win32_get_window_EXP
HWND
cogl_onscreen_win32_get_window (CoglOnscreen *onscreen);
#endif /* COGL_HAS_WIN32_SUPPORT */

#if COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
struct wl_surface *
cogl_wayland_onscreen_get_surface (CoglOnscreen *onscreen);
#endif /* COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT */

#define cogl_onscreen_set_swap_throttled cogl_onscreen_set_swap_throttled_EXP
void
cogl_onscreen_set_swap_throttled (CoglOnscreen *onscreen,
                                  gboolean throttled);

/**
 * cogl_onscreen_show:
 * @onscreen: The onscreen framebuffer to make visible
 *
 * This requests to make @onscreen visible to the user.
 *
 * Actually the precise semantics of this function depend on the
 * window system currently in use, and if you don't have a
 * multi-windowining system this function may in-fact do nothing.
 *
 * This function will implicitly allocate the given @onscreen
 * framebuffer before showing it if it hasn't already been allocated.
 *
 * <note>Since Cogl doesn't explicitly track the visibility status of
 * onscreen framebuffers it wont try to avoid redundant window system
 * requests e.g. to show an already visible window. This also means
 * that it's acceptable to alternatively use native APIs to show and
 * hide windows without confusing Cogl.</note>
 *
 * Since: 2.0
 * Stability: Unstable
 */
#define cogl_onscreen_show cogl_onscreen_show_EXP
void
cogl_onscreen_show (CoglOnscreen *onscreen);

/**
 * cogl_onscreen_hide:
 * @onscreen: The onscreen framebuffer to make invisible
 *
 * This requests to make @onscreen invisible to the user.
 *
 * Actually the precise semantics of this function depend on the
 * window system currently in use, and if you don't have a
 * multi-windowining system this function may in-fact do nothing.
 *
 * This function does not implicitly allocate the given @onscreen
 * framebuffer before hiding it.
 *
 * <note>Since Cogl doesn't explicitly track the visibility status of
 * onscreen framebuffers it wont try to avoid redundant window system
 * requests e.g. to show an already visible window. This also means
 * that it's acceptable to alternatively use native APIs to show and
 * hide windows without confusing Cogl.</note>
 *
 * Since: 2.0
 * Stability: Unstable
 */
#define cogl_onscreen_hide cogl_onscreen_hide_EXP
void
cogl_onscreen_hide (CoglOnscreen *onscreen);

#define cogl_get_draw_framebuffer cogl_get_draw_framebuffer_EXP
CoglFramebuffer *
cogl_get_draw_framebuffer (void);

#endif /* COGL_ENABLE_EXPERIMENTAL_API */

G_END_DECLS

#endif /* __COGL_FRAMEBUFFER_H */

