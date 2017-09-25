/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013, 2017 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Portions of this file are derived from gnome-desktop/libgnome-desktop/gnome-rr-config.c
 *
 * Copyright 2007, 2008, Red Hat, Inc.
 * Copyright 2010 Giovanni Campagna
 *
 * Author: Soren Sandmann <sandmann@redhat.com>
 */

#include "config.h"

#include "backends/meta-monitor-config-migration.h"

#include <gio/gio.h>
#include <string.h>

#include "backends/meta-monitor-config-manager.h"
#include "backends/meta-monitor-config-store.h"
#include "backends/meta-monitor-manager-private.h"
#include "meta/boxes.h"

#define META_MONITORS_CONFIG_MIGRATION_ERROR (meta_monitors_config_migration_error_quark ())
static GQuark meta_monitors_config_migration_error_quark (void);

G_DEFINE_QUARK (meta-monitors-config-migration-error-quark,
                meta_monitors_config_migration_error)

enum _MetaConfigMigrationError
{
  META_MONITORS_CONFIG_MIGRATION_ERROR_NOT_TILED,
  META_MONITORS_CONFIG_MIGRATION_ERROR_NOT_MAIN_TILE
} MetaConfigMigrationError;

typedef struct
{
  char *connector;
  char *vendor;
  char *product;
  char *serial;
} MetaOutputKey;

typedef struct
{
  gboolean enabled;
  MetaRectangle rect;
  float refresh_rate;
  MetaMonitorTransform transform;

  gboolean is_primary;
  gboolean is_presentation;
  gboolean is_underscanning;
} MetaOutputConfig;

typedef struct _MetaLegacyMonitorsConfig
{
  MetaOutputKey *keys;
  MetaOutputConfig *outputs;
  unsigned int n_outputs;
} MetaLegacyMonitorsConfig;

typedef enum
{
  STATE_INITIAL,
  STATE_MONITORS,
  STATE_CONFIGURATION,
  STATE_OUTPUT,
  STATE_OUTPUT_FIELD,
  STATE_CLONE
} ParserState;

typedef struct
{
  ParserState state;
  int unknown_count;

  GArray *key_array;
  GArray *output_array;
  MetaOutputKey key;
  MetaOutputConfig output;

  char *output_field;

  GHashTable *configs;
} ConfigParser;

static MetaLegacyMonitorsConfig *
legacy_config_new (void)
{
  return g_new0 (MetaLegacyMonitorsConfig, 1);
}

static void
legacy_config_free (gpointer data)
{
  MetaLegacyMonitorsConfig *config = data;

  g_free (config->keys);
  g_free (config->outputs);
  g_free (config);
}

static unsigned long
output_key_hash (const MetaOutputKey *key)
{
  return (g_str_hash (key->connector) ^
          g_str_hash (key->vendor) ^
          g_str_hash (key->product) ^
          g_str_hash (key->serial));
}

static gboolean
output_key_equal (const MetaOutputKey *one,
                  const MetaOutputKey *two)
{
  return (strcmp (one->connector, two->connector) == 0 &&
          strcmp (one->vendor, two->vendor) == 0 &&
          strcmp (one->product, two->product) == 0 &&
          strcmp (one->serial, two->serial) == 0);
}

static unsigned int
legacy_config_hash (gconstpointer data)
{
  const MetaLegacyMonitorsConfig *config = data;
  unsigned int i, hash;

  hash = 0;
  for (i = 0; i < config->n_outputs; i++)
    hash ^= output_key_hash (&config->keys[i]);

  return hash;
}

static gboolean
legacy_config_equal (gconstpointer one,
                     gconstpointer two)
{
  const MetaLegacyMonitorsConfig *c_one = one;
  const MetaLegacyMonitorsConfig *c_two = two;
  unsigned int i;
  gboolean ok;

  if (c_one->n_outputs != c_two->n_outputs)
    return FALSE;

  ok = TRUE;
  for (i = 0; i < c_one->n_outputs && ok; i++)
    ok = output_key_equal (&c_one->keys[i],
                           &c_two->keys[i]);

  return ok;
}

static void
free_output_key (MetaOutputKey *key)
{
  g_free (key->connector);
  g_free (key->vendor);
  g_free (key->product);
  g_free (key->serial);
}

static void
handle_start_element (GMarkupParseContext *context,
                      const char          *element_name,
                      const char         **attribute_names,
                      const char         **attribute_values,
                      gpointer             user_data,
                      GError             **error)
{
  ConfigParser *parser = user_data;

  switch (parser->state)
    {
    case STATE_INITIAL:
      {
        char *version;

        if (strcmp (element_name, "monitors") != 0)
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                         "Invalid document element %s", element_name);
            return;
          }

        if (!g_markup_collect_attributes (element_name,
                                          attribute_names,
                                          attribute_values,
                                          error,
                                          G_MARKUP_COLLECT_STRING,
                                          "version", &version,
                                          G_MARKUP_COLLECT_INVALID))
          return;

        if (strcmp (version, "1") != 0)
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         "Invalid or unsupported version %s", version);
            return;
          }

        parser->state = STATE_MONITORS;
        return;
      }

    case STATE_MONITORS:
      {
        if (strcmp (element_name, "configuration") != 0)
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                         "Invalid toplevel element %s", element_name);
            return;
          }

        parser->key_array = g_array_new (FALSE, FALSE,
                                         sizeof (MetaOutputKey));
        parser->output_array = g_array_new (FALSE, FALSE,
                                            sizeof (MetaOutputConfig));
        parser->state = STATE_CONFIGURATION;
        return;
      }

    case STATE_CONFIGURATION:
      {
        if (strcmp (element_name, "clone") == 0 &&
            parser->unknown_count == 0)
          {
            parser->state = STATE_CLONE;
          }
        else if (strcmp (element_name, "output") == 0 &&
                 parser->unknown_count == 0)
          {
            char *name;

            if (!g_markup_collect_attributes (element_name,
                                              attribute_names,
                                              attribute_values,
                                              error,
                                              G_MARKUP_COLLECT_STRING,
                                              "name", &name,
                                              G_MARKUP_COLLECT_INVALID))
              return;

            memset (&parser->key, 0, sizeof (MetaOutputKey));
            memset (&parser->output, 0, sizeof (MetaOutputConfig));

            parser->key.connector = g_strdup (name);
            parser->state = STATE_OUTPUT;
          }
        else
          {
            parser->unknown_count++;
          }

        return;
      }

    case STATE_OUTPUT:
      {
        if ((strcmp (element_name, "vendor") == 0 ||
             strcmp (element_name, "product") == 0 ||
             strcmp (element_name, "serial") == 0 ||
             strcmp (element_name, "width") == 0 ||
             strcmp (element_name, "height") == 0 ||
             strcmp (element_name, "rate") == 0 ||
             strcmp (element_name, "x") == 0 ||
             strcmp (element_name, "y") == 0 ||
             strcmp (element_name, "rotation") == 0 ||
             strcmp (element_name, "reflect_x") == 0 ||
             strcmp (element_name, "reflect_y") == 0 ||
             strcmp (element_name, "primary") == 0 ||
             strcmp (element_name, "presentation") == 0 ||
             strcmp (element_name, "underscanning") == 0) &&
            parser->unknown_count == 0)
          {
            parser->state = STATE_OUTPUT_FIELD;

            parser->output_field = g_strdup (element_name);
          }
        else
          {
            parser->unknown_count++;
          }

        return;
      }

    case STATE_CLONE:
    case STATE_OUTPUT_FIELD:
      {
        g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                     "Unexpected element %s", element_name);
        return;
      }

    default:
      g_assert_not_reached ();
    }
}

static void
handle_end_element (GMarkupParseContext *context,
                    const char          *element_name,
                    gpointer             user_data,
                    GError             **error)
{
  ConfigParser *parser = user_data;

  switch (parser->state)
    {
    case STATE_MONITORS:
      {
        parser->state = STATE_INITIAL;
        return;
      }

    case STATE_CONFIGURATION:
      {
        if (strcmp (element_name, "configuration") == 0 &&
            parser->unknown_count == 0)
          {
            MetaLegacyMonitorsConfig *config = legacy_config_new ();

            g_assert (parser->key_array->len == parser->output_array->len);

            config->n_outputs = parser->key_array->len;
            config->keys = (void*)g_array_free (parser->key_array, FALSE);
            config->outputs = (void*)g_array_free (parser->output_array, FALSE);

            g_hash_table_replace (parser->configs, config, config);

            parser->key_array = NULL;
            parser->output_array = NULL;
            parser->state = STATE_MONITORS;
          }
        else
          {
            parser->unknown_count--;

            g_assert (parser->unknown_count >= 0);
          }

        return;
      }

    case STATE_OUTPUT:
      {
        if (strcmp (element_name, "output") == 0 && parser->unknown_count == 0)
          {
            if (parser->key.vendor == NULL ||
                parser->key.product == NULL ||
                parser->key.serial == NULL)
              {
                /* Disconnected output, ignore */
                free_output_key (&parser->key);
              }
            else
              {
                if (parser->output.rect.width == 0 ||
                    parser->output.rect.height == 0)
                  parser->output.enabled = FALSE;
                else
                  parser->output.enabled = TRUE;

                g_array_append_val (parser->key_array, parser->key);
                g_array_append_val (parser->output_array, parser->output);
              }

            memset (&parser->key, 0, sizeof (MetaOutputKey));
            memset (&parser->output, 0, sizeof (MetaOutputConfig));

            parser->state = STATE_CONFIGURATION;
          }
        else
          {
            parser->unknown_count--;

            g_assert (parser->unknown_count >= 0);
          }

        return;
      }

    case STATE_CLONE:
      {
        parser->state = STATE_CONFIGURATION;
        return;
      }

    case STATE_OUTPUT_FIELD:
      {
        g_free (parser->output_field);
        parser->output_field = NULL;

        parser->state = STATE_OUTPUT;
        return;
      }

    case STATE_INITIAL:
    default:
      g_assert_not_reached ();
    }
}

static void
read_int (const char *text,
          gsize       text_len,
          gint       *field,
          GError    **error)
{
  char buf[64];
  gint64 v;
  char *end;

  strncpy (buf, text, text_len);
  buf[MIN (63, text_len)] = 0;

  v = g_ascii_strtoll (buf, &end, 10);

  /* Limit reasonable values (actual limits are a lot smaller that these) */
  if (*end || v < 0 || v > G_MAXINT16)
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                 "Expected a number, got %s", buf);
  else
    *field = v;
}

static void
read_float (const char  *text,
            gsize        text_len,
            gfloat      *field,
            GError     **error)
{
  char buf[64];
  gfloat v;
  char *end;

  strncpy (buf, text, text_len);
  buf[MIN (63, text_len)] = 0;

  v = g_ascii_strtod (buf, &end);

  /* Limit reasonable values (actual limits are a lot smaller that these) */
  if (*end)
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                 "Expected a number, got %s", buf);
  else
    *field = v;
}

static gboolean
read_bool (const char  *text,
           gsize        text_len,
           GError     **error)
{
  if (strncmp (text, "no", text_len) == 0)
    return FALSE;
  else if (strncmp (text, "yes", text_len) == 0)
    return TRUE;
  else
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                 "Invalid boolean value %.*s", (int)text_len, text);

  return FALSE;
}

static gboolean
is_all_whitespace (const char *text,
                   gsize       text_len)
{
  gsize i;

  for (i = 0; i < text_len; i++)
    if (!g_ascii_isspace (text[i]))
      return FALSE;

  return TRUE;
}

static void
handle_text (GMarkupParseContext *context,
             const gchar         *text,
             gsize                text_len,
             gpointer             user_data,
             GError             **error)
{
  ConfigParser *parser = user_data;

  switch (parser->state)
    {
    case STATE_MONITORS:
      {
        if (!is_all_whitespace (text, text_len))
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "Unexpected content at this point");
        return;
      }

    case STATE_CONFIGURATION:
      {
        if (parser->unknown_count == 0)
          {
            if (!is_all_whitespace (text, text_len))
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Unexpected content at this point");
          }
        else
          {
            /* Handling unknown element, ignore */
          }

        return;
      }

    case STATE_OUTPUT:
      {
        if (parser->unknown_count == 0)
          {
            if (!is_all_whitespace (text, text_len))
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Unexpected content at this point");
          }
        else
          {
            /* Handling unknown element, ignore */
          }
        return;
      }

    case STATE_CLONE:
      {
        /* Ignore the clone flag */
        return;
      }

    case STATE_OUTPUT_FIELD:
      {
        if (strcmp (parser->output_field, "vendor") == 0)
          parser->key.vendor = g_strndup (text, text_len);
        else if (strcmp (parser->output_field, "product") == 0)
          parser->key.product = g_strndup (text, text_len);
        else if (strcmp (parser->output_field, "serial") == 0)
          parser->key.serial = g_strndup (text, text_len);
        else if (strcmp (parser->output_field, "width") == 0)
          read_int (text, text_len, &parser->output.rect.width, error);
        else if (strcmp (parser->output_field, "height") == 0)
          read_int (text, text_len, &parser->output.rect.height, error);
        else if (strcmp (parser->output_field, "rate") == 0)
          read_float (text, text_len, &parser->output.refresh_rate, error);
        else if (strcmp (parser->output_field, "x") == 0)
          read_int (text, text_len, &parser->output.rect.x, error);
        else if (strcmp (parser->output_field, "y") == 0)
          read_int (text, text_len, &parser->output.rect.y, error);
        else if (strcmp (parser->output_field, "rotation") == 0)
          {
            if (strncmp (text, "normal", text_len) == 0)
              parser->output.transform = META_MONITOR_TRANSFORM_NORMAL;
            else if (strncmp (text, "left", text_len) == 0)
              parser->output.transform = META_MONITOR_TRANSFORM_90;
            else if (strncmp (text, "upside_down", text_len) == 0)
              parser->output.transform = META_MONITOR_TRANSFORM_180;
            else if (strncmp (text, "right", text_len) == 0)
              parser->output.transform = META_MONITOR_TRANSFORM_270;
            else
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid rotation type %.*s", (int)text_len, text);
          }
        else if (strcmp (parser->output_field, "reflect_x") == 0)
          parser->output.transform += read_bool (text, text_len, error) ?
            META_MONITOR_TRANSFORM_FLIPPED : 0;
        else if (strcmp (parser->output_field, "reflect_y") == 0)
          {
            if (read_bool (text, text_len, error))
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Y reflection is not supported");
          }
        else if (strcmp (parser->output_field, "primary") == 0)
          parser->output.is_primary = read_bool (text, text_len, error);
        else if (strcmp (parser->output_field, "presentation") == 0)
          parser->output.is_presentation = read_bool (text, text_len, error);
        else if (strcmp (parser->output_field, "underscanning") == 0)
          parser->output.is_underscanning = read_bool (text, text_len, error);
        else
          g_assert_not_reached ();
        return;
      }

    case STATE_INITIAL:
    default:
      g_assert_not_reached ();
    }
}

static const GMarkupParser config_parser = {
  .start_element = handle_start_element,
  .end_element = handle_end_element,
  .text = handle_text,
};

static GHashTable *
load_config_file (GFile   *file,
                  GError **error)
{
  g_autofree char *contents = NULL;
  gsize size;
  g_autoptr (GMarkupParseContext) context = NULL;
  ConfigParser parser = { 0 };

  if (!g_file_load_contents (file, NULL, &contents, &size, NULL, error))
    return FALSE;

  parser.configs = g_hash_table_new_full (legacy_config_hash,
                                          legacy_config_equal,
                                          legacy_config_free,
                                          NULL);
  parser.state = STATE_INITIAL;

  context = g_markup_parse_context_new (&config_parser,
                                        G_MARKUP_TREAT_CDATA_AS_TEXT |
                                        G_MARKUP_PREFIX_ERROR_POSITION,
                                        &parser, NULL);
  if (!g_markup_parse_context_parse (context, contents, size, error))
    {
      if (parser.key_array)
        g_array_free (parser.key_array, TRUE);
      if (parser.output_array)
        g_array_free (parser.output_array, TRUE);

      free_output_key (&parser.key);
      g_free (parser.output_field);
      g_hash_table_destroy (parser.configs);

      return NULL;
    }

  return parser.configs;
}

static MetaMonitorConfig *
create_monitor_config (MetaOutputKey    *output_key,
                       MetaOutputConfig *output_config,
                       int               mode_width,
                       int               mode_height,
                       GError          **error)
{
  MetaMonitorModeSpec *mode_spec;
  MetaMonitorSpec *monitor_spec;
  MetaMonitorConfig *monitor_config;

  mode_spec = g_new0 (MetaMonitorModeSpec, 1);
  *mode_spec = (MetaMonitorModeSpec) {
    .width = mode_width,
    .height = mode_height,
    .refresh_rate = output_config->refresh_rate
  };

  if (!meta_verify_monitor_mode_spec (mode_spec, error))
    {
      g_free (mode_spec);
      return NULL;
    }

  monitor_spec = g_new0 (MetaMonitorSpec, 1);
  *monitor_spec = (MetaMonitorSpec) {
    .connector = output_key->connector,
    .vendor = output_key->vendor,
    .product = output_key->product,
    .serial = output_key->serial
  };

  monitor_config = g_new0 (MetaMonitorConfig, 1);
  *monitor_config = (MetaMonitorConfig) {
    .monitor_spec = monitor_spec,
    .mode_spec = mode_spec,
    .enable_underscanning = output_config->is_underscanning
  };

  if (!meta_verify_monitor_config (monitor_config, error))
    {
      meta_monitor_config_free (monitor_config);
      return NULL;
    }

  return monitor_config;
}

typedef struct _MonitorTile
{
  MetaOutputKey *output_key;
  MetaOutputConfig *output_config;
} MonitorTile;

static MetaMonitorConfig *
try_derive_tiled_monitor_config (MetaLegacyMonitorsConfig *config,
                                 MetaOutputKey            *output_key,
                                 MetaOutputConfig         *output_config,
                                 MetaMonitorConfigStore   *config_store,
                                 MetaRectangle            *out_layout,
                                 GError                  **error)
{
  MonitorTile top_left_tile = { 0 };
  MonitorTile top_right_tile = { 0 };
  MonitorTile bottom_left_tile = { 0 };
  MonitorTile bottom_right_tile = { 0 };
  MonitorTile origin_tile = { 0 };
  MetaMonitorTransform transform = output_config->transform;
  unsigned int i;
  int max_x = 0;
  int min_x = INT_MAX;
  int max_y = 0;
  int min_y = INT_MAX;
  int mode_width = 0;
  int mode_height = 0;
  MetaMonitorConfig *monitor_config;

  /*
   * In order to derive a monitor configuration for a tiled monitor,
   * try to find the origin tile, then combine the discovered output
   * tiles to given the configured transform a monitor mode.
   *
   * If the origin tile is not the main tile (tile always enabled
   * even for non-tiled modes), this will fail, but since infermation
   * about tiling is lost, there is no way to discover it.
   */

  for (i = 0; i < config->n_outputs; i++)
    {
      MetaOutputKey *other_output_key = &config->keys[i];
      MetaOutputConfig *other_output_config = &config->outputs[i];
      MetaRectangle *rect;

      if (strcmp (output_key->vendor, other_output_key->vendor) != 0 ||
          strcmp (output_key->product, other_output_key->product) != 0 ||
          strcmp (output_key->serial, other_output_key->serial) != 0)
        continue;

      rect = &other_output_config->rect;
      min_x = MIN (min_x, rect->x);
      min_y = MIN (min_y, rect->y);
      max_x = MAX (max_x, rect->x + rect->width);
      max_y = MAX (max_y, rect->y + rect->height);

      if (min_x == rect->x &&
          min_y == rect->y)
        {
          top_left_tile = (MonitorTile) {
            .output_key = other_output_key,
            .output_config = other_output_config
          };
        }
      if (max_x == rect->x + rect->width &&
          min_y == rect->y)
        {
          top_right_tile = (MonitorTile) {
            .output_key = other_output_key,
            .output_config = other_output_config
          };
        }
      if (min_x == rect->x &&
          max_y == rect->y + rect->height)
        {
          bottom_left_tile = (MonitorTile) {
            .output_key = other_output_key,
            .output_config = other_output_config
          };
        }
      if (max_x == rect->x + rect->width &&
          max_y == rect->y + rect->height)
        {
          bottom_right_tile = (MonitorTile) {
            .output_key = other_output_key,
            .output_config = other_output_config
          };
        }
    }

  if (top_left_tile.output_key == bottom_right_tile.output_key)
    {
      g_set_error_literal (error,
                           META_MONITORS_CONFIG_MIGRATION_ERROR,
                           META_MONITORS_CONFIG_MIGRATION_ERROR_NOT_TILED,
                           "Not a tiled monitor");
      return NULL;
    }

  switch (transform)
    {
    case META_MONITOR_TRANSFORM_NORMAL:
      origin_tile = top_left_tile;
      mode_width = max_x - min_x;
      mode_height = max_y - min_y;
      break;
    case META_MONITOR_TRANSFORM_90:
      origin_tile = bottom_left_tile;
      mode_width = max_y - min_y;
      mode_height = max_x - min_x;
      break;
    case META_MONITOR_TRANSFORM_180:
      origin_tile = bottom_right_tile;
      mode_width = max_x - min_x;
      mode_height = max_y - min_y;
      break;
    case META_MONITOR_TRANSFORM_270:
      origin_tile = top_right_tile;
      mode_width = max_y - min_y;
      mode_height = max_x - min_x;
      break;
    case META_MONITOR_TRANSFORM_FLIPPED:
      origin_tile = bottom_left_tile;
      mode_width = max_x - min_x;
      mode_height = max_y - min_y;
      break;
    case META_MONITOR_TRANSFORM_FLIPPED_90:
      origin_tile = bottom_right_tile;
      mode_width = max_y - min_y;
      mode_height = max_x - min_x;
      break;
    case META_MONITOR_TRANSFORM_FLIPPED_180:
      origin_tile = top_right_tile;
      mode_width = max_x - min_x;
      mode_height = max_y - min_y;
      break;
    case META_MONITOR_TRANSFORM_FLIPPED_270:
      origin_tile = top_left_tile;
      mode_width = max_y - min_y;
      mode_height = max_x - min_x;
      break;
    }

  g_assert (origin_tile.output_key);
  g_assert (origin_tile.output_config);

  if (origin_tile.output_key != output_key)
    {
      g_set_error_literal (error,
                           META_MONITORS_CONFIG_MIGRATION_ERROR,
                           META_MONITORS_CONFIG_MIGRATION_ERROR_NOT_MAIN_TILE,
                           "Not the main tile");
      return NULL;
    }

  monitor_config = create_monitor_config (origin_tile.output_key,
                                          origin_tile.output_config,
                                          mode_width, mode_height,
                                          error);
  if (!monitor_config)
    return NULL;

  *out_layout = (MetaRectangle) {
    .x = min_x,
    .y = min_y,
    .width = max_x - min_x,
    .height = max_y - min_y
  };

  return monitor_config;
}

static MetaMonitorConfig *
derive_monitor_config (MetaOutputKey    *output_key,
                       MetaOutputConfig *output_config,
                       MetaRectangle    *out_layout,
                       GError          **error)
{
  int mode_width;
  int mode_height;
  MetaMonitorConfig *monitor_config;

  if (meta_monitor_transform_is_rotated (output_config->transform))
    {
      mode_width = output_config->rect.height;
      mode_height = output_config->rect.width;
    }
  else
    {
      mode_width = output_config->rect.width;
      mode_height = output_config->rect.height;
    }

  monitor_config = create_monitor_config (output_key, output_config,
                                          mode_width, mode_height,
                                          error);
  if (!monitor_config)
    return NULL;

  *out_layout = output_config->rect;

  return monitor_config;
}

static MetaLogicalMonitorConfig *
ensure_logical_monitor (GList           **logical_monitor_configs,
                        MetaOutputConfig *output_config,
                        MetaRectangle    *layout)
{
  MetaLogicalMonitorConfig *new_logical_monitor_config;
  GList *l;

  for (l = *logical_monitor_configs; l; l = l->next)
    {
      MetaLogicalMonitorConfig *logical_monitor_config = l->data;

      if (meta_rectangle_equal (&logical_monitor_config->layout, layout))
        return logical_monitor_config;
    }

  new_logical_monitor_config = g_new0 (MetaLogicalMonitorConfig, 1);
  *new_logical_monitor_config = (MetaLogicalMonitorConfig) {
    .layout = *layout,
    .is_primary = output_config->is_primary,
    .is_presentation = output_config->is_presentation,
    .transform = output_config->transform,
    .scale = -1.0,
  };

  *logical_monitor_configs = g_list_append (*logical_monitor_configs,
                                            new_logical_monitor_config);

  return new_logical_monitor_config;
}

static GList *
derive_logical_monitor_configs (MetaLegacyMonitorsConfig *config,
                                MetaMonitorConfigStore   *config_store,
                                GError                  **error)
{
  GList *logical_monitor_configs = NULL;
  unsigned int i;

  for (i = 0; i < config->n_outputs; i++)
    {
      MetaOutputKey *output_key = &config->keys[i];
      MetaOutputConfig *output_config = &config->outputs[i];
      MetaMonitorConfig *monitor_config = NULL;
      MetaRectangle layout;
      MetaLogicalMonitorConfig *logical_monitor_config;

      if (!output_config->enabled)
        continue;

      if (output_key->vendor &&
          g_strcmp0 (output_key->vendor, "unknown") != 0 &&
          output_key->product &&
          g_strcmp0 (output_key->product, "unknown") != 0 &&
          output_key->serial &&
          g_strcmp0 (output_key->serial, "unknown") != 0)
        {
          monitor_config = try_derive_tiled_monitor_config (config,
                                                            output_key,
                                                            output_config,
                                                            config_store,
                                                            &layout,
                                                            error);
          if (!monitor_config)
            {
              if ((*error)->domain == META_MONITORS_CONFIG_MIGRATION_ERROR)
                {
                  int error_code = (*error)->code;

                  g_clear_error (error);

                  switch (error_code)
                    {
                    case META_MONITORS_CONFIG_MIGRATION_ERROR_NOT_TILED:
                      break;
                    case META_MONITORS_CONFIG_MIGRATION_ERROR_NOT_MAIN_TILE:
                      continue;
                    }
                }
              else
                {
                  g_list_free_full (logical_monitor_configs,
                                    (GDestroyNotify) meta_logical_monitor_config_free);
                  return NULL;
                }
            }
        }

      if (!monitor_config)
        monitor_config = derive_monitor_config (output_key, output_config,
                                                &layout,
                                                error);

      if (!monitor_config)
        {
          g_list_free_full (logical_monitor_configs,
                            (GDestroyNotify) meta_logical_monitor_config_free);
          return NULL;
        }

      logical_monitor_config =
        ensure_logical_monitor (&logical_monitor_configs,
                                output_config, &layout);

      logical_monitor_config->monitor_configs =
        g_list_append (logical_monitor_config->monitor_configs, monitor_config);
    }

  if (!logical_monitor_configs)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Empty configuration");
      return NULL;
    }

  return logical_monitor_configs;
}

static char *
generate_config_name (MetaLegacyMonitorsConfig *config)
{
  char **output_strings;
  unsigned int i;
  char *key_name;

  output_strings = g_new0 (char *, config->n_outputs + 1);
  for (i = 0; i < config->n_outputs; i++)
    {
      MetaOutputKey *output_key = &config->keys[i];

      output_strings[i] = g_strdup_printf ("%s:%s:%s:%s",
                                           output_key->connector,
                                           output_key->vendor,
                                           output_key->product,
                                           output_key->serial);
    }

  key_name = g_strjoinv (", ", output_strings);

  g_strfreev (output_strings);

  return key_name;
}

static GList *
find_disabled_monitor_specs (MetaLegacyMonitorsConfig *legacy_config)
{
  GList *disabled_monitors = NULL;
  unsigned int i;

  for (i = 0; i < legacy_config->n_outputs; i++)
    {
      MetaOutputKey *output_key = &legacy_config->keys[i];
      MetaOutputConfig *output_config = &legacy_config->outputs[i];
      MetaMonitorSpec *monitor_spec;

      if (output_config->enabled)
        continue;

      monitor_spec = g_new0 (MetaMonitorSpec, 1);
      *monitor_spec = (MetaMonitorSpec) {
        .connector = output_key->connector,
        .vendor = output_key->vendor,
        .product = output_key->product,
        .serial = output_key->serial
      };

      disabled_monitors = g_list_prepend (disabled_monitors, monitor_spec);
    }

  return disabled_monitors;
}

static void
migrate_config (gpointer key,
                gpointer value,
                gpointer user_data)
{
  MetaLegacyMonitorsConfig *legacy_config = key;
  MetaMonitorConfigStore *config_store = user_data;
  MetaMonitorManager *monitor_manager =
    meta_monitor_config_store_get_monitor_manager (config_store);
  GList *logical_monitor_configs;
  MetaLogicalMonitorLayoutMode layout_mode;
  GError *error = NULL;
  GList *disabled_monitor_specs;
  MetaMonitorsConfig *config;

  logical_monitor_configs = derive_logical_monitor_configs (legacy_config,
                                                            config_store,
                                                            &error);
  if (!logical_monitor_configs)
    {
      g_autofree char *config_name = NULL;

      config_name = generate_config_name (legacy_config);
      g_warning ("Failed to migrate monitor configuration for %s: %s",
                 config_name, error->message);
      return;
    }

  disabled_monitor_specs = find_disabled_monitor_specs (legacy_config);

  layout_mode = META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL;
  config = meta_monitors_config_new_full (logical_monitor_configs,
                                          disabled_monitor_specs,
                                          layout_mode,
                                          META_MONITORS_CONFIG_FLAG_MIGRATED);
  if (!meta_verify_monitors_config (config, monitor_manager, &error))
    {
      g_autofree char *config_name = NULL;

      config_name = generate_config_name (legacy_config);
      g_warning ("Ignoring invalid monitor configuration for %s: %s",
                 config_name, error->message);
      g_object_unref (config);
      return;
    }

  meta_monitor_config_store_add (config_store, config);
}

gboolean
meta_migrate_old_monitors_config (MetaMonitorConfigStore *config_store,
                                  GFile                  *in_file,
                                  GError                **error)
{
  g_autoptr (GHashTable) configs = NULL;

  configs = load_config_file (in_file, error);
  if (!configs)
    return FALSE;

  g_hash_table_foreach (configs, migrate_config, config_store);

  return TRUE;
}

gboolean
meta_migrate_old_user_monitors_config (MetaMonitorConfigStore *config_store,
                                       GError                **error)
{
  g_autofree char *backup_path = NULL;
  g_autoptr (GFile) backup_file = NULL;
  g_autofree char *user_file_path = NULL;
  g_autoptr (GFile) user_file = NULL;

  user_file_path = g_build_filename (g_get_user_config_dir (),
                                     "monitors.xml",
                                     NULL);
  user_file = g_file_new_for_path (user_file_path);
  backup_path = g_build_filename (g_get_user_config_dir (),
                                  "monitors-v1-backup.xml",
                                  NULL);
  backup_file = g_file_new_for_path (backup_path);

  if (!g_file_copy (user_file, backup_file,
                    G_FILE_COPY_OVERWRITE | G_FILE_COPY_BACKUP,
                    NULL, NULL, NULL,
                    error))
    {
      g_warning ("Failed to make a backup of monitors.xml: %s",
                 (*error)->message);
      g_clear_error (error);
    }

  return meta_migrate_old_monitors_config (config_store, user_file, error);
}

gboolean
meta_finish_monitors_config_migration (MetaMonitorManager *monitor_manager,
                                       MetaMonitorsConfig *config,
                                       GError            **error)
{
  MetaMonitorConfigManager *config_manager = monitor_manager->config_manager;
  MetaMonitorConfigStore *config_store =
    meta_monitor_config_manager_get_store (config_manager);
  GList *l;

  for (l = config->logical_monitor_configs; l; l = l->next)
    {
      MetaLogicalMonitorConfig *logical_monitor_config = l->data;
      MetaMonitorConfig *monitor_config;
      MetaMonitorSpec *monitor_spec;
      MetaMonitor *monitor;
      MetaMonitorModeSpec *monitor_mode_spec;
      MetaMonitorMode *monitor_mode;
      float scale;

      monitor_config = logical_monitor_config->monitor_configs->data;
      monitor_spec = monitor_config->monitor_spec;
      monitor = meta_monitor_manager_get_monitor_from_spec (monitor_manager,
                                                            monitor_spec);
      monitor_mode_spec = monitor_config->mode_spec;
      monitor_mode = meta_monitor_get_mode_from_spec (monitor,
                                                      monitor_mode_spec);
      if (!monitor_mode)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Mode not available on monitor");
          return FALSE;
        }

      scale = meta_monitor_calculate_mode_scale (monitor, monitor_mode);

      logical_monitor_config->scale = scale;
    }

  config->layout_mode =
    meta_monitor_manager_get_default_layout_mode (monitor_manager);
  config->flags &= ~META_MONITORS_CONFIG_FLAG_MIGRATED;

  if (!meta_verify_monitors_config (config, monitor_manager, error))
    return FALSE;

  meta_monitor_config_store_add (config_store, config);

  return TRUE;
}
