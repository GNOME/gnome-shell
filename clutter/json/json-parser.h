/* json-parser.h - JSON streams parser
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

#ifndef __JSON_PARSER_H__
#define __JSON_PARSER_H__

#include <glib-object.h>
#include "json-types.h"

G_BEGIN_DECLS

#define JSON_TYPE_PARSER                (json_parser_get_type ())
#define JSON_PARSER(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), JSON_TYPE_PARSER, JsonParser))
#define JSON_IS_PARSER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), JSON_TYPE_PARSER))
#define JSON_PARSER_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), JSON_TYPE_PARSER, JsonParserClass))
#define JSON_IS_PARSER_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), JSON_TYPE_PARSER))
#define JSON_PARSER_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), JSON_TYPE_PARSER, JsonParserClass))

#define JSON_PARSER_ERROR               (json_parser_error_quark ())

typedef struct _JsonParser              JsonParser;
typedef struct _JsonParserPrivate       JsonParserPrivate;
typedef struct _JsonParserClass         JsonParserClass;

/**
 * JsonParserError:
 * @JSON_PARSER_ERROR_PARSE: parse error
 * @JSON_PARSER_ERROR_UNKNOWN: unknown error
 *
 * Error enumeration for #JsonParser
 */
typedef enum {
  JSON_PARSER_ERROR_PARSE,
  
  JSON_PARSER_ERROR_UNKNOWN
} JsonParserError;

typedef enum {
  JSON_TOKEN_INVALID = G_TOKEN_LAST,
  JSON_TOKEN_TRUE,
  JSON_TOKEN_FALSE,
  JSON_TOKEN_NULL,
  JSON_TOKEN_LAST
} JsonTokenType;

/**
 * JsonParser:
 * 
 * JSON data streams parser. The contents of the #JsonParser structure are
 * private and should only be accessed via the provided API.
 */
struct _JsonParser
{
  /*< private >*/
  GObject parent_instance;

  JsonParserPrivate *priv;
};

/**
 * JsonParserClass:
 * @parse_start: class handler for the JsonParser::parse-start signal
 * @object_start: class handler for the JsonParser::object-start signal
 * @object_member: class handler for the JsonParser::object-member signal
 * @object_end: class handler for the JsonParser::object-end signal
 * @array_start: class handler for the JsonParser::array-start signal
 * @array_element: class handler for the JsonParser::array-element signal
 * @array_end: class handler for the JsonParser::array-end signal
 * @parse_end: class handler for the JsonParser::parse-end signal
 * @error: class handler for the JsonParser::error signal
 *
 * #JsonParser class.
 */
struct _JsonParserClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public  >*/
  void (* parse_start)   (JsonParser   *parser);

  void (* object_start)  (JsonParser   *parser);
  void (* object_member) (JsonParser   *parser,
                          JsonObject   *object,
                          const gchar  *member_name);
  void (* object_end)    (JsonParser   *parser,
                          JsonObject   *object);

  void (* array_start)   (JsonParser   *parser);
  void (* array_element) (JsonParser   *parser,
                          JsonArray    *array,
                          gint          index_);
  void (* array_end)     (JsonParser   *parser,
                          JsonArray    *array);

  void (* parse_end)     (JsonParser   *parser);
  
  void (* error)         (JsonParser   *parser,
                          const GError *error);

  /*< private >*/
  /* padding for future expansion */
  void (* _json_reserved1) (void);
  void (* _json_reserved2) (void);
  void (* _json_reserved3) (void);
  void (* _json_reserved4) (void);
  void (* _json_reserved5) (void);
  void (* _json_reserved6) (void);
  void (* _json_reserved7) (void);
  void (* _json_reserved8) (void);
};

GQuark      json_parser_error_quark    (void);
GType       json_parser_get_type       (void) G_GNUC_CONST;

JsonParser *json_parser_new              (void);
gboolean    json_parser_load_from_file   (JsonParser   *parser,
                                          const gchar  *filename,
                                          GError      **error);
gboolean    json_parser_load_from_data   (JsonParser   *parser,
                                          const gchar  *data,
                                          gssize        length,
                                          GError      **error);
JsonNode *  json_parser_get_root         (JsonParser   *parser);

guint       json_parser_get_current_line (JsonParser   *parser);
guint       json_parser_get_current_pos  (JsonParser   *parser);

G_END_DECLS

#endif /* __JSON_PARSER_H__ */
