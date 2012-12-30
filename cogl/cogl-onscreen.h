/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011,2012 Intel Corporation.
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

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_ONSCREEN_H
#define __COGL_ONSCREEN_H

#include <cogl/cogl-context.h>
#include <cogl/cogl-framebuffer.h>

COGL_BEGIN_DECLS

typedef struct _CoglOnscreen CoglOnscreen;
#define COGL_ONSCREEN(X) ((CoglOnscreen *)(X))

/**
 * cogl_onscreen_new:
 * @context: A #CoglContext
 * @width: The desired framebuffer width
 * @height: The desired framebuffer height
 *
 * Instantiates an "unallocated" #CoglOnscreen framebuffer that may be
 * configured before later being allocated, either implicitly when
 * it is first used or explicitly via cogl_framebuffer_allocate().
 *
 * Return value: A newly instantiated #CoglOnscreen framebuffer
 * Since: 1.8
 * Stability: unstable
 */
CoglOnscreen *
cogl_onscreen_new (CoglContext *context, int width, int height);

#ifdef COGL_HAS_X11
typedef void (*CoglOnscreenX11MaskCallback) (CoglOnscreen *onscreen,
                                             uint32_t event_mask,
                                             void *user_data);

/**
 * cogl_x11_onscreen_set_foreign_window_xid:
 * @onscreen: The unallocated framebuffer to associated with an X
 *            window.
 * @xid: The XID of an existing X window
 * @update: A callback that notifies of updates to what Cogl requires
 *          to be in the core X protocol event mask.
 * @user_data: user data passed to @update
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
 *                                uint32_t event_mask,
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
 *   cogl_x11_onscreen_set_foreign_window_xid (onscreen,
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
void
cogl_x11_onscreen_set_foreign_window_xid (CoglOnscreen *onscreen,
                                          uint32_t xid,
                                          CoglOnscreenX11MaskCallback update,
                                          void *user_data);

/**
 * cogl_x11_onscreen_get_window_xid:
 * @onscreen: A #CoglOnscreen framebuffer
 *
 * Assuming you know the given @onscreen framebuffer is based on an x11 window
 * this queries the XID of that window. If
 * cogl_x11_onscreen_set_foreign_window_xid() was previously called then it
 * will return that same XID otherwise it will be the XID of a window Cogl
 * created internally. If the window has not been allocated yet and a foreign
 * xid has not been set then it's undefined what value will be returned.
 *
 * It's undefined what this function does if called when not using an x11 based
 * renderer.
 *
 * Since: 1.10
 * Stability: unstable
 */
uint32_t
cogl_x11_onscreen_get_window_xid (CoglOnscreen *onscreen);

/* XXX: we should maybe remove this, since nothing currently uses
 * it and the current implementation looks dubious. */
uint32_t
cogl_x11_onscreen_get_visual_xid (CoglOnscreen *onscreen);
#endif /* COGL_HAS_X11 */

#ifdef COGL_HAS_WIN32_SUPPORT
/**
 * cogl_win32_onscreen_set_foreign_window:
 * @onscreen: A #CoglOnscreen framebuffer
 * @hwnd: A win32 window handle
 *
 * Ideally we would recommend that you let Cogl be responsible for
 * creating any window required to back an onscreen framebuffer but
 * if you really need to target a window created manually this
 * function can be called before @onscreen has been allocated to set a
 * foreign XID for your existing X window.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_win32_onscreen_set_foreign_window (CoglOnscreen *onscreen,
                                        HWND hwnd);

/**
 * cogl_win32_onscreen_get_window:
 * @onscreen: A #CoglOnscreen framebuffer
 *
 * Queries the internally created window HWND backing the given @onscreen
 * framebuffer.  If cogl_win32_onscreen_set_foreign_window() has been used then
 * it will return the same handle set with that API.
 *
 * Since: 1.10
 * Stability: unstable
 */
HWND
cogl_win32_onscreen_get_window (CoglOnscreen *onscreen);
#endif /* COGL_HAS_WIN32_SUPPORT */

#if defined (COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
struct wl_surface *
cogl_wayland_onscreen_get_surface (CoglOnscreen *onscreen);
struct wl_shell_surface *
cogl_wayland_onscreen_get_shell_surface (CoglOnscreen *onscreen);

/**
 * cogl_wayland_onscreen_resize:
 * @onscreen: A #CoglOnscreen framebuffer
 * @width: The desired width of the framebuffer
 * @height: The desired height of the framebuffer
 * @offset_x: A relative x offset for the new framebuffer
 * @offset_y: A relative x offset for the new framebuffer
 *
 * Queues a resize of the given @onscreen framebuffer which will be applied
 * during the next swap buffers request. Since a buffer is usually conceptually
 * scaled with a center point the @offset_x and @offset_y arguments allow the
 * newly allocated buffer to be positioned relative to the old buffer size.
 *
 * For example a buffer that is being resized by moving the bottom right
 * corner, and the top left corner is remaining static would use x and y
 * offsets of (0, 0) since the top-left of the new buffer should have the same
 * position as the old buffer. If the center of the old buffer is being zoomed
 * into then all the corners of the new buffer move out from the center and the x
 * and y offsets would be (-half_x_size_increase, -half_y_size_increase) where
 * x/y_size_increase is how many pixels bigger the buffer is on the x and y
 * axis.
 *
 * If cogl_wayland_onscreen_resize() is called multiple times before the next
 * swap buffers request then the relative x and y offsets accumulate instead of
 * being replaced. The @width and @height values superseed the old values.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_wayland_onscreen_resize (CoglOnscreen *onscreen,
                              int           width,
                              int           height,
                              int           offset_x,
                              int           offset_y);
#endif /* COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT */

/**
 * cogl_onscreen_set_swap_throttled:
 * @onscreen: A #CoglOncsreen framebuffer
 * @throttled: Whether swap throttling is wanted or not.
 *
 * Requests that the given @onscreen framebuffer should have swap buffer
 * requests (made using cogl_framebuffer_swap_buffers()) throttled either by a
 * displays vblank period or perhaps some other mechanism in a composited
 * environment.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_onscreen_set_swap_throttled (CoglOnscreen *onscreen,
                                  CoglBool throttled);

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
void
cogl_onscreen_hide (CoglOnscreen *onscreen);

/**
 * cogl_onscreen_swap_buffers:
 * @onscreen: A #CoglOnscreen framebuffer
 *
 * Swaps the current back buffer being rendered too, to the front for display.
 *
 * This function also implicitly discards the contents of the color, depth and
 * stencil buffers as if cogl_framebuffer_discard_buffers() were used. The
 * significance of the discard is that you should not expect to be able to
 * start a new frame that incrementally builds on the contents of the previous
 * frame.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_onscreen_swap_buffers (CoglOnscreen *onscreen);

/**
 * cogl_onscreen_swap_region:
 * @onscreen: A #CoglOnscreen framebuffer
 * @rectangles: An array of integer 4-tuples representing rectangles as
 *              (x, y, width, height) tuples.
 * @n_rectangles: The number of 4-tuples to be read from @rectangles
 *
 * Swaps a region of the back buffer being rendered too, to the front for
 * display.  @rectangles represents the region as array of @n_rectangles each
 * defined by 4 sequential (x, y, width, height) integers.
 *
 * This function also implicitly discards the contents of the color, depth and
 * stencil buffers as if cogl_onscreen_discard_buffers() were used. The
 * significance of the discard is that you should not expect to be able to
 * start a new frame that incrementally builds on the contents of the previous
 * frame.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_onscreen_swap_region (CoglOnscreen *onscreen,
                           const int *rectangles,
                           int n_rectangles);


typedef void (*CoglSwapBuffersNotify) (CoglFramebuffer *framebuffer,
                                       void *user_data);

/**
 * cogl_onscreen_add_swap_buffers_callback:
 * @onscreen: A #CoglOnscreen framebuffer
 * @callback: A callback function to call when a swap has completed
 * @user_data: A private pointer to be passed to @callback
 *
 * Installs a @callback function that should be called whenever a swap buffers
 * request (made using cogl_onscreen_swap_buffers()) for the given
 * @onscreen completes.
 *
 * <note>Applications should check for the %COGL_FEATURE_ID_SWAP_BUFFERS_EVENT
 * feature before using this API. It's currently undefined when and if
 * registered callbacks will be called if this feature is not supported.</note>
 *
 * We recommend using this mechanism when available to manually throttle your
 * applications (in conjunction with  cogl_onscreen_set_swap_throttled()) so
 * your application will be able to avoid long blocks in the driver caused by
 * throttling when you request to swap buffers too quickly.
 *
 * Return value: a unique identifier that can be used to remove to remove
 *               the callback later.
 * Since: 1.10
 * Stability: unstable
 */
unsigned int
cogl_onscreen_add_swap_buffers_callback (CoglOnscreen *onscreen,
                                         CoglSwapBuffersNotify callback,
                                         void *user_data);

/**
 * cogl_onscreen_remove_swap_buffers_callback:
 * @onscreen: A #CoglOnscreen framebuffer
 * @id: An identifier returned from cogl_onscreen_add_swap_buffers_callback()
 *
 * Removes a callback that was previously registered
 * using cogl_onscreen_add_swap_buffers_callback().
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_onscreen_remove_swap_buffers_callback (CoglOnscreen *onscreen,
                                            unsigned int id);

/**
 * cogl_onscreen_set_resizable:
 * @onscreen: A #CoglOnscreen framebuffer
 *
 * Lets you request Cogl to mark an @onscreen framebuffer as
 * resizable or not.
 *
 * By default, if possible, a @onscreen will be created by Cogl
 * as non resizable, but it is not guaranteed that this is always
 * possible for all window systems.
 *
 * <note>Cogl does not know whether marking the @onscreen framebuffer
 * is truly meaningful for your current window system (consider
 * applications being run fullscreen on a phone or TV) so this
 * function may not have any useful effect. If you are running on a
 * multi windowing system such as X11 or Win32 or OSX then Cogl will
 * request to the window system that users be allowed to resize the
 * @onscreen, although it's still possible that some other window
 * management policy will block this possibility.</note>
 *
 * <note>Whenever an @onscreen framebuffer is resized the viewport
 * will be automatically updated to match the new size of the
 * framebuffer with an origin of (0,0). If your application needs more
 * specialized control of the viewport it will need to register a
 * resize handler using cogl_onscreen_add_resize_handler() so that it
 * can track when the viewport has been changed automatically.</note>
 *
 * Since: 2.0
 */
void
cogl_onscreen_set_resizable (CoglOnscreen *onscreen,
                             CoglBool resizable);

/**
 * cogl_onscreen_get_resizable:
 * @onscreen: A #CoglOnscreen framebuffer
 *
 * Lets you query whether @onscreen has been marked as resizable via
 * the cogl_onscreen_set_resizable() api.
 *
 * By default, if possible, a @onscreen will be created by Cogl
 * as non resizable, but it is not guaranteed that this is always
 * possible for all window systems.
 *
 * <note>If cogl_onscreen_set_resizable(@onscreen, %TRUE) has been
 * previously called then this function will return %TRUE, but it's
 * possible that the current windowing system being used does not
 * support window resizing (consider fullscreen windows on a phone or
 * a TV). This function is not aware of whether resizing is truly
 * meaningful with your window system, only whether the @onscreen has
 * been marked as resizable.</note>
 *
 * Return value: Returns whether @onscreen has been marked as
 *               resizable or not.
 * Since: 2.0
 */
CoglBool
cogl_onscreen_get_resizable (CoglOnscreen *onscreen);

/**
 * CoglOnscreenResizeCallback:
 * @onscreen: A #CoglOnscreen framebuffer that was resized
 * @width: The new width of @onscreen
 * @height: The new height of @onscreen
 * @user_data: The private passed to
 *             cogl_onscreen_add_resize_handler()
 *
 * Is a callback type used with the
 * cogl_onscreen_add_resize_handler() allowing applications to be
 * notified whenever an @onscreen framebuffer is resized.
 *
 * <note>Cogl automatically updates the viewport of an @onscreen
 * framebuffer that is resized so this callback is also an indication
 * that the viewport has been modified too</note>
 *
 * <note>A resize callback will only ever be called while dispatching
 * Cogl events from the system mainloop; so for example during
 * cogl_poll_dispatch(). This is so that callbacks shouldn't occur
 * while an application might have arbitrary locks held for
 * example.</note>
 *
 * Since: 2.0
 */
typedef void (*CoglOnscreenResizeCallback) (CoglOnscreen *onscreen,
                                            int width,
                                            int height,
                                            void *user_data);

/**
 * cogl_onscreen_add_resize_handler:
 * @onscreen: A #CoglOnscreen framebuffer
 * @callback: A #CoglOnscreenResizeCallback to call when the @onscreen
 *            changes size.
 * @user_data: Private data to be passed to @callback.
 *
 * Registers a @callback with @onscreen that will be called whenever
 * the @onscreen framebuffer changes size.
 *
 * The @callback can be removed using
 * cogl_onscreen_remove_resize_handler() passing the same @callback
 * and @user_data pair.
 *
 * <note>Since Cogl automatically updates the viewport of an @onscreen
 * framebuffer that is resized, a resize callback can also be used to
 * track when the viewport has been changed automatically by Cogl in
 * case your application needs more specialized control over the
 * viewport.</note>
 *
 * <note>A resize callback will only ever be called while dispatching
 * Cogl events from the system mainloop; so for example during
 * cogl_poll_dispatch(). This is so that callbacks shouldn't occur
 * while an application might have arbitrary locks held for
 * example.</note>
 *
 * Return value: a unique identifier that can be used to remove to remove
 *               the callback later.
 *
 * Since: 2.0
 */
unsigned int
cogl_onscreen_add_resize_handler (CoglOnscreen *onscreen,
                                  CoglOnscreenResizeCallback callback,
                                  void *user_data);

/**
 * cogl_onscreen_remove_resize_handler:
 * @onscreen: A #CoglOnscreen framebuffer
 * @id: An identifier returned from cogl_onscreen_add_resize_handler()
 *
 * Removes a resize @callback and @user_data pair that were previously
 * associated with @onscreen via cogl_onscreen_add_resize_handler().
 *
 * Since: 2.0
 */
void
cogl_onscreen_remove_resize_handler (CoglOnscreen *onscreen,
                                     unsigned int id);

/**
 * cogl_is_onscreen:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a #CoglOnscreen.
 *
 * Return value: %TRUE if the object references a #CoglOnscreen
 *   and %FALSE otherwise.
 * Since: 1.10
 * Stability: unstable
 */
CoglBool
cogl_is_onscreen (void *object);

COGL_END_DECLS

#endif /* __COGL_ONSCREEN_H */
