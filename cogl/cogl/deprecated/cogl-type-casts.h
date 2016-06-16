/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2013 Intel Corporation.
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

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_TYPE_CASTS_H__
#define __COGL_TYPE_CASTS_H__

/* The various interface types in Cogl used to be more strongly typed
 * which required lots type casting by developers. We provided
 * macros for performing these casts following a widely used Gnome
 * coding style. Since we now consistently typedef these interfaces
 * as void for the public C api and use runtime type checking to
 * catch programming errors the casts have become redundant and
 * so these macros are only kept for compatibility...
 */

#if !defined(COGL_ENABLE_MUTTER_API) && !defined(COGL_GIR_SCANNING)
#define COGL_FRAMEBUFFER(X) (X)
#define COGL_BUFFER(X) (X)
#define COGL_TEXTURE(X) (X)
#define COGL_META_TEXTURE(X) (X)
#define COGL_PRIMITIVE_TEXTURE(X) (X)
#endif

#endif /* __COGL_TYPE_CASTS_H__ */
