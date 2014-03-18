/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

/**
 * SECTION:clutter-egl
 * @short_description: EGL specific API
 *
 * The EGL backend for Clutter provides some EGL specific API
 *
 * You need to include `clutter-egl.h` to have access to the functions documented here.
 */

#ifndef __CLUTTER_EGL_H__
#define __CLUTTER_EGL_H__

#include <glib.h>

#ifdef COGL_HAS_XLIB_SUPPORT
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#endif

#include "clutter-egl-headers.h"
#include <clutter/clutter.h>

G_BEGIN_DECLS

/**
 * clutter_eglx_display:
 *
 * Retrieves the #EGLDisplay used by Clutter,
 * if Clutter has been compiled with EGL and X11 support.
 *
 * Return value: the EGL display
 *
 * Since: 0.4
 *
 * Deprecated: 1.6: Use clutter_egl_get_egl_display() instead
 */
CLUTTER_DEPRECATED_FOR(clutter_egl_get_egl_display)
EGLDisplay      clutter_eglx_display            (void);

/**
 * clutter_egl_display:
 *
 * Retrieves the #EGLDisplay used by Clutter
 *
 * Return value: the EGL display
 *
 * Deprecated: 1.6: Use clutter_egl_get_egl_display() instead
 */
CLUTTER_DEPRECATED_FOR(clutter_egl_get_egl_display)
EGLDisplay      clutter_egl_display             (void);

/**
 * clutter_egl_get_egl_display:
 *
 * Retrieves the  #EGLDisplay used by Clutter.
 *
 * Return value: the EGL display
 *
 * Since: 1.6
 */
CLUTTER_AVAILABLE_IN_1_6
EGLDisplay      clutter_egl_get_egl_display     (void);

#ifdef COGL_HAS_EGL_PLATFORM_KMS_SUPPORT
CLUTTER_AVAILABLE_IN_1_18
void            clutter_egl_set_kms_fd          (int fd);
#endif

G_END_DECLS

#endif /* __CLUTTER_EGL_H__ */
