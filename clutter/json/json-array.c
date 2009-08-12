/* json-array.c - JSON array implementation
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

#include "json-types-private.h"

/**
 * SECTION:json-array
 * @short_description: a JSON array representation
 *
 * #JsonArray is the representation of the array type inside JSON. It contains
 * #JsonNode<!-- -->s, which may contain fundamental types, other arrays or
 * objects.
 *
 * Since arrays can be expensive, they are reference counted. You can control
 * the lifetime of a #JsonArray using json_array_ref() and json_array_unref().
 *
 * To append an element, use json_array_add_element().
 * To extract an element at a given index, use json_array_get_element().
 * To retrieve the entire array in list form, use json_array_get_elements().
 * To retrieve the length of the array, use json_array_get_length().
 */

GType
json_array_get_type (void)
{
  static GType array_type = 0;

  if (G_UNLIKELY (!array_type))
    array_type = g_boxed_type_register_static (g_intern_static_string ("JsonArray"),
                                               (GBoxedCopyFunc) json_array_ref,
                                               (GBoxedFreeFunc) json_array_unref);

  return array_type;
}

/**
 * json_array_new:
 *
 * Creates a new #JsonArray.
 *
 * Return value: the newly created #JsonArray
 */
JsonArray *
json_array_new (void)
{
  JsonArray *array;

  array = g_slice_new (JsonArray);

  array->ref_count = 1;
  array->elements = g_ptr_array_new ();

  return array;
}

/**
 * json_array_sized_new:
 * @n_elements: number of slots to pre-allocate
 *
 * Creates a new #JsonArray with @n_elements slots already allocated.
 *
 * Return value: the newly created #JsonArray
 */
JsonArray *
json_array_sized_new (guint n_elements)
{
  JsonArray *array;

  array = g_slice_new (JsonArray);
  
  array->ref_count = 1;
  array->elements = g_ptr_array_sized_new (n_elements);

  return array;
}

/**
 * json_array_ref:
 * @array: a #JsonArray
 *
 * Increase by one the reference count of a #JsonArray.
 *
 * Return value: the passed #JsonArray, with the reference count
 *   increased by one.
 */
JsonArray *
json_array_ref (JsonArray *array)
{
  g_return_val_if_fail (array != NULL, NULL);
  g_return_val_if_fail (array->ref_count > 0, NULL);

  g_atomic_int_exchange_and_add (&array->ref_count, 1);

  return array;
}

/**
 * json_array_unref:
 * @array: a #JsonArray
 *
 * Decreases by one the reference count of a #JsonArray. If the
 * reference count reaches zero, the array is destroyed and all
 * its allocated resources are freed.
 */
void
json_array_unref (JsonArray *array)
{
  gint old_ref;

  g_return_if_fail (array != NULL);
  g_return_if_fail (array->ref_count > 0);

  old_ref = g_atomic_int_get (&array->ref_count);
  if (old_ref > 1)
    g_atomic_int_compare_and_exchange (&array->ref_count, old_ref, old_ref - 1);
  else
    {
      guint i;

      for (i = 0; i < array->elements->len; i++)
        json_node_free (g_ptr_array_index (array->elements, i));

      g_ptr_array_free (array->elements, TRUE);
      array->elements = NULL;

      g_slice_free (JsonArray, array);
    }
}

/**
 * json_array_get_elements:
 * @array: a #JsonArray
 *
 * Gets the elements of a #JsonArray as a list of #JsonNode<!-- -->s.
 *
 * Return value: a #GList containing the elements of the array. The
 *   contents of the list are owned by the array and should never be
 *   modified or freed. Use g_list_free() on the returned list when
 *   done using it
 */
GList *
json_array_get_elements (JsonArray *array)
{
  GList *retval;
  guint i;

  g_return_val_if_fail (array != NULL, NULL);

  retval = NULL;
  for (i = 0; i < array->elements->len; i++)
    retval = g_list_prepend (retval,
                             g_ptr_array_index (array->elements, i));

  return g_list_reverse (retval);
}

/**
 * json_array_dup_element:
 * @array: a #JsonArray
 * @index_: the index of the element to retrieve
 *
 * Retrieves a copy of the #JsonNode containing the value of the
 * element at @index_ inside a #JsonArray
 *
 * Return value: a copy of the #JsonNode at the requested index.
 *   Use json_node_free() when done.
 *
 * Since: 0.6
 */
JsonNode *
json_array_dup_element (JsonArray *array,
                        guint      index_)
{
  JsonNode *retval;

  g_return_val_if_fail (array != NULL, NULL);
  g_return_val_if_fail (index_ < array->elements->len, NULL);

  retval = json_array_get_element (array, index_);
  if (!retval)
    return NULL;

  return json_node_copy (retval);
}

/**
 * json_array_get_element:
 * @array: a #JsonArray
 * @index_: the index of the element to retrieve
 * 
 * Retrieves the #JsonNode containing the value of the element at @index_
 * inside a #JsonArray.
 *
 * Return value: a pointer to the #JsonNode at the requested index
 */
JsonNode *
json_array_get_element (JsonArray *array,
                        guint      index_)
{
  g_return_val_if_fail (array != NULL, NULL);
  g_return_val_if_fail (index_ < array->elements->len, NULL);

  return g_ptr_array_index (array->elements, index_);
}

/**
 * json_array_get_int_element:
 * @array: a #JsonArray
 * @index_: the index of the element to retrieve
 *
 * Conveniently retrieves the integer value of the element at @index_
 * inside @array
 *
 * See also: json_array_get_element(), json_node_get_int()
 *
 * Return value: the integer value
 *
 * Since: 0.8
 */
gint64
json_array_get_int_element (JsonArray *array,
                            guint      index_)
{
  JsonNode *node;

  g_return_val_if_fail (array != NULL, 0);
  g_return_val_if_fail (index_ < array->elements->len, 0);

  node = g_ptr_array_index (array->elements, index_);
  g_return_val_if_fail (node != NULL, 0);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, 0);

  return json_node_get_int (node);
}

/**
 * json_array_get_double_element:
 * @array: a #JsonArray
 * @index_: the index of the element to retrieve
 *
 * Conveniently retrieves the floating point value of the element at
 * @index_ inside @array
 *
 * See also: json_array_get_element(), json_node_get_double()
 *
 * Return value: the floating point value
 *
 * Since: 0.8
 */
gdouble
json_array_get_double_element (JsonArray *array,
                               guint      index_)
{
  JsonNode *node;

  g_return_val_if_fail (array != NULL, 0.0);
  g_return_val_if_fail (index_ < array->elements->len, 0.0);

  node = g_ptr_array_index (array->elements, index_);
  g_return_val_if_fail (node != NULL, 0.0);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, 0.0);

  return json_node_get_double (node);
}

/**
 * json_array_get_boolean_element:
 * @array: a #JsonArray
 * @index_: the index of the element to retrieve
 *
 * Conveniently retrieves the boolean value of the element at @index_
 * inside @array
 *
 * See also: json_array_get_element(), json_node_get_boolean()
 *
 * Return value: the integer value
 *
 * Since: 0.8
 */
gboolean
json_array_get_boolean_element (JsonArray *array,
                                guint      index_)
{
  JsonNode *node;

  g_return_val_if_fail (array != NULL, FALSE);
  g_return_val_if_fail (index_ < array->elements->len, FALSE);

  node = g_ptr_array_index (array->elements, index_);
  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, FALSE);

  return json_node_get_boolean (node);
}

/**
 * json_array_get_string_element:
 * @array: a #JsonArray
 * @index_: the index of the element to retrieve
 *
 * Conveniently retrieves the string value of the element at @index_
 * inside @array
 *
 * See also: json_array_get_element(), json_node_get_string()
 *
 * Return value: the string value; the returned string is owned by
 *   the #JsonArray and should not be modified or freed
 *
 * Since: 0.8
 */
G_CONST_RETURN gchar *
json_array_get_string_element (JsonArray *array,
                               guint      index_)
{
  JsonNode *node;

  g_return_val_if_fail (array != NULL, NULL);
  g_return_val_if_fail (index_ < array->elements->len, NULL);

  node = g_ptr_array_index (array->elements, index_);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_VALUE, NULL);

  return json_node_get_string (node);
}

/**
 * json_array_get_null_element:
 * @array: a #JsonArray
 * @index_: the index of the element to retrieve
 *
 * Conveniently retrieves whether the element at @index_ is set to null
 *
 * See also: json_array_get_element(), JSON_NODE_TYPE(), %JSON_NODE_NULL
 *
 * Return value: %TRUE if the element is null
 *
 * Since: 0.8
 */
gboolean
json_array_get_null_element (JsonArray *array,
                             guint      index_)
{
  JsonNode *node;

  g_return_val_if_fail (array != NULL, FALSE);
  g_return_val_if_fail (index_ < array->elements->len, FALSE);

  node = g_ptr_array_index (array->elements, index_);
  g_return_val_if_fail (node != NULL, FALSE);

  return JSON_NODE_TYPE (node) == JSON_NODE_NULL;
}

/**
 * json_array_get_array_element:
 * @array: a #JsonArray
 * @index_: the index of the element to retrieve
 *
 * Conveniently retrieves the array from the element at @index_
 * inside @array
 *
 * See also: json_array_get_element(), json_node_get_array()
 *
 * Return value: the array
 *
 * Since: 0.8
 */
JsonArray *
json_array_get_array_element (JsonArray *array,
                              guint      index_)
{
  JsonNode *node;

  g_return_val_if_fail (array != NULL, NULL);
  g_return_val_if_fail (index_ < array->elements->len, NULL);

  node = g_ptr_array_index (array->elements, index_);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_ARRAY, NULL);

  return json_node_get_array (node);
}

/**
 * json_array_get_object_element:
 * @array: a #JsonArray
 * @index_: the index of the element to retrieve
 *
 * Conveniently retrieves the object from the element at @index_
 * inside @array
 *
 * See also: json_array_get_element(), json_node_get_object()
 *
 * Return value: the object
 *
 * Since: 0.8
 */
JsonObject *
json_array_get_object_element (JsonArray *array,
                               guint      index_)
{
  JsonNode *node;

  g_return_val_if_fail (array != NULL, NULL);
  g_return_val_if_fail (index_ < array->elements->len, NULL);

  node = g_ptr_array_index (array->elements, index_);
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (JSON_NODE_TYPE (node) == JSON_NODE_OBJECT, NULL);

  return json_node_get_object (node);
}

/**
 * json_array_get_length:
 * @array: a #JsonArray
 *
 * Retrieves the length of a #JsonArray
 *
 * Return value: the length of the array
 */
guint
json_array_get_length (JsonArray *array)
{
  g_return_val_if_fail (array != NULL, 0);

  return array->elements->len;
}

/**
 * json_array_add_element:
 * @array: a #JsonArray
 * @node: a #JsonNode
 *
 * Appends @node inside @array. The array will take ownership of the
 * #JsonNode.
 */
void
json_array_add_element (JsonArray *array,
                        JsonNode  *node)
{
  g_return_if_fail (array != NULL);
  g_return_if_fail (node != NULL);

  g_ptr_array_add (array->elements, node);
}

/**
 * json_array_add_int_element:
 * @array: a #JsonArray
 * @value: an integer value
 *
 * Conveniently adds an integer @value into @array
 *
 * See also: json_array_add_element(), json_node_set_int()
 *
 * Since: 0.8
 */
void
json_array_add_int_element (JsonArray *array,
                            gint64     value)
{
  JsonNode *node;

  g_return_if_fail (array != NULL);

  node = json_node_new (JSON_NODE_VALUE);
  json_node_set_int (node, value);

  g_ptr_array_add (array->elements, node);
}

/**
 * json_array_add_double_element:
 * @array: a #JsonArray
 * @value: a floating point value
 *
 * Conveniently adds a floating point @value into @array
 *
 * See also: json_array_add_element(), json_node_set_double()
 *
 * Since: 0.8
 */
void
json_array_add_double_element (JsonArray *array,
                               gdouble    value)
{
  JsonNode *node;

  g_return_if_fail (array != NULL);

  node = json_node_new (JSON_NODE_VALUE);
  json_node_set_double (node, value);

  g_ptr_array_add (array->elements, node);
}

/**
 * json_array_add_boolean_element:
 * @array: a #JsonArray
 * @value: a boolean value
 *
 * Conveniently adds a boolean @value into @array
 *
 * See also: json_array_add_element(), json_node_set_boolean()
 *
 * Since: 0.8
 */
void
json_array_add_boolean_element (JsonArray *array,
                                gboolean   value)
{
  JsonNode *node;

  g_return_if_fail (array != NULL);

  node = json_node_new (JSON_NODE_VALUE);
  json_node_set_boolean (node, value);

  g_ptr_array_add (array->elements, node);
}

/**
 * json_array_add_string_element:
 * @array: a #JsonArray
 * @value: a string value
 *
 * Conveniently adds a string @value into @array
 *
 * See also: json_array_add_element(), json_node_set_string()
 *
 * Since: 0.8
 */
void
json_array_add_string_element (JsonArray   *array,
                               const gchar *value)
{
  JsonNode *node;

  g_return_if_fail (array != NULL);
  g_return_if_fail (value != NULL);

  node = json_node_new (JSON_NODE_VALUE);
  json_node_set_string (node, value);

  g_ptr_array_add (array->elements, node);
}

/**
 * json_array_add_null_element:
 * @array: a #JsonArray
 *
 * Conveniently adds a null element into @array
 *
 * See also: json_array_add_element(), %JSON_NODE_NULL
 *
 * Since: 0.8
 */
void
json_array_add_null_element (JsonArray *array)
{
  JsonNode *node;

  g_return_if_fail (array != NULL);

  node = json_node_new (JSON_NODE_NULL);

  g_ptr_array_add (array->elements, node);
}

/**
 * json_array_add_array_element:
 * @array: a #JsonArray
 * @value: a #JsonArray
 *
 * Conveniently adds an array into @array. The @array takes ownership
 * of the newly added #JsonArray
 *
 * See also: json_array_add_element(), json_node_take_array()
 *
 * Since: 0.8
 */
void
json_array_add_array_element (JsonArray *array,
                              JsonArray *value)
{
  JsonNode *node;

  g_return_if_fail (array != NULL);
  g_return_if_fail (value != NULL);

  node = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (node, value);

  g_ptr_array_add (array->elements, node);
}

/**
 * json_array_add_object_element:
 * @array: a #JsonArray
 * @value: a #JsonObject
 *
 * Conveniently adds an object into @array. The @array takes ownership
 * of the newly added #JsonObject
 *
 * See also: json_array_add_element(), json_node_take_object()
 *
 * Since: 0.8
 */
void
json_array_add_object_element (JsonArray  *array,
                               JsonObject *value)
{
  JsonNode *node;

  g_return_if_fail (array != NULL);
  g_return_if_fail (value != NULL);

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (node, value);

  g_ptr_array_add (array->elements, node);
}

/**
 * json_array_remove_element:
 * @array: a #JsonArray
 * @index_: the position of the element to be removed
 *
 * Removes the #JsonNode inside @array at @index_ freeing its allocated
 * resources.
 */
void
json_array_remove_element (JsonArray *array,
                           guint      index_)
{
  g_return_if_fail (array != NULL);
  g_return_if_fail (index_ < array->elements->len);

  json_node_free (g_ptr_array_remove_index (array->elements, index_));
}

/**
 * json_array_foreach_element:
 * @array: a #JsonArray
 * @func: the function to be called on each element
 * @data: data to be passed to the function
 *
 * Iterates over all elements of @array and calls @func on
 * each one of them.
 *
 * It is safe to change the value of a #JsonNode of the @array
 * from within the iterator @func, but it is not safe to add or
 * remove elements from the @array.
 *
 * Since: 0.8
 */
void
json_array_foreach_element (JsonArray        *array,
                            JsonArrayForeach  func,
                            gpointer          data)
{
  gint i;

  g_return_if_fail (array != NULL);
  g_return_if_fail (func != NULL);

  for (i = 0; i < array->elements->len; i++)
    {
      JsonNode *element_node;

      element_node = g_ptr_array_index (array->elements, i);

      (* func) (array, i, element_node, data);
    }
}
