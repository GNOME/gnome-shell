/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009,2010 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_OBJECT_H
#define __COGL_OBJECT_H

typedef struct _CoglObject      CoglObject;

#define COGL_OBJECT(X)          ((CoglObject *)X)

/**
 * CoglUserDataKey:
 * @unused: ignored.
 *
 * A #CoglUserDataKey is used to declare a key for attaching data to a
 * #CoglObject using cogl_object_set_user_data. The typedef only exists as a
 * formality to make code self documenting since only the unique address of a
 * #CoglUserDataKey is used.
 *
 * Typically you would declare a static #CoglUserDataKey and set private data
 * on an object something like this:
 *
 * |[
 * static CoglUserDataKey path_private_key;
 *
 * static void
 * destroy_path_private_cb (void *data)
 * {
 *   g_free (data);
 * }
 *
 * static void
 * my_path_set_data (CoglPath *path, void *data)
 * {
 *   cogl_object_set_user_data (COGL_OBJECT (path),
 *                              &private_key,
 *                              data,
 *                              destroy_path_private_cb);
 * }
 * ]|
 *
 * Since: 1.4
 */
typedef struct
{
  int unused;
} CoglUserDataKey;

/**
 * CoglUserDataDestroyCallback:
 * @user_data: The data whos association with a #CoglObject has been
 *             destoyed.
 *
 * When associating private data with a #CoglObject a callback can be
 * given which will be called either if the object is destroyed or if
 * cogl_object_set_user_data() is called with NULL user_data for the
 * same key.
 *
 * Since: 1.4
 */
typedef void (*CoglUserDataDestroyCallback) (void *user_data);

/**
 * cogl_object_set_user_data:
 * @object: The object to associate private data with
 * @key: The address of a #CoglUserDataKey which provides a unique value
 *       with which to index the private data.
 * @user_data: The data to associate with the given object, or NULL to
 *             remove a previous association.
 * @destroy: A #CoglUserDataDestroyCallback to call if the object is
 *           destroyed or if the association is removed by later setting
 *           NULL data for the same key.
 *
 * Associates some private @user_data with a given #CoglObject. To
 * later remove the association call cogl_object_set_user_data() with
 * the same @key but NULL for the @user_data.
 *
 * Since: 1.4
 */
void
cogl_object_set_user_data (CoglObject *object,
                           CoglUserDataKey *key,
                           void *user_data,
                           CoglUserDataDestroyCallback destroy);

/**
 * cogl_object_get_user_data:
 * @object: The object with associated private data to query
 * @key: The address of a #CoglUserDataKey which provides a unique value
 *       with which to index the private data.
 *
 * Finds the user data previously associated with @object using
 * the given @key. If no user data has been associated with @object
 * for the given @key this function returns NULL.
 *
 * Returns: The user data previously associated with @object using
 * the given @key; or NULL if no associated data is found.
 *
 * Since: 1.4
 */
void *
cogl_object_get_user_data (CoglObject *object,
                           CoglUserDataKey *key);
#endif /* __COGL_OBJECT_H */

