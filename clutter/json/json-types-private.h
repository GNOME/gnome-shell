/* json-types-private.h - JSON data types private header
 *
 * This file is part of JSON-GLib
 * Copyright (C) 2007  OpenedHand Ltd
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

#ifndef __JSON_TYPES_PRIVATE_H__
#define __JSON_TYPES_PRIVATE_H__

#include "json-types.h"

G_BEGIN_DECLS

struct _JsonNode
{
  /*< private >*/
  JsonNodeType type;

  union {
    JsonObject *object;
    JsonArray *array;
    GValue value;
  } data;

  JsonNode *parent;
};

struct _JsonArray
{
  GPtrArray *elements;

  volatile gint ref_count;
};

struct _JsonObject
{
  GHashTable *members;

  volatile gint ref_count;
};

G_END_DECLS

#endif /* __JSON_TYPES_PRIVATE_H__ */
