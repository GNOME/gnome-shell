/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
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

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_CLUTTER_H__
#define __COGL_CLUTTER_H__

COGL_BEGIN_DECLS

#define cogl_clutter_check_extension cogl_clutter_check_extension_CLUTTER
COGL_DEPRECATED_IN_1_16
CoglBool
cogl_clutter_check_extension (const char *name, const char *ext);

#define cogl_clutter_winsys_has_feature cogl_clutter_winsys_has_feature_CLUTTER
COGL_DEPRECATED_FOR (cogl_has_feature)
CoglBool
cogl_clutter_winsys_has_feature (CoglWinsysFeature feature);

#define cogl_onscreen_clutter_backend_set_size cogl_onscreen_clutter_backend_set_size_CLUTTER
void
cogl_onscreen_clutter_backend_set_size (int width, int height);

COGL_END_DECLS

#endif /* __COGL_CLUTTER_H__ */
