/* command-enable.c
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

#include "commands.h"
#include "common.h"
#include "config.h"

static gboolean
enable_extension_gsettings (const char *uuid)
{
  g_autoptr(GSettings) settings = get_shell_settings ();

  if (settings == NULL)
    return FALSE;

  return settings_list_add (settings, "enabled-extensions", uuid) &&
         settings_list_remove (settings, "disabled-extensions", uuid);
}

static gboolean
enable_extension_dbus (GDBusProxy *proxy,
                       const char *uuid)
{
  g_autoptr (GVariant) response = NULL;
  g_autoptr (GError) error = NULL;
  gboolean success = FALSE;

  response = g_dbus_proxy_call_sync (proxy,
                                     "EnableExtension",
                                     g_variant_new ("(s)", uuid),
                                     0,
                                     -1,
                                     NULL,
                                     &error);

  if (response == NULL)
    return enable_extension_gsettings (uuid);

  g_variant_get (response, "(b)", &success);

  if (!success)
    g_printerr (_("Extension “%s” does not exist\n"), uuid);

  return success;
}

static gboolean
enable_extension (const char *uuid)
{
  g_autoptr (GDBusProxy) proxy = NULL;
  g_autoptr (GError) error = NULL;

  proxy = get_shell_proxy (&error);

  if (proxy != NULL)
    return enable_extension_dbus (proxy, uuid);
  else
    return enable_extension_gsettings (uuid);
}

int
handle_enable (int argc, char *argv[], gboolean do_help)
{
  g_autoptr (GOptionContext) context = NULL;
  g_autoptr (GError) error = NULL;
  g_auto(GStrv) uuids = NULL;
  GOptionEntry entries[] = {
    { .long_name = G_OPTION_REMAINING,
      .arg_description = "UUID",
      .arg = G_OPTION_ARG_STRING_ARRAY, .arg_data = &uuids },
    { NULL }
  };

  g_set_prgname ("gnome-extensions enable");

  context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Enable an extension"));
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

  if (uuids == NULL)
    {
      show_help (context, _("No UUID given"));
      return 1;
    }
  else if (g_strv_length (uuids) > 1)
    {
      show_help (context, _("More than one UUID given"));
      return 1;
    }

  return enable_extension (*uuids) ? 0 : 2;
}
