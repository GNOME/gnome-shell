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
                                        "org.gnome.Shell",
                                        "/org/gnome/Shell",
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
  g_printerr ("%s\n", _("Usage:"));
  g_printerr ("  gnome-extensions %s %s\n", _("COMMAND"), _("[ARGS…]"));
  g_printerr ("\n");
  g_printerr ("%s\n", _("Commands:"));
  g_printerr ("  help      %s\n", _("Print help"));
  g_printerr ("  version   %s\n", _("Print version"));
  g_printerr ("  enable    %s\n", _("Enable extension"));
  g_printerr ("  disable   %s\n", _("Disable extension"));
  g_printerr ("  create    %s\n", _("Create extension"));
  g_printerr ("\n");
  g_printerr (_("Use %s to get detailed help.\n"), "“gnome-extensions help COMMAND”");
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
  else if (g_str_equal (command, "create"))
    return handle_create (argc, argv, do_help);
  else
    usage ();

  return 1;
}
