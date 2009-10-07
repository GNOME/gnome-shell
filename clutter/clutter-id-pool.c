/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006-2008 OpenedHand
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
 *
 * ClutterIDPool: pool of reusable integer ids associated with pointers.
 *
 * Author: Øyvind Kolås <pippin@o-hand-com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-debug.h"
#include "clutter-id-pool.h"

struct _ClutterIDPool
{
  GArray *array;     /* Array of pointers    */
  GSList *free_ids;  /* A stack of freed ids */
};

ClutterIDPool *
clutter_id_pool_new  (guint initial_size)
{
  ClutterIDPool *self;

  self = g_slice_new (ClutterIDPool);

  self->array = g_array_sized_new (FALSE, FALSE, 
                                   sizeof (gpointer), initial_size);
  self->free_ids = NULL;
  return self;
}

void
clutter_id_pool_free (ClutterIDPool *id_pool)
{
  g_return_if_fail (id_pool != NULL);

  g_array_free (id_pool->array, TRUE);
  g_slist_free (id_pool->free_ids);
  g_slice_free (ClutterIDPool, id_pool);
}

guint32
clutter_id_pool_add (ClutterIDPool *id_pool,
                     gpointer       ptr)
{
  gpointer *array;
  guint32   id;

  g_return_val_if_fail (id_pool != NULL, 0);

  if (id_pool->free_ids) /* There are items on our freelist, reuse one */
    {
      array = (void*) id_pool->array->data;
      id = GPOINTER_TO_UINT (id_pool->free_ids->data);

      id_pool->free_ids = g_slist_remove (id_pool->free_ids,
                                          id_pool->free_ids->data);
      array[id] = ptr;
      return id;
    }

  /* Allocate new id */
  id = id_pool->array->len;
  g_array_append_val (id_pool->array, ptr);
  return id;
}

void
clutter_id_pool_remove (ClutterIDPool *id_pool,
                        guint32        id)
{
  gpointer *array;

  g_return_if_fail (id_pool != NULL);
  array = (void*) id_pool->array->data;

  array[id] = (void*)0xdecafbad;   /* set pointer to a recognizably voided
                                      value */

  id_pool->free_ids = g_slist_prepend (id_pool->free_ids,
                                       GUINT_TO_POINTER (id));
}

gpointer
clutter_id_pool_lookup (ClutterIDPool *id_pool,
                        guint32        id)
{
  gpointer *array;

  g_return_val_if_fail (id_pool != NULL, NULL);
  g_return_val_if_fail (id_pool->array != NULL, NULL);

  array = (void*) id_pool->array->data;

  if (id >= id_pool->array->len || array[id] == NULL)
    {
      g_warning ("The required ID of %u does not refer to an existing actor; "
                 "this usually implies that the pick() of an actor is not "
                 "correctly implemented or that there is an error in the "
                 "glReadPixels() implementation of the GL driver.", id);
      return NULL;
    }

  return array[id];
}
