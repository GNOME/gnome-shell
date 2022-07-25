/* main.c
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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "config.h"
#include "commands.h"
#include "common.h"

static const char *
extension_state_to_string (ExtensionState state)
{
  switch (state)
    {
    case STATE_ENABLED:
      return "ENABLED";
    case STATE_DISABLED:
      return "DISABLED";
    case STATE_ERROR:
      return "ERROR";
    case STATE_OUT_OF_DATE:
      return "OUT OF DATE";
    case STATE_DOWNLOADING:
      return "DOWNLOADING";
    case STATE_INITIALIZED:
      return "INITIALIZED";
    case STATE_DISABLING:
      return "DISABLING";
    case STATE_ENABLING:
      return "ENABLING";
    case STATE_UNINSTALLED:
      return "UNINSTALLED";
    }
  return "UNKNOWN";
}

static void
print_nothing (const char *message)
{
}

static gboolean
quiet_cb (const gchar  *option_name,
          const gchar  *value,
          gpointer      data,
          GError      **error)
{
  g_set_printerr_handler (print_nothing);
  return TRUE;
}

GOptionGroup *
get_option_group ()
{
  GOptionEntry entries[] = {
    { .long_name = "quiet", .short_name = 'q',
      .description = _("Do not print error messages"),
      .arg = G_OPTION_ARG_CALLBACK, .arg_data = &quiet_cb,
      .flags = G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_IN_MAIN },
    { NULL }
  };
  GOptionGroup *group;

  group = g_option_group_new ("Common", "common options", "common options", NULL, NULL);
  g_option_group_add_entries (group, entries);

  return group;
}

void
show_help (GOptionContext *context, const char *message)
{
  g_autofree char *help = NULL;

  if (message)
    g_printerr ("gnome-extensions: %s\n\n", message);

  help = g_option_context_get_help (context, TRUE, NULL);
  g_printerr ("%s", help);
}

GDBusProxy *
get_shell_proxy (GError **error)
{
  return g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        NULL,
                                        "org.gnome.Shell.Extensions",
                                        "/org/gnome/Shell/Extensions",
                                        "org.gnome.Shell.Extensions",
                                        NULL,
                                        error);
}

GSettings *
get_shell_settings (void)
{
  g_autoptr (GSettingsSchema) schema = NULL;
  GSettingsSchemaSource *schema_source;

  schema_source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (schema_source,
                                            "org.gnome.shell",
                                            TRUE);

  if (schema == NULL)
    return NULL;

  return g_settings_new_full (schema, NULL, NULL);
}

GVariant *
get_extension_property (GDBusProxy *proxy,
                        const char *uuid,
                        const char *property)
{
  g_autoptr (GVariant) response = NULL;
  g_autoptr (GVariant) asv = NULL;
  g_autoptr (GVariantDict) info = NULL;
  g_autoptr (GError) error = NULL;

  response = g_dbus_proxy_call_sync (proxy,
                                     "GetExtensionInfo",
                                     g_variant_new ("(s)", uuid),
                                     0,
                                     -1,
                                     NULL,
                                     &error);
  if (response == NULL)
    {
      g_printerr (_("Failed to connect to GNOME Shell\n"));
      return NULL;
    }

  asv = g_variant_get_child_value (response, 0);
  info = g_variant_dict_new (asv);

  if (!g_variant_dict_contains (info, "uuid"))
    {
      g_printerr (_("Extension “%s” doesn't exist\n"), uuid);
      return NULL;
    }

  return g_variant_dict_lookup_value (info, property, NULL);
}

gboolean
settings_list_add (GSettings  *settings,
                   const char *key,
                   const char *value)
{
  g_auto(GStrv) list = NULL;
  g_auto(GStrv) new_value = NULL;
  guint n_values;
  int i;

  if (!g_settings_is_writable (settings, key))
    return FALSE;

  list = g_settings_get_strv (settings, key);

  if (g_strv_contains ((const char **)list, value))
    return TRUE;

  n_values = g_strv_length (list);
  new_value = g_new0 (char *, n_values + 2);
  for (i = 0; i < n_values; i++)
    new_value[i] = g_strdup (list[i]);
  new_value[i] = g_strdup (value);

  g_settings_set_strv (settings, key, (const char **)new_value);
  g_settings_sync ();

  return TRUE;
}

gboolean
settings_list_remove (GSettings  *settings,
                      const char *key,
                      const char *value)
{
  g_auto(GStrv) list = NULL;
  g_auto(GStrv) new_value = NULL;
  const char **s;
  guint n_values;
  int i;

  if (!g_settings_is_writable (settings, key))
    return FALSE;

  list = g_settings_get_strv (settings, key);

  if (!g_strv_contains ((const char **)list, value))
    return TRUE;

  n_values = g_strv_length (list);
  new_value = g_new0 (char *, n_values);
  i = 0;
  for (s = (const char **)list; *s != NULL; s++)
    if (!g_str_equal (*s, value))
      new_value[i++] = g_strdup (*s);

  g_settings_set_strv (settings, key, (const char **)new_value);
  g_settings_sync ();

  return TRUE;
}

void
print_extension_info (GVariantDict  *info,
                      DisplayFormat  format)
{
  const char *uuid, *name, *desc, *path, *url, *author;
  double state, version;

  g_variant_dict_lookup (info, "uuid", "&s", &uuid);
  g_print ("%s\n", uuid);

  if (format == DISPLAY_ONELINE)
    return;

  g_variant_dict_lookup (info, "name", "&s", &name);
  g_print ("  %s: %s\n", _("Name"), name);

  g_variant_dict_lookup (info, "description", "&s", &desc);
  g_print ("  %s: %s\n", _("Description"), desc);

  g_variant_dict_lookup (info, "path", "&s", &path);
  g_print ("  %s: %s\n", _("Path"), path);

  if (g_variant_dict_lookup (info, "url", "&s", &url))
    g_print ("  %s: %s\n", _("URL"), url);

  if (g_variant_dict_lookup (info, "original-author", "&s", &author))
    g_print ("  %s: %s\n", _("Original author"), author);

  if (g_variant_dict_lookup (info, "version", "d", &version))
    g_print ("  %s: %.0f\n", _("Version"), version);

  g_variant_dict_lookup (info, "state", "d", &state);
  g_print ("  %s: %s\n", _("State"), extension_state_to_string (state));
}

gboolean
file_delete_recursively (GFile   *file,
                         GError **error)
{
  g_autoptr (GFileEnumerator) file_enum = NULL;
  GFile *child;

  file_enum = g_file_enumerate_children (file,
                                         G_FILE_ATTRIBUTE_STANDARD_NAME,
                                         G_FILE_QUERY_INFO_NONE,
                                         NULL,
                                         NULL);
  if (file_enum)
    while (TRUE)
      {
        if (!g_file_enumerator_iterate (file_enum, NULL, &child, NULL, error))
          return FALSE;

        if (child == NULL)
          break;

        if (!file_delete_recursively (child, error))
          return FALSE;
      }

  return g_file_delete (file, NULL, error);
}


static int
handle_version (int argc, char *argv[], gboolean do_help)
{
  if (do_help || argc > 1)
    {
      if (!do_help)
        g_printerr ("gnome-extensions: %s\n\n", _("“version” takes no arguments"));

      g_printerr ("%s\n", _("Usage:"));
      g_printerr ("  gnome-extensions version\n");
      g_printerr ("\n");
      g_printerr ("%s\n", _("Print version information and exit."));

      return do_help ? 0 : 2;
    }

  g_print ("%s\n", VERSION);

  return 0;
}

static void
usage (void)
{
  g_autofree char *help_command = NULL;

  help_command = g_strdup_printf ("gnome-extensions help %s", _("COMMAND"));

  g_printerr ("%s\n", _("Usage:"));
  g_printerr ("  gnome-extensions %s %s\n", _("COMMAND"), _("[ARGS…]"));
  g_printerr ("\n");
  g_printerr ("%s\n", _("Commands:"));
  g_printerr ("  help      %s\n", _("Print help"));
  g_printerr ("  version   %s\n", _("Print version"));
  g_printerr ("  enable    %s\n", _("Enable extension"));
  g_printerr ("  disable   %s\n", _("Disable extension"));
  g_printerr ("  reset     %s\n", _("Reset extension"));
  g_printerr ("  uninstall %s\n", _("Uninstall extension"));
  g_printerr ("  list      %s\n", _("List extensions"));
  g_printerr ("  info      %s\n", _("Show extension info"));
  g_printerr ("  show      %s\n", _("Show extension info"));
  g_printerr ("  prefs     %s\n", _("Open extension preferences"));
  g_printerr ("  create    %s\n", _("Create extension"));
  g_printerr ("  pack      %s\n", _("Package extension"));
  g_printerr ("  install   %s\n", _("Install extension bundle"));
  g_printerr ("\n");
  g_printerr (_("Use “%s” to get detailed help.\n"), help_command);
}

int
main (int argc, char *argv[])
{
  const char *command;
  gboolean do_help = FALSE;

  setlocale (LC_ALL, "");
  textdomain (GETTEXT_PACKAGE);
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);

#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  if (argc < 2)
    {
      usage ();
      return 1;
    }

  command = argv[1];
  argc--;
  argv++;

  if (g_str_equal (command, "help"))
    {
      if (argc == 1)
        {
          usage ();
          return 0;
        }
      else
        {
          command = argv[1];
          do_help = TRUE;
        }
    }
  else if (g_str_equal (command, "--help"))
    {
      usage ();
      return 0;
    }
  else if (g_str_equal (command, "--version"))
    {
      command = "version";
    }

  if (g_str_equal (command, "version"))
    return handle_version (argc, argv, do_help);
  else if (g_str_equal (command, "enable"))
    return handle_enable (argc, argv, do_help);
  else if (g_str_equal (command, "disable"))
    return handle_disable (argc, argv, do_help);
  else if (g_str_equal (command, "reset"))
    return handle_reset (argc, argv, do_help);
  else if (g_str_equal (command, "list"))
    return handle_list (argc, argv, do_help);
  else if (g_str_equal (command, "info"))
    return handle_info (argc, argv, do_help);
  else if (g_str_equal (command, "show"))
    return handle_info (argc, argv, do_help);
  else if (g_str_equal (command, "prefs"))
    return handle_prefs (argc, argv, do_help);
  else if (g_str_equal (command, "create"))
    return handle_create (argc, argv, do_help);
  else if (g_str_equal (command, "pack"))
    return handle_pack (argc, argv, do_help);
  else if (g_str_equal (command, "install"))
    return handle_install (argc, argv, do_help);
  else if (g_str_equal (command, "uninstall"))
    return handle_uninstall (argc, argv, do_help);
  else
    usage ();

  return 1;
}
