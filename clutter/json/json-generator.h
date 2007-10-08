/* json-generator.h - JSON streams generator
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

#ifndef __JSON_GENERATOR_H__
#define __JSON_GENERATOR_H__

#include <glib-object.h>
#include "json-types.h"

G_BEGIN_DECLS

#define JSON_TYPE_GENERATOR             (json_generator_get_type ())
#define JSON_GENERATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), JSON_TYPE_GENERATOR, JsonGenerator))
#define JSON_IS_GENERATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), JSON_TYPE_GENERATOR))
#define JSON_GENERATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), JSON_TYPE_GENERATOR, JsonGeneratorClass))
#define JSON_IS_GENERATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), JSON_TYPE_GENERATOR))
#define JSON_GENERATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), JSON_TYPE_GENERATOR, JsonGeneratorClass))

typedef struct _JsonGenerator           JsonGenerator;
typedef struct _JsonGeneratorPrivate    JsonGeneratorPrivate;
typedef struct _JsonGeneratorClass      JsonGeneratorClass;

/**
 * JsonGenerator:
 *
 * JSON data streams generator. The contents of the #JsonGenerator structure
 * are private and should only be accessed via the provided API.
 */
struct _JsonGenerator
{
  /*< private >*/
  GObject parent_instance;

  JsonGeneratorPrivate *priv;
};

/**
 * JsonGeneratorClass:
 *
 * #JsonGenerator class
 */
struct _JsonGeneratorClass
{
  /*< private >*/
  GObjectClass parent_class;

  /* padding, for future expansion */
  void (* _json_reserved1) (void);
  void (* _json_reserved2) (void);
  void (* _json_reserved3) (void);
  void (* _json_reserved4) (void);
};

GType json_generator_get_type (void) G_GNUC_CONST;

JsonGenerator *json_generator_new (void);
gchar *        json_generator_to_data  (JsonGenerator  *generator,
                                        gsize          *length);
gboolean       json_generator_to_file  (JsonGenerator  *generator,
                                        const gchar    *filename,
                                        GError        **error);
void           json_generator_set_root (JsonGenerator  *generator,
                                        JsonNode       *node);

G_END_DECLS

#endif /* __JSON_GENERATOR_H__ */
