/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
 * ClutterTimeoutPool: pool of timeout functions using the same slice of
 *                     the GLib main loop
 *
 * Author: Emmanuele Bassi <ebassi@openedhand.com>
 *
 * Based on similar code by Tristan van Berkom
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_TIMEOUT_POOL_H__
#define __CLUTTER_TIMEOUT_POOL_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _ClutterTimeoutPool    ClutterTimeoutPool;

ClutterTimeoutPool *clutter_timeout_pool_new    (gint                priority);
guint               clutter_timeout_pool_add    (ClutterTimeoutPool *pool,
                                                 guint               fps,
                                                 GSourceFunc         func,
                                                 gpointer            data,
                                                 GDestroyNotify      notify);
void                clutter_timeout_pool_remove (ClutterTimeoutPool *pool,
                                                 guint               id);

G_END_DECLS

#endif /* __CLUTTER_TIMEOUT_POOL_H__ */
