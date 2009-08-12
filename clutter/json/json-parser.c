/* json-parser.c - JSON streams parser
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

/**
 * SECTION:json-parser
 * @short_description: Parse JSON data streams
 *
 * #JsonParser provides an object for parsing a JSON data stream, either
 * inside a file or inside a static buffer.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "json-types-private.h"

#include "json-marshal.h"
#include "json-parser.h"

GQuark
json_parser_error_quark (void)
{
  return g_quark_from_static_string ("json-parser-error");
}

#define JSON_PARSER_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), JSON_TYPE_PARSER, JsonParserPrivate))

struct _JsonParserPrivate
{
  JsonNode *root;
  JsonNode *current_node;

  GScanner *scanner;

  GError *last_error;
};

static const GScannerConfig json_scanner_config =
{
  ( " \t\r\n" )		/* cset_skip_characters */,
  (
   "_"
   G_CSET_a_2_z
   G_CSET_A_2_Z
  )			/* cset_identifier_first */,
  (
   G_CSET_DIGITS
   "-_"
   G_CSET_a_2_z
   G_CSET_A_2_Z
  )			/* cset_identifier_nth */,
  ( "#\n" )		/* cpair_comment_single */,
  TRUE			/* case_sensitive */,
  TRUE			/* skip_comment_multi */,
  TRUE			/* skip_comment_single */,
  FALSE			/* scan_comment_multi */,
  TRUE			/* scan_identifier */,
  TRUE			/* scan_identifier_1char */,
  FALSE			/* scan_identifier_NULL */,
  TRUE			/* scan_symbols */,
  TRUE			/* scan_binary */,
  TRUE			/* scan_octal */,
  TRUE			/* scan_float */,
  TRUE			/* scan_hex */,
  TRUE			/* scan_hex_dollar */,
  TRUE			/* scan_string_sq */,
  TRUE			/* scan_string_dq */,
  TRUE			/* numbers_2_int */,
  FALSE			/* int_2_float */,
  FALSE			/* identifier_2_string */,
  TRUE			/* char_2_token */,
  TRUE			/* symbol_2_token */,
  FALSE			/* scope_0_fallback */,
  TRUE                  /* store_int64 */
};


static const gchar symbol_names[] =
  "true\0"
  "false\0"
  "null\0";

static const struct
{
  guint name_offset;
  guint token;
} symbols[] = {
  {  0, JSON_TOKEN_TRUE },
  {  5, JSON_TOKEN_FALSE },
  { 11, JSON_TOKEN_NULL }
};

static const guint n_symbols = G_N_ELEMENTS (symbols);

enum
{
  PARSE_START,
  OBJECT_START,
  OBJECT_MEMBER,
  OBJECT_END,
  ARRAY_START,
  ARRAY_ELEMENT,
  ARRAY_END,
  PARSE_END,
  ERROR,

  LAST_SIGNAL
};

static guint parser_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (JsonParser, json_parser, G_TYPE_OBJECT);

static guint json_parse_array  (JsonParser *parser,
                                GScanner   *scanner,
                                gboolean    nested);
static guint json_parse_object (JsonParser *parser,
                                GScanner   *scanner,
                                gboolean    nested);

static void
json_parser_dispose (GObject *gobject)
{
  JsonParserPrivate *priv = JSON_PARSER_GET_PRIVATE (gobject);

  if (priv->root)
    {
      json_node_free (priv->root);
      priv->root = NULL;
    }

  if (priv->last_error)
    {
      g_error_free (priv->last_error);
      priv->last_error = NULL;
    }

  G_OBJECT_CLASS (json_parser_parent_class)->dispose (gobject);
}

static void
json_parser_class_init (JsonParserClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (JsonParserPrivate));

  gobject_class->dispose = json_parser_dispose;

  /**
   * JsonParser::parse-start:
   * @parser: the #JsonParser that received the signal
   * 
   * The ::parse-start signal is emitted when the parser began parsing
   * a JSON data stream.
   */
  parser_signals[PARSE_START] =
    g_signal_new ("parse-start",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, parse_start),
                  NULL, NULL,
                  _json_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  /**
   * JsonParser::parse-end:
   * @parser: the #JsonParser that received the signal
   *
   * The ::parse-end signal is emitted when the parser successfully
   * finished parsing a JSON data stream
   */
  parser_signals[PARSE_END] =
    g_signal_new ("parse-end",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, parse_end),
                  NULL, NULL,
                  _json_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  /**
   * JsonParser::object-start:
   * @parser: the #JsonParser that received the signal
   * 
   * The ::object-start signal is emitted each time the #JsonParser
   * starts parsing a #JsonObject.
   */
  parser_signals[OBJECT_START] =
    g_signal_new ("object-start",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, object_start),
                  NULL, NULL,
                  _json_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  /**
   * JsonParser::object-member:
   * @parser: the #JsonParser that received the signal
   * @object: a #JsonObject
   * @member_name: the name of the newly parsed member
   *
   * The ::object-member signal is emitted each time the #JsonParser
   * has successfully parsed a single member of a #JsonObject. The
   * object and member are passed to the signal handlers.
   */
  parser_signals[OBJECT_MEMBER] =
    g_signal_new ("object-member",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, object_member),
                  NULL, NULL,
                  _json_marshal_VOID__BOXED_STRING,
                  G_TYPE_NONE, 2,
                  JSON_TYPE_OBJECT,
                  G_TYPE_STRING);
  /**
   * JsonParser::object-end:
   * @parser: the #JsonParser that received the signal
   * @object: the parsed #JsonObject
   *
   * The ::object-end signal is emitted each time the #JsonParser
   * has successfully parsed an entire #JsonObject.
   */
  parser_signals[OBJECT_END] =
    g_signal_new ("object-end",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, object_end),
                  NULL, NULL,
                  _json_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  JSON_TYPE_OBJECT);
  /**
   * JsonParser::array-start:
   * @parser: the #JsonParser that received the signal
   *
   * The ::array-start signal is emitted each time the #JsonParser
   * starts parsing a #JsonArray
   */
  parser_signals[ARRAY_START] =
    g_signal_new ("array-start",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, array_start),
                  NULL, NULL,
                  _json_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  /**
   * JsonParser::array-element:
   * @parser: the #JsonParser that received the signal
   * @array: a #JsonArray
   * @index_: the index of the newly parsed element
   *
   * The ::array-element signal is emitted each time the #JsonParser
   * has successfully parsed a single element of a #JsonArray. The
   * array and element index are passed to the signal handlers.
   */
  parser_signals[ARRAY_ELEMENT] =
    g_signal_new ("array-element",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, array_element),
                  NULL, NULL,
                  _json_marshal_VOID__BOXED_INT,
                  G_TYPE_NONE, 2,
                  JSON_TYPE_ARRAY,
                  G_TYPE_INT);
  /**
   * JsonParser::array-end:
   * @parser: the #JsonParser that received the signal
   * @array: the parsed #JsonArrary
   *
   * The ::array-end signal is emitted each time the #JsonParser
   * has successfully parsed an entire #JsonArray
   */
  parser_signals[ARRAY_END] =
    g_signal_new ("array-end",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, array_end),
                  NULL, NULL,
                  _json_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1,
                  JSON_TYPE_ARRAY);
  /**
   * JsonParser::error:
   * @parser: the parser instance that received the signal
   * @error: a pointer to the #GError
   *
   * The ::error signal is emitted each time a #JsonParser encounters
   * an error in a JSON stream.
   */
  parser_signals[ERROR] =
    g_signal_new ("error",
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (JsonParserClass, error),
                  NULL, NULL,
                  _json_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);
}

static void
json_parser_init (JsonParser *parser)
{
  JsonParserPrivate *priv;

  parser->priv = priv = JSON_PARSER_GET_PRIVATE (parser);

  priv->root = NULL;
  priv->current_node = NULL;
}

static guint
json_parse_array (JsonParser *parser,
                  GScanner   *scanner,
                  gboolean    nested)
{
  JsonParserPrivate *priv = parser->priv;
  JsonArray *array;
  guint token;

  if (!nested)
    {
      /* the caller already swallowed the opening '[' */
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_LEFT_BRACE)
        return G_TOKEN_LEFT_BRACE;
    }

  g_signal_emit (parser, parser_signals[ARRAY_START], 0);

  array = json_array_new ();

  token = g_scanner_get_next_token (scanner);
  while (token != G_TOKEN_RIGHT_BRACE)
    {
      JsonNode *node = NULL;
      gboolean negative = FALSE;

      if (token == G_TOKEN_COMMA)
        {
          /* swallow the comma */
          token = g_scanner_get_next_token (scanner);
          continue;
        }

      /* nested object */
      if (token == G_TOKEN_LEFT_CURLY)
        {
          JsonNode *old_node = priv->current_node;

          priv->current_node = json_node_new (JSON_NODE_OBJECT);

          token = json_parse_object (parser, scanner, TRUE);

          node = priv->current_node;
          priv->current_node = old_node;

          if (token != G_TOKEN_NONE)
            {
              json_node_free (node);
              json_array_unref (array);

              return token;
            }

          json_array_add_element (array, node);
          node->parent = priv->current_node;

          g_signal_emit (parser, parser_signals[ARRAY_ELEMENT], 0,
                         array,
                         json_array_get_length (array));

          token = g_scanner_get_next_token (scanner);
          if (token == G_TOKEN_RIGHT_BRACE)
            break;

          continue;
        }

      /* nested array */
      if (token == G_TOKEN_LEFT_BRACE)
        {
          JsonNode *old_node = priv->current_node;

          priv->current_node = json_node_new (JSON_NODE_ARRAY);

          token = json_parse_array (parser, scanner, TRUE);

          node = priv->current_node;
          priv->current_node = old_node;

          if (token != G_TOKEN_NONE)
            {
              json_node_free (node);
              json_array_unref (array);

              return token;
            }

          json_array_add_element (array, node);
          node->parent = priv->current_node;

          g_signal_emit (parser, parser_signals[ARRAY_ELEMENT], 0,
                         array,
                         json_array_get_length (array));

          token = g_scanner_get_next_token (scanner);
          if (token == G_TOKEN_RIGHT_BRACE)
            break;

          continue;
        }

      if (token == '-')
        {
          guint next_token = g_scanner_peek_next_token (scanner);

          if (next_token == G_TOKEN_INT ||
              next_token == G_TOKEN_FLOAT)
            {
              negative = TRUE;
              token = g_scanner_get_next_token (scanner);
            }
          else
            {
              json_array_unref (array);

              return G_TOKEN_INT;
            }
        }

      switch (token)
        {
        case G_TOKEN_INT:
          node = json_node_new (JSON_NODE_VALUE);
          json_node_set_int (node, negative ? scanner->value.v_int64 * -1
                                            : scanner->value.v_int64);
          break;

        case G_TOKEN_FLOAT:
          node = json_node_new (JSON_NODE_VALUE);
          json_node_set_double (node, negative ? scanner->value.v_float * -1.0
                                               : scanner->value.v_float);
          break;

        case G_TOKEN_STRING:
          node = json_node_new (JSON_NODE_VALUE);
          json_node_set_string (node, scanner->value.v_string);
          break;

        case JSON_TOKEN_TRUE:
        case JSON_TOKEN_FALSE:
          node = json_node_new (JSON_NODE_VALUE);
          json_node_set_boolean (node, token == JSON_TOKEN_TRUE ? TRUE
                                                                : FALSE);
          break;

        case JSON_TOKEN_NULL:
          node = json_node_new (JSON_NODE_NULL);
          break;

        default:
          json_array_unref (array);
          return G_TOKEN_RIGHT_BRACE;
        }

      if (node)
        {
          json_array_add_element (array, node);
          node->parent = priv->current_node;

          g_signal_emit (parser, parser_signals[ARRAY_ELEMENT], 0,
                         array,
                         json_array_get_length (array));
        }

      token = g_scanner_get_next_token (scanner);
    }

  json_node_take_array (priv->current_node, array);

  g_signal_emit (parser, parser_signals[ARRAY_END], 0, array);

  return G_TOKEN_NONE;
}

static guint
json_parse_object (JsonParser *parser,
                   GScanner   *scanner,
                   gboolean    nested)
{
  JsonParserPrivate *priv = parser->priv;
  JsonObject *object;
  guint token;

  if (!nested)
    {
      /* the caller already swallowed the opening '{' */
      token = g_scanner_get_next_token (scanner);
      if (token != G_TOKEN_LEFT_CURLY)
        return G_TOKEN_LEFT_CURLY;
    }

  g_signal_emit (parser, parser_signals[OBJECT_START], 0);

  object = json_object_new ();

  token = g_scanner_get_next_token (scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      JsonNode *node = NULL;
      gchar *name = NULL;
      gboolean negative = FALSE;

      if (token == G_TOKEN_COMMA)
        {
          /* swallow the comma */
          token = g_scanner_get_next_token (scanner);
          continue;
        }

      if (token == G_TOKEN_STRING)
        {
          name = g_strdup (scanner->value.v_string);

          token = g_scanner_get_next_token (scanner);
          if (token != ':')
            {
              g_free (name);
              json_object_unref (object);

              return ':';
            }
          else
            {
              /* swallow the colon */
              token = g_scanner_get_next_token (scanner);
            }
        }

      if (!name)
        {
          json_object_unref (object);

          return G_TOKEN_STRING;
        }

      if (token == G_TOKEN_LEFT_CURLY)
        {
          JsonNode *old_node = priv->current_node;
      
          priv->current_node = json_node_new (JSON_NODE_OBJECT);

          token = json_parse_object (parser, scanner, TRUE);

          node = priv->current_node;
          priv->current_node = old_node;

          if (token != G_TOKEN_NONE)
            {
              g_free (name);
              
              if (node)
                json_node_free (node);

              json_object_unref (object);

              return token;
            }

          json_object_set_member (object, name, node);
          node->parent = priv->current_node;

          g_signal_emit (parser, parser_signals[OBJECT_MEMBER], 0,
                         object,
                         name);

          g_free (name);

          token = g_scanner_get_next_token (scanner);
          if (token == G_TOKEN_RIGHT_CURLY)
            break;

          continue;
        }
     
      if (token == G_TOKEN_LEFT_BRACE)
        {
          JsonNode *old_node = priv->current_node;

          priv->current_node = json_node_new (JSON_NODE_ARRAY);

          token = json_parse_array (parser, scanner, TRUE);

          node = priv->current_node;
          priv->current_node = old_node;

          if (token != G_TOKEN_NONE)
            {
              g_free (name);
              json_node_free (node);
              json_object_unref (object);

              return token;
            }

          json_object_set_member (object, name, node);
          node->parent = priv->current_node;
          
          g_signal_emit (parser, parser_signals[OBJECT_MEMBER], 0,
                         object,
                         name);

          g_free (name);

          token = g_scanner_get_next_token (scanner);
          if (token == G_TOKEN_RIGHT_BRACE)
            break;

          continue;
        }

      if (token == '-')
        {
          guint next_token = g_scanner_peek_next_token (scanner);

          if (next_token == G_TOKEN_INT || next_token == G_TOKEN_FLOAT)
            {
              negative = TRUE;
              token = g_scanner_get_next_token (scanner);
            }
          else
            {
              json_object_unref (object);

              return G_TOKEN_INT;
            }
        }

      switch (token)
        {
        case G_TOKEN_INT:
          node = json_node_new (JSON_NODE_VALUE);
          json_node_set_int (node, negative ? scanner->value.v_int64 * -1
                                            : scanner->value.v_int64);
          break;

        case G_TOKEN_FLOAT:
          node = json_node_new (JSON_NODE_VALUE);
          json_node_set_double (node, negative ? scanner->value.v_float * -1.0
                                               : scanner->value.v_float);
          break;

        case G_TOKEN_STRING:
          node = json_node_new (JSON_NODE_VALUE);
          json_node_set_string (node, scanner->value.v_string);
          break;

        case JSON_TOKEN_TRUE:
        case JSON_TOKEN_FALSE:
          node = json_node_new (JSON_NODE_VALUE);
          json_node_set_boolean (node, token == JSON_TOKEN_TRUE ? TRUE
                                                                : FALSE);
          break;

        case JSON_TOKEN_NULL:
          node = json_node_new (JSON_NODE_NULL);
          break;

        default:
          json_object_unref (object);
          return G_TOKEN_SYMBOL;
        }

      if (node)
        {
          json_object_set_member (object, name, node);
          node->parent = priv->current_node;
          
          g_signal_emit (parser, parser_signals[OBJECT_MEMBER], 0,
                         object,
                         name);
        }

      g_free (name);

      token = g_scanner_get_next_token (scanner);
    }

  json_node_take_object (priv->current_node, object);

  g_signal_emit (parser, parser_signals[OBJECT_END], 0, object);

  return G_TOKEN_NONE;
}

static guint
json_parse_statement (JsonParser *parser,
                      GScanner   *scanner)
{
  JsonParserPrivate *priv = parser->priv;
  guint token;

  token = g_scanner_peek_next_token (scanner);
  switch (token)
    {
    case G_TOKEN_LEFT_CURLY:
      priv->root = priv->current_node = json_node_new (JSON_NODE_OBJECT);
      return json_parse_object (parser, scanner, FALSE);

    case G_TOKEN_LEFT_BRACE:
      priv->root = priv->current_node = json_node_new (JSON_NODE_ARRAY);
      return json_parse_array (parser, scanner, FALSE);

    case JSON_TOKEN_NULL:
      priv->root = priv->current_node = json_node_new (JSON_NODE_NULL);
      return G_TOKEN_NONE;

    case JSON_TOKEN_TRUE:
    case JSON_TOKEN_FALSE:
      priv->root = priv->current_node = json_node_new (JSON_NODE_VALUE);
      json_node_set_boolean (priv->current_node,
                             token == JSON_TOKEN_TRUE ? TRUE : FALSE);
      return G_TOKEN_NONE;

    case '-':
      {
        guint next_token = g_scanner_peek_next_token (scanner);

        if (next_token == G_TOKEN_INT || next_token == G_TOKEN_FLOAT)
          {
            priv->root = priv->current_node = json_node_new (JSON_NODE_VALUE);
            
            token = g_scanner_get_next_token (scanner);
            switch (token)
              {
              case G_TOKEN_INT:
                json_node_set_int (priv->current_node, scanner->value.v_int64);
                break;
              case G_TOKEN_FLOAT:
                json_node_set_double (priv->current_node, scanner->value.v_float);
                break;
              default:
                break;
              }

            return G_TOKEN_NONE;
          }
        else
          return G_TOKEN_INT;
      }
      break;

    case G_TOKEN_INT:
    case G_TOKEN_FLOAT:
    case G_TOKEN_STRING:
      priv->root = priv->current_node = json_node_new (JSON_NODE_VALUE);
      if (token == G_TOKEN_INT)
        json_node_set_int (priv->current_node, scanner->value.v_int64);
      else if (token == G_TOKEN_FLOAT)
        json_node_set_double (priv->current_node, scanner->value.v_float);
      else
        json_node_set_string (priv->current_node, scanner->value.v_string);
      return G_TOKEN_NONE;

    default:
      g_scanner_get_next_token (scanner);
      return G_TOKEN_SYMBOL;
    }
}

static void
json_scanner_msg_handler (GScanner *scanner,
                          gchar    *message,
                          gboolean  is_error)
{
  JsonParser *parser = scanner->user_data;

  if (is_error)
    {
      GError *error = NULL;

      g_set_error (&error, JSON_PARSER_ERROR,
                   JSON_PARSER_ERROR_PARSE,
                   "Parse error on line %d: %s",
                   scanner->line,
                   message);
      
      parser->priv->last_error = error;
      g_signal_emit (parser, parser_signals[ERROR], 0, error);
    }
  else
    g_warning ("Line %d: %s", scanner->line, message);
}

static GScanner *
json_scanner_new (JsonParser *parser)
{
  GScanner *scanner;

  scanner = g_scanner_new (&json_scanner_config);
  scanner->msg_handler = json_scanner_msg_handler;
  scanner->user_data = parser;

  return scanner;
}

/**
 * json_parser_new:
 * 
 * Creates a new #JsonParser instance. You can use the #JsonParser to
 * load a JSON stream from either a file or a buffer and then walk the
 * hierarchy using the data types API.
 *
 * Return value: the newly created #JsonParser. Use g_object_unref()
 *   to release all the memory it allocates.
 */
JsonParser *
json_parser_new (void)
{
  return g_object_new (JSON_TYPE_PARSER, NULL);
}

/**
 * json_parser_load_from_file:
 * @parser: a #JsonParser
 * @filename: the path for the file to parse
 * @error: return location for a #GError, or %NULL
 *
 * Loads a JSON stream from the content of @filename and parses it. See
 * json_parser_load_from_data().
 *
 * Return value: %TRUE if the file was successfully loaded and parsed.
 *   In case of error, @error is set accordingly and %FALSE is returned
 */
gboolean
json_parser_load_from_file (JsonParser   *parser,
                            const gchar  *filename,
                            GError      **error)
{
  GError *internal_error;
  gchar *data;
  gsize length;
  gboolean retval = TRUE;

  g_return_val_if_fail (JSON_IS_PARSER (parser), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  internal_error = NULL;
  if (!g_file_get_contents (filename, &data, &length, &internal_error))
    {
      g_propagate_error (error, internal_error);
      return FALSE;
    }

  if (!json_parser_load_from_data (parser, data, length, &internal_error))
    {
      g_propagate_error (error, internal_error);
      retval = FALSE;
    }
  
  g_free (data);

  return retval;
}

/**
 * json_parser_load_from_data:
 * @parser: a #JsonParser
 * @data: the buffer to parse
 * @length: the length of the buffer, or -1
 * @error: return location for a #GError, or %NULL
 *
 * Loads a JSON stream from a buffer and parses it. You can call this function
 * multiple times with the same #JsonParser object, but the contents of the
 * parser will be destroyed each time.
 *
 * Return value: %TRUE if the buffer was succesfully parser. In case
 *   of error, @error is set accordingly and %FALSE is returned
 */
gboolean
json_parser_load_from_data (JsonParser   *parser,
                            const gchar  *data,
                            gssize        length,
                            GError      **error)
{
  GScanner *scanner;
  gboolean done;
  gboolean retval = TRUE;
  gint i;

  g_return_val_if_fail (JSON_IS_PARSER (parser), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  if (length < 0)
    length = strlen (data);

  if (parser->priv->root)
    {
      json_node_free (parser->priv->root);
      parser->priv->root = NULL;
    }

  scanner = json_scanner_new (parser);
  g_scanner_input_text (scanner, data, length);

  for (i = 0; i < n_symbols; i++)
    {
      g_scanner_scope_add_symbol (scanner, 0,
                                  symbol_names + symbols[i].name_offset,
                                  GINT_TO_POINTER (symbols[i].token));
    }

  parser->priv->scanner = scanner;

  g_signal_emit (parser, parser_signals[PARSE_START], 0);

  done = FALSE;
  while (!done)
    {
      if (g_scanner_peek_next_token (scanner) == G_TOKEN_EOF)
        done = TRUE;
      else
        {
          guint expected_token;

          expected_token = json_parse_statement (parser, scanner);
          if (expected_token != G_TOKEN_NONE)
            {
              const gchar *symbol_name;
              gchar *msg;

              msg = NULL;
              symbol_name = NULL;
              if (scanner->scope_id == 0)
                {
                  if (expected_token > JSON_TOKEN_INVALID &&
                      expected_token < JSON_TOKEN_LAST)
                    {
                      for (i = 0; i < n_symbols; i++)
                        if (symbols[i].token == expected_token)
                          symbol_name = symbol_names + symbols[i].name_offset;

                      if (!msg)
                        msg = g_strconcat ("e.g. '", symbol_name, "'", NULL);
                    }

                  if (scanner->token > JSON_TOKEN_INVALID &&
                      scanner->token < JSON_TOKEN_LAST)
                    {
                      symbol_name = "???";

                      for (i = 0; i < n_symbols; i++)
                        if (symbols[i].token == scanner->token)
                          symbol_name = symbol_names + symbols[i].name_offset;
                    }
                }

              /* this will emit the ::error signal via the custom
               * message handler we install
               */
              g_scanner_unexp_token (scanner, expected_token,
                                     NULL, "keyword",
                                     symbol_name, msg,
                                     TRUE);

              if (parser->priv->last_error)
                {
                  g_propagate_error (error, parser->priv->last_error);
                  parser->priv->last_error = NULL;
                }

              retval = FALSE;

              g_free (msg);
              done = TRUE;
            }
        }
    }

  g_scanner_destroy (scanner);
  parser->priv->scanner = NULL;
  parser->priv->current_node = NULL;

  g_signal_emit (parser, parser_signals[PARSE_END], 0);

  return retval;
}

/**
 * json_parser_get_root:
 * @parser: a #JsonParser
 *
 * Retrieves the top level node from the parsed JSON stream.
 *
 * Return value: (transfer none): the root #JsonNode . The returned node
 *   is owned by the #JsonParser and should never be modified or freed.
 */
JsonNode *
json_parser_get_root (JsonParser *parser)
{
  g_return_val_if_fail (JSON_IS_PARSER (parser), NULL);

  return parser->priv->root;
}

/**
 * json_parser_get_current_line:
 * @parser: a #JsonParser
 *
 * Retrieves the line currently parsed, starting from 1.
 *
 * Return value: the currently parsed line.
 */
guint
json_parser_get_current_line (JsonParser *parser)
{
  g_return_val_if_fail (JSON_IS_PARSER (parser), 0);

  if (parser->priv->scanner)
    return g_scanner_cur_line (parser->priv->scanner);

  return 0;
}

/**
 * json_parser_get_current_pos:
 * @parser: a #JsonParser
 *
 * Retrieves the current position inside the current line, starting
 * from 0.
 *
 * Return value: the position in the current line
 */
guint
json_parser_get_current_pos (JsonParser *parser)
{
  g_return_val_if_fail (JSON_IS_PARSER (parser), 0);

  if (parser->priv->scanner)
    return g_scanner_cur_line (parser->priv->scanner);

  return 0;
}
