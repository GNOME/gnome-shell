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
#include "shell-wm.h"

static void gnome_shell_plugin_constructed (GObject *object);
static void gnome_shell_plugin_dispose     (GObject *object);
static void gnome_shell_plugin_finalize    (GObject *object);

static void     gnome_shell_plugin_minimize         (MutterPlugin         *plugin,
                                                     MutterWindow         *actor);
static void     gnome_shell_plugin_maximize         (MutterPlugin         *plugin,
                                                     MutterWindow         *actor,
                                                     gint                  x,
                                                     gint                  y,
                                                     gint                  width,
                                                     gint                  height);
static void     gnome_shell_plugin_unmaximize       (MutterPlugin         *plugin,
                                                     MutterWindow         *actor,
                                                     gint                  x,
                                                     gint                  y,
                                                     gint                  width,
                                                     gint                  height);
static void     gnome_shell_plugin_map              (MutterPlugin         *plugin,
                                                     MutterWindow         *actor);
static void     gnome_shell_plugin_destroy          (MutterPlugin         *plugin,
                                                     MutterWindow         *actor);

static void     gnome_shell_plugin_switch_workspace (MutterPlugin         *plugin,
                                                     const GList         **actors,
                                                     gint                  from,
                                                     gint                  to,
                                                     MetaMotionDirection   direction);
static void     gnome_shell_plugin_kill_effect      (MutterPlugin         *plugin,
                                                     MutterWindow         *actor,
                                                     gulong                events);

static gboolean                gnome_shell_plugin_xevent_filter (MutterPlugin *plugin,
                                                                 XEvent       *event);
static const MutterPluginInfo *gnome_shell_plugin_plugin_info   (MutterPlugin *plugin);

#define GNOME_TYPE_SHELL_PLUGIN            (gnome_shell_plugin_get_type ())
#define GNOME_SHELL_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_TYPE_SHELL_PLUGIN, GnomeShellPlugin))
#define GNOME_SHELL_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GNOME_TYPE_SHELL_PLUGIN, GnomeShellPluginClass))
#define GNOME_IS_SHELL_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GNOME_SHELL_PLUGIN_TYPE))
#define GNOME_IS_SHELL_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GNOME_TYPE_SHELL_PLUGIN))
#define GNOME_SHELL_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GNOME_TYPE_SHELL_PLUGIN, GnomeShellPluginClass))

typedef struct _GnomeShellPlugin        GnomeShellPlugin;
typedef struct _GnomeShellPluginClass   GnomeShellPluginClass;

struct _GnomeShellPlugin
{
  MutterPlugin parent;

  GjsContext *gjs_context;
  Atom panel_action;
  Atom panel_action_run_dialog;
  Atom panel_action_main_menu;
};

struct _GnomeShellPluginClass
{
  MutterPluginClass parent_class;
};

/*
 * Create the plugin struct; function pointers initialized in
 * g_module_check_init().
 */
MUTTER_PLUGIN_DECLARE(GnomeShellPlugin, gnome_shell_plugin);

static void
gnome_shell_plugin_class_init (GnomeShellPluginClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  MutterPluginClass *plugin_class  = MUTTER_PLUGIN_CLASS (klass);

  gobject_class->constructed     = gnome_shell_plugin_constructed;
  gobject_class->dispose         = gnome_shell_plugin_dispose;
  gobject_class->finalize        = gnome_shell_plugin_finalize;

  plugin_class->map              = gnome_shell_plugin_map;
  plugin_class->minimize         = gnome_shell_plugin_minimize;
  plugin_class->maximize         = gnome_shell_plugin_maximize;
  plugin_class->unmaximize       = gnome_shell_plugin_unmaximize;
  plugin_class->destroy          = gnome_shell_plugin_destroy;

  plugin_class->switch_workspace = gnome_shell_plugin_switch_workspace;
  plugin_class->kill_effect      = gnome_shell_plugin_kill_effect;

  plugin_class->xevent_filter    = gnome_shell_plugin_xevent_filter;
  plugin_class->plugin_info      = gnome_shell_plugin_plugin_info;
}

static void
gnome_shell_plugin_init (GnomeShellPlugin *shell_plugin)
{
  _shell_global_set_plugin (shell_global_get(), MUTTER_PLUGIN(shell_plugin));
}

static void
gnome_shell_plugin_constructed (GObject *object)
{
  MutterPlugin *plugin = MUTTER_PLUGIN (object);
  GnomeShellPlugin *shell_plugin = GNOME_SHELL_PLUGIN (object);
  MetaScreen *screen;
  MetaDisplay *display;
  GError *error = NULL;
  int status;
  const char *shell_js;
  char **search_path;
  ClutterBackend *backend;
  cairo_font_options_t *font_options;

  /* Disable text mipmapping; it causes problems on pre-GEM Intel
   * drivers and we should just be rendering text at the right
   * size rather than scaling it. If we do effects where we dynamically
   * zoom labels, then we might want to reconsider.
   */
  clutter_set_font_flags (clutter_get_font_flags () & ~CLUTTER_FONT_MIPMAPPING);

  /* Clutter (as of 0.9) passes comprehensively wrong font options
   * override whatever set_font_flags() did above.
   *
   * http://bugzilla.openedhand.com/show_bug.cgi?id=1456
   */
  backend = clutter_get_default_backend ();
  font_options = cairo_font_options_create ();
  /* Default options for everything is reasonable; except that
   * we want to turn off subpixel anti-aliasing; since Clutter
   * doesn't currently have the code to support ARGB masks,
   * generating them then squashing them back to A8 is pointless.
   */
  cairo_font_options_set_antialias (font_options, CAIRO_ANTIALIAS_GRAY);
  clutter_backend_set_font_options (backend, font_options);
  cairo_font_options_destroy (font_options);

  screen = mutter_plugin_get_screen (plugin);
  display = meta_screen_get_display (screen);

  g_irepository_prepend_search_path (GNOME_SHELL_PKGLIBDIR);

  shell_js = g_getenv("GNOME_SHELL_JS");
  if (!shell_js)
    shell_js = JSDIR;

  search_path = g_strsplit(shell_js, ":", -1);
  shell_plugin->gjs_context = gjs_context_new_with_search_path(search_path);
  g_strfreev(search_path);

  shell_plugin->panel_action = XInternAtom (meta_display_get_xdisplay (display),
                                            "_GNOME_PANEL_ACTION", FALSE);
  shell_plugin->panel_action_run_dialog = XInternAtom (meta_display_get_xdisplay (display),
                                                       "_GNOME_PANEL_ACTION_RUN_DIALOG", FALSE);
  shell_plugin->panel_action_main_menu = XInternAtom (meta_display_get_xdisplay (display),
                                                      "_GNOME_PANEL_ACTION_MAIN_MENU", FALSE);

  if (!gjs_context_eval (shell_plugin->gjs_context,
                         "const Main = imports.ui.main; Main.start();",
                         -1,
                         "<main>",
                         &status,
                         &error))
    {
      g_warning ("Evaling main.js failed: %s", error->message);
      g_error_free (error);
    }
}

static void
gnome_shell_plugin_dispose (GObject *object)
{
  G_OBJECT_CLASS(gnome_shell_plugin_parent_class)->dispose (object);
}

static void
gnome_shell_plugin_finalize (GObject *object)
{
  G_OBJECT_CLASS(gnome_shell_plugin_parent_class)->finalize (object);
}

static ShellWM *
get_shell_wm (void)
{
  ShellWM *wm;

  g_object_get (shell_global_get (),
                "window-manager", &wm,
                NULL);
  /* drop extra ref added by g_object_get */
  g_object_unref (wm);

  return wm;
}

static void
gnome_shell_plugin_minimize (MutterPlugin         *plugin,
			     MutterWindow         *actor)
{
  _shell_wm_minimize (get_shell_wm (),
                      actor);

}

static void
gnome_shell_plugin_maximize (MutterPlugin         *plugin,
                             MutterWindow         *actor,
                             gint                  x,
                             gint                  y,
                             gint                  width,
                             gint                  height)
{
  _shell_wm_maximize (get_shell_wm (),
                      actor, x, y, width, height);
}

static void
gnome_shell_plugin_unmaximize (MutterPlugin         *plugin,
                               MutterWindow         *actor,
                               gint                  x,
                               gint                  y,
                               gint                  width,
                               gint                  height)
{
  _shell_wm_unmaximize (get_shell_wm (),
                        actor, x, y, width, height);
}

static void
gnome_shell_plugin_map (MutterPlugin         *plugin,
                        MutterWindow         *actor)
{
  _shell_wm_map (get_shell_wm (),
                 actor);
}

static void
gnome_shell_plugin_destroy (MutterPlugin         *plugin,
                            MutterWindow         *actor)
{
  _shell_wm_destroy (get_shell_wm (),
                     actor);
}

static void
gnome_shell_plugin_switch_workspace (MutterPlugin         *plugin,
                                     const GList         **actors,
                                     gint                  from,
                                     gint                  to,
                                     MetaMotionDirection   direction)
{
  _shell_wm_switch_workspace (get_shell_wm(),
                              actors, from, to, direction);
}

static void
gnome_shell_plugin_kill_effect (MutterPlugin         *plugin,
                                MutterWindow         *actor,
                                gulong                events)
{
  _shell_wm_kill_effect (get_shell_wm(),
                         actor, events);
}

static gboolean
handle_panel_event (GnomeShellPlugin *shell_plugin,
                    XEvent           *xev)
{
  MutterPlugin *plugin = MUTTER_PLUGIN (shell_plugin);
  MetaScreen *screen;
  MetaDisplay *display;
  XClientMessageEvent *xev_client;
  Window root;

  screen = mutter_plugin_get_screen (plugin);
  display = meta_screen_get_display (screen);

  if (xev->type != ClientMessage)
    return FALSE;

  root = meta_screen_get_xroot (screen);

  xev_client = (XClientMessageEvent*) xev;
  if (!(xev_client->window == root &&
        xev_client->message_type == shell_plugin->panel_action &&
        xev_client->format == 32))
    return FALSE;

  if (xev_client->data.l[0] == shell_plugin->panel_action_run_dialog)
    g_signal_emit_by_name (shell_global_get (), "panel-run-dialog",
                           (guint32) xev_client->data.l[1]);
  else if (xev_client->data.l[0] == shell_plugin->panel_action_main_menu)
    g_signal_emit_by_name (shell_global_get (), "panel-main-menu",
                           (guint32) xev_client->data.l[1]);

  return TRUE;
}

static gboolean
gnome_shell_plugin_xevent_filter (MutterPlugin *plugin,
                                  XEvent       *xev)
{
  GnomeShellPlugin *shell_plugin = GNOME_SHELL_PLUGIN (plugin);

  if (handle_panel_event (shell_plugin, xev))
    return TRUE;
  return clutter_x11_handle_event (xev) != CLUTTER_X11_FILTER_CONTINUE;
}

static const
MutterPluginInfo *gnome_shell_plugin_plugin_info (MutterPlugin *plugin)
{
  static const MutterPluginInfo info = {
    .name = "GNOME Shell",
    .version = "0.1",
    .author = "Various",
    .license = "GPLv2+",
    .description = "Provides GNOME Shell core functionality"
  };

  return &info;
}
