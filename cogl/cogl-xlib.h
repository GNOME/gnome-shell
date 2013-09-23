/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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

#ifndef __COGL_XLIB_H__
#define __COGL_XLIB_H__

#include <X11/Xlib.h>

#define __COGL_XLIB_H_INSIDE__
#define __COGL_H_INSIDE__

#include <cogl/cogl-types.h>
#include <cogl/deprecated/cogl-clutter-xlib.h>
#include <cogl/cogl-xlib-renderer.h>
#include <cogl/cogl-macros.h>

COGL_BEGIN_DECLS

/*
 * cogl_xlib_get_display:
 *
 * Return value: the Xlib display that will be used by the Xlib winsys
 * backend. The display needs to be set with _cogl_xlib_set_display()
 * before this function is called.
 *
 * Stability: Unstable
 * Deprecated: 1.16: Use cogl_xlib_renderer_get_display() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_xlib_renderer_get_display)
Display *
cogl_xlib_get_display (void);

/*
 * cogl_xlib_set_display:
 *
 * Sets the Xlib display that Cogl will use for the Xlib winsys
 * backend. This function should eventually go away when Cogl gains a
 * more complete winsys abstraction.
 *
 * Stability: Unstable
 * Deprecated: 1.16: Use cogl_xlib_renderer_set_foreign_display()
 *                   instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_xlib_renderer_set_foreign_display)
void
cogl_xlib_set_display (Display *display);

/*
 * cogl_xlib_handle_event:
 * @xevent: pointer to XEvent structure
 *
 * This function processes a single X event; it can be used to hook
 * into external X event retrieval (for example that done by Clutter
 * or GDK).
 *
 * Return value: #CoglXlibFilterReturn. %COGL_XLIB_FILTER_REMOVE
 * indicates that Cogl has internally handled the event and the
 * caller should do no further processing. %COGL_XLIB_FILTER_CONTINUE
 * indicates that Cogl is either not interested in the event,
 * or has used the event to update internal state without taking
 * any exclusive action.
 *
 * Stability: Unstable
 * Deprecated: 1.16: Use cogl_xlib_renderer_handle_event() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_xlib_renderer_handle_event)
CoglFilterReturn
cogl_xlib_handle_event (XEvent *xevent);

COGL_END_DECLS

#undef __COGL_XLIB_H_INSIDE__

#endif /* __COGL_XLIB_H__ */
