/* commands-uninstall.c
 *
 * Copyright 2019 Florian Müllner <fmuellner@gnome.org>
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
uninstall_extension (const char *uuid)
{
  g_autoptr (GDBusProxy) proxy = NULL;
  g_autoptr (GVariant) info = NULL;
  g_autoptr (GVariant) response = NULL;
  g_autoptr (GError) error = NULL;
  gboolean success = FALSE;
  double type;

  proxy = get_shell_proxy (&error);
  if (proxy == NULL)
    return FALSE;

  info = get_extension_property (proxy, uuid, "type");
  if (info == NULL)
    return FALSE;

  type = g_variant_get_double (info);
  if (type == TYPE_SYSTEM)
    {
      g_printerr (_("Cannot uninstall system extensions\n"));
      return FALSE;
    }

  response = g_dbus_proxy_call_sync (proxy,
                                     "UninstallExtension",
                                     g_variant_new ("(s)", uuid),
                                     0,
                                     -1,
                                     NULL,
                                     &error);

  g_variant_get (response, "(b)", &success);

  if (!success)
    g_printerr (_("Failed to uninstall “%s”\n"), uuid);

  return success;
}

int
handle_uninstall (int argc, char *argv[], gboolean do_help)
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

  g_set_prgname ("gnome-extensions uninstall");

  context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Uninstall an extension"));
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

  return uninstall_extension (*uuids) ? 0 : 2;
}
