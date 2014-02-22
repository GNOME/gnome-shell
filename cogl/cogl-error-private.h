/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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
 */

#ifndef __COGL_ERROR_PRIVATE_H__
#define __COGL_ERROR_PRIVATE_H__

#include "cogl-error.h"

void
_cogl_set_error (CoglError **error,
                 uint32_t domain,
                 int code,
                 const char *format,
                 ...) G_GNUC_PRINTF (4, 5);

void
_cogl_set_error_literal (CoglError **error,
                         uint32_t domain,
                         int code,
                         const char *message);

void
_cogl_propagate_error (CoglError **dest,
                       CoglError *src);

#ifdef COGL_HAS_GLIB_SUPPORT
void
_cogl_propagate_gerror (CoglError **dest,
                        GError *src);
#endif /* COGL_HAS_GLIB_SUPPORT */

#define _cogl_clear_error(X) g_clear_error ((GError **)X)

#endif /* __COGL_ERROR_PRIVATE_H__ */
