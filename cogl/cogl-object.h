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

#include <cogl/cogl-types.h>

COGL_BEGIN_DECLS

typedef struct _CoglObject      CoglObject;

#define COGL_OBJECT(X)          ((CoglObject *)X)

/**
 * cogl_object_ref: (skip)
 * @object: a #CoglObject
 *
 * Increases the reference count of @object by 1
 *
 * Returns: the @object, with its reference count increased
 */
void *
cogl_object_ref (void *object);

/**
 * cogl_object_unref: (skip)
 * @object: a #CoglObject
 *
 * Drecreases the reference count of @object by 1; if the reference
 * count reaches 0, the resources allocated by @object will be freed
 */
void
cogl_object_unref (void *object);

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
typedef struct {
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
#ifdef COGL_HAS_GTYPE_SUPPORT
typedef GDestroyNotify CoglUserDataDestroyCallback;
#else
typedef void (*CoglUserDataDestroyCallback) (void *user_data);
#endif

/**
 * CoglDebugObjectTypeInfo:
 * @name: A human readable name for the type.
 * @instance_count: The number of objects of this type that are
 *   currently in use
 *
 * This struct is used to pass information to the callback when
 * cogl_debug_object_foreach_type() is called.
 *
 * Since: 1.8
 * Stability: unstable
 */
typedef struct {
  const char *name;
  unsigned long instance_count;
} CoglDebugObjectTypeInfo;

/**
 * CoglDebugObjectForeachTypeCallback:
 * @info: A pointer to a struct containing information about the type.
 *
 * A callback function to use for cogl_debug_object_foreach_type().
 *
 * Since: 1.8
 * Stability: unstable
 */
typedef void
(* CoglDebugObjectForeachTypeCallback) (const CoglDebugObjectTypeInfo *info,
                                        void *user_data);

/**
 * cogl_object_set_user_data: (skip)
 * @object: The object to associate private data with
 * @key: The address of a #CoglUserDataKey which provides a unique value
 *   with which to index the private data.
 * @user_data: The data to associate with the given object,
 *   or %NULL to remove a previous association.
 * @destroy: A #CoglUserDataDestroyCallback to call if the object is
 *   destroyed or if the association is removed by later setting
 *   %NULL data for the same key.
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
 * cogl_object_get_user_data: (skip)
 * @object: The object with associated private data to query
 * @key: The address of a #CoglUserDataKey which provides a unique value
 *       with which to index the private data.
 *
 * Finds the user data previously associated with @object using
 * the given @key. If no user data has been associated with @object
 * for the given @key this function returns NULL.
 *
 * Returns: (transfer none): The user data previously associated
 *   with @object using the given @key; or %NULL if no associated
 *   data is found.
 *
 * Since: 1.4
 */
void *
cogl_object_get_user_data (CoglObject *object,
                           CoglUserDataKey *key);

#ifdef COGL_ENABLE_EXPERIMENTAL_API

/**
 * cogl_debug_object_foreach_type:
 * @func: (scope call): A callback function for each type
 * @user_data: (closure): A pointer to pass to @func
 *
 * Invokes @func once for each type of object that Cogl uses and
 * passes a count of the number of objects for that type. This is
 * intended to be used solely for debugging purposes to track down
 * issues with objects leaking.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_debug_object_foreach_type (CoglDebugObjectForeachTypeCallback func,
                                void *user_data);

/**
 * cogl_debug_object_print_instances:
 *
 * Prints a list of all the object types that Cogl uses along with the
 * number of objects of that type that are currently in use. This is
 * intended to be used solely for debugging purposes to track down
 * issues with objects leaking.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_debug_object_print_instances (void);

#endif /* COGL_ENABLE_EXPERIMENTAL_API */

COGL_END_DECLS

#endif /* __COGL_OBJECT_H */

