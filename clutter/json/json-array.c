/* json-array.c - JSON array implementation
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

#include "json-types.h"

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

struct _JsonArray
{
  GPtrArray *elements;

  volatile gint ref_count;
};

GType
json_array_get_type (void)
{
  static GType array_type = 0;

  if (G_UNLIKELY (!array_type))
    array_type = g_boxed_type_register_static ("JsonArray",
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
      gint i;

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
 * Return value: (transfer container) (element-type JsonNode): a #GList containing
 *   the elements of the array. The contents of the list are owned by the array and
 *   should never be  modified or freed. Use g_list_free() on the returned list when
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
 * json_array_get_element:
 * @array: a #JsonArray
 * @index_: the index of the element to retrieve
 * 
 * Retrieves the #JsonNode containing the value of the element at @index_
 * inside a #JsonArray.
 *
 * Return value: (transfer none): a pointer to the #JsonNode at the requested index
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
