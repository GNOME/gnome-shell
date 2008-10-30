/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2008 OpenedHand
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
 * ClutterIDPool: pool of reusable integer ids associated with pointers.
 *
 * Author: Øyvind Kolås <pippin@o-hand.com>
 */

#ifndef __CLUTTER_ID_POOL_H__
#define __CLUTTER_ID_POOL_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _ClutterIDPool   ClutterIDPool;


ClutterIDPool *clutter_id_pool_new     (guint          initial_size);
void           clutter_id_pool_free    (ClutterIDPool *id_pool);
guint32        clutter_id_pool_add     (ClutterIDPool *id_pool,
                                        gpointer       ptr);
void           clutter_id_pool_remove  (ClutterIDPool *id_pool,
                                        guint32        id);
gpointer       clutter_id_pool_lookup  (ClutterIDPool *id_pool,
                                        guint32        id);


G_END_DECLS

#endif /* __CLUTTER_ID_POOL_H__ */
