/* command-install.c
 *
 * Copyright 2018 Florian Müllner <fmuellner@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <gnome-autoar/gnome-autoar.h>
#include <json-glib/json-glib.h>

#include "commands.h"
#include "common.h"
#include "config.h"

static JsonObject *
load_metadata (GFile   *dir,
               GError **error)
{
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (GInputStream) stream = NULL;
  g_autoptr (GFile) file = NULL;

  file = g_file_get_child (dir, "metadata.json");
  stream = G_INPUT_STREAM (g_file_read (file, NULL, error));
  if (stream == NULL)
    return NULL;

  parser = json_parser_new_immutable ();
  if (!json_parser_load_from_stream (parser, stream, NULL, error))
    return NULL;

  return json_node_dup_object (json_parser_get_root (parser));
}

static void
on_error (AutoarExtractor *extractor,
          GError          *error,
          gpointer         data)
{
  *((GError **)data) = g_error_copy (error);
}

static GFile *
on_decide_destination (AutoarExtractor *extractor,
                       GFile           *dest,
                       GList           *files,
                       gpointer         data)
{
  g_autofree char *dest_path = NULL;
  GFile *new_dest;
  int copy = 1;

  dest_path = g_file_get_path (dest);
  new_dest = g_object_ref (dest);

  while (g_file_query_exists (new_dest, NULL))
    {
      g_autofree char *new_path = g_strdup_printf ("%s (%d)", dest_path, copy);

      g_object_unref (new_dest);
      new_dest = g_file_new_for_path (new_path);

      copy++;
    }

  *((GFile **)data) = g_object_ref (new_dest);

  return new_dest;
}

static int
install_extension (const char *bundle,
                   gboolean    force)
{
  g_autoptr (AutoarExtractor) extractor = NULL;
  g_autoptr (JsonObject) metadata = NULL;
  g_autoptr (GFile) cachedir = NULL;
  g_autoptr (GFile) tmpdir = NULL;
  g_autoptr (GFile) src = NULL;
  g_autoptr (GFile) dst = NULL;
  g_autoptr (GFile) dstdir = NULL;
  g_autoptr (GFile) schemadir = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *cwd = NULL;
  const char *uuid;

  cwd = g_get_current_dir ();
  src = g_file_new_for_commandline_arg_and_cwd (bundle, cwd);
  cachedir = g_file_new_for_path (g_get_user_cache_dir ());

  extractor = autoar_extractor_new (src, cachedir);

  g_signal_connect (extractor, "error", G_CALLBACK (on_error), &error);
  g_signal_connect (extractor, "decide-destination", G_CALLBACK (on_decide_destination), &tmpdir);

  autoar_extractor_start (extractor, NULL);

  if (error != NULL)
    goto err;

  metadata = load_metadata (tmpdir, &error);
  if (metadata == NULL)
    goto err;

  dstdir = g_file_new_build_filename (g_get_user_data_dir (),
                                      "gnome-shell", "extensions", NULL);

  if (!g_file_make_directory_with_parents (dstdir, NULL, &error))
    {
      if (error->code == G_IO_ERROR_EXISTS)
        g_clear_error (&error);
      else
        goto err;
    }

  uuid = json_object_get_string_member (metadata, "uuid");
  dst = g_file_get_child (dstdir, uuid);

  if (g_file_query_exists (dst, NULL))
    {
      if (!force)
        {
          g_set_error (&error, G_IO_ERROR, G_IO_ERROR_EXISTS,
                       "%s exists and --force was not specified", uuid);
          goto err;
        }
      else if (!file_delete_recursively (dst, &error))
        {
          goto err;
        }
    }

  if (!g_file_move (tmpdir, dst, G_FILE_COPY_NONE, NULL, NULL, NULL, &error))
    goto err;

  schemadir = g_file_get_child (dst, "schemas");

  if (g_file_query_exists (schemadir, NULL))
    {
      g_autoptr (GSubprocess) proc = NULL;
      g_autofree char *schemapath = NULL;

      schemapath = g_file_get_path (schemadir);
      proc = g_subprocess_new (G_SUBPROCESS_FLAGS_STDERR_SILENCE, &error,
                               "glib-compile-schemas", "--strict", schemapath,
                               NULL);

      if (!g_subprocess_wait_check (proc, NULL, &error))
        goto err;
    }

  return 0;

err:
  if (error != NULL)
    g_printerr ("%s\n", error->message);

  if (tmpdir != NULL)
    file_delete_recursively (tmpdir, NULL);

  return 2;
}

int
handle_install (int argc, char *argv[], gboolean do_help)
{
  g_autoptr (GOptionContext) context = NULL;
  g_autoptr (GError) error = NULL;
  g_auto (GStrv) filenames = NULL;
  gboolean force = FALSE;
  GOptionEntry entries[] = {
    { .long_name = "force", .short_name = 'f',
      .arg = G_OPTION_ARG_NONE, .arg_data = &force,
      .description = _("Overwrite an existing extension") },
    { .long_name = G_OPTION_REMAINING,
      .arg_description =_("EXTENSION_BUNDLE"),
      .arg = G_OPTION_ARG_FILENAME_ARRAY, .arg_data = &filenames },
    { NULL }
  };

  g_set_prgname ("gnome-extensions install");

  context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Install an extension bundle"));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, get_option_group());

  if (do_help)
    {
      show_help (context, NULL);
      return 0;
    }

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      show_help (context, error->message);
      return 1;
    }

  if (filenames == NULL)
    {
      show_help (context, _("No extension bundle specified"));
      return 1;
    }

  if (g_strv_length (filenames) > 1)
    {
      show_help (context, _("More than one extension bundle specified"));
      return 1;
    }

  return install_extension (*filenames, force);
}
