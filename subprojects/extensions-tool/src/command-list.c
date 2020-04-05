/* command-list.c
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


typedef enum {
  LIST_FLAGS_NONE     = 0,
  LIST_FLAGS_USER     = 1 << 0,
  LIST_FLAGS_SYSTEM   = 1 << 1,
  LIST_FLAGS_ENABLED  = 1 << 2,
  LIST_FLAGS_DISABLED = 1 << 3,
  LIST_FLAGS_NO_PREFS = 1 << 4,
  LIST_FLAGS_NO_UPDATES = 1 << 5,
} ListFilterFlags;

static gboolean
list_extensions (ListFilterFlags filter, DisplayFormat format)
{
  g_autoptr (GDBusProxy) proxy = NULL;
  g_autoptr (GVariant) response = NULL;
  g_autoptr (GVariant) extensions = NULL;
  g_autoptr (GError) error = NULL;
  gboolean needs_newline = FALSE;
  GVariantIter iter;
  GVariant *value;
  char *uuid;

  proxy = get_shell_proxy (&error);
  if (proxy == NULL)
    return FALSE;

  response = g_dbus_proxy_call_sync (proxy,
                                     "ListExtensions",
                                     NULL,
                                     0,
                                     -1,
                                     NULL,
                                     &error);
  if (response == NULL)
    {
      g_printerr (_("Failed to connect to GNOME Shell\n"));
      return FALSE;
    }

  extensions = g_variant_get_child_value (response, 0);

  g_variant_iter_init (&iter, extensions);
  while (g_variant_iter_loop (&iter, "{s@a{sv}}", &uuid, &value))
    {
      g_autoptr (GVariantDict) info = NULL;
      double type, state;
      gboolean has_prefs;
      gboolean has_update;

      info = g_variant_dict_new (value);
      g_variant_dict_lookup (info, "type", "d", &type);
      g_variant_dict_lookup (info, "state", "d", &state);
      g_variant_dict_lookup (info, "hasPrefs", "b", &has_prefs);
      g_variant_dict_lookup (info, "hasUpdate", "b", &has_update);

      if (type == TYPE_USER && (filter & LIST_FLAGS_USER) == 0)
        continue;

      if (type == TYPE_SYSTEM && (filter & LIST_FLAGS_SYSTEM) == 0)
        continue;

      if (state == STATE_ENABLED && (filter & LIST_FLAGS_ENABLED) == 0)
        continue;

      if (state != STATE_ENABLED && (filter & LIST_FLAGS_DISABLED) == 0)
        continue;

      if (!has_prefs && (filter & LIST_FLAGS_NO_PREFS) == 0)
        continue;

      if (!has_update && (filter & LIST_FLAGS_NO_UPDATES) == 0)
        continue;

      if (needs_newline)
        g_print ("\n");

      print_extension_info (info, format);
      needs_newline = (format != DISPLAY_ONELINE);
    }

  return TRUE;
}

int
handle_list (int argc, char *argv[], gboolean do_help)
{
  g_autoptr (GOptionContext) context = NULL;
  g_autoptr (GError) error = NULL;
  int flags = LIST_FLAGS_NONE;
  gboolean details = FALSE;
  gboolean user = FALSE;
  gboolean system = FALSE;
  gboolean enabled = FALSE;
  gboolean disabled = FALSE;
  gboolean has_prefs = FALSE;
  gboolean has_updates = FALSE;
  GOptionEntry entries[] = {
    { .long_name = "user",
      .arg = G_OPTION_ARG_NONE, .arg_data = &user,
      .description = _("Show user-installed extensions") },
    { .long_name = "system",
      .arg = G_OPTION_ARG_NONE, .arg_data = &system,
      .description = _("Show system-installed extensions") },
    { .long_name = "enabled",
      .arg = G_OPTION_ARG_NONE, .arg_data = &enabled,
      .description = _("Show enabled extensions") },
    { .long_name = "disabled",
      .arg = G_OPTION_ARG_NONE, .arg_data = &disabled,
      .description = _("Show disabled extensions") },
    { .long_name = "prefs",
      .arg = G_OPTION_ARG_NONE, .arg_data = &has_prefs,
      .description = _("Show extensions with preferences") },
    { .long_name = "updates",
      .arg = G_OPTION_ARG_NONE, .arg_data = &has_updates,
      .description = _("Show extensions with updates") },
    { .long_name = "details", .short_name = 'd',
      .arg = G_OPTION_ARG_NONE, .arg_data = &details,
      .description = _("Print extension details") },
    { NULL }
  };

  g_set_prgname ("gnome-extensions list");

  context = g_option_context_new (NULL);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("List installed extensions"));
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

  if (argc > 1)
    {
      show_help (context, _("Unknown arguments"));
      return 1;
    }

  if (user || !system)
    flags |= LIST_FLAGS_USER;

  if (system || !user)
    flags |= LIST_FLAGS_SYSTEM;

  if (enabled || !disabled)
    flags |= LIST_FLAGS_ENABLED;

  if (disabled || !enabled)
    flags |= LIST_FLAGS_DISABLED;

  if (!has_prefs)
    flags |= LIST_FLAGS_NO_PREFS;

  if (!has_updates)
    flags |= LIST_FLAGS_NO_UPDATES;

  return list_extensions (flags, details ? DISPLAY_DETAILED
                                         : DISPLAY_ONELINE) ? 0 : 2;
}
