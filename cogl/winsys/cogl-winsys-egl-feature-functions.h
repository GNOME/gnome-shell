/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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

/* This can be included multiple times with different definitions for
 * the COGL_WINSYS_FEATURE_* functions.
 */

/* Macro prototypes:
 * COGL_WINSYS_FEATURE_BEGIN (name, namespaces, extension_names,
 *                            implied_private_egl_feature_flags)
 * COGL_WINSYS_FEATURE_FUNCTION (return_type, function_name,
 *                               (arguments))
 * ...
 * COGL_WINSYS_FEATURE_END ()
 *
 * Note: You can list multiple namespace and extension names if the
 * corresponding _FEATURE_FUNCTIONS have the same semantics accross
 * the different extension variants.
 *
 * XXX: NB: Don't add a trailing semicolon when using these macros
 */

COGL_WINSYS_FEATURE_BEGIN (swap_region,
                           "NOK\0",
                           "swap_region\0",
                           COGL_EGL_WINSYS_FEATURE_SWAP_REGION)
COGL_WINSYS_FEATURE_FUNCTION (EGLBoolean, eglSwapBuffersRegion,
                              (EGLDisplay dpy,
                               EGLSurface surface,
                               EGLint numRects,
                               const EGLint *rects))
COGL_WINSYS_FEATURE_END ()
/* XXX: These macros can't handle falling back to looking for
 * EGL_KHR_image if EGL_KHR_image_base and EGL_KHR_image_pixmap aren't
 * found... */
#ifdef EGL_KHR_image_base
COGL_WINSYS_FEATURE_BEGIN (image_base,
                           "KHR\0",
                           "image_base\0",
                           0)
COGL_WINSYS_FEATURE_FUNCTION (EGLImageKHR, eglCreateImage,
                              (EGLDisplay dpy,
                               EGLContext ctx,
                               EGLenum target,
                               EGLClientBuffer buffer,
                               const EGLint *attrib_list))
COGL_WINSYS_FEATURE_FUNCTION (EGLBoolean, eglDestroyImage,
                              (EGLDisplay dpy,
                               EGLImageKHR image))
COGL_WINSYS_FEATURE_END ()
#endif
COGL_WINSYS_FEATURE_BEGIN (image_pixmap,
                           "KHR\0",
                           "image_pixmap\0",
                           COGL_EGL_WINSYS_FEATURE_EGL_IMAGE_FROM_X11_PIXMAP)
COGL_WINSYS_FEATURE_END ()
#ifdef EGL_WL_bind_wayland_display
COGL_WINSYS_FEATURE_BEGIN (bind_wayland_display,
                           "WL\0",
                           "bind_wayland_display\0",
                           COGL_EGL_WINSYS_FEATURE_EGL_IMAGE_FROM_WAYLAND_BUFFER)
COGL_WINSYS_FEATURE_FUNCTION (EGLImageKHR, eglBindWaylandDisplay,
                              (EGLDisplay dpy,
                               struct wl_display *wayland_display))
COGL_WINSYS_FEATURE_FUNCTION (EGLBoolean, eglUnbindWaylandDisplay,
                              (EGLDisplay dpy,
                               struct wl_display *wayland_display))
COGL_WINSYS_FEATURE_FUNCTION (EGLBoolean, eglQueryWaylandBuffer,
                              (EGLDisplay dpy,
                               struct wl_resource *buffer,
                               EGLint attribute, EGLint *value))
COGL_WINSYS_FEATURE_END ()
#endif /* EGL_WL_bind_wayland_display */

COGL_WINSYS_FEATURE_BEGIN (create_context,
                           "KHR\0",
                           "create_context\0",
                           COGL_EGL_WINSYS_FEATURE_CREATE_CONTEXT)
COGL_WINSYS_FEATURE_END ()

COGL_WINSYS_FEATURE_BEGIN (buffer_age,
                           "EXT\0",
                           "buffer_age\0",
                           COGL_EGL_WINSYS_FEATURE_BUFFER_AGE)
COGL_WINSYS_FEATURE_END ()

COGL_WINSYS_FEATURE_BEGIN (swap_buffers_with_damage,
                           "EXT\0",
                           "swap_buffers_with_damage\0",
                           0)
COGL_WINSYS_FEATURE_FUNCTION (EGLBoolean, eglSwapBuffersWithDamage,
                              (EGLDisplay dpy,
                               EGLSurface surface,
                               const EGLint *rects,
                               EGLint n_rects))
COGL_WINSYS_FEATURE_END ()

#if defined(EGL_KHR_fence_sync) || defined(EGL_KHR_reusable_sync)
COGL_WINSYS_FEATURE_BEGIN (fence_sync,
                           "KHR\0",
                           "fence_sync\0",
                           COGL_EGL_WINSYS_FEATURE_FENCE_SYNC)
COGL_WINSYS_FEATURE_FUNCTION (EGLSyncKHR, eglCreateSync,
                              (EGLDisplay dpy,
                               EGLenum type,
                               const EGLint *attrib_list))
COGL_WINSYS_FEATURE_FUNCTION (EGLint, eglClientWaitSync,
                              (EGLDisplay dpy,
                               EGLSyncKHR sync,
                               EGLint flags,
                               EGLTimeKHR timeout))
COGL_WINSYS_FEATURE_FUNCTION (EGLBoolean, eglDestroySync,
                              (EGLDisplay dpy,
                               EGLSyncKHR sync))
COGL_WINSYS_FEATURE_END ()
#endif
