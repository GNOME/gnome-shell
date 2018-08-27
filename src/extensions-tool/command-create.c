/* command-create.c
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
#include <gio/gunixinputstream.h>

#include "commands.h"
#include "common.h"

static char *
get_shell_version (GError **error)
{
  g_autoptr (GDBusProxy) proxy = NULL;
  g_autoptr (GVariant) variant = NULL;
  g_auto (GStrv) split_version = NULL;

  proxy = get_shell_proxy (error);
  if (proxy == NULL)
    return NULL;

  variant = g_dbus_proxy_get_cached_property (proxy, "ShellVersion");
  if (variant == NULL)
    return NULL;

  split_version = g_strsplit (g_variant_get_string (variant, NULL), ".", 3);
  if (g_ascii_strtoll (split_version[1], NULL, 10) % 2 == 0)
    g_clear_pointer (&split_version[2], g_free);

  return g_strjoinv (".", split_version);
}

static gboolean
create_metadata (GFile       *target_dir,
                 const char  *uuid,
                 const char  *name,
                 const char  *description,
                 GError     **error)
{
  g_autoptr (GFile) target = NULL;
  g_autoptr (GString) json = NULL;
  g_autofree char *version = NULL;

  version = get_shell_version (error);
  if (version == NULL)
    return FALSE;

  json = g_string_new ("{\n");

  g_string_append_printf (json, "  \"name\": \"%s\",\n", name);
  g_string_append_printf (json, "  \"description\": \"%s\",\n", description);
  g_string_append_printf (json, "  \"uuid\": \"%s\",\n", uuid);
  g_string_append_printf (json, "  \"shell-version\": [\n");
  g_string_append_printf (json, "    \"%s\"\n", version);
  g_string_append_printf (json, "  ]\n}\n");

  target = g_file_get_child (target_dir, "metadata.json");
  return g_file_replace_contents (target,
                                  json->str,
                                  json->len,
                                  NULL,
                                  FALSE,
                                  0,
                                  NULL,
                                  NULL,
                                  error);
}


#define TEMPLATE_PATH "/org/gnome/extensions-tool/template"
static gboolean
copy_extension_template (GFile *target_dir, GError **error)
{
  g_auto (GStrv) templates;
  char **s;

  templates = g_resources_enumerate_children (TEMPLATE_PATH, 0, NULL);
  for (s = templates; *s; s++)
    {
      g_autoptr (GFile) target = NULL;
      g_autoptr (GFile) source = NULL;
      g_autofree char *uri = NULL;

      uri = g_strdup_printf ("resource://%s/%s", TEMPLATE_PATH, *s);
      source = g_file_new_for_uri (uri);
      target = g_file_get_child (target_dir, *s);

      if (!g_file_copy (source, target, G_FILE_COPY_TARGET_DEFAULT_PERMS, NULL, NULL, NULL, error))
        return FALSE;
    }

  return TRUE;
}

static gboolean
launch_extension_source (GFile *dir, GError **error)
{
  g_autoptr (GFile) main_source = NULL;
  g_autoptr (GAppInfo) handler = NULL;
  GList l;

  main_source = g_file_get_child (dir, "extension.js");
  handler = g_file_query_default_handler (main_source, NULL, error);
  if (handler == NULL)
    return FALSE;

  l.data = main_source;
  l.next = l.prev = NULL;

  return g_app_info_launch (handler, &l, NULL, error);
}

static gboolean
create_extension (const char *uuid, const char *name, const char *description)
{
  g_autoptr (GFile) dir = NULL;
  g_autoptr (GError) error = NULL;

  dir = g_file_new_build_filename (g_get_user_data_dir (),
                                   "gnome-shell",
                                   "extensions",
                                   uuid,
                                   NULL);

  if (!g_file_make_directory_with_parents (dir, NULL, &error))
    {
      g_printerr ("%s\n", error->message);
      return FALSE;
    }

  if (!create_metadata (dir, uuid, name, description, &error))
    {
      g_printerr ("%s\n", error->message);
      return FALSE;
    }

  if (!copy_extension_template (dir, &error))
    {
      g_printerr ("%s\n", error->message);
      return FALSE;
    }

  if (!launch_extension_source (dir, &error))
    {
      g_printerr ("%s\n", error->message);
      return FALSE;
    }

  return TRUE;
}

static void
prompt_metadata (char **uuid, char **name, char **description)
{
  g_autoptr (GInputStream) stdin = NULL;
  g_autoptr (GDataInputStream) istream = NULL;

  stdin = g_unix_input_stream_new (0, FALSE);
  istream = g_data_input_stream_new (stdin);

  if (name != NULL)
    {
      char *line;

      g_print (
        _("Name should be a very short (ideally descriptive) string.\n"
          "Examples are: %s"),
        "“Click To Focus”, “Adblock”, “Shell Window Shrinker”\n");
      g_print ("%s: ", _("Name"));

      line = g_data_input_stream_read_line_utf8 (istream, NULL, NULL, NULL);
      *name = g_strdelimit (line, "\n", '\0');
    }

  if (description != NULL)
    {
      char *line;

      g_print (
        _("Description is a single-sentence explanation of what your extension does.\n"
          "Examples are: %s"),
        "“Make windows visible on click”, “Block advertisement popups”, “Animate windows shrinking on minimize”\n");
      g_print ("%s: ", _("Description"));

      line = g_data_input_stream_read_line_utf8 (istream, NULL, NULL, NULL);
      *description = g_strdelimit (line, "\n", '\0');
    }

  if (uuid != NULL)
    {
      char *line;

      g_print (
        _("UUID is a globally-unique identifier for your extension.\n"
          "This should be in the format of an email address (clicktofocus@janedoe.example.com)\n"));
      g_print ("UUID: ");

      line = g_data_input_stream_read_line_utf8 (istream, NULL, NULL, NULL);
      *uuid = g_strdelimit (line, "\n", '\0');
    }
}

int
handle_create (int argc, char *argv[], gboolean do_help)
{
  g_autoptr (GOptionContext) context = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *name = NULL;
  g_autofree char *description = NULL;
  g_autofree char *uuid = NULL;

  g_set_prgname ("gnome-extensions create");

  context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Create a new extension"));

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

  if (argc > 1)
    {
      show_help (context, _("Unknown arguments"));
      return 1;
    }

  prompt_metadata (&uuid, &name, &description);

  return create_extension (uuid, name, description) ? 0 : 2;
}
