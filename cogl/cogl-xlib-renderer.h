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

#if !defined(__COGL_XLIB_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl-xlib.h> can be included directly."
#endif

#ifndef __COGL_XLIB_RENDERER_H__
#define __COGL_XLIB_RENDERER_H__

#include <X11/Xlib.h>

/* NB: this is a top-level header that can be included directly but we
 * want to be careful not to define __COGL_H_INSIDE__ when this is
 * included internally while building Cogl itself since
 * __COGL_H_INSIDE__ is used in headers to guard public vs private api
 * definitions
 */
#ifndef COGL_COMPILATION

/* Note: When building Cogl .gir we explicitly define
 * __COGL_H_INSIDE__ */
#ifndef __COGL_H_INSIDE__
#define __COGL_H_INSIDE__
#define __COGL_XLIB_RENDERER_H_MUST_UNDEF_COGL_H_INSIDE__
#endif

#endif /* COGL_COMPILATION */

#include <cogl/cogl-renderer.h>

COGL_BEGIN_DECLS

/*
 * cogl_xlib_renderer_handle_event:
 * @renderer: a #CoglRenderer
 * @event: pointer to an XEvent structure
 *
 * This function processes a single event; it can be used to hook into
 * external event retrieval (for example that done by Clutter or
 * GDK).
 *
 * Return value: #CoglFilterReturn. %COGL_FILTER_REMOVE indicates that
 * Cogl has internally handled the event and the caller should do no
 * further processing. %COGL_FILTER_CONTINUE indicates that Cogl is
 * either not interested in the event, or has used the event to update
 * internal state without taking any exclusive action.
 */
CoglFilterReturn
cogl_xlib_renderer_handle_event (CoglRenderer *renderer,
                                 XEvent *event);

/*
 * CoglXlibFilterFunc:
 * @event: pointer to an XEvent structure
 * @data: the data that was given when the filter was added
 *
 * A callback function that can be registered with
 * cogl_xlib_renderer_add_filter(). The function should return
 * %COGL_FILTER_REMOVE if it wants to prevent further processing or
 * %COGL_FILTER_CONTINUE otherwise.
 */
typedef CoglFilterReturn (* CoglXlibFilterFunc) (XEvent *event,
                                                 void *data);

/*
 * cogl_xlib_renderer_add_filter:
 * @renderer: a #CoglRenderer
 * @func: the callback function
 * @data: user data passed to @func when called
 *
 * Adds a callback function that will receive all native events. The
 * function can stop further processing of the event by return
 * %COGL_FILTER_REMOVE.
 */
void
cogl_xlib_renderer_add_filter (CoglRenderer *renderer,
                               CoglXlibFilterFunc func,
                               void *data);

/*
 * cogl_xlib_renderer_remove_filter:
 * @renderer: a #CoglRenderer
 * @func: the callback function
 * @data: user data given when the callback was installed
 *
 * Removes a callback that was previously added with
 * cogl_xlib_renderer_add_filter().
 */
void
cogl_xlib_renderer_remove_filter (CoglRenderer *renderer,
                                  CoglXlibFilterFunc func,
                                  void *data);

/*
 * cogl_xlib_renderer_get_foreign_display:
 * @renderer: a #CoglRenderer
 *
 * Return value: the foreign Xlib display that will be used by any Xlib based
 * winsys backend. The display needs to be set with
 * cogl_xlib_renderer_set_foreign_display() before this function is called.
 */
Display *
cogl_xlib_renderer_get_foreign_display (CoglRenderer *renderer);

/*
 * cogl_xlib_renderer_set_foreign_display:
 * @renderer: a #CoglRenderer
 *
 * Sets a foreign Xlib display that Cogl will use for and Xlib based winsys
 * backend.
 *
 * Note that calling this function will automatically call
 * cogl_xlib_renderer_set_event_retrieval_enabled() to disable Cogl's
 * event retrieval. Cogl still needs to see all of the X events so the
 * application should also use cogl_xlib_renderer_handle_event() if it
 * uses this function.
 */
void
cogl_xlib_renderer_set_foreign_display (CoglRenderer *renderer,
                                        Display *display);

/**
 * cogl_xlib_renderer_set_event_retrieval_enabled:
 * @renderer: a #CoglRenderer
 * @enable: The new value
 *
 * Sets whether Cogl should automatically retrieve events from the X
 * display. This defaults to %TRUE unless
 * cogl_xlib_renderer_set_foreign_display() is called. It can be set
 * to %FALSE if the application wants to handle its own event
 * retrieval. Note that Cogl still needs to see all of the X events to
 * function properly so the application should call
 * cogl_xlib_renderer_handle_event() for each event if it disables
 * automatic event retrieval.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_xlib_renderer_set_event_retrieval_enabled (CoglRenderer *renderer,
                                                CoglBool enable);

Display *
cogl_xlib_renderer_get_display (CoglRenderer *renderer);

COGL_END_DECLS

/* The gobject introspection scanner seems to parse public headers in
 * isolation which means we need to be extra careful about how we
 * define and undefine __COGL_H_INSIDE__ used to detect when internal
 * headers are incorrectly included by developers. In the gobject
 * introspection case we have to manually define __COGL_H_INSIDE__ as
 * a commandline argument for the scanner which means we must be
 * careful not to undefine it in a header...
 */
#ifdef __COGL_XLIB_RENDERER_H_MUST_UNDEF_COGL_H_INSIDE__
#undef __COGL_H_INSIDE__
#undef __COGL_XLIB_RENDERER_H_MUST_UNDEF_COGL_H_INSIDE__
#endif

#endif /* __COGL_XLIB_RENDERER_H__ */
