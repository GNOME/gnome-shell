/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 */

/* This can be included multiple times with different definitions for
 * the COGL_WINSYS_FEATURE_* functions.
 */

/* Macro prototypes:
 * COGL_WINSYS_FEATURE_BEGIN (major_glx_version, minor_glx_version,
 *                            name, namespaces, extension_names,
 *                            implied_public_feature_flags,
 *                            implied_winsys_feature)
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

/* Base functions that we assume are always available */
COGL_WINSYS_FEATURE_BEGIN (0, 0, /* always available */
                           base_glx_functions,
                           "\0",
                           "\0",
                           0, /* no implied public feature */
                           0 /* no winsys feature */)
COGL_WINSYS_FEATURE_FUNCTION (void, glXDestroyContext,
                              (Display *dpy, GLXContext ctx))
COGL_WINSYS_FEATURE_FUNCTION (void, glXSwapBuffers,
                              (Display *dpy, GLXDrawable drawable))
COGL_WINSYS_FEATURE_FUNCTION (Bool, glXIsDirect,
                              (Display *dpy, GLXContext ctx))
COGL_WINSYS_FEATURE_FUNCTION (int, glXGetFBConfigAttrib,
                              (Display *dpy, GLXFBConfig config,
                               int attribute, int *value))
COGL_WINSYS_FEATURE_FUNCTION (GLXWindow, glXCreateWindow,
                              (Display *dpy, GLXFBConfig config,
                               Window win, const int *attribList))
COGL_WINSYS_FEATURE_FUNCTION (void, glXDestroyWindow,
                              (Display *dpy, GLXWindow window))
COGL_WINSYS_FEATURE_FUNCTION (GLXPixmap, glXCreatePixmap,
                              (Display *dpy, GLXFBConfig config,
                               Pixmap pixmap, const int *attribList))
COGL_WINSYS_FEATURE_FUNCTION (void, glXDestroyPixmap,
                              (Display *dpy, GLXPixmap pixmap))
COGL_WINSYS_FEATURE_FUNCTION (GLXContext, glXCreateNewContext,
                              (Display *dpy, GLXFBConfig config,
                           int renderType, GLXContext shareList,
                               Bool direct))
COGL_WINSYS_FEATURE_FUNCTION (Bool, glXMakeContextCurrent,
                              (Display *dpy, GLXDrawable draw,
                               GLXDrawable read, GLXContext ctx))
COGL_WINSYS_FEATURE_FUNCTION (void, glXSelectEvent,
                              (Display *dpy, GLXDrawable drawable,
                               unsigned long mask))
COGL_WINSYS_FEATURE_FUNCTION (GLXFBConfig *, glXGetFBConfigs,
                              (Display *dpy, int screen, int *nelements))
COGL_WINSYS_FEATURE_FUNCTION (GLXFBConfig *, glXChooseFBConfig,
                              (Display *dpy, int screen,
                               const int *attrib_list, int *nelements))
COGL_WINSYS_FEATURE_FUNCTION (XVisualInfo *, glXGetVisualFromFBConfig,
                              (Display *dpy, GLXFBConfig config))
COGL_WINSYS_FEATURE_END ()

COGL_WINSYS_FEATURE_BEGIN (255, 255,
                           texture_from_pixmap,
                           "EXT\0",
                           "texture_from_pixmap\0",
                           0,
                           COGL_WINSYS_FEATURE_TEXTURE_FROM_PIXMAP)
COGL_WINSYS_FEATURE_FUNCTION (void, glXBindTexImage,
                              (Display *display,
                               GLXDrawable drawable,
                               int buffer,
                               int *attribList))
COGL_WINSYS_FEATURE_FUNCTION (void, glXReleaseTexImage,
                              (Display *display,
                               GLXDrawable drawable,
                               int buffer))
COGL_WINSYS_FEATURE_END ()

COGL_WINSYS_FEATURE_BEGIN (255, 255,
                           video_sync,
                           "SGI\0",
                           "video_sync\0",
                           0,
                           COGL_WINSYS_FEATURE_VBLANK_COUNTER)
COGL_WINSYS_FEATURE_FUNCTION (int, glXGetVideoSync,
                              (unsigned int *count))
COGL_WINSYS_FEATURE_FUNCTION (int, glXWaitVideoSync,
                              (int divisor,
                               int remainder,
                               unsigned int *count))
COGL_WINSYS_FEATURE_END ()

COGL_WINSYS_FEATURE_BEGIN (255, 255,
                           swap_control,
                           "SGI\0",
                           "swap_control\0",
                           0,
                           COGL_WINSYS_FEATURE_SWAP_THROTTLE)
COGL_WINSYS_FEATURE_FUNCTION (int, glXSwapInterval,
                              (int interval))
COGL_WINSYS_FEATURE_END ()

COGL_WINSYS_FEATURE_BEGIN (255, 255,
                           sync_control,
                           "OML\0",
                           "sync_control\0",
                           0,
                           0)
COGL_WINSYS_FEATURE_FUNCTION (Bool, glXGetSyncValues,
                              (Display* dpy,
                               GLXDrawable drawable,
                               int64_t* ust,
                               int64_t* msc,
                               int64_t* sbc))
COGL_WINSYS_FEATURE_FUNCTION (Bool, glXWaitForMsc,
                              (Display* dpy,
                               GLXDrawable drawable,
                               int64_t target_msc,
                               int64_t divisor,
                               int64_t remainder,
                               int64_t* ust,
                               int64_t* msc,
                               int64_t* sbc))
COGL_WINSYS_FEATURE_END ()

COGL_WINSYS_FEATURE_BEGIN (255, 255,
                           copy_sub_buffer,
                           "MESA\0",
                           "copy_sub_buffer\0",
                           0,
/* We initially assumed that copy_sub_buffer is synchronized on
 * which is only the case for a subset of GPUs for example it is not
 * synchronized on INTEL gen6 and gen7, so we remove this assumption
 * for now
 */
#if 0
                           COGL_WINSYS_FEATURE_SWAP_REGION_SYNCHRONIZED)
#endif
                           0)
COGL_WINSYS_FEATURE_FUNCTION (void, glXCopySubBuffer,
                              (Display *dpy,
                               GLXDrawable drawable,
                               int x, int y, int width, int height))
COGL_WINSYS_FEATURE_END ()

COGL_WINSYS_FEATURE_BEGIN (255, 255,
                           swap_event,
                           "INTEL\0",
                           "swap_event\0",
                           0,
                           COGL_WINSYS_FEATURE_SYNC_AND_COMPLETE_EVENT)
COGL_WINSYS_FEATURE_END ()

COGL_WINSYS_FEATURE_BEGIN (255, 255,
                           create_context,
                           "ARB\0",
                           "create_context",
                           0,
                           0)
COGL_WINSYS_FEATURE_FUNCTION (GLXContext, glXCreateContextAttribs,
                              (Display *dpy,
                               GLXFBConfig config,
                               GLXContext share_context,
                               Bool direct,
                               const int *attrib_list))
COGL_WINSYS_FEATURE_END ()

COGL_WINSYS_FEATURE_BEGIN (255, 255,
                           buffer_age,
                           "EXT\0",
                           "buffer_age\0",
                           0,
                           COGL_WINSYS_FEATURE_BUFFER_AGE)
COGL_WINSYS_FEATURE_END ()
