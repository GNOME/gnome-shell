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

#define MONITORS_CONFIG_XML_FORMAT_VERSION 2

#define QUOTE1(a) #a
#define QUOTE(a) QUOTE1(a)

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
 *       <transform>
 *         <rotation>right</rotation>
 *         <flipped>no</flipped>
 *       </transform>
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

enum
{
  PROP_0,

  PROP_MONITOR_MANAGER,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

struct _MetaMonitorConfigStore
{
  GObject parent;

  MetaMonitorManager *monitor_manager;

  GHashTable *configs;

  GCancellable *save_cancellable;

  GFile *user_file;
  GFile *custom_file;
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
  STATE_TRANSFORM,
  STATE_TRANSFORM_ROTATION,
  STATE_TRANSFORM_FLIPPED,
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
  gboolean current_transform_flipped;
  MetaMonitorTransform current_transform;
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

        if (!g_str_equal (version, QUOTE (MONITORS_CONFIG_XML_FORMAT_VERSION)))
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
        else if (g_str_equal (element_name, "transform"))
          {
            parser->state = STATE_TRANSFORM;
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

    case STATE_TRANSFORM:
      {
        if (g_str_equal (element_name, "rotation"))
          {
            parser->state = STATE_TRANSFORM_ROTATION;
          }
        else if (g_str_equal (element_name, "flipped"))
          {
            parser->state = STATE_TRANSFORM_FLIPPED;
          }

        return;
      }

    case STATE_TRANSFORM_ROTATION:
    case STATE_TRANSFORM_FLIPPED:
      {
        g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                     "Invalid transform element '%s'", element_name);
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
derive_logical_monitor_layout (MetaLogicalMonitorConfig    *logical_monitor_config,
                               MetaLogicalMonitorLayoutMode layout_mode,
                               GError                     **error)
{
  MetaMonitorConfig *monitor_config;
  int mode_width, mode_height;
  int width = 0, height = 0;
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

  if (meta_monitor_transform_is_rotated (logical_monitor_config->transform))
    {
      width = mode_height;
      height = mode_width;
    }
  else
    {
      width = mode_width;
      height = mode_height;
    }

  switch (layout_mode)
    {
    case META_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL:
      width /= logical_monitor_config->scale;
      height /= logical_monitor_config->scale;
      break;
    case META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL:
      break;
    }

  logical_monitor_config->layout.width = width;
  logical_monitor_config->layout.height = height;

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

    case STATE_TRANSFORM:
      {
        g_assert (g_str_equal (element_name, "transform"));

        parser->current_logical_monitor_config->transform =
          parser->current_transform;
        if (parser->current_transform_flipped)
          {
            parser->current_logical_monitor_config->transform +=
              META_MONITOR_TRANSFORM_FLIPPED;
          }

        parser->current_transform = META_MONITOR_TRANSFORM_NORMAL;
        parser->current_transform_flipped = FALSE;

        parser->state = STATE_LOGICAL_MONITOR;
        return;
      }

    case STATE_TRANSFORM_ROTATION:
    case STATE_TRANSFORM_FLIPPED:
      {
        parser->state = STATE_TRANSFORM;
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

        parser->current_logical_monitor_configs =
          g_list_append (parser->current_logical_monitor_configs,
                         logical_monitor_config);
        parser->current_logical_monitor_config = NULL;

        parser->state = STATE_CONFIGURATION;
        return;
      }

    case STATE_CONFIGURATION:
      {
        MetaMonitorConfigStore *store = parser->config_store;
        MetaMonitorsConfig *config;
        GList *l;
        MetaLogicalMonitorLayoutMode layout_mode;

        g_assert (g_str_equal (element_name, "configuration"));

        layout_mode =
          meta_monitor_manager_get_default_layout_mode (store->monitor_manager);

        for (l = parser->current_logical_monitor_configs; l; l = l->next)
          {
            MetaLogicalMonitorConfig *logical_monitor_config = l->data;

            if (!derive_logical_monitor_layout (logical_monitor_config,
                                                layout_mode,
                                                error))
              return;

            if (!meta_verify_logical_monitor_config (logical_monitor_config,
                                                     layout_mode,
                                                     error))
              return;
          }

        config =
          meta_monitors_config_new (parser->current_logical_monitor_configs,
                                    layout_mode);

        parser->current_logical_monitor_configs = NULL;

        if (!meta_verify_monitors_config (config, store->monitor_manager,
                                          error))
          {
            g_object_unref (config);
            return;
          }

        g_hash_table_replace (parser->config_store->configs,
                              config->key, config);

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
    case STATE_TRANSFORM:
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
        if (!read_float (text, text_len,
                         &parser->current_logical_monitor_config->scale, error))
          return;

        if (parser->current_logical_monitor_config->scale < 1.0)
          {
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Logical monitor scale '%g' invalid",
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

    case STATE_TRANSFORM_ROTATION:
      {
        if (strncmp (text, "normal", text_len) == 0)
          parser->current_transform = META_MONITOR_TRANSFORM_NORMAL;
        else if (strncmp (text, "left", text_len) == 0)
          parser->current_transform = META_MONITOR_TRANSFORM_90;
        else if (strncmp (text, "upside_down", text_len) == 0)
          parser->current_transform = META_MONITOR_TRANSFORM_180;
        else if (strncmp (text, "right", text_len) == 0)
          parser->current_transform = META_MONITOR_TRANSFORM_270;
        else
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "Invalid rotation type %.*s", (int)text_len, text);

        return;
      }

    case STATE_TRANSFORM_FLIPPED:
      {
        read_bool (text, text_len,
                   &parser->current_transform_flipped,
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
                   &parser->current_monitor_config->enable_underscanning,
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

static void
append_monitors (GString *buffer,
                 GList   *monitor_configs)
{
  GList *l;

  for (l = monitor_configs; l; l = l->next)
    {
      MetaMonitorConfig *monitor_config = l->data;
      char rate_str[G_ASCII_DTOSTR_BUF_SIZE];

      g_ascii_dtostr (rate_str, sizeof (rate_str),
                      monitor_config->mode_spec->refresh_rate);

      g_string_append (buffer, "      <monitor>\n");
      g_string_append (buffer, "        <monitorspec>\n");
      g_string_append_printf (buffer, "          <connector>%s</connector>\n",
                              monitor_config->monitor_spec->connector);
      g_string_append_printf (buffer, "          <vendor>%s</vendor>\n",
                              monitor_config->monitor_spec->vendor);
      g_string_append_printf (buffer, "          <product>%s</product>\n",
                              monitor_config->monitor_spec->product);
      g_string_append_printf (buffer, "          <serial>%s</serial>\n",
                              monitor_config->monitor_spec->serial);
      g_string_append (buffer, "        </monitorspec>\n");
      g_string_append (buffer, "        <mode>\n");
      g_string_append_printf (buffer, "          <width>%d</width>\n",
                              monitor_config->mode_spec->width);
      g_string_append_printf (buffer, "          <height>%d</height>\n",
                              monitor_config->mode_spec->height);
      g_string_append_printf (buffer, "          <rate>%s</rate>\n",
                              rate_str);
      g_string_append (buffer, "        </mode>\n");
      if (monitor_config->enable_underscanning)
        g_string_append (buffer, "        <underscanning>yes</underscanning>\n");
      g_string_append (buffer, "      </monitor>\n");
    }
}

static const char *
bool_to_string (gboolean value)
{
  return value ? "yes" : "no";
}

static void
append_transform (GString             *buffer,
                  MetaMonitorTransform transform)
{
  const char *rotation = NULL;
  gboolean flipped = FALSE;

  switch (transform)
    {
    case META_MONITOR_TRANSFORM_NORMAL:
      return;
    case META_MONITOR_TRANSFORM_90:
      rotation = "left";
      break;
    case META_MONITOR_TRANSFORM_180:
      rotation = "upside_down";
      break;
    case META_MONITOR_TRANSFORM_270:
      rotation = "right";
      break;
    case META_MONITOR_TRANSFORM_FLIPPED:
      rotation = "normal";
      flipped = TRUE;
      break;
    case META_MONITOR_TRANSFORM_FLIPPED_90:
      rotation = "left";
      flipped = TRUE;
      break;
    case META_MONITOR_TRANSFORM_FLIPPED_180:
      rotation = "upside_down";
      flipped = TRUE;
      break;
    case META_MONITOR_TRANSFORM_FLIPPED_270:
      rotation = "right";
      flipped = TRUE;
      break;
    }

  g_string_append (buffer, "      <transform>\n");
  g_string_append_printf (buffer, "        <rotation>%s</rotation>\n",
                          rotation);
  g_string_append_printf (buffer, "        <flipped>%s</flipped>\n",
                          bool_to_string (flipped));
  g_string_append (buffer, "      </transform>\n");
}

static void
append_logical_monitor_xml (GString                  *buffer,
                            MetaLogicalMonitorConfig *logical_monitor_config)
{
  g_string_append (buffer, "    <logicalmonitor>\n");
  g_string_append_printf (buffer, "      <x>%d</x>\n",
                          logical_monitor_config->layout.x);
  g_string_append_printf (buffer, "      <y>%d</y>\n",
                          logical_monitor_config->layout.y);
  g_string_append_printf (buffer, "      <scale>%g</scale>\n",
                          logical_monitor_config->scale);
  if (logical_monitor_config->is_primary)
    g_string_append (buffer, "      <primary>yes</primary>\n");
  if (logical_monitor_config->is_presentation)
    g_string_append (buffer, "      <presentation>yes</presentation>\n");
  append_transform (buffer, logical_monitor_config->transform);
  append_monitors (buffer, logical_monitor_config->monitor_configs);
  g_string_append (buffer, "    </logicalmonitor>\n");
}

static GString *
generate_config_xml (MetaMonitorConfigStore *config_store)
{
  GString *buffer;
  GHashTableIter iter;
  MetaMonitorsConfig *config;

  buffer = g_string_new ("");
  g_string_append_printf (buffer, "<monitors version=\"%d\">\n",
                          MONITORS_CONFIG_XML_FORMAT_VERSION);

  g_hash_table_iter_init (&iter, config_store->configs);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &config))
    {
      GList *l;

      g_string_append (buffer, "  <configuration>\n");

      for (l = config->logical_monitor_configs; l; l = l->next)
        {
          MetaLogicalMonitorConfig *logical_monitor_config = l->data;

          append_logical_monitor_xml (buffer, logical_monitor_config);
        }

      g_string_append (buffer, "  </configuration>\n");
    }

  g_string_append (buffer, "</monitors>\n");

  return buffer;
}

typedef struct _SaveData
{
  MetaMonitorConfigStore *config_store;
  GString *buffer;
} SaveData;

static void
saved_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
  SaveData *data = user_data;
  GError *error = NULL;

  if (!g_file_replace_contents_finish (G_FILE (object), result, NULL, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Saving monitor configuration failed: %s\n", error->message);
      else
        g_clear_object (&data->config_store->save_cancellable);

      g_error_free (error);
    }
  else
    {
      g_clear_object (&data->config_store->save_cancellable);
    }

  g_clear_object (&data->config_store);
  g_string_free (data->buffer, TRUE);
  g_free (data);
}

static void
meta_monitor_config_store_save (MetaMonitorConfigStore *config_store)
{
  GString *buffer;
  SaveData *data;

  if (config_store->save_cancellable)
    {
      g_cancellable_cancel (config_store->save_cancellable);
      g_clear_object (&config_store->save_cancellable);
    }

  config_store->save_cancellable = g_cancellable_new ();

  buffer = generate_config_xml (config_store);

  data = g_new0 (SaveData, 1);
  *data = (SaveData) {
    .config_store = g_object_ref (config_store),
    .buffer = buffer
  };

  g_file_replace_contents_async (config_store->user_file,
                                 buffer->str, buffer->len,
                                 NULL,
                                 TRUE,
                                 G_FILE_CREATE_REPLACE_DESTINATION,
                                 config_store->save_cancellable,
                                 saved_cb, data);
}

void
meta_monitor_config_store_add (MetaMonitorConfigStore *config_store,
                               MetaMonitorsConfig     *config)
{
  g_hash_table_replace (config_store->configs,
                        config->key, g_object_ref (config));

  if (!config_store->custom_file)
    meta_monitor_config_store_save (config_store);
}

gboolean
meta_monitor_config_store_set_custom (MetaMonitorConfigStore *config_store,
                                      const char             *path,
                                      GError                **error)
{
  g_clear_object (&config_store->custom_file);
  g_hash_table_remove_all (config_store->configs);

  config_store->custom_file = g_file_new_for_path (path);

  return read_config_file (config_store, config_store->custom_file, error);
}

int
meta_monitor_config_store_get_config_count (MetaMonitorConfigStore *config_store)
{
  return (int) g_hash_table_size (config_store->configs);
}

MetaMonitorConfigStore *
meta_monitor_config_store_new (MetaMonitorManager *monitor_manager)
{
  return g_object_new (META_TYPE_MONITOR_CONFIG_STORE,
                       "monitor-manager", monitor_manager,
                       NULL);
}

static void
meta_monitor_config_store_constructed (GObject *object)
{
  MetaMonitorConfigStore *config_store = META_MONITOR_CONFIG_STORE (object);
  char *user_file_path;
  GError *error = NULL;

  user_file_path = g_build_filename (g_get_user_config_dir (),
                                     "monitors-experimental.xml",
                                     NULL);
  config_store->user_file = g_file_new_for_path (user_file_path);

  if (g_file_test (user_file_path, G_FILE_TEST_EXISTS))
    {
      if (!read_config_file (config_store, config_store->user_file, &error))
        {
          g_warning ("Failed to read monitors config file '%s': %s",
                     user_file_path, error->message);
          g_error_free (error);
        }
    }

  g_free (user_file_path);
}

static void
meta_monitor_config_store_dispose (GObject *object)
{
  MetaMonitorConfigStore *config_store = META_MONITOR_CONFIG_STORE (object);

  g_clear_pointer (&config_store->configs, g_hash_table_destroy);

  g_clear_object (&config_store->user_file);
  g_clear_object (&config_store->custom_file);

  G_OBJECT_CLASS (meta_monitor_config_store_parent_class)->dispose (object);
}

static void
meta_monitor_config_store_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  MetaMonitorConfigStore *config_store = META_MONITOR_CONFIG_STORE (object);

  switch (prop_id)
    {
    case PROP_MONITOR_MANAGER:
      g_value_set_object (value, &config_store->monitor_manager);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_monitor_config_store_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  MetaMonitorConfigStore *config_store = META_MONITOR_CONFIG_STORE (object);

  switch (prop_id)
    {
    case PROP_MONITOR_MANAGER:
      config_store->monitor_manager = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
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

  object_class->constructed = meta_monitor_config_store_constructed;
  object_class->dispose = meta_monitor_config_store_dispose;
  object_class->get_property = meta_monitor_config_store_get_property;
  object_class->set_property = meta_monitor_config_store_set_property;

  obj_props[PROP_MONITOR_MANAGER] =
    g_param_spec_object ("monitor-manager",
                         "MetaMonitorManager",
                         "MetaMonitorManager",
                         META_TYPE_MONITOR_MANAGER,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}
