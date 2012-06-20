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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
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
 * cogl_poll_get_info() and cogl_poll_dispatch() in applications that
 * are already using the GLib main loop. After this is called the
 * #GSource should be attached to the main loop using
 * g_source_attach().
 *
 * Return value: a new #GSource
 *
 * Stability: unstable
 * Since: 1.10
 */
GSource *
cogl_glib_source_new (CoglContext *context,
                      int priority);

G_END_DECLS

#endif /* __COGL_GSOURCE_H__ */
