/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#ifndef __COGL_EGL_H__
#define __COGL_EGL_H__

#ifdef COGL_HAS_EGL_SUPPORT

#include "cogl-egl-defines.h"

G_BEGIN_DECLS

#define NativeDisplayType EGLNativeDisplayType
#define NativeWindowType EGLNativeWindowType

#ifndef GL_OES_EGL_image
#define GLeglImageOES void *
#endif

/**
 * cogl_egl_context_get_egl_display:
 * @context: A #CoglContext pointer
 *
 * If you have done a runtime check to determine that Cogl is using
 * EGL internally then this API can be used to retrieve the EGLDisplay
 * handle that was setup internally. The result is undefined if Cogl
 * is not using EGL.
 *
 * Return value: The internally setup EGLDisplay handle.
 * Since: 1.8
 * Stability: unstable
 */
EGLDisplay
cogl_egl_context_get_egl_display (CoglContext *context);

G_END_DECLS

#endif /* COGL_HAS_EGL_SUPPORT */

#endif
