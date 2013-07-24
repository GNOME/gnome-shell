/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* 
 * Copyright (C) 2001, 2002 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat Inc.
 * Some ICCCM manager selection code derived from fvwm2,
 * Copyright (C) 2001 Dominik Vogt, Matthias Clasen, and fvwm2 team
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
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

#include <string.h>
#include <clutter/clutter.h>

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/dpms.h>
#endif

#include <meta/main.h>
#include <meta/errors.h>
#include "monitor-private.h"
#ifdef HAVE_WAYLAND
#include "meta-wayland-private.h"
#endif

#include "meta-dbus-xrandr.h"

#define ALL_WL_TRANSFORMS ((1 << (WL_OUTPUT_TRANSFORM_FLIPPED_270 + 1)) - 1)

/* These two structures represent the intended/persistent configuration,
   as stored in the monitors.xml file.
*/

typedef struct {
  char *connector;
  char *vendor;
  char *product;
  char *serial;
} MetaOutputKey;

typedef struct {
  gboolean enabled;
  MetaRectangle rect;
  float refresh_rate;
  enum wl_output_transform transform;

  gboolean is_primary;
  gboolean is_presentation;
} MetaOutputConfig;

typedef struct {
  MetaOutputKey *keys;
  MetaOutputConfig *outputs;
  unsigned int n_outputs;
} MetaConfiguration;

struct _MetaMonitorConfig {
  GObject parent_instance;

  GHashTable *configs;
  MetaConfiguration *current;
  gboolean current_is_stored;

  GFile *file;
  GCancellable *save_cancellable;
};

struct _MetaMonitorConfigClass {
  GObjectClass parent;
};

G_DEFINE_TYPE (MetaMonitorConfig, meta_monitor_config, G_TYPE_OBJECT);

static void
free_output_key (MetaOutputKey *key)
{
  g_free (key->connector);
  g_free (key->vendor);
  g_free (key->product);
  g_free (key->serial);
}

static void
config_clear (MetaConfiguration *config)
{
  unsigned int i;

  for (i = 0; i < config->n_outputs; i++)
    free_output_key (&config->keys[i]);

  g_free (config->keys);
  g_free (config->outputs);
}

static void
config_free (gpointer config)
{
  config_clear (config);
  g_slice_free (MetaConfiguration, config);
}

static unsigned long
output_key_hash (const MetaOutputKey *key)
{
  return g_str_hash (key->connector) ^
    g_str_hash (key->vendor) ^
    g_str_hash (key->product) ^
    g_str_hash (key->serial);
}

static gboolean
output_key_equal (const MetaOutputKey *one,
                  const MetaOutputKey *two)
{
  return strcmp (one->connector, two->connector) == 0 &&
    strcmp (one->vendor, two->vendor) == 0 &&
    strcmp (one->product, two->product) == 0 &&
    strcmp (one->serial, two->serial) == 0;
}

static unsigned int
config_hash (gconstpointer data)
{
  const MetaConfiguration *config = data;
  unsigned int i, hash;

  hash = 0;
  for (i = 0; i < config->n_outputs; i++)
    hash ^= output_key_hash (&config->keys[i]);

  return hash;
}

static gboolean
config_equal (gconstpointer one,
              gconstpointer two)
{
  const MetaConfiguration *c_one = one;
  const MetaConfiguration *c_two = two;
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
meta_monitor_config_init (MetaMonitorConfig *self)
{
  const char *filename;
  char *path;

  self->configs = g_hash_table_new_full (config_hash, config_equal, NULL, config_free);

  filename = g_getenv ("MUTTER_MONITOR_FILENAME");
  if (filename == NULL)
    filename = "monitors-test.xml"; /* FIXME after testing */

  path = g_build_filename (g_get_user_config_dir (), filename, NULL);
  self->file = g_file_new_for_path (path);
  g_free (path);
}

static void
meta_monitor_config_finalize (GObject *object)
{
  MetaMonitorConfig *self = META_MONITOR_CONFIG (object);

  g_hash_table_destroy (self->configs);
}

static void
meta_monitor_config_class_init (MetaMonitorConfigClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_monitor_config_finalize;
}

typedef enum {
  STATE_INITIAL,
  STATE_MONITORS,
  STATE_CONFIGURATION,
  STATE_OUTPUT,
  STATE_OUTPUT_FIELD,
  STATE_CLONE
} ParserState;

typedef struct {
  MetaMonitorConfig *config;
  ParserState state;
  int unknown_count;

  GArray *key_array;
  GArray *output_array;
  MetaOutputKey key;
  MetaOutputConfig output;

  char *output_field;
} ConfigParser;

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

        if (strcmp (element_name, "monitors") != 0)
          {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                         "Invalid document element %s", element_name);
            return;
          }

        if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values,
                                          error,
                                          G_MARKUP_COLLECT_STRING, "version", &version,
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

        parser->key_array = g_array_new (FALSE, FALSE, sizeof (MetaOutputKey));
        parser->output_array = g_array_new (FALSE, FALSE, sizeof (MetaOutputConfig));
        parser->state = STATE_CONFIGURATION;
        return;
      }

    case STATE_CONFIGURATION:
      {
        if (strcmp (element_name, "clone") == 0 && parser->unknown_count == 0)
          {
            parser->state = STATE_CLONE;
          }
        else if (strcmp (element_name, "output") == 0 && parser->unknown_count == 0)
          {
            char *name;

            if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values,
                                              error,
                                              G_MARKUP_COLLECT_STRING, "name", &name,
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
             strcmp (element_name, "presentation") == 0) && parser->unknown_count == 0)
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
handle_end_element (GMarkupParseContext  *context,
                    const char           *element_name,
                    gpointer              user_data,
                    GError              **error)
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
        if (strcmp (element_name, "configuration") == 0 && parser->unknown_count == 0)
          {
            MetaConfiguration *config = g_slice_new (MetaConfiguration);

            g_assert (parser->key_array->len == parser->output_array->len);

            config->n_outputs = parser->key_array->len;
            config->keys = (void*)g_array_free (parser->key_array, FALSE);
            config->outputs = (void*)g_array_free (parser->output_array, FALSE);

            g_hash_table_replace (parser->config->configs, config, config);

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
                if (parser->output.rect.width == 0 &&
                    parser->output.rect.width == 0)
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
read_int (const char  *text,
          gsize        text_len,
          gint        *field,
          GError     **error)
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
              parser->output.transform = WL_OUTPUT_TRANSFORM_NORMAL;
            else if (strncmp (text, "left", text_len) == 0)
              parser->output.transform = WL_OUTPUT_TRANSFORM_90;
            else if (strncmp (text, "upside_down", text_len) == 0)
              parser->output.transform = WL_OUTPUT_TRANSFORM_180;
            else if (strncmp (text, "right", text_len) == 0)
              parser->output.transform = WL_OUTPUT_TRANSFORM_270;
            else
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid rotation type %.*s", (int)text_len, text);
          }
        else if (strcmp (parser->output_field, "reflect_x") == 0)
          parser->output.transform += read_bool (text, text_len, error) ?
            WL_OUTPUT_TRANSFORM_FLIPPED : 0;
        else if (strcmp (parser->output_field, "reflect_y") == 0)
          {
            /* FIXME (look at the rotation map in monitor.c) */
            if (read_bool (text, text_len, error))
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Y reflection is not supported");
          }
        else if (strcmp (parser->output_field, "primary") == 0)
          parser->output.is_primary = read_bool (text, text_len, error);
        else if (strcmp (parser->output_field, "presentation") == 0)
          parser->output.is_presentation = read_bool (text, text_len, error);
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

static void
meta_monitor_config_load (MetaMonitorConfig  *self)
{
  char *contents;
  gsize size;
  gboolean ok;
  GError *error;
  GMarkupParseContext *context;
  ConfigParser parser;

  /* Note: we're explicitly loading this file synchronously because
     we don't want to leave the default configuration on for even a frame, ie we
     want atomic modeset as much as possible.

     This function is called only at early initialization anyway, before
     we connect to X or create the wayland socket.
  */

  error = NULL;
  ok = g_file_load_contents (self->file, NULL, &contents, &size, NULL, &error);
  if (!ok)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        meta_warning ("Failed to load stored monitor configuration: %s\n", error->message);

      g_error_free (error);
      return;
    }

  memset (&parser, 0, sizeof (ConfigParser));
  parser.config = self;
  parser.state = STATE_INITIAL;

  context = g_markup_parse_context_new (&config_parser,
                                        G_MARKUP_TREAT_CDATA_AS_TEXT |
                                        G_MARKUP_PREFIX_ERROR_POSITION,
                                        &parser, NULL);
  ok = g_markup_parse_context_parse (context, contents, size, &error);

  if (!ok)
    {
      meta_warning ("Failed to parse stored monitor configuration: %s\n", error->message);

      g_error_free (error);

      if (parser.key_array)
        g_array_free (parser.key_array, TRUE);
      if (parser.output_array)
        g_array_free (parser.output_array, TRUE);

      free_output_key (&parser.key);
    }
}

MetaMonitorConfig *
meta_monitor_config_new (void)
{
  MetaMonitorConfig *self;

  self = g_object_new (META_TYPE_MONITOR_CONFIG, NULL);
  meta_monitor_config_load (self);

  return self;
}

static void
init_key_from_output (MetaOutputKey *key,
                      MetaOutput    *output)
{
  key->connector = g_strdup (output->name);
  key->product = g_strdup (output->product);
  key->vendor = g_strdup (output->vendor);
  key->serial = g_strdup (output->serial);
}

static void
make_config_key (MetaConfiguration *key,
                 MetaOutput        *outputs,
                 unsigned           n_outputs)
{
  unsigned int i;

  key->n_outputs = n_outputs;
  key->outputs = NULL;
  key->keys = g_new0 (MetaOutputKey, n_outputs);

  for (i = 0; i < key->n_outputs; i++)
    init_key_from_output (&key->keys[i], &outputs[i]);
}

gboolean
meta_monitor_config_match_current (MetaMonitorConfig  *self,
                                   MetaMonitorManager *manager)
{
  MetaOutput *outputs;
  unsigned n_outputs;
  MetaConfiguration key;
  gboolean ok;

  if (self->current == NULL)
    return FALSE;

  outputs = meta_monitor_manager_get_outputs (manager, &n_outputs);

  make_config_key (&key, outputs, n_outputs);
  ok = config_equal (&key, self->current);

  config_clear (&key);
  return ok;
}

static MetaConfiguration *
meta_monitor_config_get_stored (MetaMonitorConfig *self,
				MetaOutput        *outputs,
				unsigned           n_outputs)
{
  MetaConfiguration key;
  MetaConfiguration *stored;

  make_config_key (&key, outputs, n_outputs);
  stored = g_hash_table_lookup (self->configs, &key);

  config_clear (&key);
  return stored;
}

static void
make_crtcs (MetaConfiguration   *config,
	    MetaMonitorManager  *manager,
	    GVariant           **crtcs,
	    GVariant           **outputs)
{
  *crtcs = NULL;
  *outputs = NULL;
  /* FIXME */
}

static void
apply_configuration (MetaMonitorConfig  *self,
                     MetaConfiguration  *config,
		     MetaMonitorManager *manager,
                     gboolean            stored)
{
  GVariant *crtcs, *outputs;
		     
  make_crtcs (config, manager, &crtcs, &outputs);
  meta_monitor_manager_apply_configuration (manager, crtcs, outputs);

  if (self->current && !self->current_is_stored)
    config_free (self->current);
  self->current = config;
  self->current_is_stored = stored;

  g_variant_unref (crtcs);
  g_variant_unref (outputs);
}

gboolean
meta_monitor_config_apply_stored (MetaMonitorConfig  *self,
				  MetaMonitorManager *manager)
{
  MetaOutput *outputs;
  MetaConfiguration *stored;
  unsigned n_outputs;

  outputs = meta_monitor_manager_get_outputs (manager, &n_outputs);
  stored = meta_monitor_config_get_stored (self, outputs, n_outputs);

  if (stored)
    {
      apply_configuration (self, stored, manager, TRUE);
      return TRUE;
    }
  else
    return FALSE;
}

static MetaConfiguration *
make_default_config (MetaOutput *outputs,
		     unsigned    n_outputs)
{
  /* FIXME */
  return NULL;
}

void
meta_monitor_config_make_default (MetaMonitorConfig  *self,
				  MetaMonitorManager *manager)
{
  MetaOutput *outputs;
  MetaConfiguration *default_config;
  unsigned n_outputs;

  outputs = meta_monitor_manager_get_outputs (manager, &n_outputs);
  default_config = make_default_config (outputs, n_outputs);

  if (default_config != NULL)
    apply_configuration (self, default_config, manager, FALSE);
  else
    {
      meta_warning ("Could not make default configuration for current output layout, leaving unconfigured\n");
      meta_monitor_config_update_current (self, manager);
    }
}

static void
init_config_from_output (MetaOutputConfig *config,
                         MetaOutput       *output)
{
  config->enabled = (output->crtc != NULL);

  if (!config->enabled)
    return;

  config->rect = output->crtc->rect;
  config->refresh_rate = output->crtc->current_mode->refresh_rate;
  config->transform = output->crtc->transform;
  config->is_primary = output->is_primary;
  config->is_presentation = output->is_presentation;
}

void
meta_monitor_config_update_current (MetaMonitorConfig  *self,
                                    MetaMonitorManager *manager)
{
  MetaOutput *outputs;
  unsigned n_outputs;
  MetaConfiguration *current;
  unsigned int i;

  outputs = meta_monitor_manager_get_outputs (manager, &n_outputs);

  current = g_slice_new (MetaConfiguration);
  current->n_outputs = n_outputs;
  current->outputs = g_new0 (MetaOutputConfig, n_outputs);
  current->keys = g_new0 (MetaOutputKey, n_outputs);

  for (i = 0; i < current->n_outputs; i++)
    {
      init_key_from_output (&current->keys[i], &outputs[i]);
      init_config_from_output (&current->outputs[i], &outputs[i]);
    }

  if (self->current && !self->current_is_stored)
    config_free (self->current);

  self->current = current;
  self->current_is_stored = FALSE;
}

typedef struct {
  MetaMonitorConfig *config;
  GString *buffer;
} SaveClosure;

static void
saved_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
  SaveClosure *closure = user_data;
  GError *error;
  gboolean ok;

  error = NULL;
  ok = g_file_replace_contents_finish (G_FILE (object), result, NULL, &error);
  if (!ok)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        meta_warning ("Saving monitor configuration failed: %s\n", error->message);

      g_error_free (error);
    }

  g_clear_object (&closure->config->save_cancellable);
  g_object_unref (closure->config);
  g_string_free (closure->buffer, TRUE);

  g_slice_free (SaveClosure, closure);
}

static void
meta_monitor_config_save (MetaMonitorConfig *self)
{
  static const char * const rotation_map[4] = {
    "normal",
    "left",
    "upside_down",
    "right"
  };
  SaveClosure *closure;
  GString *buffer;
  GHashTableIter iter;
  MetaConfiguration *config;
  unsigned int i;

  if (self->save_cancellable)
    {
      g_cancellable_cancel (self->save_cancellable);
      g_object_unref (self->save_cancellable);
      self->save_cancellable = NULL;
    }

  self->save_cancellable = g_cancellable_new ();

  buffer = g_string_new ("<monitors version=\"1\">\n");

  g_hash_table_iter_init (&iter, self->configs);
  while (g_hash_table_iter_next (&iter, (gpointer*) &config, NULL))
    {
      /* Note: we don't distinguish clone vs non-clone here, that's
         something for the UI (ie gnome-control-center) to handle,
         and our configurations are more complex anyway.
      */

      g_string_append (buffer,
                       "  <configuration>\n"
                       "    <clone>no</clone>\n");

      for (i = 0; i < config->n_outputs; i++)
        {
          MetaOutputKey *key = &config->keys[i];
          MetaOutputConfig *output = &config->outputs[i];

          g_string_append_printf (buffer,
                                  "    <output name=\"%s\">\n"
                                  "      <vendor>%s</vendor>\n"
                                  "      <product>%s</product>\n"
                                  "      <serial>%s</serial>\n",
                                  key->connector, key->vendor,
                                  key->product, key->serial);

          if (output->enabled)
            {
              char refresh_rate[G_ASCII_DTOSTR_BUF_SIZE];

              g_ascii_dtostr (refresh_rate, sizeof (refresh_rate), output->refresh_rate);
              g_string_append_printf (buffer,
                                      "      <width>%d</width>\n"
                                      "      <height>%d</height>\n"
                                      "      <rate>%s</rate>\n"
                                      "      <x>%d</x>\n"
                                      "      <y>%d</y>\n"
                                      "      <rotation>%s</rotation>\n"
                                      "      <reflect_x>%s</reflect_x>\n"
                                      "      <reflect_y>no</reflect_y>\n"
                                      "      <primary>%s</primary>\n"
                                      "      <presentation>%s</presentation>\n",
                                      output->rect.width,
                                      output->rect.height,
                                      refresh_rate,
                                      output->rect.x,
                                      output->rect.y,
                                      rotation_map[output->transform & 0x3],
                                      output->transform >= WL_OUTPUT_TRANSFORM_FLIPPED ? "yes" : "no",
                                      output->is_primary ? "yes" : "no",
                                      output->is_presentation ? "yes" : "no");
            }

          g_string_append (buffer, "    </output>\n");
        }

      g_string_append (buffer, "  </configuration>\n");
    }

  g_string_append (buffer, "</monitors>\n");

  closure = g_slice_new (SaveClosure);
  closure->config = g_object_ref (self);
  closure->buffer = buffer;

  g_file_replace_contents_async (self->file,
                                 buffer->str, buffer->len,
                                 NULL, /* etag */
                                 TRUE,
                                 G_FILE_CREATE_REPLACE_DESTINATION,
                                 self->save_cancellable,
                                 saved_cb, closure);
}

void
meta_monitor_config_make_persistent (MetaMonitorConfig *self)
{
  if (self->current_is_stored)
    return;

  self->current_is_stored = TRUE;
  g_hash_table_replace (self->configs, self->current, self->current);

  meta_monitor_config_save (self);
}
