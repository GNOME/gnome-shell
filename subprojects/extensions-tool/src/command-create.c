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

#define _GNU_SOURCE /* for strcasestr */
#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gunixinputstream.h>

#include "commands.h"
#include "common.h"
#include "config.h"

#define TEMPLATES_PATH "/org/gnome/extensions-tool/templates"
#define TEMPLATE_KEY "Path"
#define SORT_DATA "desktop-id"

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

  split_version = g_strsplit (g_variant_get_string (variant, NULL), ".", 2);
  return g_steal_pointer(&split_version[0]);
}

static GDesktopAppInfo *
load_app_info_from_resource (const char *uri)
{
  g_autoptr (GFile) file = NULL;
  g_autofree char *contents = NULL;
  g_autoptr (GKeyFile) keyfile = NULL;

  file = g_file_new_for_uri (uri);
  if (!g_file_load_contents (file, NULL, &contents, NULL, NULL, NULL))
    return NULL;

  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_data (keyfile, contents, -1, G_KEY_FILE_NONE, NULL))
    return NULL;

  return g_desktop_app_info_new_from_keyfile (keyfile);
}

static int
sort_func (gconstpointer a, gconstpointer b)
{
  GObject *info1 = *((GObject **) a);
  GObject *info2 = *((GObject **) b);
  const char *desktop1 = g_object_get_data (info1, SORT_DATA);
  const char *desktop2 = g_object_get_data (info2, SORT_DATA);

  return g_strcmp0 (desktop1, desktop2);
}

static GPtrArray *
get_templates (void)
{
  g_auto (GStrv) children = NULL;
  GPtrArray *templates = g_ptr_array_new_with_free_func (g_object_unref);
  char **s;

  children = g_resources_enumerate_children (TEMPLATES_PATH, 0, NULL);

  for (s = children; *s; s++)
    {
      g_autofree char *uri = NULL;
      GDesktopAppInfo *info;

      if (!g_str_has_suffix (*s, ".desktop"))
        continue;

      uri = g_strdup_printf ("resource://" TEMPLATES_PATH "/%s", *s);
      info = load_app_info_from_resource (uri);
      if (!info)
        continue;

      g_object_set_data_full (G_OBJECT (info), SORT_DATA, g_strdup (*s), g_free);
      g_ptr_array_add (templates, info);
    }

  g_ptr_array_sort (templates, sort_func);

  return templates;
}

static char *
escape_json_string (const char *string)
{
  GString *escaped = g_string_new (string);

  for (gsize i = 0; i < escaped->len; ++i)
    {
      if (escaped->str[i] == '"' || escaped->str[i] == '\\')
        {
          g_string_insert_c (escaped, i, '\\');
          ++i;
        }
    }

  return g_string_free (escaped, FALSE);
}

static gboolean
create_metadata (GFile       *target_dir,
                 const char  *uuid,
                 const char  *name,
                 const char  *description,
                 GError     **error)
{
  g_autofree char *uuid_escaped = NULL;
  g_autofree char *name_escaped = NULL;
  g_autofree char *desc_escaped = NULL;
  g_autoptr (GFile) target = NULL;
  g_autoptr (GString) json = NULL;
  g_autofree char *version = NULL;

  version = get_shell_version (error);
  if (version == NULL)
    return FALSE;

  uuid_escaped = escape_json_string (uuid);
  name_escaped = escape_json_string (name);
  desc_escaped = escape_json_string (description);

  json = g_string_new ("{\n");

  g_string_append_printf (json, "  \"name\": \"%s\",\n", name_escaped);
  g_string_append_printf (json, "  \"description\": \"%s\",\n", desc_escaped);
  g_string_append_printf (json, "  \"uuid\": \"%s\",\n", uuid_escaped);
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


static gboolean
copy_extension_template (const char *template, GFile *target_dir, GError **error)
{
  g_auto (GStrv) templates = NULL;
  g_autofree char *path = NULL;
  char **s;

  path = g_strdup_printf (TEMPLATES_PATH "/%s", template);
  templates = g_resources_enumerate_children (path, 0, NULL);

  if (templates == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "No template %s", template);
      return FALSE;
    }

  for (s = templates; *s; s++)
    {
      g_autoptr (GFile) target = NULL;
      g_autoptr (GFile) source = NULL;
      g_autofree char *uri = NULL;

      uri = g_strdup_printf ("resource://%s/%s", path, *s);
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
  handler = g_file_query_default_handler (main_source, NULL, NULL);

  /* Translators: a file path to an extension directory */
  g_print (_("The new extension was successfully created in %s.\n"),
            g_file_peek_path (dir));

  if (handler == NULL)
      return TRUE;

  l.data = main_source;
  l.next = l.prev = NULL;

  return g_app_info_launch (handler, &l, NULL, error);
}

static gboolean
create_extension (const char *uuid, const char *name, const char *description, const char *template)
{
  g_autoptr (GFile) dir = NULL;
  g_autoptr (GError) error = NULL;

  if (template == NULL)
    template = "plain";

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

  if (!copy_extension_template (template, dir, &error))
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
prompt_metadata (char **uuid, char **name, char **description, char **template)
{
  g_autoptr (GInputStream) stdin = NULL;
  g_autoptr (GDataInputStream) istream = NULL;

  if ((uuid == NULL || *uuid != NULL) &&
      (name == NULL || *name != NULL) &&
      (description == NULL || *description != NULL) &&
      (template == NULL || *template != NULL))
    return;

  stdin = g_unix_input_stream_new (0, FALSE);
  istream = g_data_input_stream_new (stdin);

  if (name != NULL && *name == NULL)
    {
      char *line = NULL;

      g_print (
        _("Name should be a very short (ideally descriptive) string.\n"
          "Examples are: %s"),
        "“Click To Focus”, “Adblock”, “Shell Window Shrinker”\n");

      while (line == NULL)
        {
          g_print ("%s: ", _("Name"));

          line = g_data_input_stream_read_line_utf8 (istream, NULL, NULL, NULL);
        }
      *name = g_strdelimit (line, "\n", '\0');

      g_print ("\n");
    }

  if (description != NULL && *description == NULL)
    {
      char *line = NULL;

      g_print (
        _("Description is a single-sentence explanation of what your extension does.\n"
          "Examples are: %s"),
        "“Make windows visible on click”, “Block advertisement popups”, “Animate windows shrinking on minimize”\n");

      while (line == NULL)
        {
          g_print ("%s: ", _("Description"));

          line = g_data_input_stream_read_line_utf8 (istream, NULL, NULL, NULL);
        }
      *description = g_strdelimit (line, "\n", '\0');

      g_print ("\n");
    }

  if (uuid != NULL && *uuid == NULL)
    {
      char *line = NULL;

      g_print (
        _("UUID is a globally-unique identifier for your extension.\n"
          "This should be in the format of an email address (clicktofocus@janedoe.example.com)\n"));

      while (line == NULL)
        {
          g_print ("UUID: ");

          line = g_data_input_stream_read_line_utf8 (istream, NULL, NULL, NULL);
        }
      *uuid = g_strdelimit (line, "\n", '\0');

      g_print ("\n");
    }

  if (template != NULL && *template == NULL)
    {
      g_autoptr (GPtrArray) templates = get_templates ();

      if (templates->len == 1)
        {
          GDesktopAppInfo *info = g_ptr_array_index (templates, 0);
          *template = g_desktop_app_info_get_string (info, TEMPLATE_KEY);
        }
      else
        {
          int i;

          g_print (_("Choose one of the available templates:\n"));
          for (i = 0; i < templates->len; i++)
            {
              GAppInfo *info = g_ptr_array_index (templates, i);
              g_print ("%d) %-10s  –  %s\n",
                       i + 1,
                       g_app_info_get_name (info),
                       g_app_info_get_description (info));
            }

          while (*template == NULL)
            {
              g_autofree char *line = NULL;

              g_print ("%s [1-%d]: ", _("Template"), templates->len);

              line = g_data_input_stream_read_line_utf8 (istream, NULL, NULL, NULL);

              if (line == NULL)
                continue;

              if (g_ascii_isdigit (*line))
                {
                  long i = strtol (line, NULL, 10);

                  if (i > 0 && i <= templates->len)
                    {
                      GDesktopAppInfo *info;

                      info = g_ptr_array_index (templates, i - 1);
                      *template =
                        g_desktop_app_info_get_string (info, TEMPLATE_KEY);
                    }
                }
              else
                {
                  for (i = 0; i < templates->len; i++)
                    {
                      GDesktopAppInfo *info = g_ptr_array_index (templates, i);
                      g_autofree char *cur_template = NULL;

                      cur_template =
                        g_desktop_app_info_get_string (info, TEMPLATE_KEY);

                      if (strcasestr (cur_template, line) != NULL)
                        *template = g_steal_pointer (&cur_template);
                    }
                }
            }
          g_print ("\n");
        }
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
  g_autofree char *template = NULL;
  gboolean interactive = FALSE;
  gboolean list_templates = FALSE;
  GOptionEntry entries[] = {
    { .long_name = "uuid",
      .arg = G_OPTION_ARG_STRING, .arg_data = &uuid,
      .arg_description = "UUID",
      .description = _("The unique identifier of the new extension") },
    { .long_name = "name",
      .arg = G_OPTION_ARG_STRING, .arg_data = &name,
      .arg_description = _("NAME"),
      .description = _("The user-visible name of the new extension") },
    { .long_name = "description",
      .arg_description = _("DESCRIPTION"),
      .arg = G_OPTION_ARG_STRING, .arg_data = &description,
      .description = _("A short description of what the extension does") },
    { .long_name = "template",
      .arg = G_OPTION_ARG_STRING, .arg_data = &template,
      .arg_description = _("TEMPLATE"),
      .description = _("The template to use for the new extension") },
    { .long_name = "list-templates",
      .arg = G_OPTION_ARG_NONE, .arg_data = &list_templates,
      .flags = G_OPTION_FLAG_HIDDEN },
    { .long_name = "interactive", .short_name = 'i',
      .arg = G_OPTION_ARG_NONE, .arg_data = &interactive,
      .description = _("Enter extension information interactively") },
    { NULL }
  };

  g_set_prgname ("gnome-extensions create");

  context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Create a new extension"));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, get_option_group ());

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

  if (list_templates)
    {
      g_autoptr (GPtrArray) templates = get_templates ();
      int i;

      for (i = 0; i < templates->len; i++)
        {
          GDesktopAppInfo *info = g_ptr_array_index (templates, i);
          g_autofree char *template = NULL;

          template = g_desktop_app_info_get_string (info, TEMPLATE_KEY);
          g_print ("%s\n", template);
        }
      return 0;
    }

  if (interactive)
    prompt_metadata (&uuid, &name, &description, &template);

  if (uuid == NULL || name == NULL || description == NULL)
    {
      show_help (context, _("UUID, name and description are required"));
      return 1;
    }

  return create_extension (uuid, name, description, template) ? 0 : 2;
}
