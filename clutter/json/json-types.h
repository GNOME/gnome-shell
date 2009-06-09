/* json-types.h - JSON data types
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

#ifndef __JSON_TYPES_H__
#define __JSON_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * JSON_NODE_TYPE:
 * @node: a #JsonNode
 *
 * Evaluates to the #JsonNodeType contained by @node
 */
#define JSON_NODE_TYPE(node)    (json_node_get_node_type ((node)))

#define JSON_TYPE_NODE          (json_node_get_type ())
#define JSON_TYPE_OBJECT        (json_object_get_type ())
#define JSON_TYPE_ARRAY         (json_array_get_type ())

/**
 * JsonNode:
 * @type: the type of node
 *
 * A generic container of JSON data types. The contents of the #JsonNode
 * structure are private and should only be accessed via the provided
 * functions and never directly.
 */
typedef struct _JsonNode        JsonNode;

/**
 * JsonObject:
 *
 * A JSON object type. The contents of the #JsonObject structure are private
 * and should only be accessed by the provided API
 */
typedef struct _JsonObject      JsonObject;

/**
 * JsonArray:
 *
 * A JSON array type. The contents of the #JsonArray structure are private
 * and should only be accessed by the provided API
 */
typedef struct _JsonArray       JsonArray;

/**
 * JsonNodeType:
 * @JSON_NODE_OBJECT: The node contains a #JsonObject
 * @JSON_NODE_ARRAY: The node contains a #JsonArray
 * @JSON_NODE_VALUE: The node contains a fundamental type
 * @JSON_NODE_NULL: Special type, for nodes containing null
 *
 * Indicates the content of a #JsonNode.
 */
typedef enum {
  JSON_NODE_OBJECT,
  JSON_NODE_ARRAY,
  JSON_NODE_VALUE,
  JSON_NODE_NULL
} JsonNodeType;

/**
 * JsonObjectForeach:
 * @object: the iterated #JsonObject
 * @member_name: the name of the member
 * @member_node: a #JsonNode containing the @member_name value
 * @user_data: data passed to the function
 *
 * The function to be passed to json_object_foreach_member(). You
 * should not add or remove members to and from @object within
 * this function. It is safe to change the value of @member_node.
 *
 * Since: 0.8
 */
typedef void (* JsonObjectForeach) (JsonObject  *object,
                                    const gchar *member_name,
                                    JsonNode    *member_node,
                                    gpointer     user_data);

/**
 * JsonArrayForeach:
 * @array: the iterated #JsonArray
 * @index_: the index of the element
 * @element_node: a #JsonNode containing the value at @index_
 * @user_data: data passed to the function
 *
 * The function to be passed to json_array_foreach_element(). You
 * should not add or remove elements to and from @array within
 * this function. It is safe to change the value of @element_node.
 *
 * Since: 0.8
 */
typedef void (* JsonArrayForeach) (JsonArray  *array,
                                   guint       index_,
                                   JsonNode   *element_node,
                                   gpointer    user_data);

/*
 * JsonNode
 */
GType                 json_node_get_type        (void) G_GNUC_CONST;
JsonNode *            json_node_new             (JsonNodeType  type);
JsonNode *            json_node_copy            (JsonNode     *node);
void                  json_node_free            (JsonNode     *node);
JsonNodeType          json_node_get_node_type   (JsonNode     *node);
GType                 json_node_get_value_type  (JsonNode     *node);
JsonNode *            json_node_get_parent      (JsonNode     *node);
G_CONST_RETURN gchar *json_node_type_name       (JsonNode     *node);

void                  json_node_set_object      (JsonNode     *node,
                                                 JsonObject   *object);
void                  json_node_take_object     (JsonNode     *node,
                                                 JsonObject   *object);
JsonObject *          json_node_get_object      (JsonNode     *node);
JsonObject *          json_node_dup_object      (JsonNode     *node);
void                  json_node_set_array       (JsonNode     *node,
                                                 JsonArray    *array);
void                  json_node_take_array      (JsonNode     *node,
                                                 JsonArray    *array);
JsonArray *           json_node_get_array       (JsonNode     *node);
JsonArray *           json_node_dup_array       (JsonNode     *node);
void                  json_node_set_value       (JsonNode     *node,
                                                 const GValue *value);
void                  json_node_get_value       (JsonNode     *node,
                                                 GValue       *value);
void                  json_node_set_string      (JsonNode     *node,
                                                 const gchar  *value);
G_CONST_RETURN gchar *json_node_get_string      (JsonNode     *node);
gchar *               json_node_dup_string      (JsonNode     *node);
void                  json_node_set_int         (JsonNode     *node,
                                                 gint          value);
gint                  json_node_get_int         (JsonNode     *node);
void                  json_node_set_double      (JsonNode     *node,
                                                 gdouble       value);
gdouble               json_node_get_double      (JsonNode     *node);
void                  json_node_set_boolean     (JsonNode     *node,
                                                 gboolean      value);
gboolean              json_node_get_boolean     (JsonNode     *node);
gboolean              json_node_is_null         (JsonNode     *node);

/*
 * JsonObject
 */
GType                 json_object_get_type           (void) G_GNUC_CONST;
JsonObject *          json_object_new                (void);
JsonObject *          json_object_ref                (JsonObject  *object);
void                  json_object_unref              (JsonObject  *object);

#ifndef JSON_DISABLE_DEPRECATED
void                  json_object_add_member         (JsonObject  *object,
                                                      const gchar *member_name,
                                                      JsonNode    *node) G_GNUC_DEPRECATED;
#endif /* JSON_DISABLE_DEPRECATED */

void                  json_object_set_member         (JsonObject  *object,
                                                      const gchar *member_name,
                                                      JsonNode    *node);
void                  json_object_set_int_member     (JsonObject  *object,
                                                      const gchar *member_name,
                                                      gint         value);
void                  json_object_set_double_member  (JsonObject  *object,
                                                      const gchar *member_name,
                                                      gdouble      value);
void                  json_object_set_boolean_member (JsonObject  *object,
                                                      const gchar *member_name,
                                                      gboolean     value);
void                  json_object_set_string_member  (JsonObject  *object,
                                                      const gchar *member_name,
                                                      const gchar *value);
void                  json_object_set_null_member    (JsonObject  *object,
                                                      const gchar *member_name);
void                  json_object_set_array_member   (JsonObject  *object,
                                                      const gchar *member_name,
                                                      JsonArray   *value);
void                  json_object_set_object_member  (JsonObject  *object,
                                                      const gchar *member_name,
                                                      JsonObject  *value);
GList *               json_object_get_members        (JsonObject  *object);
JsonNode *            json_object_get_member         (JsonObject  *object,
                                                      const gchar *member_name);
JsonNode *            json_object_dup_member         (JsonObject  *object,
                                                      const gchar *member_name);
gint                  json_object_get_int_member     (JsonObject  *object,
                                                      const gchar *member_name);
gdouble               json_object_get_double_member  (JsonObject  *object,
                                                      const gchar *member_name);
gboolean              json_object_get_boolean_member (JsonObject  *object,
                                                      const gchar *member_name);
G_CONST_RETURN gchar *json_object_get_string_member  (JsonObject  *object,
                                                      const gchar *member_name);
gboolean              json_object_get_null_member    (JsonObject  *object,
                                                      const gchar *member_name);
JsonArray *           json_object_get_array_member   (JsonObject  *object,
                                                      const gchar *member_name);
JsonObject *          json_object_get_object_member  (JsonObject  *object,
                                                      const gchar *member_name);
gboolean              json_object_has_member         (JsonObject  *object,
                                                      const gchar *member_name);
void                  json_object_remove_member      (JsonObject  *object,
                                                      const gchar *member_name);
GList *               json_object_get_values         (JsonObject  *object);
guint                 json_object_get_size           (JsonObject  *object);
void                  json_object_foreach_member     (JsonObject  *object,
                                                      JsonObjectForeach func,
                                                      gpointer     data);

GType                 json_array_get_type            (void) G_GNUC_CONST;
JsonArray *           json_array_new                 (void);
JsonArray *           json_array_sized_new           (guint        n_elements);
JsonArray *           json_array_ref                 (JsonArray   *array);
void                  json_array_unref               (JsonArray   *array);
void                  json_array_add_element         (JsonArray   *array,
                                                      JsonNode    *node);
void                  json_array_add_int_element     (JsonArray   *array,
                                                      gint         value);
void                  json_array_add_double_element  (JsonArray   *array,
                                                      gdouble      value);
void                  json_array_add_boolean_element (JsonArray   *array,
                                                      gboolean     value);
void                  json_array_add_string_element  (JsonArray   *array,
                                                      const gchar *value);
void                  json_array_add_null_element    (JsonArray   *array);
void                  json_array_add_array_element   (JsonArray   *array,
                                                      JsonArray   *value);
void                  json_array_add_object_element  (JsonArray   *array,
                                                      JsonObject  *value);
GList *               json_array_get_elements        (JsonArray   *array);
JsonNode *            json_array_get_element         (JsonArray   *array,
                                                      guint        index_);
gint                  json_array_get_int_element     (JsonArray   *array,
                                                      guint        index_);
gdouble               json_array_get_double_element  (JsonArray   *array,
                                                      guint        index_);
gboolean              json_array_get_boolean_element (JsonArray   *array,
                                                      guint        index_);
G_CONST_RETURN gchar *json_array_get_string_element  (JsonArray   *array,
                                                      guint        index_);
gboolean              json_array_get_null_element    (JsonArray   *array,
                                                      guint        index_);
JsonArray *           json_array_get_array_element   (JsonArray   *array,
                                                      guint        index_);
JsonObject *          json_array_get_object_element  (JsonArray   *array,
                                                      guint        index_);
JsonNode *            json_array_dup_element         (JsonArray   *array,
                                                      guint        index_);
void                  json_array_remove_element      (JsonArray   *array,
                                                      guint        index_);
guint                 json_array_get_length          (JsonArray   *array);
void                  json_array_foreach_element     (JsonArray   *array,
                                                      JsonArrayForeach func,
                                                      gpointer     data);

G_END_DECLS

#endif /* __JSON_TYPES_H__ */
