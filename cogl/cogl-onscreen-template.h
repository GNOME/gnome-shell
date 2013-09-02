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
 *
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_ONSCREEN_TEMPLATE_H__
#define __COGL_ONSCREEN_TEMPLATE_H__

#include <cogl/cogl-swap-chain.h>

#ifdef COGL_HAS_GTYPE_SUPPORT
#include <glib-object.h>
#endif

COGL_BEGIN_DECLS

typedef struct _CoglOnscreenTemplate	      CoglOnscreenTemplate;

#define COGL_ONSCREEN_TEMPLATE(OBJECT) ((CoglOnscreenTemplate *)OBJECT)

#ifdef COGL_HAS_GTYPE_SUPPORT
/**
 * cogl_onscreen_template_get_gtype:
 *
 * Returns: a #GType that can be used with the GLib type system.
 */
GType cogl_onscreen_template_get_gtype (void);
#endif

CoglOnscreenTemplate *
cogl_onscreen_template_new (CoglSwapChain *swap_chain);

/**
 * cogl_onscreen_template_set_samples_per_pixel:
 * @onscreen_template: A #CoglOnscreenTemplate template framebuffer
 * @n: The minimum number of samples per pixel
 *
 * Requires that any future CoglOnscreen framebuffers derived from
 * this template must support making at least @n samples per pixel
 * which will all contribute to the final resolved color for that
 * pixel.
 *
 * By default this value is usually set to 0 and that is referred to
 * as "single-sample" rendering. A value of 1 or greater is referred
 * to as "multisample" rendering.
 *
 * <note>There are some semantic differences between single-sample
 * rendering and multisampling with just 1 point sample such as it
 * being redundant to use the cogl_framebuffer_resolve_samples() and
 * cogl_framebuffer_resolve_samples_region() apis with single-sample
 * rendering.</note>
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_onscreen_template_set_samples_per_pixel (
                                          CoglOnscreenTemplate *onscreen_template,
                                          int n);

/**
 * cogl_onscreen_template_set_swap_throttled:
 * @onscreen_template: A #CoglOnscreenTemplate template framebuffer
 * @throttled: Whether throttling should be enabled
 *
 * Requests that any future #CoglOnscreen framebuffers derived from this
 * template should enable or disable swap throttling according to the given
 * @throttled argument.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_onscreen_template_set_swap_throttled (
                                          CoglOnscreenTemplate *onscreen_template,
                                          CoglBool throttled);

/**
 * cogl_is_onscreen_template:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a #CoglOnscreenTemplate.
 *
 * Return value: %TRUE if the object references a #CoglOnscreenTemplate
 *   and %FALSE otherwise.
 * Since: 1.10
 * Stability: unstable
 */
CoglBool
cogl_is_onscreen_template (void *object);

COGL_END_DECLS

#endif /* __COGL_ONSCREEN_TEMPLATE_H__ */
