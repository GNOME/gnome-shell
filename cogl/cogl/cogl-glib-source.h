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
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_GSOURCE_H__
#define __COGL_GSOURCE_H__

#include <glib.h>
#include <cogl/cogl-context.h>

G_BEGIN_DECLS

/**
 * cogl_glib_source_new:
 * @context: A #CoglContext
 * @priority: The priority of the #GSource
 *
 * Creates a #GSource which handles Cogl's internal system event
 * processing. This can be used as a convenience instead of
 * cogl_poll_renderer_get_info() and cogl_poll_renderer_dispatch() in
 * applications that are already using the GLib main loop. After this
 * is called the #GSource should be attached to the main loop using
 * g_source_attach().
 *
 * Applications that manually connect to a #CoglRenderer before they
 * create a #CoglContext should instead use
 * cogl_glib_renderer_source_new() so that events may be dispatched
 * before a context has been created. In that case you don't need to
 * use this api in addition later, it is simply enough to use
 * cogl_glib_renderer_source_new() instead.
 *
 * <note>This api is actually just a thin convenience wrapper around
 * cogl_glib_renderer_source_new()</note>
 *
 * Return value: a new #GSource
 *
 * Stability: unstable
 * Since: 1.10
 */
GSource *
cogl_glib_source_new (CoglContext *context,
                      int priority);

/**
 * cogl_glib_renderer_source_new:
 * @renderer: A #CoglRenderer
 * @priority: The priority of the #GSource
 *
 * Creates a #GSource which handles Cogl's internal system event
 * processing. This can be used as a convenience instead of
 * cogl_poll_renderer_get_info() and cogl_poll_renderer_dispatch() in
 * applications that are already using the GLib main loop. After this
 * is called the #GSource should be attached to the main loop using
 * g_source_attach().
 *
 * Return value: a new #GSource
 *
 * Stability: unstable
 * Since: 1.16
 */
GSource *
cogl_glib_renderer_source_new (CoglRenderer *renderer,
                               int priority);

G_END_DECLS

#endif /* __COGL_GSOURCE_H__ */
