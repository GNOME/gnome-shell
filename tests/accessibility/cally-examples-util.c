/* CALLY - The Clutter Accessibility Implementation Library
 *
 * Copyright (C) 2009 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */


#include <gmodule.h>
#include <stdlib.h>

#include <clutter/clutter.h>

#include "cally-examples-util.h"

/* Checking the at-spi sources, the module directory is
 *   $(libdir)/gtk-2.0/modules
 *
 * It is supposed cally would be installed on the same libdir.
 *
 * You could use the option atk-bridge-dir to use other directory.
 */
#define ATK_BRIDGE_DEFAULT_MODULE_DIRECTORY PREFIXDIR"/gtk-2.0/modules"

static gchar *
_search_for_bridge_module (const gchar *module_name)
{
  /* We simplify the search for the atk bridge, see see the definition
   * of the macro for more information*/
  return g_strdup (ATK_BRIDGE_DEFAULT_MODULE_DIRECTORY);
}

static gchar*
_a11y_check_custom_bridge (int    *argc,
                           char ***argv)
{
  GError *error = NULL;
  GOptionContext *context;
  static gchar *bridge_dir = NULL;
  static GOptionEntry entries [] =
    {
      {"atk-bridge-dir", 'd', 0, G_OPTION_ARG_STRING, &bridge_dir, "atk-bridge module directory", NULL}
    };

  context = g_option_context_new ("- cally examples");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, argc, argv, &error))
    {
      g_print ("%s\n", error->message);
      g_print ("Use --help for more information.\n");
      exit (0);
    }

  return bridge_dir;
}


static gboolean
_a11y_invoke_module (const gchar  *module_path,
                     gboolean      init)
{
  GModule    *handle;
  void      (*invoke_fn) (void);
  const char *method;

  if (init)
    method = "gnome_accessibility_module_init";
  else
    method = "gnome_accessibility_module_shutdown";

  if (!module_path)
    return FALSE;

  if (!(handle = g_module_open (module_path, G_MODULE_BIND_LAZY)))
    {
      g_warning ("Accessibility: failed to load module '%s': '%s'",
                 module_path, g_module_error ());

      return FALSE;
    }

  if (!g_module_symbol (handle, method, (gpointer *)&invoke_fn))
    {
      g_warning ("Accessibility: error library '%s' does not include "
                 "method '%s' required for accessibility support",
                 module_path, method);
      g_module_close (handle);

      return FALSE;
    }

  g_debug ("Module %s loaded successfully", module_path);
  invoke_fn ();

  return TRUE;
}

/**
 * This method will initialize the accessibility support provided by cally.
 *
 * Basically it will load the cally module using gmodule functions.
 *
 * Returns if it was able to init the a11y support or not.
 */
gboolean
cally_util_a11y_init (int *argc, char ***argv)
{
  gchar *bridge_dir = NULL;
  gchar *bridge_path = NULL;
  gboolean result = FALSE;

  if (clutter_get_accessibility_enabled () == FALSE)
    {
      g_warning ("Accessibility: clutter has no accessibility enabled"
                 " skipping the atk-bridge load");
      return FALSE;
    }

  bridge_dir = _a11y_check_custom_bridge (argc, argv);
  if (bridge_dir == NULL)
    bridge_dir = _search_for_bridge_module ("atk-bridge");

  bridge_path = g_module_build_path (bridge_dir, "libatk-bridge");

  result = _a11y_invoke_module (bridge_path, TRUE);

  g_free (bridge_dir);
  g_free (bridge_path);

  return result;
}
