/* commands-prefs.c
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
launch_extension_prefs (const char *uuid)
{
  g_autoptr (GDBusProxy) proxy = NULL;
  g_autoptr (GVariant) response = NULL;
  g_autoptr (GVariant) asv = NULL;
  g_autoptr (GVariantDict) info = NULL;
  g_autoptr (GError) error = NULL;
  gboolean has_prefs;

  proxy = get_shell_proxy (&error);
  if (proxy == NULL)
    return FALSE;

  response = g_dbus_proxy_call_sync (proxy,
                                     "GetExtensionInfo",
                                     g_variant_new ("(s)", uuid),
                                     0,
                                     -1,
                                     NULL,
                                     &error);
  if (response == NULL)
    return FALSE;

  asv = g_variant_get_child_value (response, 0);
  info = g_variant_dict_new (asv);

  if (!g_variant_dict_contains (info, "uuid"))
    return FALSE;

  g_variant_dict_lookup (info, "hasPrefs", "b", &has_prefs);
  if (!has_prefs)
    return FALSE;

  g_dbus_proxy_call_sync (proxy,
                          "OpenExtensionPrefs",
                          g_variant_new ("(ssa{sv})", uuid, "", NULL),
                          0,
                          -1,
                          NULL,
                          &error);

  return TRUE;
}

int
handle_prefs (int argc, char *argv[], gboolean do_help)
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

  g_set_prgname ("gnome-extensions prefs");

  context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Opens extension preferences"));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

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

  return launch_extension_prefs (*uuids) ? 0 : 2;
}
