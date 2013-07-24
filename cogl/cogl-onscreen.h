/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011,2012,2013 Intel Corporation.
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
#include <cogl/cogl-frame-info.h>
#include <cogl/cogl-object.h>

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
 * cogl_wayland_onscreen_set_foreign_surface:
 * @onscreen: An unallocated framebuffer.
 * @surface A Wayland surface to associate with the @onscreen.
 *
 * Allows you to explicitly notify Cogl of an existing Wayland surface to use,
 * which prevents Cogl from allocating a surface and shell surface for the
 * @onscreen. An allocated surface will not be destroyed when the @onscreen is
 * freed.
 *
 * This function must be called before @onscreen is allocated.
 *
 * Since: 1.16
 * Stability: unstable
 */
void
cogl_wayland_onscreen_set_foreign_surface (CoglOnscreen *onscreen,
                                           struct wl_surface *surface);

/**
 * cogl_wayland_onscreen_resize:
 * @onscreen: A #CoglOnscreen framebuffer
 * @width: The desired width of the framebuffer
 * @height: The desired height of the framebuffer
 * @offset_x: A relative x offset for the new framebuffer
 * @offset_y: A relative y offset for the new framebuffer
 *
 * Resizes the backbuffer of the given @onscreen framebuffer to the
 * given size. Since a buffer is usually conceptually scaled with a
 * center point the @offset_x and @offset_y arguments allow the newly
 * allocated buffer to be positioned relative to the old buffer size.
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
 * Note that if some drawing commands have been applied to the
 * framebuffer since the last swap buffers then the resize will be
 * queued and will only take effect in the next swap buffers.
 *
 * If multiple calls to cogl_wayland_onscreen_resize() get queued
 * before the next swap buffers request then the relative x and y
 * offsets accumulate instead of being replaced. The @width and
 * @height values superseed the old values.
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
 * @onscreen: A #CoglOnscreen framebuffer
 * @throttled: Whether swap throttling is wanted or not.
 *
 * Requests that the given @onscreen framebuffer should have swap buffer
 * requests (made using cogl_onscreen_swap_buffers()) throttled either by a
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
 * When using the Wayland winsys calling this will set the surface to
 * a toplevel type which will make it appear. If the application wants
 * to set a different type for the surface, it can avoid calling
 * cogl_onscreen_show() and set its own type directly with the Wayland
 * client API via cogl_wayland_onscreen_get_surface().
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
 * <note>It is highly recommended that applications use
 * cogl_onscreen_swap_buffers_with_damage() instead whenever possible
 * and also use the cogl_onscreen_get_buffer_age() api so they can
 * perform incremental updates to older buffers instead of having to
 * render a full buffer for every frame.</note>
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_onscreen_swap_buffers (CoglOnscreen *onscreen);


/**
 * cogl_onscreen_get_buffer_age:
 * @onscreen: A #CoglOnscreen framebuffer
 *
 * Gets the current age of the buffer contents.
 *
 * This function allows applications to query the age of the current
 * back buffer contents for a #CoglOnscreen as the number of frames
 * elapsed since the contents were most recently defined.
 *
 * These age values exposes enough information to applications about
 * how Cogl internally manages back buffers to allow applications to
 * re-use the contents of old frames and minimize how much must be
 * redrawn for the next frame.
 *
 * The back buffer contents can either be reported as invalid (has an
 * age of 0) or it may be reported to be the same contents as from n
 * frames prior to the current frame.
 *
 * The queried value remains valid until the next buffer swap.
 *
 * <note>One caveat is that under X11 the buffer age does not reflect
 * changes to buffer contents caused by the window systems. X11
 * applications must track Expose events to determine what buffer
 * regions need to additionally be repaired each frame.</note>
 *
 * The recommended way to take advantage of this buffer age api is to
 * build up a circular buffer of length 3 for tracking damage regions
 * over the last 3 frames and when starting a new frame look at the
 * age of the buffer and combine the damage regions for the current
 * frame with the damage regions of previous @age frames so you know
 * everything that must be redrawn to update the old contents for the
 * new frame.
 *
 * <note>If the system doesn't not support being able to track the age
 * of back buffers then this function will always return 0 which
 * implies that the contents are undefined.</note>
 *
 * Return value: The age of the buffer contents or 0 when the buffer
 *               contents are undefined.
 *
 * Since: 1.14
 * Stability: stable
 */
int
cogl_onscreen_get_buffer_age (CoglOnscreen *onscreen);

/**
 * cogl_onscreen_swap_buffers_with_damage:
 * @onscreen: A #CoglOnscreen framebuffer
 * @rectangles: An array of integer 4-tuples representing damaged
 *              rectangles as (x, y, width, height) tuples.
 * @n_rectangles: The number of 4-tuples to be read from @rectangles
 *
 * Swaps the current back buffer being rendered too, to the front for
 * display and provides information to any system compositor about
 * what regions of the buffer have changed (damage) with respect to
 * the last swapped buffer.
 *
 * This function has the same semantics as
 * cogl_framebuffer_swap_buffers() except that it additionally allows
 * applications to pass a list of damaged rectangles which may be
 * passed on to a compositor so that it can minimize how much of the
 * screen is redrawn in response to this applications newly swapped
 * front buffer.
 *
 * For example if your application is only animating a small object in
 * the corner of the screen and everything else is remaining static
 * then it can help the compositor to know that only the bottom right
 * corner of your newly swapped buffer has really changed with respect
 * to your previously swapped front buffer.
 *
 * If @n_rectangles is 0 then the whole buffer will implicitly be
 * reported as damaged as if cogl_onscreen_swap_buffers() had been
 * called.
 *
 * This function also implicitly discards the contents of the color,
 * depth and stencil buffers as if cogl_framebuffer_discard_buffers()
 * were used. The significance of the discard is that you should not
 * expect to be able to start a new frame that incrementally builds on
 * the contents of the previous frame. If you want to perform
 * incremental updates to older back buffers then please refer to the
 * cogl_onscreen_get_buffer_age() api.
 *
 * Whenever possible it is recommended that applications use this
 * function instead of cogl_onscreen_swap_buffers() to improve
 * performance when running under a compositor.
 *
 * <note>It is highly recommended to use this API in conjunction with
 * the cogl_onscreen_get_buffer_age() api so that your application can
 * perform incremental rendering based on old back buffers.</note>
 *
 * Since: 1.16
 * Stability: unstable
 */
void
cogl_onscreen_swap_buffers_with_damage (CoglOnscreen *onscreen,
                                        const int *rectangles,
                                        int n_rectangles);

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
 * stencil buffers as if cogl_framebuffer_discard_buffers() were used. The
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

/**
 * CoglFrameEvent:
 * @COGL_FRAME_EVENT_SYNC: Notifies that the system compositor has
 *                         acknowledged a frame and is ready for a
 *                         new frame to be created.
 * @COGL_FRAME_EVENT_COMPLETE: Notifies that a frame has ended. This
 *                             is a good time for applications to
 *                             collect statistics about the frame
 *                             since the #CoglFrameInfo should hold
 *                             the most data at this point. No other
 *                             events should be expected after a
 *                             @COGL_FRAME_EVENT_COMPLETE event.
 *
 * Identifiers that are passed to #CoglFrameCallback functions
 * (registered using cogl_onscreen_add_frame_callback()) that
 * mark the progression of a frame in some way which usually
 * means that new information will have been accumulated in the
 * frame's corresponding #CoglFrameInfo object.
 *
 * The last event that will be sent for a frame will be a
 * @COGL_FRAME_EVENT_COMPLETE event and so these are a good
 * opportunity to collect statistics about a frame since the
 * #CoglFrameInfo should hold the most data at this point.
 *
 * <note>A frame may not be completed before the next frame can start
 * so applications should avoid needing to collect all statistics for
 * a particular frame before they can start a new frame.</note>
 *
 * Since: 1.14
 * Stability: unstable
 */
typedef enum _CoglFrameEvent
{
  COGL_FRAME_EVENT_SYNC = 1,
  COGL_FRAME_EVENT_COMPLETE
} CoglFrameEvent;

/**
 * CoglFrameCallback:
 * @onscreen: The onscreen that the frame is associated with
 * @event: A #CoglFrameEvent notifying how the frame has progressed
 * @info: The meta information, such as timing information, about
 *        the frame that has progressed.
 * @user_data: The user pointer passed to
 *             cogl_onscreen_add_frame_callback()
 *
 * Is a callback that can be registered via
 * cogl_onscreen_add_frame_callback() to be called when a frame
 * progresses in some notable way.
 *
 * Please see the documentation for #CoglFrameEvent and
 * cogl_onscreen_add_frame_callback() for more details about what
 * events can be notified.
 *
 * Since: 1.14
 * Stability: unstable
 */
typedef void (*CoglFrameCallback) (CoglOnscreen *onscreen,
                                   CoglFrameEvent event,
                                   CoglFrameInfo *info,
                                   void *user_data);

/**
 * CoglFrameClosure:
 *
 * An opaque type that tracks a #CoglFrameCallback and associated user
 * data. A #CoglFrameClosure pointer will be returned from
 * cogl_onscreen_add_frame_callback() and it allows you to remove a
 * callback later using cogl_onscreen_remove_frame_callback().
 *
 * Since: 1.14
 * Stability: unstable
 */
typedef struct _CoglClosure CoglFrameClosure;

/**
 * cogl_onscreen_add_frame_callback:
 * @onscreen: A #CoglOnscreen framebuffer
 * @callback: A callback function to call for frame events
 * @user_data: A private pointer to be passed to @callback
 * @destroy: An optional callback to destroy @user_data when the
 *           @callback is removed or @onscreen is freed.
 *
 * Installs a @callback function that will be called for significant
 * events relating to the given @onscreen framebuffer.
 *
 * The @callback will be used to notify when the system compositor is
 * ready for this application to render a new frame. In this case
 * %COGL_FRAME_EVENT_SYNC will be passed as the event argument to the
 * given @callback in addition to the #CoglFrameInfo corresponding to
 * the frame beeing acknowledged by the compositor.
 *
 * The @callback will also be called to notify when the frame has
 * ended. In this case %COGL_FRAME_EVENT_COMPLETE will be passed as
 * the event argument to the given @callback in addition to the
 * #CoglFrameInfo corresponding to the newly presented frame.  The
 * meaning of "ended" here simply means that no more timing
 * information will be collected within the corresponding
 * #CoglFrameInfo and so this is a good opportunity to analyse the
 * given info. It does not necessarily mean that the GPU has finished
 * rendering the corresponding frame.
 *
 * We highly recommend throttling your application according to
 * %COGL_FRAME_EVENT_SYNC events so that your application can avoid
 * wasting resources, drawing more frames than your system compositor
 * can display.
 *
 * Return value: a #CoglFrameClosure pointer that can be used to
 *               remove the callback and associated @user_data later.
 * Since: 1.14
 * Stability: unstable
 */
CoglFrameClosure *
cogl_onscreen_add_frame_callback (CoglOnscreen *onscreen,
                                  CoglFrameCallback callback,
                                  void *user_data,
                                  CoglUserDataDestroyCallback destroy);

/**
 * cogl_onscreen_remove_frame_callback:
 * @onscreen: A #CoglOnscreen
 * @closure: A #CoglFrameClosure returned from
 *           cogl_onscreen_add_frame_callback()
 *
 * Removes a callback and associated user data that were previously
 * registered using cogl_onscreen_add_frame_callback().
 *
 * If a destroy callback was passed to
 * cogl_onscreen_add_frame_callback() to destroy the user data then
 * this will get called.
 *
 * Since: 1.14
 * Stability: unstable
 */
void
cogl_onscreen_remove_frame_callback (CoglOnscreen *onscreen,
                                     CoglFrameClosure *closure);

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
 * Deprecated: 1.14: Use cogl_onscreen_add_frame_callback() instead
 */
COGL_DEPRECATED_IN_1_14_FOR (cogl_onscreen_add_frame_callback)
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
 * Deprecated: 1.14: Use cogl_onscreen_remove_frame_callback() instead
 */

COGL_DEPRECATED_IN_1_14_FOR (cogl_onscreen_remove_frame_callback)
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
 * resize handler using cogl_onscreen_add_resize_callback() so that it
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
 *             cogl_onscreen_add_resize_callback()
 *
 * Is a callback type used with the
 * cogl_onscreen_add_resize_callback() allowing applications to be
 * notified whenever an @onscreen framebuffer is resized.
 *
 * <note>Cogl automatically updates the viewport of an @onscreen
 * framebuffer that is resized so this callback is also an indication
 * that the viewport has been modified too</note>
 *
 * <note>A resize callback will only ever be called while dispatching
 * Cogl events from the system mainloop; so for example during
 * cogl_poll_renderer_dispatch(). This is so that callbacks shouldn't
 * occur while an application might have arbitrary locks held for
 * example.</note>
 *
 * Since: 2.0
 */
typedef void (*CoglOnscreenResizeCallback) (CoglOnscreen *onscreen,
                                            int width,
                                            int height,
                                            void *user_data);

/**
 * CoglOnscreenResizeClosure:
 *
 * An opaque type that tracks a #CoglOnscreenResizeCallback and
 * associated user data. A #CoglOnscreenResizeClosure pointer will be
 * returned from cogl_onscreen_add_resize_callback() and it allows you
 * to remove a callback later using
 * cogl_onscreen_remove_resize_callback().
 *
 * Since: 2.0
 * Stability: unstable
 */
typedef struct _CoglClosure CoglOnscreenResizeClosure;

/**
 * cogl_onscreen_add_resize_callback:
 * @onscreen: A #CoglOnscreen framebuffer
 * @callback: A #CoglOnscreenResizeCallback to call when the @onscreen
 *            changes size.
 * @user_data: Private data to be passed to @callback.
 * @destroy: An optional callback to destroy @user_data when the
 *           @callback is removed or @onscreen is freed.
 *
 * Registers a @callback with @onscreen that will be called whenever
 * the @onscreen framebuffer changes size.
 *
 * The @callback can be removed using
 * cogl_onscreen_remove_resize_callback() passing the returned closure
 * pointer.
 *
 * <note>Since Cogl automatically updates the viewport of an @onscreen
 * framebuffer that is resized, a resize callback can also be used to
 * track when the viewport has been changed automatically by Cogl in
 * case your application needs more specialized control over the
 * viewport.</note>
 *
 * <note>A resize callback will only ever be called while dispatching
 * Cogl events from the system mainloop; so for example during
 * cogl_poll_renderer_dispatch(). This is so that callbacks shouldn't
 * occur while an application might have arbitrary locks held for
 * example.</note>
 *
 * Return value: a #CoglOnscreenResizeClosure pointer that can be used to
 *               remove the callback and associated @user_data later.
 * Since: 2.0
 */
CoglOnscreenResizeClosure *
cogl_onscreen_add_resize_callback (CoglOnscreen *onscreen,
                                   CoglOnscreenResizeCallback callback,
                                   void *user_data,
                                   CoglUserDataDestroyCallback destroy);

/**
 * cogl_onscreen_remove_resize_callback:
 * @onscreen: A #CoglOnscreen framebuffer
 * @closure: An identifier returned from cogl_onscreen_add_resize_callback()
 *
 * Removes a resize @callback and @user_data pair that were previously
 * associated with @onscreen via cogl_onscreen_add_resize_callback().
 *
 * Since: 2.0
 */
void
cogl_onscreen_remove_resize_callback (CoglOnscreen *onscreen,
                                      CoglOnscreenResizeClosure *closure);

/**
 * CoglOnscreenDirtyInfo:
 * @x: Left edge of the dirty rectangle
 * @y: Top edge of the dirty rectangle, measured from the top of the window
 * @width: Width of the dirty rectangle
 * @height: Height of the dirty rectangle
 *
 * A structure passed to callbacks registered using
 * cogl_onscreen_add_dirty_callback(). The members describe a
 * rectangle within the onscreen buffer that should be redrawn.
 *
 * Since: 1.16
 * Stability: unstable
 */
typedef struct _CoglOnscreenDirtyInfo CoglOnscreenDirtyInfo;

struct _CoglOnscreenDirtyInfo
{
  int x, y;
  int width, height;
};

/**
 * CoglOnscreenDirtyCallback:
 * @onscreen: The onscreen that the frame is associated with
 * @info: A #CoglOnscreenDirtyInfo struct containing the details of the
 *   dirty area
 * @user_data: The user pointer passed to
 *             cogl_onscreen_add_frame_callback()
 *
 * Is a callback that can be registered via
 * cogl_onscreen_add_dirty_callback() to be called when the windowing
 * system determines that a region of the onscreen window has been
 * lost and the application should redraw it.
 *
 * Since: 1.16
 * Stability: unstable
 */
typedef void (*CoglOnscreenDirtyCallback) (CoglOnscreen *onscreen,
                                           const CoglOnscreenDirtyInfo *info,
                                           void *user_data);

/**
 * CoglOnscreenDirtyClosure:
 *
 * An opaque type that tracks a #CoglOnscreenDirtyCallback and associated
 * user data. A #CoglOnscreenDirtyClosure pointer will be returned from
 * cogl_onscreen_add_dirty_callback() and it allows you to remove a
 * callback later using cogl_onscreen_remove_dirty_callback().
 *
 * Since: 1.16
 * Stability: unstable
 */
typedef struct _CoglClosure CoglOnscreenDirtyClosure;

/**
 * cogl_onscreen_add_dirty_callback:
 * @onscreen: A #CoglOnscreen framebuffer
 * @callback: A callback function to call for dirty events
 * @user_data: A private pointer to be passed to @callback
 * @destroy: An optional callback to destroy @user_data when the
 *           @callback is removed or @onscreen is freed.
 *
 * Installs a @callback function that will be called whenever the
 * window system has lost the contents of a region of the onscreen
 * buffer and the application should redraw it to repair the buffer.
 * For example this may happen in a window system without a compositor
 * if a window that was previously covering up the onscreen window has
 * been moved causing a region of the onscreen to be exposed.
 *
 * The @callback will be passed a #CoglOnscreenDirtyInfo struct which
 * decribes a rectangle containing the newly dirtied region. Note that
 * this may be called multiple times to describe a non-rectangular
 * region composed of multiple smaller rectangles.
 *
 * The dirty events are separate from %COGL_FRAME_EVENT_SYNC events so
 * the application should also listen for this event before rendering
 * the dirty region to ensure that the framebuffer is actually ready
 * for rendering.
 *
 * Return value: a #CoglOnscreenDirtyClosure pointer that can be used to
 *               remove the callback and associated @user_data later.
 * Since: 1.16
 * Stability: unstable
 */
CoglOnscreenDirtyClosure *
cogl_onscreen_add_dirty_callback (CoglOnscreen *onscreen,
                                  CoglOnscreenDirtyCallback callback,
                                  void *user_data,
                                  CoglUserDataDestroyCallback destroy);

/**
 * cogl_onscreen_remove_dirty_callback:
 * @onscreen: A #CoglOnscreen
 * @closure: A #CoglOnscreenDirtyClosure returned from
 *           cogl_onscreen_add_dirty_callback()
 *
 * Removes a callback and associated user data that were previously
 * registered using cogl_onscreen_add_dirty_callback().
 *
 * If a destroy callback was passed to
 * cogl_onscreen_add_dirty_callback() to destroy the user data then
 * this will also get called.
 *
 * Since: 1.16
 * Stability: unstable
 */
void
cogl_onscreen_remove_dirty_callback (CoglOnscreen *onscreen,
                                     CoglOnscreenDirtyClosure *closure);

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

/**
 * cogl_onscreen_get_frame_counter:
 *
 * Gets the value of the framebuffers frame counter. This is
 * a counter that increases by one each time
 * cogl_onscreen_swap_buffers() or cogl_onscreen_swap_region()
 * is called.
 *
 * Return value: the current frame counter value
 * Since: 1.14
 * Stability: unstable
 */
int64_t
cogl_onscreen_get_frame_counter (CoglOnscreen *onscreen);

COGL_END_DECLS

#endif /* __COGL_ONSCREEN_H */
