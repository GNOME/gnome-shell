/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (c) 2008 Red Hat, Inc.
 * Copyright (c) 2008 Intel Corp.
 *
 * Based on plugin skeleton by:
 * Author: Tomas Frydrych <tf@linux.intel.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#define MUTTER_BUILDING_PLUGIN 1
#include <mutter-plugin.h>

#include <glib/gi18n-lib.h>

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gjs/gjs.h>
#include <girepository.h>
#include <gmodule.h>
#include <string.h>

#include "display.h"

#include "shell-global.h"

static gboolean do_init (const char *params);
static gboolean reload  (const char *params);

static gboolean xevent_filter (XEvent *xev);

/*
 * Create the plugin struct; function pointers initialized in
 * g_module_check_init().
 */
MUTTER_DECLARE_PLUGIN();

/*
 * Plugin private data that we store in the .plugin_private member.
 */
typedef struct _PluginState
{
  gboolean               debug_mode : 1;
  GjsContext            *gjs_context;
  Atom panel_action;
  Atom panel_action_run_dialog;
  Atom panel_action_main_menu;
} PluginState;


static PluginState *plugin_state;

const gchar * g_module_check_init (GModule *module);
const gchar *
g_module_check_init (GModule *module)
{
  MutterPlugin *plugin = mutter_get_plugin ();

  /* Human readable name (for use in UI) */
  plugin->name = "GNOME Shell";

  /* Plugin load time initialiser */
  plugin->do_init = do_init;

  /* The reload handler */
  plugin->reload = reload;

  /* Event handling */
  plugin->xevent_filter = xevent_filter;

  /* This will also create the ShellWM, which will set the appropriate
   * window management callbacks in plugin.
   */
  _shell_global_set_plugin (shell_global_get(), plugin);

  return NULL;
}

/*
 * Core of the plugin init function, called for initial initialization and
 * by the reload() function. Returns TRUE on success.
 */
static gboolean
do_init (const char *params)
{
  MutterPlugin *plugin = mutter_get_plugin();
  MetaScreen *screen;
  MetaDisplay *display;
  GError *error = NULL;
  int status;
  const char *shell_js;
  char **search_path;

  screen = mutter_plugin_get_screen (plugin);
  display = meta_screen_get_display (screen);

  plugin_state = g_new0 (PluginState, 1);

  if (params)
    {
      if (strstr (params, "debug"))
        {
          g_debug ("%s: Entering debug mode.", mutter_get_plugin()->name);

          plugin_state->debug_mode = TRUE;
        }
    }

  g_irepository_prepend_search_path (GNOME_SHELL_PKGLIBDIR);

  shell_js = g_getenv("GNOME_SHELL_JS");
  if (!shell_js)
    shell_js = JSDIR;

  search_path = g_strsplit(shell_js, ":", -1);
  plugin_state->gjs_context = gjs_context_new_with_search_path(search_path);
  g_strfreev(search_path);

  plugin_state->panel_action = XInternAtom (meta_display_get_xdisplay (display),
                                            "_GNOME_PANEL_ACTION", FALSE);
  plugin_state->panel_action_run_dialog = XInternAtom (meta_display_get_xdisplay (display),
                                                       "_GNOME_PANEL_ACTION_RUN_DIALOG", FALSE);
  plugin_state->panel_action_main_menu = XInternAtom (meta_display_get_xdisplay (display),
                                                      "_GNOME_PANEL_ACTION_MAIN_MENU", FALSE);

  if (!gjs_context_eval (plugin_state->gjs_context,
                         "const Main = imports.ui.main; Main.start();",
                         -1,
                         "<main>",
                         &status,
                         &error))
    {
      g_warning ("Evaling main.js failed: %s", error->message);
      g_error_free (error);
    }

  return TRUE;
}

static void
free_plugin_private (PluginState *state)
{
  if (!state)
    return;

  g_free (state);
}

/*
 * Called by the plugin manager when we stuff like the command line parameters
 * changed.
 */
static gboolean
reload (const char *params)
{
  PluginState *state;

  state = plugin_state;

  if (do_init (params))
    {
      /* Success; free the old state */
      free_plugin_private (plugin_state);
      return TRUE;
    }
  else
    {
      /* Fail -- fall back to the old state. */
      plugin_state = state;
    }

  return FALSE;
}

static gboolean
handle_panel_event (XEvent *xev)
{
  MetaScreen *screen;
  MetaDisplay *display;
  XClientMessageEvent *xev_client;
  Window root;

  screen = mutter_plugin_get_screen (mutter_get_plugin ());
  display = meta_screen_get_display (screen);

  if (xev->type != ClientMessage)
    return FALSE;

  root = meta_screen_get_xroot (screen);

  xev_client = (XClientMessageEvent*) xev;
  if (!(xev_client->window == root &&
        xev_client->message_type == plugin_state->panel_action &&
        xev_client->format == 32))
    return FALSE;

  if (xev_client->data.l[0] == plugin_state->panel_action_run_dialog)
    g_signal_emit_by_name (shell_global_get (), "panel-run-dialog",
                           (guint32) xev_client->data.l[1]);
  else if (xev_client->data.l[0] == plugin_state->panel_action_main_menu)
    g_signal_emit_by_name (shell_global_get (), "panel-main-menu",
                           (guint32) xev_client->data.l[1]);

  return TRUE;
}

static gboolean
xevent_filter (XEvent *xev)
{
  if (handle_panel_event (xev))
    return TRUE;
  return clutter_x11_handle_event (xev) != CLUTTER_X11_FILTER_CONTINUE;
}

/*
 * GModule unload function -- do any cleanup required.
 */
G_MODULE_EXPORT void g_module_unload (GModule *module);
G_MODULE_EXPORT void
g_module_unload (GModule *module)
{
  free_plugin_private (plugin_state);
}

