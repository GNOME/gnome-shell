/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 * Copyright (C) 2009 Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __CLUTTER_EVENT_GLX_H__
#define __CLUTTER_EVENT_GLX_H__

#include <glib.h>
#include <clutter/clutter-event.h>
#include <clutter/clutter-backend.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

gboolean
clutter_backend_glx_handle_event (ClutterBackendX11 *backend,
                                  XEvent            *xevent);

G_END_DECLS

#endif /* __CLUTTER_EVENT_GLX_H__ */

