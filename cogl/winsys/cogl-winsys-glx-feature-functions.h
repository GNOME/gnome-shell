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
 * COGL_WINSYS_FEATURE_BEGIN (name, namespaces, extension_names,
 *                            implied_public_feature_flags,
 *                            implied_private_feature_flags,
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

COGL_WINSYS_FEATURE_BEGIN (texture_from_pixmap,
                           "EXT\0",
                           "texture_from_pixmap\0",
                           0,
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

COGL_WINSYS_FEATURE_BEGIN (video_sync,
                           "SGI\0",
                           "video_sync\0",
                           0,
                           0,
                           COGL_WINSYS_FEATURE_VBLANK_COUNTER)
COGL_WINSYS_FEATURE_FUNCTION (int, glXGetVideoSync,
                              (unsigned int *count))
COGL_WINSYS_FEATURE_FUNCTION (int, glXWaitVideoSync,
                              (int divisor,
                               int remainder,
                               unsigned int *count))
COGL_WINSYS_FEATURE_END ()

COGL_WINSYS_FEATURE_BEGIN (swap_control,
                           "SGI\0",
                           "swap_control\0",
                           0,
                           0,
                           COGL_WINSYS_FEATURE_SWAP_THROTTLE)
COGL_WINSYS_FEATURE_FUNCTION (int, glXSwapInterval,
                              (int interval))
COGL_WINSYS_FEATURE_END ()

COGL_WINSYS_FEATURE_BEGIN (copy_sub_buffer,
                           "MESA\0",
                           "copy_sub_buffer\0",
                           0,
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

COGL_WINSYS_FEATURE_BEGIN (swap_event,
                           "INTEL\0",
                           "swap_event\0",
                           0,
                           0,
                           COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT)
COGL_WINSYS_FEATURE_END ()

COGL_WINSYS_FEATURE_BEGIN (create_context,
                           "ARB\0",
                           "create_context",
                           0,
                           0,
                           0)
COGL_WINSYS_FEATURE_FUNCTION (GLXContext, glXCreateContextAttribs,
                              (Display *dpy,
                               GLXFBConfig config,
                               GLXContext share_context,
                               Bool direct,
                               const int *attrib_list))
COGL_WINSYS_FEATURE_END ()
