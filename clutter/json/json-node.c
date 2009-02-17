/* json-node.c - JSON object model node
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
 * SECTION:json-node
 * @short_description: Node in a JSON object model
 *
 * A #JsonNode is a generic container of elements inside a JSON stream.
 * It can contain fundamental types (integers, booleans, floating point
 * numbers, strings) and complex types (arrays and objects).
 *
 * When parsing a JSON data stream you extract the root node and walk
 * the node tree by retrieving the type of data contained inside the
 * node with the %JSON_NODE_TYPE macro. If the node contains a fundamental
 * type you can retrieve a copy of the GValue holding it with the
 * json_node_get_value() function, and then use the GValue API to extract
 * the data; if the node contains a complex type you can retrieve the
 * #JsonObject or the #JsonArray using json_node_get_object() or
 * json_node_get_array() respectively, and then retrieve the nodes
 * they contain.
 */

/**
 * json_node_new:
 * @type: a #JsonNodeType
 *
 * Creates a new #JsonNode of @type.
 *
 * Return value: the newly created #JsonNode
 */
JsonNode *
json_node_new (JsonNodeType type)
{
  JsonNode *data;

  g_return_val_if_fail (type >= JSON_NODE_OBJECT && type <= JSON_NODE_NULL, NULL);

  data = g_slice_new0 (JsonNode);
  data->type = type;

  return data;
}

/**
 * json_node_copy:
 * @node: a #JsonNode
 *
 * Copies @node. If the node contains complex data types then the reference
 * count of the objects is increased.
 *
 * Return value: the copied #JsonNode
 */
JsonNode *
json_node_copy (JsonNode *node)
{
  JsonNode *copy;

  g_return_val_if_fail (node != NULL, NULL);

  copy = g_slice_new (JsonNode);
  *copy = *node;

  switch (copy->type)
    {
    case JSON_NODE_OBJECT:
      copy->data.object = json_object_ref (node->data.object);
      break;
    case JSON_NODE_ARRAY:
      copy->data.array = json_array_ref (node->data.array);
      break;
    case JSON_NODE_VALUE:
      g_value_init (&(copy->data.value), G_VALUE_TYPE (&(node->data.value)));
      g_value_copy (&(node->data.value), &(copy->data.value));
      break;
    case JSON_NODE_NULL:
      break;
    default:
      g_assert_not_reached ();
    }

  return copy;
}

/**
 * json_node_set_object:
 * @node: a #JsonNode
 * @object: a #JsonObject
 *
 * Sets @objects inside @node. The reference count of @object is increased.
 */
void
json_node_set_object (JsonNode   *node,
                      JsonObject *object)
{
  g_return_if_fail (node != NULL);
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_OBJECT);

  if (node->data.object)
    json_object_unref (node->data.object);

  if (object)
    node->data.object = json_object_ref (object);
  else
    node->data.object = NULL;
}

/**
 * json_node_take_object:
 * @node: a #JsonNode
 * @object: a #JsonObject
 *
 * Sets @object inside @node. The reference count of @object is not increased.
 */
void
json_node_take_object (JsonNode   *node,
                       JsonObject *object)
{
  g_return_if_fail (node != NULL);
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_OBJECT);

  if (node->data.object)
    {
      json_object_unref (node->data.object);
      node->data.object = NULL;
    }

  if (object)
    node->data.object = object;
}

/**
 * json_node_get_object:
 * @node: a #JsonNode
 *
 * Retrieves the #JsonObject stored inside a #JsonNode
 *
 * Return value: (transfer none): the #JsonObject
 */
JsonObject *
json_node_get_object (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_OBJECT, NULL);

  return node->data.object;
}

/**
 * json_node_dup_object:
 * @node: a #JsonNode
 *
 * Retrieves the #JsonObject inside @node. The reference count of
 * the returned object is increased.
 *
 * Return value: the #JsonObject
 */
JsonObject *
json_node_dup_object (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_OBJECT, NULL);

  if (node->data.object)
    return json_object_ref (node->data.object);
  
  return NULL;
}

/**
 * json_node_set_array:
 * @node: a #JsonNode
 * @array: a #JsonArray
 *
 * Sets @array inside @node and increases the #JsonArray reference count
 */
void
json_node_set_array (JsonNode  *node,
                     JsonArray *array)
{
  g_return_if_fail (node != NULL);
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_ARRAY);

  if (node->data.array)
    json_array_unref (node->data.array);

  if (array)
    node->data.array = json_array_ref (array);
  else
    node->data.array = NULL;
}

/**
 * json_node_take_array:
 * @node: a #JsonNode
 * @array: a #JsonArray
 *
 * Sets @array into @node without increasing the #JsonArray reference count.
 */
void
json_node_take_array (JsonNode  *node,
                      JsonArray *array)
{
  g_return_if_fail (node != NULL);
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_ARRAY);

  if (node->data.array)
    {
      json_array_unref (node->data.array);
      node->data.array = NULL;
    }

  if (array)
    node->data.array = array;
}

/**
 * json_node_get_array:
 * @node: a #JsonNode
 *
 * Retrieves the #JsonArray stored inside a #JsonNode
 *
 * Return value: (transfer none): the #JsonArray
 */
JsonArray *
json_node_get_array (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_ARRAY, NULL);

  return node->data.array;
}

/**
 * json_node_dup_array
 * @node: a #JsonNode
 *
 * Retrieves the #JsonArray stored inside a #JsonNode and returns it
 * with its reference count increased by one.
 *
 * Return value: the #JsonArray with its reference count increased.
 */
JsonArray *
json_node_dup_array (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_ARRAY, NULL);

  if (node->data.array)
    return json_array_ref (node->data.array);

  return NULL;
}

/**
 * json_node_get_value:
 * @node: a #JsonNode
 * @value: return location for an uninitialized value
 *
 * Retrieves a value from a #JsonNode and copies into @value. When done
 * using it, call g_value_unset() on the #GValue.
 */
void
json_node_get_value (JsonNode *node,
                     GValue   *value)
{
  g_return_if_fail (node != NULL);
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE);

  if (G_VALUE_TYPE (&(node->data.value)) != 0)
    {
      g_value_init (value, G_VALUE_TYPE (&(node->data.value)));
      g_value_copy (&(node->data.value), value);
    }
}

/**
 * json_node_set_value:
 * @node: a #JsonNode
 * @value: the #GValue to set
 *
 * Sets @value inside @node. The passed #GValue is copied into the #JsonNode
 */
void
json_node_set_value (JsonNode     *node,
                     const GValue *value)
{
  g_return_if_fail (node != NULL);
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE);

  if (G_VALUE_TYPE (&(node->data.value)) != 0)
    g_value_unset (&(node->data.value));

  g_value_init (&(node->data.value), G_VALUE_TYPE (value));
  g_value_copy (value, &(node->data.value));
}

/**
 * json_node_free:
 * @node: a #JsonNode
 *
 * Frees the resources allocated by @node.
 */
void
json_node_free (JsonNode *node)
{
  if (G_LIKELY (node))
    {
      switch (node->type)
        {
        case JSON_NODE_OBJECT:
          if (node->data.object)
            json_object_unref (node->data.object);
          break;

        case JSON_NODE_ARRAY:
          if (node->data.array)
            json_array_unref (node->data.array);
          break;

        case JSON_NODE_VALUE:
          g_value_unset (&(node->data.value));
          break;

        case JSON_NODE_NULL:
          break;
        }

      g_slice_free (JsonNode, node);
    }
}

/**
 * json_node_type_name:
 * @node: a #JsonNode
 *
 * Retrieves the user readable name of the data type contained by @node.
 *
 * Return value: a string containing the name of the type. The returned string
 *   is owned by the node and should never be modified or freed
 */
G_CONST_RETURN gchar *
json_node_type_name (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, "(null)");

  switch (node->type)
    {
    case JSON_NODE_OBJECT:
      return "JsonObject";

    case JSON_NODE_ARRAY:
      return "JsonArray";

    case JSON_NODE_NULL:
      return "NULL";

    case JSON_NODE_VALUE:
      return g_type_name (G_VALUE_TYPE (&(node->data.value)));
    }

  return "unknown";
}

/**
 * json_node_get_parent:
 * @node: a #JsonNode
 *
 * Retrieves the parent #JsonNode of @node.
 *
 * Return value: (transfer none): the parent node, or %NULL if @node is the root node
 */
JsonNode *
json_node_get_parent (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);

  return node->parent;
}

/**
 * json_node_set_string:
 * @node: a #JsonNode of type %JSON_NODE_VALUE
 * @value: a string value
 *
 * Sets @value as the string content of the @node, replacing any existing
 * content.
 */
void
json_node_set_string (JsonNode    *node,
                      const gchar *value)
{
  g_return_if_fail (node != NULL);
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE);

  if (G_VALUE_TYPE (&(node->data.value)) == G_TYPE_STRING)
    g_value_set_string (&(node->data.value), value);
  else
    {
      GValue copy = { 0, };

      g_value_init (&copy, G_TYPE_STRING);
      g_value_set_string (&copy, value);

      json_node_set_value (node, &copy);

      g_value_unset (&copy);
    }
}

/**
 * json_node_get_string:
 * @node: a #JsonNode of type %JSON_NODE_VALUE
 *
 * Gets the string value stored inside a #JsonNode
 *
 * Return value: a string value.
 */
G_CONST_RETURN gchar *
json_node_get_string (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, NULL);

  if (G_VALUE_TYPE (&(node->data.value)) == G_TYPE_STRING)
    return g_value_get_string (&(node->data.value));

  return NULL;
}

gchar *
json_node_dup_string (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, NULL);

  if (G_VALUE_TYPE (&(node->data.value)) == G_TYPE_STRING)
    return g_strdup (g_value_get_string (&(node->data.value)));

  return NULL;
}

/**
 * json_node_set_int:
 * @node: a #JsonNode of type %JSON_NODE_VALUE
 * @value: an integer value
 *
 * Sets @value as the integer content of the @node, replacing any existing
 * content.
 */
void
json_node_set_int (JsonNode *node,
                   gint      value)
{
  g_return_if_fail (node != NULL);
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE);

  if (G_VALUE_TYPE (&(node->data.value)) == G_TYPE_INT)
    g_value_set_int (&(node->data.value), value);
  else
    {
      GValue copy = { 0, };

      g_value_init (&copy, G_TYPE_INT);
      g_value_set_int (&copy, value);

      json_node_set_value (node, &copy);

      g_value_unset (&copy);
    }
}

/**
 * json_node_get_int:
 * @node: a #JsonNode of type %JSON_NODE_VALUE
 *
 * Gets the integer value stored inside a #JsonNode
 *
 * Return value: an integer value.
 */
gint
json_node_get_int (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, 0);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, 0);

  if (G_VALUE_TYPE (&(node->data.value)) == G_TYPE_INT)
    return g_value_get_int (&(node->data.value));

  return 0;
}

/**
 * json_node_set_double:
 * @node: a #JsonNode of type %JSON_NODE_VALUE
 * @value: a double value
 *
 * Sets @value as the double content of the @node, replacing any existing
 * content.
 */
void
json_node_set_double (JsonNode *node,
                      gdouble   value)
{
  g_return_if_fail (node != NULL);
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE);

  if (G_VALUE_TYPE (&(node->data.value)) == G_TYPE_DOUBLE)
    g_value_set_double (&(node->data.value), value);
  else
    {
      GValue copy = { 0, };

      g_value_init (&copy, G_TYPE_DOUBLE);
      g_value_set_double (&copy, value);

      json_node_set_value (node, &copy);

      g_value_unset (&copy);
    }
}

/**
 * json_node_get_double:
 * @node: a #JsonNode of type %JSON_NODE_VALUE
 *
 * Gets the double value stored inside a #JsonNode
 *
 * Return value: a double value.
 */
gdouble
json_node_get_double (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, 0.0);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, 0.0);

  if (G_VALUE_TYPE (&(node->data.value)) == G_TYPE_DOUBLE)
    return g_value_get_double (&(node->data.value));

  return 0.0;
}

/**
 * json_node_set_boolean:
 * @node: a #JsonNode of type %JSON_NODE_VALUE
 * @value: a boolean value
 *
 * Sets @value as the boolean content of the @node, replacing any existing
 * content.
 */
void
json_node_set_boolean (JsonNode *node,
                       gboolean  value)
{
  g_return_if_fail (node != NULL);
  g_return_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE);

  if (G_VALUE_TYPE (&(node->data.value)) == G_TYPE_BOOLEAN)
    g_value_set_boolean (&(node->data.value), value);
  else
    {
      GValue copy = { 0, };

      g_value_init (&copy, G_TYPE_BOOLEAN);
      g_value_set_boolean (&copy, value);

      json_node_set_value (node, &copy);

      g_value_unset (&copy);
    }
}

/**
 * json_node_get_boolean:
 * @node: a #JsonNode of type %JSON_NODE_VALUE
 *
 * Gets the boolean value stored inside a #JsonNode
 *
 * Return value: a boolean value.
 */
gboolean
json_node_get_boolean (JsonNode *node)
{
  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, FALSE);

  if (G_VALUE_TYPE (&(node->data.value)) == G_TYPE_BOOLEAN)
    return g_value_get_boolean (&(node->data.value));

  return FALSE;
}
