/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2014 Canonical Ltd.
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

#ifndef __COGL_MIR_RENDERER_H__
#define __COGL_MIR_RENDERER_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-renderer.h>

#include <mir_toolkit/mir_client_library.h>

COGL_BEGIN_DECLS

/**
 * cogl_mir_renderer_set_foreign_connection:
 * @renderer: A #CoglRenderer
 * @connection: A Mir connection
 *
 * Allows you to explicitly control what Mir connection you want Cogl
 * to work with instead of leaving Cogl to automatically connect to a
 * mir server.
 *
 * Returns: whether @connection has been marked as been set as connection or not.
 *
 * Since: 1.8
 * Stability: unstable
 */
CoglBool
cogl_mir_renderer_set_foreign_connection (CoglRenderer *renderer,
                                          MirConnection *connection);

/**
 * cogl_mir_renderer_get_connection:
 * @renderer: A #CoglRenderer
 *
 * Retrieves the Mir Connection that Cogl is using. If a foreign
 * connection has been specified using
 * cogl_mir_renderer_set_foreign_connection() then that connection will
 * be returned. If no foreign connection has been specified then the
 * display that Cogl creates internally will be returned unless the
 * renderer has not yet been connected (either implicitly or explicitly by
 * calling cogl_renderer_connect()) in which case %NULL is returned.
 *
 * Returns: The mir connection currently associated with @renderer,
 *          or %NULL if the renderer hasn't yet been connected and no
 *          foreign connection has been specified.
 *
 * Since: 1.8
 * Stability: unstable
 */
MirConnection *
cogl_mir_renderer_get_connection (CoglRenderer *renderer);

/*
 * CoglMirEvent:
 * @onscreen: pointer to a #CoglOnscreen structure
 * @surface: pointer to a #MirSurface structure
 * @event: pointer to a #MirEvent structure
 */
typedef struct
{
  CoglOnscreen *onscreen;
  MirSurface *surface;
  MirEvent *event;
} CoglMirEvent;

/*
 * CoglMirEventCallback:
 * @event: pointer to a CoglMirEvent structure
 * @data: (closure): the data that was given when the filter was added
 *
 * A callback function that can be registered with
 * cogl_mir_renderer_add_event_listener().
 *
 * Since: 1.8
 * Stability: unstable
 */
typedef void (* CoglMirEventCallback) (CoglMirEvent *event,
                                       void *data);

/*
 * cogl_mir_renderer_add_event_listener:
 * @renderer: a #CoglRenderer
 * @func: the callback function
 * @data: user data passed to @func when called
 *
 * Adds a callback function that will receive all native events.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_mir_renderer_add_event_listener (CoglRenderer *renderer,
                                      CoglMirEventCallback func,
                                      void *data);

/*
 * cogl_mir_renderer_remove_event_listener:
 * @renderer: a #CoglRenderer
 * @func: the callback function
 * @data: user data given when the callback was installed
 *
 * Removes a callback that was previously added with
 * cogl_mir_renderer_add_filter().
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_mir_renderer_remove_event_listener (CoglRenderer *renderer,
                                         CoglMirEventCallback func,
                                         void *data);

COGL_END_DECLS

#endif /* __COGL_MIR_RENDERER_H__ */
