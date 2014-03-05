/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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

#ifndef __COGL_EGL_H__
#define __COGL_EGL_H__

#ifdef COGL_HAS_EGL_SUPPORT

#include "cogl-egl-defines.h"

COGL_BEGIN_DECLS

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

COGL_END_DECLS

#endif /* COGL_HAS_EGL_SUPPORT */

#endif
