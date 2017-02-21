/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include "backends/meta-monitor-config-store.h"

#include <gio/gio.h>
#include <string.h>

#include "backends/meta-monitor-config-manager.h"

/*
 * Example configuration:
 *
 * <monitors version="2">
 *   <configuration>
 *     <logicalmonitor>
 *       <x>0</x>
 *       <y>0</y>
 *       <scale>1</scale>
 *       <monitor>
 *         <monitorspec>
 *           <connector>LVDS1</connector>
 *           <vendor>Vendor A</vendor>
 *           <product>Product A</product>
 *           <serial>Serial A</serial>
 *         </monitorspec>
 *         <mode>
 *           <width>1920</width>
 *           <height>1080</height>
 *           <rate>60.049972534179688</rate>
 *         </mode>
 *       </monitor>
 *       <primary>yes</primary>
 *       <presentation>no</presentation>
 *     </logicalmonitor>
 *     <logicalmonitor>
 *       <x>1920</x>
 *       <y>1080</y>
 *       <monitor>
 *         <monitorspec>
 *           <connector>LVDS2</connector>
 *           <vendor>Vendor B</vendor>
 *           <product>Product B</product>
 *           <serial>Serial B</serial>
 *         </monitorspec>
 *         <mode>
 *           <width>1920</width>
 *           <height>1080</height>
 *           <rate>60.049972534179688</rate>
 *         </mode>
 *         <underscanning>yes</underscanning>
 *       </monitor>
 *       <presentation>yes</presentation>
 *     </logicalmonitor>
 *   </configuration>
 * </monitors>
 *
 */

struct _MetaMonitorConfigStore
{
  GObject parent;

  GHashTable *configs;
};

typedef enum
{
  STATE_INITIAL,
  STATE_MONITORS,
  STATE_CONFIGURATION,
  STATE_LOGICAL_MONITOR,
  STATE_LOGICAL_MONITOR_X,
  STATE_LOGICAL_MONITOR_Y,
  STATE_LOGICAL_MONITOR_PRIMARY,
  STATE_LOGICAL_MONITOR_PRESENTATION,
  STATE_LOGICAL_MONITOR_SCALE,
  STATE_MONITOR,
  STATE_MONITOR_SPEC,
  STATE_MONITOR_SPEC_CONNECTOR,
  STATE_MONITOR_SPEC_VENDOR,
  STATE_MONITOR_SPEC_PRODUCT,
  STATE_MONITOR_SPEC_SERIAL,
  STATE_MONITOR_MODE,
  STATE_MONITOR_MODE_WIDTH,
  STATE_MONITOR_MODE_HEIGHT,
  STATE_MONITOR_MODE_RATE,
  STATE_MONITOR_UNDERSCANNING
} ParserState;

typedef struct
{
  ParserState state;
  MetaMonitorConfigStore *config_store;

  GList *current_logical_monitor_configs;
  MetaMonitorSpec *current_monitor_spec;
  MetaMonitorModeSpec *current_monitor_mode_spec;
  MetaMonitorConfig *current_monitor_config;
  MetaLogicalMonitorConfig *current_logical_monitor_config;
} ConfigParser;

G_DEFINE_TYPE (MetaMonitorConfigStore, meta_monitor_config_store,
               G_TYPE_OBJECT)

static void
handle_start_element (GMarkupParseContext  *context,
                      const char           *element_name,
                      const char          **attribute_names,
                      const char          **attribute_values,
                      gpointer              user_data,
                      GError              **error)
{
  ConfigParser *parser = user_data;

  switch (parser->state)
    {
    case STATE_INITIAL:
      {
        char *version;

        if (!g_str_equal (element_name, "monitors"))
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                         "Invalid document element '%s'", element_name);
            return;
          }

        if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values,
                                          error,
                                          G_MARKUP_COLLECT_STRING, "version", &version,
                                          G_MARKUP_COLLECT_INVALID))
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         "Missing config file format version");
          }
        
        /* TODO: Handle converting version 1 configuration files. */

        if (!g_str_equal (version, "2"))
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         "Invalid or unsupported version '%s'", version);
            return;
          }

        parser->state = STATE_MONITORS;
        return;
      }

    case STATE_MONITORS:
      {
        if (!g_str_equal (element_name, "configuration"))
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                         "Invalid toplevel element '%s'", element_name);
            return;
          }

        parser->state = STATE_CONFIGURATION;
        return;
      }

    case STATE_CONFIGURATION:
      {
        if (!g_str_equal (element_name, "logicalmonitor"))
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                         "Invalid configuration element '%s'", element_name);
            return;
          }

        parser->current_logical_monitor_config =
          g_new0 (MetaLogicalMonitorConfig, 1);

        parser->state = STATE_LOGICAL_MONITOR;
        return;
      }

    case STATE_LOGICAL_MONITOR:
      {
        if (g_str_equal (element_name, "x"))
          {
            parser->state = STATE_LOGICAL_MONITOR_X;
          }
        else if (g_str_equal (element_name, "y"))
          {
            parser->state = STATE_LOGICAL_MONITOR_Y;
          }
        else if (g_str_equal (element_name, "scale"))
          {
            parser->state = STATE_LOGICAL_MONITOR_SCALE;
          }
        else if (g_str_equal (element_name, "primary"))
          {
            parser->state = STATE_LOGICAL_MONITOR_PRIMARY;
          }
        else if (g_str_equal (element_name, "presentation"))
          {
            parser->state = STATE_LOGICAL_MONITOR_PRESENTATION;
          }
        else if (g_str_equal (element_name, "monitor"))
          {
            parser->current_monitor_config = g_new0 (MetaMonitorConfig, 1);;

            parser->state = STATE_MONITOR;
          }
        else
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                         "Invalid monitor logicalmonitor element '%s'", element_name);
            return;
          }

        return;
      }

    case STATE_LOGICAL_MONITOR_X:
    case STATE_LOGICAL_MONITOR_Y:
    case STATE_LOGICAL_MONITOR_SCALE:
    case STATE_LOGICAL_MONITOR_PRIMARY:
    case STATE_LOGICAL_MONITOR_PRESENTATION:
      {
        g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                     "Invalid logical monitor element '%s'", element_name);
        return;
      }

    case STATE_MONITOR:
      {
        if (g_str_equal (element_name, "monitorspec"))
          {
            parser->current_monitor_spec = g_new0 (MetaMonitorSpec, 1);

            parser->state = STATE_MONITOR_SPEC;
          }
        else if (g_str_equal (element_name, "mode"))
          {
            parser->current_monitor_mode_spec = g_new0 (MetaMonitorModeSpec, 1);

            parser->state = STATE_MONITOR_MODE;
          }
        else if (g_str_equal (element_name, "underscanning"))
          {
            parser->state = STATE_MONITOR_UNDERSCANNING;
          }
        else
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                         "Invalid monitor element '%s'", element_name);
            return;
          }

        return;
      }

    case STATE_MONITOR_SPEC:
      {
        if (g_str_equal (element_name, "connector"))
          {
            parser->state = STATE_MONITOR_SPEC_CONNECTOR;
          }
        else if (g_str_equal (element_name, "vendor"))
          {
            parser->state = STATE_MONITOR_SPEC_VENDOR;
          }
        else if (g_str_equal (element_name, "product"))
          {
            parser->state = STATE_MONITOR_SPEC_PRODUCT;
          }
        else if (g_str_equal (element_name, "serial"))
          {
            parser->state = STATE_MONITOR_SPEC_SERIAL;
          }
        else
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                         "Invalid monitor spec element '%s'", element_name);
            return;
          }

        return;
      }

    case STATE_MONITOR_SPEC_CONNECTOR:
    case STATE_MONITOR_SPEC_VENDOR:
    case STATE_MONITOR_SPEC_PRODUCT:
    case STATE_MONITOR_SPEC_SERIAL:
      {
        g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                     "Invalid monitor spec element '%s'", element_name);
        return;
      }

    case STATE_MONITOR_MODE:
      {
        if (g_str_equal (element_name, "width"))
          {
            parser->state = STATE_MONITOR_MODE_WIDTH;
          }
        else if (g_str_equal (element_name, "height"))
          {
            parser->state = STATE_MONITOR_MODE_HEIGHT;
          }
        else if (g_str_equal (element_name, "rate"))
          {
            parser->state = STATE_MONITOR_MODE_RATE;
          }
        else
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                         "Invalid mode element '%s'", element_name);
            return;
          }

        return;
      }

    case STATE_MONITOR_MODE_WIDTH:
    case STATE_MONITOR_MODE_HEIGHT:
    case STATE_MONITOR_MODE_RATE:
      {
        g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                     "Invalid mode sub element '%s'", element_name);
        return;
      }

    case STATE_MONITOR_UNDERSCANNING:
      {
        g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                     "Invalid element '%s' under underscanning", element_name);
        return;
      }
    }
}

static gboolean
derive_logical_monitor_layout (MetaLogicalMonitorConfig *logical_monitor_config,
                               GError                  **error)
{
  MetaMonitorConfig *monitor_config;
  int mode_width, mode_height;
  GList *l;

  monitor_config = logical_monitor_config->monitor_configs->data;
  mode_width = monitor_config->mode_spec->width;
  mode_height = monitor_config->mode_spec->height;

  for (l = logical_monitor_config->monitor_configs->next; l; l = l->next)
    {
      monitor_config = l->data;

      if (monitor_config->mode_spec->width != mode_width ||
          monitor_config->mode_spec->height != mode_height)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       "Monitors in logical monitor incompatible");
          return FALSE;
        }
    }

  logical_monitor_config->layout.width = mode_width;
  logical_monitor_config->layout.height = mode_height;

  return TRUE;
}

static void
handle_end_element (GMarkupParseContext  *context,
                    const char           *element_name,
                    gpointer              user_data,
                    GError              **error)
{
  ConfigParser *parser = user_data;

  switch (parser->state)
    {
    case STATE_LOGICAL_MONITOR_X:
    case STATE_LOGICAL_MONITOR_Y:
    case STATE_LOGICAL_MONITOR_SCALE:
    case STATE_LOGICAL_MONITOR_PRIMARY:
    case STATE_LOGICAL_MONITOR_PRESENTATION:
      {
        parser->state = STATE_LOGICAL_MONITOR;
        return;
      }

    case STATE_MONITOR_SPEC_CONNECTOR:
    case STATE_MONITOR_SPEC_VENDOR:
    case STATE_MONITOR_SPEC_PRODUCT:
    case STATE_MONITOR_SPEC_SERIAL:
      {
        parser->state = STATE_MONITOR_SPEC;
        return;
      }

    case STATE_MONITOR_SPEC:
      {
        g_assert (g_str_equal (element_name, "monitorspec"));

        if (!meta_verify_monitor_spec (parser->current_monitor_spec, error))
          return;

        parser->current_monitor_config->monitor_spec =
          parser->current_monitor_spec;
        parser->current_monitor_spec = NULL;

        parser->state = STATE_MONITOR;
        return;
      }

    case STATE_MONITOR_MODE_WIDTH:
    case STATE_MONITOR_MODE_HEIGHT:
    case STATE_MONITOR_MODE_RATE:
      {
        parser->state = STATE_MONITOR_MODE;
        return;
      }

    case STATE_MONITOR_MODE:
      {
        g_assert (g_str_equal (element_name, "mode"));

        if (!meta_verify_monitor_mode_spec (parser->current_monitor_mode_spec,
                                            error))
          return;

        parser->current_monitor_config->mode_spec =
          parser->current_monitor_mode_spec;
        parser->current_monitor_mode_spec = NULL;

        parser->state = STATE_MONITOR;
        return;
      }

    case STATE_MONITOR_UNDERSCANNING:
      {
        g_assert (g_str_equal (element_name, "underscanning"));

        parser->state = STATE_MONITOR;
        return;
      }

    case STATE_MONITOR:
      {
        MetaLogicalMonitorConfig *logical_monitor_config;

        g_assert (g_str_equal (element_name, "monitor"));

        if (!meta_verify_monitor_config (parser->current_monitor_config, error))
          return;

        logical_monitor_config = parser->current_logical_monitor_config;

        logical_monitor_config->monitor_configs =
          g_list_append (logical_monitor_config->monitor_configs,
                         parser->current_monitor_config);
        parser->current_monitor_config = NULL;

        parser->state = STATE_LOGICAL_MONITOR;
        return;
      }

    case STATE_LOGICAL_MONITOR:
      {
        MetaLogicalMonitorConfig *logical_monitor_config =
          parser->current_logical_monitor_config;

        g_assert (g_str_equal (element_name, "logicalmonitor"));

        if (logical_monitor_config->scale == 0)
          logical_monitor_config->scale = 1;

        if (!derive_logical_monitor_layout (logical_monitor_config, error))
          return;

        if (!meta_verify_logical_monitor_config (logical_monitor_config, error))
          return;

        parser->current_logical_monitor_configs =
          g_list_append (parser->current_logical_monitor_configs,
                         logical_monitor_config);
        parser->current_logical_monitor_config = NULL;

        parser->state = STATE_CONFIGURATION;
        return;
      }

    case STATE_CONFIGURATION:
      {
        MetaMonitorsConfig *config;

        g_assert (g_str_equal (element_name, "configuration"));

        config =
          meta_monitors_config_new (parser->current_logical_monitor_configs);

        if (!meta_verify_monitors_config (config, error))
          {
            g_object_unref (config);
            return;
          }

        g_hash_table_replace (parser->config_store->configs,
                              config->key, config);

        parser->current_logical_monitor_configs = NULL;

        parser->state = STATE_MONITORS;
        return;
      }

    case STATE_MONITORS:
      {
        g_assert (g_str_equal (element_name, "monitors"));

        parser->state = STATE_INITIAL;
        return;
      }

    case STATE_INITIAL:
      {
        g_assert_not_reached ();
      }
    }
}

static gboolean
read_int (const char  *text,
          gsize        text_len,
          gint        *out_value,
          GError     **error)
{
  char buf[64];
  int64_t value;
  char *end;

  strncpy (buf, text, text_len);
  buf[MIN (63, text_len)] = 0;

  value = g_ascii_strtoll (buf, &end, 10);

  if (*end || value < 0 || value > G_MAXINT16)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Expected a number, got %s", buf);
      return FALSE;
    }
  else
    {
      *out_value = value;
      return TRUE;
    }
}

static gboolean
read_float (const char  *text,
            gsize        text_len,
            float       *out_value,
            GError     **error)
{
  char buf[64];
  float value;
  char *end;

  strncpy (buf, text, text_len);
  buf[MIN (63, text_len)] = 0;

  value = g_ascii_strtod (buf, &end);

  if (*end)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Expected a number, got %s", buf);
      return FALSE;
    }
  else
    {
      *out_value = value;
      return TRUE;
    }
}

static gboolean
read_bool (const char  *text,
           gsize        text_len,
           gboolean    *out_value,
           GError     **error)
{
  if (strncmp (text, "no", text_len) == 0)
    {
      *out_value = FALSE;
      return TRUE;
    }
  else if (strncmp (text, "yes", text_len) == 0)
    {
      *out_value = TRUE;
      return TRUE;
    }
  else
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Invalid boolean value '%.*s'", (int) text_len, text);
      return FALSE;
    }
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
    case STATE_INITIAL:
    case STATE_MONITORS:
    case STATE_CONFIGURATION:
    case STATE_LOGICAL_MONITOR:
    case STATE_MONITOR:
    case STATE_MONITOR_SPEC:
    case STATE_MONITOR_MODE:
      {
        if (!is_all_whitespace (text, text_len))
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "Unexpected content at this point");
        return;
      }

    case STATE_MONITOR_SPEC_CONNECTOR:
      {
        parser->current_monitor_spec->connector = g_strndup (text, text_len);
        return;
      }

    case STATE_MONITOR_SPEC_VENDOR:
      {
        parser->current_monitor_spec->vendor = g_strndup (text, text_len);
        return;
      }

    case STATE_MONITOR_SPEC_PRODUCT:
      {
        parser->current_monitor_spec->product = g_strndup (text, text_len);
        return;
      }

    case STATE_MONITOR_SPEC_SERIAL:
      {
        parser->current_monitor_spec->serial = g_strndup (text, text_len);
        return;
      }

    case STATE_LOGICAL_MONITOR_X:
      {
        read_int (text, text_len,
                  &parser->current_logical_monitor_config->layout.x, error);
        return;
      }

    case STATE_LOGICAL_MONITOR_Y:
      {
        read_int (text, text_len,
                  &parser->current_logical_monitor_config->layout.y, error);
        return;
      }

    case STATE_LOGICAL_MONITOR_SCALE:
      {
        if (!read_int (text, text_len,
                       &parser->current_logical_monitor_config->scale, error))
          return;

        if (parser->current_logical_monitor_config->scale <= 0)
          {
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Logical monitor scale '%d' invalid",
                         parser->current_logical_monitor_config->scale);
            return;
          }

        return;
      }

    case STATE_LOGICAL_MONITOR_PRIMARY:
      {
        read_bool (text, text_len,
                   &parser->current_logical_monitor_config->is_primary,
                   error);
        return;
      }

    case STATE_LOGICAL_MONITOR_PRESENTATION:
      {
        read_bool (text, text_len,
                   &parser->current_logical_monitor_config->is_presentation,
                   error);
        return;
      }

    case STATE_MONITOR_MODE_WIDTH:
      {
        read_int (text, text_len,
                  &parser->current_monitor_mode_spec->width,
                  error);
        return;
      }

    case STATE_MONITOR_MODE_HEIGHT:
      {
        read_int (text, text_len,
                  &parser->current_monitor_mode_spec->height,
                  error);
        return;
      }

    case STATE_MONITOR_MODE_RATE:
      {
        read_float (text, text_len,
                    &parser->current_monitor_mode_spec->refresh_rate,
                    error);
        return;
      }

    case STATE_MONITOR_UNDERSCANNING:
      {
        read_bool (text, text_len,
                   &parser->current_monitor_config->is_underscanning,
                   error);
        return;
      }
    }
}

static const GMarkupParser config_parser = {
  .start_element = handle_start_element,
  .end_element = handle_end_element,
  .text = handle_text
};

static gboolean
read_config_file (MetaMonitorConfigStore *config_store,
                  GFile                  *file,
                  GError                **error)
{
  char *buffer;
  gsize size;
  ConfigParser parser;
  GMarkupParseContext *parse_context;

  if (!g_file_load_contents (file, NULL, &buffer, &size, NULL, error))
    return FALSE;

  parser = (ConfigParser) {
    .state = STATE_INITIAL,
    .config_store = config_store
  };

  parse_context = g_markup_parse_context_new (&config_parser,
                                              G_MARKUP_TREAT_CDATA_AS_TEXT |
                                              G_MARKUP_PREFIX_ERROR_POSITION,
                                              &parser, NULL);
  if (!g_markup_parse_context_parse (parse_context, buffer, size, error))
    {
      g_list_free_full (parser.current_logical_monitor_configs,
                        (GDestroyNotify) meta_logical_monitor_config_free);
      g_clear_pointer (&parser.current_monitor_spec,
                       meta_monitor_spec_free);
      g_free (parser.current_monitor_mode_spec);
      g_clear_pointer (&parser.current_monitor_config,
                      meta_monitor_config_free);
      g_clear_pointer (&parser.current_logical_monitor_config,
                       meta_logical_monitor_config_free);
      return FALSE;
    }

  g_markup_parse_context_free (parse_context);
  g_free (buffer);

  return TRUE;
}

MetaMonitorsConfig *
meta_monitor_config_store_lookup (MetaMonitorConfigStore *config_store,
                                  MetaMonitorsConfigKey  *key)
{
  return META_MONITORS_CONFIG (g_hash_table_lookup (config_store->configs,
                                                    key));
}

void
meta_monitor_config_store_add (MetaMonitorConfigStore *config_store,
                               MetaMonitorsConfig     *config)
{
  g_hash_table_insert (config_store->configs,
                       config->key, g_object_ref (config));
}

gboolean
meta_monitor_config_store_set_custom (MetaMonitorConfigStore *config_store,
                                      const char             *path,
                                      GError                **error)
{
  g_autoptr (GFile) custom_file = NULL;

  g_hash_table_remove_all (config_store->configs);

  custom_file = g_file_new_for_path (path);

  return read_config_file (config_store, custom_file, error);
}

int
meta_monitor_config_store_get_config_count (MetaMonitorConfigStore *config_store)
{
  return (int) g_hash_table_size (config_store->configs);
}

static void
meta_monitor_config_store_dispose (GObject *object)
{
  MetaMonitorConfigStore *config_store = META_MONITOR_CONFIG_STORE (object);

  g_clear_pointer (&config_store->configs, g_hash_table_destroy);

  G_OBJECT_CLASS (meta_monitor_config_store_parent_class)->dispose (object);
}

static void
meta_monitor_config_store_init (MetaMonitorConfigStore *config_store)
{
  config_store->configs = g_hash_table_new_full (meta_monitors_config_key_hash,
                                                 meta_monitors_config_key_equal,
                                                 NULL,
                                                 g_object_unref);
}

static void
meta_monitor_config_store_class_init (MetaMonitorConfigStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = meta_monitor_config_store_dispose;
}
