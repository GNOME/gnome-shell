/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>

#include <meta/main.h>
#include <meta/util.h>
#include <glib/gi18n-lib.h>
#include "meta-plugin-manager.h"

#include <glib.h>

static gboolean
print_version (const gchar    *option_name,
               const gchar    *value,
               gpointer        data,
               GError        **error)
{
  const int latest_year = 2011;

  g_print (_("mutter %s\n"
             "Copyright (C) 2001-%d Havoc Pennington, Red Hat, Inc., and others\n"
             "This is free software; see the source for copying conditions.\n"
             "There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"),
           VERSION, latest_year);
  exit (0);
}

static gchar *plugin = "default";

GOptionEntry mutter_options[] = {
  {
    "version", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
    print_version,
    N_("Print version"),
    NULL
  },
  {
    "mutter-plugin", 0, 0, G_OPTION_ARG_STRING,
    &plugin,
    N_("Mutter plugin to use"),
    "PLUGIN",
  },
  { NULL }
};

int
main (int argc, char **argv)
{
  GOptionContext *ctx;
  GError *error = NULL;

  ctx = meta_get_option_context ();
  g_option_context_add_main_entries (ctx, mutter_options, GETTEXT_PACKAGE);
  if (!g_option_context_parse (ctx, &argc, &argv, &error))
    {
      g_printerr ("mutter: %s\n", error->message);
      exit (1);
    }
  g_option_context_free (ctx);

  if (plugin)
    meta_plugin_manager_load (plugin);

  meta_init ();
  meta_register_with_session ();
  return meta_run ();
}
