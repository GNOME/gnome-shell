/* json-object.c - JSON object implementation
 * 
 * This file is part of JSON-GLib
 * Copyright (C) 2007  OpenedHand Ltd.
 * Copyright (C) 2009  Intel Corp.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:
 *   Emmanuele Bassi  <ebassi@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include "json-types-private.h"

/**
 * SECTION:json-object
 * @short_description: a JSON object representation
 *
 * #JsonArray is the representation of the object type inside JSON. It contains
 * #JsonNode<!-- -->s, which may contain fundamental types, arrays or other
 * objects. Each member of an object is accessed using its name. Please note
 * that the member names are normalized internally before being used; every
 * delimiter matching the %G_STR_DELIMITER macro will be transformed into an
 * underscore, so for instance "member-name" and "member_name" are equivalent
 * for a #JsonObject.
 *
 * Since objects can be expensive, they are reference counted. You can control
 * the lifetime of a #JsonObject using json_object_ref() and json_object_unref().
 *
 * To add or overwrite a member with a given name, use json_object_set_member().
 * To extract a member with a given name, use json_object_get_member().
 * To retrieve the list of members, use json_object_get_members().
 * To retrieve the size of the object (that is, the number of members it has),
 * use json_object_get_size().
 */

GType
json_object_get_type (void)
{
  static GType object_type = 0;

  if (G_UNLIKELY (!object_type))
    object_type = g_boxed_type_register_static (g_intern_static_string ("JsonObject"),
                                                (GBoxedCopyFunc) json_object_ref,
                                                (GBoxedFreeFunc) json_object_unref);

  return object_type;
}

/**
 * json_object_new:
 * 
 * Creates a new #JsonObject, an JSON object type representation.
 *
 * Return value: the newly created #JsonObject
 */
JsonObject *
json_object_new (void)
{
  JsonObject *object;

  object = g_slice_new (JsonObject);
  
  object->ref_count = 1;
  object->members = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free,
                                           (GDestroyNotify) json_node_free);

  return object;
}

/**
 * json_object_ref:
 * @object: a #JsonObject
 *
 * Increase by one the reference count of a #JsonObject.
 *
 * Return value: the passed #JsonObject, with the reference count
 *   increased by one.
 */
JsonObject *
json_object_ref (JsonObject *object)
{
  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (object->ref_count > 0, NULL);

  g_atomic_int_exchange_and_add (&object->ref_count, 1);

  return object;
}

/**
 * json_object_unref:
 * @object: a #JsonObject
 *
 * Decreases by one the reference count of a #JsonObject. If the
 * reference count reaches zero, the object is destroyed and all
 * its allocated resources are freed.
 */
void
json_object_unref (JsonObject *object)
{
  gint old_ref;

  g_return_if_fail (object != NULL);
  g_return_if_fail (object->ref_count > 0);

  old_ref = g_atomic_int_get (&object->ref_count);
  if (old_ref > 1)
    g_atomic_int_compare_and_exchange (&object->ref_count, old_ref, old_ref - 1);
  else
    {
      g_hash_table_destroy (object->members);
      object->members = NULL;

      g_slice_free (JsonObject, object);
    }
}

static inline void
object_set_member_internal (JsonObject  *object,
                            const gchar *member_name,
                            JsonNode    *node)
{
  gchar *name;

  name = g_strdelimit (g_strdup (member_name), G_STR_DELIMITERS, '_');
  g_hash_table_replace (object->members, name, node);
}

/**
 * json_object_add_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 * @node: the value of the member
 *
 * Adds a member named @member_name and containing @node into a #JsonObject.
 * The object will take ownership of the #JsonNode.
 *
 * This function will return if the @object already contains a member
 * @member_name.
 *
 * Deprecated: 0.8: Use json_object_set_member() instead
 */
void
json_object_add_member (JsonObject  *object,
                        const gchar *member_name,
                        JsonNode    *node)
{
  gchar *name;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);
  g_return_if_fail (node != NULL);

  if (json_object_has_member (object, member_name))
    {
      g_warning ("JsonObject already has a `%s' member of type `%s'",
                 member_name,
                 json_node_type_name (node));
      return;
    }

  object_set_member_internal (object, member_name, node);
}

/**
 * json_object_set_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 * @node: the value of the member
 *
 * Sets @node as the value of @member_name inside @object.
 *
 * If @object already contains a member called @member_name then
 * the member's current value is overwritten. Otherwise, a new
 * member is added to @object.
 *
 * Since: 0.8
 */
void
json_object_set_member (JsonObject  *object,
                        const gchar *member_name,
                        JsonNode    *node)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);
  g_return_if_fail (node != NULL);

  object_set_member_internal (object, member_name, node);
}

/**
 * json_object_set_int_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 * @value: the value of the member
 *
 * Convenience function for setting an integer @value of
 * @member_name inside @object.
 *
 * See also: json_object_set_member()
 *
 * Since: 0.8
 */
void
json_object_set_int_member (JsonObject  *object,
                            const gchar *member_name,
                            gint         value)
{
  JsonNode *node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  node = json_node_new (JSON_NODE_VALUE);
  json_node_set_int (node, value);
  object_set_member_internal (object, member_name, node);
}

/**
 * json_object_set_double_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 * @value: the value of the member
 *
 * Convenience function for setting a floating point @value
 * of @member_name inside @object.
 *
 * See also: json_object_set_member()
 *
 * Since: 0.8
 */
void
json_object_set_double_member (JsonObject  *object,
                               const gchar *member_name,
                               gdouble      value)
{
  JsonNode *node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  node = json_node_new (JSON_NODE_VALUE);
  json_node_set_double (node, value);
  object_set_member_internal (object, member_name, node);
}

/**
 * json_object_set_boolean_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 * @value: the value of the member
 *
 * Convenience function for setting a boolean @value of
 * @member_name inside @object.
 *
 * See also: json_object_set_member()
 *
 * Since: 0.8
 */
void
json_object_set_boolean_member (JsonObject  *object,
                                const gchar *member_name,
                                gboolean     value)
{
  JsonNode *node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  node = json_node_new (JSON_NODE_VALUE);
  json_node_set_boolean (node, value);
  object_set_member_internal (object, member_name, node);
}

/**
 * json_object_set_string_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 * @value: the value of the member
 *
 * Convenience function for setting a string @value of
 * @member_name inside @object.
 *
 * See also: json_object_set_member()
 *
 * Since: 0.8
 */
void
json_object_set_string_member (JsonObject  *object,
                               const gchar *member_name,
                               const gchar *value)
{
  JsonNode *node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  node = json_node_new (JSON_NODE_VALUE);
  json_node_set_string (node, value);
  object_set_member_internal (object, member_name, node);
}

/**
 * json_object_set_null_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 *
 * Convenience function for setting a null @value of
 * @member_name inside @object.
 *
 * See also: json_object_set_member()
 *
 * Since: 0.8
 */
void
json_object_set_null_member (JsonObject  *object,
                             const gchar *member_name)
{
  JsonNode *node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  node = json_node_new (JSON_NODE_NULL);
  object_set_member_internal (object, member_name, node);
}

/**
 * json_object_set_array_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 * @value: the value of the member
 *
 * Convenience function for setting an array @value of
 * @member_name inside @object.
 *
 * The @object will take ownership of the passed #JsonArray
 *
 * See also: json_object_set_member()
 *
 * Since: 0.8
 */
void
json_object_set_array_member (JsonObject  *object,
                              const gchar *member_name,
                              JsonArray   *value)
{
  JsonNode *node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  node = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (node, value);
  object_set_member_internal (object, member_name, node);
}

/**
 * json_object_set_object_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 * @value: the value of the member
 *
 * Convenience function for setting an object @value of
 * @member_name inside @object.
 *
 * The @object will take ownership of the passed #JsonArray
 *
 * See also: json_object_set_member()
 *
 * Since: 0.8
 */
void
json_object_set_object_member (JsonObject  *object,
                               const gchar *member_name,
                               JsonObject  *value)
{
  JsonNode *node;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  node = json_node_new (JSON_NODE_ARRAY);
  json_node_take_object (node, value);
  object_set_member_internal (object, member_name, node);
}

/* FIXME: yuck */
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 14
static void
get_keys (gpointer key,
          gpointer value,
          gpointer user_data)
{
  GList **keys = user_data;

  *keys = g_list_prepend (*keys, key);
}

static void
get_values (gpointer key,
            gpointer value,
            gpointer user_data)
{
  GList **values = user_data;

  *values = g_list_prepend (*values, value);
}

static GList *
g_hash_table_get_keys (GHashTable *hash_table)
{
  GList *retval = NULL;

  g_return_val_if_fail (hash_table != NULL, NULL);

  g_hash_table_foreach (hash_table, get_keys, &retval);

  return retval;
}

static GList *
g_hash_table_get_values (GHashTable *hash_table)
{
  GList *retval = NULL;

  g_return_val_if_fail (hash_table != NULL, NULL);

  g_hash_table_foreach (hash_table, get_values, &retval);

  return retval;
}
#endif /* GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 14 */

/**
 * json_object_get_members:
 * @object: a #JsonObject
 *
 * Retrieves all the names of the members of a #JsonObject. You can
 * obtain the value for each member using json_object_get_member().
 *
 * Return value: a #GList of member names. The content of the list
 *   is owned by the #JsonObject and should never be modified or
 *   freed. When you have finished using the returned list, use
 *   g_list_free() to free the resources it has allocated.
 */
GList *
json_object_get_members (JsonObject *object)
{
  g_return_val_if_fail (object != NULL, NULL);

  return g_hash_table_get_keys (object->members);
}

/**
 * json_object_get_values:
 * @object: a #JsonObject
 *
 * Retrieves all the values of the members of a #JsonObject.
 *
 * Return value: a #GList of #JsonNode<!-- -->s. The content of the
 *   list is owned by the #JsonObject and should never be modified
 *   or freed. When you have finished using the returned list, use
 *   g_list_free() to free the resources it has allocated.
 */
GList *
json_object_get_values (JsonObject *object)
{
  g_return_val_if_fail (object != NULL, NULL);

  return g_hash_table_get_values (object->members);
}

/**
 * json_object_dup_member:
 * @object: a #JsonObject
 * @member_name: the name of the JSON object member to access
 *
 * Retrieves a copy of the #JsonNode containing the value of @member_name
 * inside a #JsonObject
 *
 * Return value: a copy of the node for the requested object member
 *   or %NULL. Use json_node_free() when done.
 *
 * Since: 0.6
 */
JsonNode *
json_object_dup_member (JsonObject  *object,
                        const gchar *member_name)
{
  JsonNode *retval;

  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (member_name != NULL, NULL);

  retval = json_object_get_member (object, member_name);
  if (!retval)
    return NULL;

  return json_node_copy (retval);
}

static inline JsonNode *
object_get_member_internal (JsonObject  *object,
                            const gchar *member_name)
{
  JsonNode *retval;
  gchar *name;

  name = g_strdelimit (g_strdup (member_name), G_STR_DELIMITERS, '_');

  retval = g_hash_table_lookup (object->members, name);

  g_free (name);

  return retval;
}

/**
 * json_object_get_member:
 * @object: a #JsonObject
 * @member_name: the name of the JSON object member to access
 *
 * Retrieves the #JsonNode containing the value of @member_name inside
 * a #JsonObject.
 *
 * Return value: a pointer to the node for the requested object
 *   member, or %NULL
 */
JsonNode *
json_object_get_member (JsonObject  *object,
                        const gchar *member_name)
{
  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (member_name != NULL, NULL);

  return object_get_member_internal (object, member_name);
}

/**
 * json_object_get_int_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 *
 * Convenience function that retrieves the integer value
 * stored in @member_name of @object
 *
 * See also: json_object_get_member()
 *
 * Return value: the integer value of the object's member
 *
 * Since: 0.8
 */
gint
json_object_get_int_member (JsonObject  *object,
                            const gchar *member_name)
{
  JsonNode *node;

  g_return_val_if_fail (object != NULL, 0);
  g_return_val_if_fail (member_name != NULL, 0);

  node = object_get_member_internal (object, member_name);
  g_return_val_if_fail (node != NULL, 0);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, 0);

  return json_node_get_int (node);
}

/**
 * json_object_get_double_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 *
 * Convenience function that retrieves the floating point value
 * stored in @member_name of @object
 *
 * See also: json_object_get_member()
 *
 * Return value: the floating point value of the object's member
 *
 * Since: 0.8
 */
gdouble
json_object_get_double_member (JsonObject  *object,
                               const gchar *member_name)
{
  JsonNode *node;

  g_return_val_if_fail (object != NULL, 0.0);
  g_return_val_if_fail (member_name != NULL, 0.0);

  node = object_get_member_internal (object, member_name);
  g_return_val_if_fail (node != NULL, 0.0);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, 0.0);

  return json_node_get_double (node);
}

/**
 * json_object_get_boolean_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 *
 * Convenience function that retrieves the boolean value
 * stored in @member_name of @object
 *
 * See also: json_object_get_member()
 *
 * Return value: the boolean value of the object's member
 *
 * Since: 0.8
 */
gboolean
json_object_get_boolean_member (JsonObject  *object,
                                const gchar *member_name)
{
  JsonNode *node;

  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (member_name != NULL, FALSE);

  node = object_get_member_internal (object, member_name);
  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, FALSE);

  return json_node_get_boolean (node);
}

/**
 * json_object_get_null_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 *
 * Convenience function that checks whether the value
 * stored in @member_name of @object is null
 *
 * See also: json_object_get_member()
 *
 * Return value: %TRUE if the value is null
 *
 * Since: 0.8
 */
gboolean
json_object_get_null_member (JsonObject  *object,
                             const gchar *member_name)
{
  JsonNode *node;

  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (member_name != NULL, FALSE);

  node = object_get_member_internal (object, member_name);
  g_return_val_if_fail (node != NULL, FALSE);

  return JSON_NODE_TYPE (node) == JSON_NODE_NULL;
}

/**
 * json_object_get_string_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 *
 * Convenience function that retrieves the string value
 * stored in @member_name of @object
 *
 * See also: json_object_get_member()
 *
 * Return value: the string value of the object's member
 *
 * Since: 0.8
 */
G_CONST_RETURN gchar *
json_object_get_string_member (JsonObject  *object,
                               const gchar *member_name)
{
  JsonNode *node;

  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (member_name != NULL, NULL);

  node = object_get_member_internal (object, member_name);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, NULL);

  return json_node_get_string (node);
}

/**
 * json_object_get_array_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 *
 * Convenience function that retrieves the array
 * stored in @member_name of @object
 *
 * See also: json_object_get_member()
 *
 * Return value: the array inside the object's member
 *
 * Since: 0.8
 */
JsonArray *
json_object_get_array_member (JsonObject  *object,
                              const gchar *member_name)
{
  JsonNode *node;

  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (member_name != NULL, NULL);

  node = object_get_member_internal (object, member_name);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_ARRAY, NULL);

  return json_node_get_array (node);
}

/**
 * json_object_get_object_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 *
 * Convenience function that retrieves the object
 * stored in @member_name of @object
 *
 * See also: json_object_get_member()
 *
 * Return value: the object inside the object's member
 *
 * Since: 0.8
 */
JsonObject *
json_object_get_object_member (JsonObject  *object,
                               const gchar *member_name)
{
  JsonNode *node;

  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (member_name != NULL, NULL);

  node = object_get_member_internal (object, member_name);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_OBJECT, NULL);

  return json_node_get_object (node);
}

/**
 * json_object_has_member:
 * @object: a #JsonObject
 * @member_name: the name of a JSON object member
 *
 * Checks whether @object has a member named @member_name.
 *
 * Return value: %TRUE if the JSON object has the requested member
 */
gboolean
json_object_has_member (JsonObject *object,
                        const gchar *member_name)
{
  gchar *name;
  gboolean retval;

  g_return_val_if_fail (object != NULL, FALSE);
  g_return_val_if_fail (member_name != NULL, FALSE);

  name = g_strdelimit (g_strdup (member_name), G_STR_DELIMITERS, '_');
  retval = (g_hash_table_lookup (object->members, name) != NULL);
  g_free (name);

  return retval;
}

/**
 * json_object_get_size:
 * @object: a #JsonObject
 *
 * Retrieves the number of members of a #JsonObject.
 *
 * Return value: the number of members
 */
guint
json_object_get_size (JsonObject *object)
{
  g_return_val_if_fail (object != NULL, 0);

  return g_hash_table_size (object->members);
}

/**
 * json_object_remove_member:
 * @object: a #JsonObject
 * @member_name: the name of the member to remove
 *
 * Removes @member_name from @object, freeing its allocated resources.
 */
void
json_object_remove_member (JsonObject  *object,
                           const gchar *member_name)
{
  gchar *name;

  g_return_if_fail (object != NULL);
  g_return_if_fail (member_name != NULL);

  name = g_strdelimit (g_strdup (member_name), G_STR_DELIMITERS, '_');
  g_hash_table_remove (object->members, name);
  g_free (name);
}

typedef struct _ForeachClosure  ForeachClosure;

struct _ForeachClosure
{
  JsonObject *object;

  JsonObjectForeach func;
  gpointer data;
};

static void
json_object_foreach_internal (gpointer key,
                              gpointer value,
                              gpointer data)
{
  ForeachClosure *clos = data;
  const gchar *member_name = key;
  JsonNode *member_node = value;

  clos->func (clos->object, member_name, member_node, clos->data);
}

/**
 * json_object_foreach_member:
 * @object: a #JsonObject
 * @func: the function to be called on each member
 * @data: data to be passed to the function
 *
 * Iterates over all members of @object and calls @func on
 * each one of them.
 *
 * It is safe to change the value of a #JsonNode of the @object
 * from within the iterator @func, but it is not safe to add or
 * remove members from the @object.
 *
 * Since: 0.8
 */
void
json_object_foreach_member (JsonObject        *object,
                            JsonObjectForeach  func,
                            gpointer           data)
{
  ForeachClosure clos;

  g_return_if_fail (object != NULL);
  g_return_if_fail (func != NULL);

  clos.object = object;
  clos.func = func;
  clos.data = data;
  g_hash_table_foreach (object->members,
                        json_object_foreach_internal,
                        &clos);
}
