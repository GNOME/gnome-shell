/* json-object.c - JSON object implementation
 * 
 * This file is part of JSON-GLib
 * Copyright (C) 2007  OpenedHand Ltd.
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
 * Author:
 *   Emmanuele Bassi  <ebassi@openedhand.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include "json-types.h"

/**
 * SECTION:json-object
 * @short_description: a JSON object representation
 *
 * #JsonArray is the representation of the object type inside JSON. It contains
 * #JsonNode<!-- -->s, which may contain fundamental types, arrays or other
 * objects. Each member of an object is accessed using its name.
 *
 * Since objects can be expensive, they are reference counted. You can control
 * the lifetime of a #JsonObject using json_object_ref() and json_object_unref().
 *
 * To add a member with a given name, use json_object_add_member().
 * To extract a member with a given name, use json_object_get_member().
 * To retrieve the list of members, use json_object_get_members().
 * To retrieve the size of the object (that is, the number of members it has), use
 * json_object_get_size().
 */

struct _JsonObject
{
  GHashTable *members;

  volatile gint ref_count;
};

GType
json_object_get_type (void)
{
  static GType object_type = 0;

  if (G_UNLIKELY (!object_type))
    object_type = g_boxed_type_register_static ("JsonObject",
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

/**
 * json_object_add_member:
 * @object: a #JsonObject
 * @member_name: the name of the member
 * @node: the value of the member
 *
 * Adds a member named @member_name and containing @node into a #JsonObject.
 * The object will take ownership of the #JsonNode.
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
      g_warning ("JsonObject already has a '%s' member of type '%s'",
                 member_name,
                 json_node_type_name (node));
      return;
    }

  name = g_strdelimit (g_strdup (member_name), G_STR_DELIMITERS, '_');
  g_hash_table_replace (object->members, name, node);
}

/* FIXME: yuck. we really need to depend on GLib 2.14 */
#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 14
static void
get_keys (gpointer key,
          gpointer value,
          gpointer user_data)
{
  GList **keys = user_data;

  *keys = g_list_prepend (*keys, key);
}

static GList *
g_hash_table_get_keys (GHashTable *hash_table)
{
  GList *retval = NULL;

  g_return_val_if_fail (hash_table != NULL, NULL);

  g_hash_table_foreach (hash_table, get_keys, &retval);

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
 * Return value: (transfer container) (element-type utf8): a #GList of
 *   member names. The content of the list is owned by the #JsonObject
 *   and should never be modified or freed. When you have finished using
 *   the returned list, use g_list_free() to free the resources it has
 *   allocated.
 */
GList *
json_object_get_members (JsonObject *object)
{
  g_return_val_if_fail (object != NULL, NULL);

  return g_hash_table_get_keys (object->members);
}

/**
 * json_object_get_member:
 * @object: a #JsonObject
 * @member_name: the name of the JSON object member to access
 *
 * Retrieves the #JsonNode containing the value of @member_name inside
 * a #JsonObject.
 *
 * Return value: (transfer none): a pointer to the node for the requested object
 *   member, or %NULL
 */
JsonNode *
json_object_get_member (JsonObject *object,
                        const gchar *member_name)
{
  gchar *name;
  JsonNode *retval;

  g_return_val_if_fail (object != NULL, NULL);
  g_return_val_if_fail (member_name != NULL, NULL);

  name = g_strdelimit (g_strdup (member_name), G_STR_DELIMITERS, '_');
  retval = g_hash_table_lookup (object->members, name);
  g_free (name);

  return retval;
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
