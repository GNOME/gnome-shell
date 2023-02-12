/* command-pack.c
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

typedef struct _ExtensionPack {
  GHashTable *files;
  JsonObject *metadata;
  GFile *tmpdir;
  char *srcdir;
} ExtensionPack;

static void extension_pack_free (ExtensionPack *);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ExtensionPack, extension_pack_free);

static ExtensionPack *
extension_pack_new (const char *srcdir)
{
  ExtensionPack *pack = g_new0 (ExtensionPack, 1);
  pack->srcdir = g_strdup (srcdir);
  pack->files = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       g_free, g_object_unref);
  return pack;
}

static void
extension_pack_free (ExtensionPack *pack)
{
  if (pack->tmpdir)
    file_delete_recursively (pack->tmpdir, NULL);

  g_clear_pointer (&pack->files, g_hash_table_destroy);
  g_clear_pointer (&pack->metadata, json_object_unref);
  g_clear_pointer (&pack->srcdir, g_free);
  g_clear_object (&pack->tmpdir);
  g_free (pack);
}

static void
extension_pack_add_source (ExtensionPack *pack,
                           const char    *filename)
{
  g_autoptr (GFile) file = NULL;
  file = g_file_new_for_commandline_arg_and_cwd (filename, pack->srcdir);
  if (g_file_query_exists (file, NULL))
    g_hash_table_insert (pack->files,
                         g_path_get_basename (filename), g_steal_pointer (&file));
}

static gboolean
extension_pack_check_required_file (ExtensionPack  *pack,
                                    const char     *filename,
                                    GError        **error)
{
  if (!g_hash_table_contains (pack->files, filename))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "Missing %s in extension pack", filename);
      return FALSE;
    }
  return TRUE;
}

static gboolean
ensure_tmpdir (ExtensionPack  *pack,
               GError        **error)
{
  g_autofree char *path = NULL;

  if (pack->tmpdir != NULL)
    return TRUE;

  path = g_dir_make_tmp ("gnome-extensions.XXXXXX", error);
  if (path != NULL)
    pack->tmpdir = g_file_new_for_path (path);

  return pack->tmpdir != NULL;
}

static gboolean
ensure_metadata (ExtensionPack  *pack,
                 GError        **error)
{
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (GInputStream) stream = NULL;
  GFile *file = NULL;

  if (pack->metadata != NULL)
    return TRUE;

  if (!extension_pack_check_required_file (pack, "metadata.json", error))
    return FALSE;

  file = g_hash_table_lookup (pack->files, "metadata.json");
  stream = G_INPUT_STREAM (g_file_read (file, NULL, error));

  if (stream == NULL)
    return FALSE;

  parser = json_parser_new_immutable ();

  if (!json_parser_load_from_stream (parser, stream, NULL, error))
    return FALSE;

  pack->metadata = json_node_dup_object (json_parser_get_root (parser));
  return TRUE;
}

static gboolean
extension_pack_add_schemas (ExtensionPack  *pack,
                            char          **schemas,
                            GError        **error)
{
  g_autoptr (GSubprocess) proc = NULL;
  g_autoptr (GFile) dstdir = NULL;
  g_autofree char *dstpath = NULL;
  char **s;

  if (!ensure_tmpdir (pack, error))
    return FALSE;

  dstdir = g_file_get_child (pack->tmpdir, "schemas");
  if (!g_file_make_directory (dstdir, NULL, error))
    return FALSE;

  for (s = schemas; s && *s; s++)
    {
      g_autoptr (GFile) src = NULL;
      g_autoptr (GFile) dst = NULL;
      g_autofree char *basename = NULL;

      src = g_file_new_for_commandline_arg_and_cwd (*s, pack->srcdir);

      basename = g_file_get_basename (src);
      dst = g_file_get_child (dstdir, basename);

      if (!g_file_copy (src, dst, G_FILE_COPY_NONE, NULL, NULL, NULL, error))
        return FALSE;
    }

#if MAJOR_VERSION >= 46
#error "Outdated compatibility code, please remove"
#else
  dstpath = g_file_get_path (dstdir);
  proc = g_subprocess_new (G_SUBPROCESS_FLAGS_STDERR_SILENCE, error,
                           "glib-compile-schemas", "--strict", dstpath, NULL);

  if (!g_subprocess_wait_check (proc, NULL, error))
    return FALSE;
#endif

  g_hash_table_insert (pack->files,
                       g_strdup ("schemas"), g_steal_pointer (&dstdir));
  return TRUE;
}

static gboolean
extension_pack_add_locales (ExtensionPack  *pack,
                            const char     *podir,
                            const char     *gettext_domain,
                            GError        **error)
{
  g_autoptr (GFile) dstdir = NULL;
  g_autoptr (GFile) srcdir = NULL;
  g_autoptr (GFileEnumerator) file_enum = NULL;
  g_autofree char *dstpath = NULL;
  g_autofree char *moname = NULL;
  GFile *child;
  GFileInfo *info;

  if (!ensure_tmpdir (pack, error))
    return FALSE;

  dstdir = g_file_get_child (pack->tmpdir, "locale");
  if (!g_file_make_directory (dstdir, NULL, error))
    return FALSE;

  srcdir = g_file_new_for_commandline_arg_and_cwd (podir, pack->srcdir);
  file_enum = g_file_enumerate_children (srcdir,
                                         G_FILE_ATTRIBUTE_STANDARD_NAME,
                                         G_FILE_QUERY_INFO_NONE,
                                         NULL,
                                         error);
  if (file_enum == NULL)
    return FALSE;

  if (gettext_domain == NULL)
    {
      if (!ensure_metadata (pack, error))
        return FALSE;

      if (json_object_has_member (pack->metadata, "gettext-domain"))
        gettext_domain = json_object_get_string_member (pack->metadata,
                                                        "gettext-domain");
      else
        gettext_domain = json_object_get_string_member (pack->metadata,
                                                        "uuid");
    }

  dstpath = g_file_get_path (dstdir);
  moname = g_strdup_printf ("%s.mo", gettext_domain);

  while (TRUE)
    {
      g_autoptr (GSubprocess) proc = NULL;
      g_autoptr (GFile) modir = NULL;
      g_autofree char *popath = NULL;
      g_autofree char *mopath = NULL;
      g_autofree char *lang = NULL;
      const char *name;

      if (!g_file_enumerator_iterate (file_enum, &info, &child, NULL, error))
        return FALSE;

      if (info == NULL)
        break;

      name = g_file_info_get_name (info);
      if (!g_str_has_suffix (name, ".po"))
        continue;

      lang = g_strndup (name, strlen (name) - 3 /* strlen (".po") */);
      modir = g_file_new_build_filename (dstpath, lang, "LC_MESSAGES", NULL);
      if (!g_file_make_directory_with_parents (modir, NULL, error))
        return FALSE;

      mopath = g_build_filename (dstpath, lang, "LC_MESSAGES", moname, NULL);
      popath = g_file_get_path (child);

      proc = g_subprocess_new (G_SUBPROCESS_FLAGS_STDERR_SILENCE, error,
                               "msgfmt", "-o", mopath, popath, NULL);

      if (!g_subprocess_wait_check (proc, NULL, error))
        return FALSE;
    }

  g_hash_table_insert (pack->files,
                       g_strdup ("locale"), g_steal_pointer (&dstdir));
  return TRUE;
}

static void
on_error (AutoarCompressor *compressor,
          GError           *error,
          gpointer          data)
{
  *((GError **)data) = g_error_copy (error);
}

static gboolean
extension_pack_compress (ExtensionPack  *pack,
                         const char     *outdir,
                         gboolean        overwrite,
                         GError        **error)
{
  g_autoptr (AutoarCompressor) compressor = NULL;
  g_autoptr (GError) err = NULL;
  g_autoptr (GFile) outfile = NULL;
  g_autofree char *name = NULL;
  const char *uuid;

  if (!ensure_metadata (pack, error))
    return FALSE;

  uuid = json_object_get_string_member (pack->metadata, "uuid");
  name = g_strdup_printf ("%s.shell-extension.zip", uuid);
  outfile = g_file_new_for_commandline_arg_and_cwd (name, outdir);

  if (g_file_query_exists (outfile, NULL))
    {
      if (!overwrite)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_EXISTS,
                       "%s exists and --force was not specified", name);
          return FALSE;
        }
      else if (!g_file_delete (outfile, NULL, error))
        {
          return FALSE;
        }
    }

  compressor = autoar_compressor_new (g_hash_table_get_values (pack->files),
                                      outfile,
                                      AUTOAR_FORMAT_ZIP,
                                      AUTOAR_FILTER_NONE,
                                      FALSE);
  autoar_compressor_set_output_is_dest (compressor, TRUE);

  g_signal_connect (compressor, "error", G_CALLBACK (on_error), err);

  autoar_compressor_start (compressor, NULL);

  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
}

static char **
find_schemas (const char  *basepath,
              GError     **error)
{
  g_autoptr (GFile) basedir = NULL;
  g_autoptr (GFile) schemadir = NULL;
  g_autoptr (GFileEnumerator) file_enum = NULL;
  g_autoptr (GPtrArray) schemas = NULL;
  GFile *child;
  GFileInfo *info;

  basedir = g_file_new_for_path (basepath);
  schemadir = g_file_get_child (basedir, "schemas");
  file_enum = g_file_enumerate_children (schemadir,
                                         G_FILE_ATTRIBUTE_STANDARD_NAME,
                                         G_FILE_QUERY_INFO_NONE,
                                         NULL, error);

  if (error && *error)
    {
      if (g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) ||
          g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY))
        g_clear_error (error);
      return NULL;
    }

  schemas = g_ptr_array_new_with_free_func (g_free);

  while (TRUE)
    {
      if (!g_file_enumerator_iterate (file_enum, &info, &child, NULL, error))
        return NULL;

      if (child == NULL)
        break;

      if (!g_str_has_suffix (g_file_info_get_name (info), ".gschema.xml"))
        continue;

      g_ptr_array_add (schemas, g_file_get_relative_path (basedir, child));
    }
  g_ptr_array_add (schemas, NULL);

  return (char **)g_ptr_array_free (g_ptr_array_ref (schemas), FALSE);
}

static int
pack_extension (char      *srcdir,
                char      *dstdir,
                gboolean   force,
                char     **extra_sources,
                char     **schemas,
                char      *podir,
                char      *gettext_domain)
{
  g_autoptr (ExtensionPack) pack = NULL;
  g_autoptr (GError) error = NULL;
  char **s;

  pack = extension_pack_new (srcdir);
  extension_pack_add_source (pack, "extension.js");
  extension_pack_add_source (pack, "metadata.json");
  extension_pack_add_source (pack, "stylesheet.css");
  extension_pack_add_source (pack, "prefs.js");

  for (s = extra_sources; s && *s; s++)
    extension_pack_add_source (pack, *s);

  if (!extension_pack_check_required_file (pack, "extension.js", &error))
    goto err;

  if (!extension_pack_check_required_file (pack, "metadata.json", &error))
    goto err;

  if (schemas == NULL)
    schemas = find_schemas (srcdir, &error);

  if (schemas != NULL)
    extension_pack_add_schemas (pack, schemas, &error);

  if (error)
    goto err;

  if (podir == NULL)
    {
      g_autoptr (GFile) dir = NULL;

      dir = g_file_new_for_commandline_arg_and_cwd ("po", srcdir);
      if (g_file_query_exists (dir, NULL))
        podir = (char *)"po";
    }

  if (podir != NULL)
    extension_pack_add_locales (pack, podir, gettext_domain, &error);

  if (error)
    goto err;

  extension_pack_compress (pack, dstdir, force, &error);

err:
  if (error)
    {
      g_printerr ("%s\n", error->message);
      return 2;
    }

  return 0;
}

int
handle_pack (int argc, char *argv[], gboolean do_help)
{
  g_autoptr (GOptionContext) context = NULL;
  g_autoptr (GError) error = NULL;
  g_auto(GStrv) extra_sources = NULL;
  g_auto(GStrv) schemas = NULL;
  g_auto(GStrv) srcdirs = NULL;
  g_autofree char *podir = NULL;
  g_autofree char *srcdir = NULL;
  g_autofree char *dstdir = NULL;
  g_autofree char *gettext_domain = NULL;
  gboolean force = FALSE;
  GOptionEntry entries[] = {
    { .long_name = "extra-source",
      .arg = G_OPTION_ARG_FILENAME_ARRAY, .arg_data = &extra_sources,
      .arg_description = _("FILE"),
      .description = _("Additional source to include in the bundle") },
    { .long_name = "schema",
      .arg = G_OPTION_ARG_FILENAME_ARRAY, .arg_data = &schemas,
      .arg_description = _("SCHEMA"),
      .description = _("A GSettings schema that should be included") },
    { .long_name = "podir",
      .arg_description = _("DIRECTORY"),
      .arg = G_OPTION_ARG_FILENAME, .arg_data = &podir,
      .description = _("The directory where translations are found") },
    { .long_name = "gettext-domain",
      .arg_description = _("DOMAIN"),
      .arg = G_OPTION_ARG_STRING, .arg_data = &gettext_domain,
      .description = _("The gettext domain to use for translations") },
    { .long_name = "force", .short_name = 'f',
      .arg = G_OPTION_ARG_NONE, .arg_data = &force,
      .description = _("Overwrite an existing pack") },
    { .long_name = "out-dir", .short_name = 'o',
      .arg_description = _("DIRECTORY"),
      .arg = G_OPTION_ARG_FILENAME, .arg_data = &dstdir,
      .description = _("The directory where the pack should be created") },
    { .long_name = G_OPTION_REMAINING,
      .arg_description =_("SOURCE_DIRECTORY"),
      .arg = G_OPTION_ARG_FILENAME_ARRAY, .arg_data = &srcdirs },
    { NULL }
  };

  g_set_prgname ("gnome-extensions pack");

  context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Create an extension bundle"));
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

  if (srcdirs)
    {
      if (g_strv_length (srcdirs) > 1)
        {
          show_help (context, _("More than one source directory specified"));
          return 1;
        }
      srcdir = g_strdup (*srcdirs);
    }
  else
    {
      srcdir = g_get_current_dir ();
    }

  if (dstdir == NULL)
    dstdir = g_get_current_dir ();

  return pack_extension (srcdir, dstdir, force,
                         extra_sources, schemas, podir, gettext_domain);
}
