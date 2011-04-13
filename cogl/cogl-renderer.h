/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_RENDERER_H__
#define __COGL_RENDERER_H__

#include <glib.h>

#include <cogl/cogl-types.h>
#include <cogl/cogl-onscreen-template.h>

#ifdef COGL_HAS_XLIB
#include <X11/Xlib.h>
#endif

G_BEGIN_DECLS

/**
 * SECTION:cogl-renderer
 * @short_description:
 *
 */

#define cogl_renderer_error_quark cogl_renderer_error_quark_EXP

#define COGL_RENDERER_ERROR cogl_renderer_error_quark ()
GQuark
cogl_renderer_error_quark (void);

typedef struct _CoglRenderer CoglRenderer;

#define cogl_is_renderer cogl_is_renderer_EXP
gboolean
cogl_is_renderer (void *object);

#define cogl_renderer_new cogl_renderer_new_EXP
CoglRenderer *
cogl_renderer_new (void);

/* optional configuration APIs */

#define cogl_renderer_handle_native_event cogl_renderer_handle_native_event_EXP
/*
 * cogl_renderer_handle_native_event:
 * @event: pointer to native event structure
 *
 * This function processes a single event; it can be used to hook into
 * external event retrieval (for example that done by Clutter or
 * GDK). The type of the structure that event points to depends on the
 * window system used for the renderer. On an xlib renderer it would
 * be a pointer to an XEvent or an a Windows renderer it would be a
 * pointer to a MSG struct.
 *
 * Return value: #CoglFilterReturn. %COGL_FILTER_REMOVE indicates that
 * Cogl has internally handled the event and the caller should do no
 * further processing. %COGL_FILTER_CONTINUE indicates that Cogl is
 * either not interested in the event, or has used the event to update
 * internal state without taking any exclusive action.
 */
CoglFilterReturn
cogl_renderer_handle_native_event (CoglRenderer *renderer,
                                   void *event);

#define cogl_renderer_add_native_filter cogl_renderer_add_native_filter_EXP
/*
 * _cogl_renderer_add_native_filter:
 *
 * Adds a callback function that will receive all native events. The
 * function can stop further processing of the event by return
 * %COGL_FILTER_REMOVE. What is considered a native event depends on
 * the type of renderer used. An xlib based renderer would pass all
 * XEvents whereas a Windows based renderer would pass MSGs.
 */
void
cogl_renderer_add_native_filter (CoglRenderer *renderer,
                                 CoglNativeFilterFunc func,
                                 void *data);

#define cogl_renderer_remove_native_filter \
  cogl_renderer_remove_native_filter_EXP
/*
 * _cogl_renderer_remove_native_filter:
 *
 * Removes a callback that was previously added with
 * _cogl_renderer_add_native_filter().
 */
void
cogl_renderer_remove_native_filter (CoglRenderer *renderer,
                                    CoglNativeFilterFunc func,
                                    void *data);

#ifdef COGL_HAS_XLIB

#define cogl_renderer_xlib_get_foreign_display \
  cogl_renderer_xlib_get_foreign_display_EXP
/*
 * cogl_renderer_xlib_get_foreign_display:
 *
 * Return value: the foreign Xlib display that will be used by any Xlib based
 * winsys backend. The display needs to be set with
 * cogl_renderer_xlib_set_foreign_display() before this function is called.
 */
Display *
cogl_renderer_xlib_get_foreign_display (CoglRenderer *renderer);

#define cogl_renderer_xlib_set_foreign_display \
  cogl_renderer_xlib_set_foreign_display_EXP
/*
 * cogl_renderer_xlib_set_foreign_display:
 *
 * Sets a foreign Xlib display that Cogl will use for and Xlib based winsys
 * backend.
 */
void
cogl_renderer_xlib_set_foreign_display (CoglRenderer *renderer,
                                        Display *display);

#define cogl_renderer_xlib_get_display cogl_renderer_xlib_get_display_EXP
Display *
cogl_renderer_xlib_get_display (CoglRenderer *renderer);

#endif /* COGL_HAS_XLIB */

#define cogl_renderer_check_onscreen_template \
  cogl_renderer_check_onscreen_template_EXP
gboolean
cogl_renderer_check_onscreen_template (CoglRenderer *renderer,
                                       CoglOnscreenTemplate *onscreen_template,
                                       GError **error);

/* Final connection API */

#define cogl_renderer_connect cogl_renderer_connect_EXP
gboolean
cogl_renderer_connect (CoglRenderer *renderer, GError **error);

G_END_DECLS

#endif /* __COGL_RENDERER_H__ */

