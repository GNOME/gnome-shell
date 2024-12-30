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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * GnomeShellPlugin is the entry point for for GNOME Shell into and out of
 * Mutter. By registering itself into Mutter using
 * meta_plugin_manager_set_plugin_type(), Mutter will call the vfuncs of the
 * plugin at the appropriate time.
 *
 * The functions in in GnomeShellPlugin are all just stubs, which just call the
 * similar methods in GnomeShellWm.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <clutter/clutter.h>
#include <gjs/gjs.h>
#include <meta/display.h>
#include <meta/meta-plugin.h>
#include <meta/util.h>
#include <mtk/mtk.h>

#include "shell-global-private.h"
#include "shell-perf-log.h"
#include "shell-wm-private.h"

#define GNOME_TYPE_SHELL_PLUGIN (gnome_shell_plugin_get_type ())
G_DECLARE_FINAL_TYPE (GnomeShellPlugin, gnome_shell_plugin,
                      GNOME, SHELL_PLUGIN,
                      MetaPlugin)

struct _GnomeShellPlugin
{
  MetaPlugin parent;

  ShellGlobal *global;
};

G_DEFINE_TYPE (GnomeShellPlugin, gnome_shell_plugin, META_TYPE_PLUGIN)

static void
gnome_shell_plugin_start (MetaPlugin *plugin)
{
  GnomeShellPlugin *shell_plugin = GNOME_SHELL_PLUGIN (plugin);

  shell_plugin->global = shell_global_get ();
  _shell_global_set_plugin (shell_plugin->global, META_PLUGIN (shell_plugin));
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
gnome_shell_plugin_minimize (MetaPlugin         *plugin,
			     MetaWindowActor    *actor)
{
  _shell_wm_minimize (get_shell_wm (),
                      actor);

}

static void
gnome_shell_plugin_unminimize (MetaPlugin         *plugin,
                               MetaWindowActor    *actor)
{
  _shell_wm_unminimize (get_shell_wm (),
                      actor);

}

static void
gnome_shell_plugin_size_changed (MetaPlugin         *plugin,
                                 MetaWindowActor    *actor)
{
  _shell_wm_size_changed (get_shell_wm (), actor);
}

static void
gnome_shell_plugin_size_change (MetaPlugin         *plugin,
                                MetaWindowActor    *actor,
                                MetaSizeChange      which_change,
                                MtkRectangle       *old_frame_rect,
                                MtkRectangle       *old_buffer_rect)
{
  _shell_wm_size_change (get_shell_wm (), actor, which_change, old_frame_rect, old_buffer_rect);
}

static void
gnome_shell_plugin_map (MetaPlugin         *plugin,
                        MetaWindowActor    *actor)
{
  _shell_wm_map (get_shell_wm (),
                 actor);
}

static void
gnome_shell_plugin_destroy (MetaPlugin         *plugin,
                            MetaWindowActor    *actor)
{
  _shell_wm_destroy (get_shell_wm (),
                     actor);
}

static void
gnome_shell_plugin_switch_workspace (MetaPlugin         *plugin,
                                     gint                from,
                                     gint                to,
                                     MetaMotionDirection direction)
{
  _shell_wm_switch_workspace (get_shell_wm(), from, to, direction);
}

static void
gnome_shell_plugin_kill_window_effects (MetaPlugin         *plugin,
                                        MetaWindowActor    *actor)
{
  _shell_wm_kill_window_effects (get_shell_wm(), actor);
}

static void
gnome_shell_plugin_kill_switch_workspace (MetaPlugin         *plugin)
{
  _shell_wm_kill_switch_workspace (get_shell_wm());
}

static void
gnome_shell_plugin_show_tile_preview (MetaPlugin      *plugin,
                                      MetaWindow      *window,
                                      MtkRectangle    *tile_rect,
                                      int              tile_monitor)
{
  _shell_wm_show_tile_preview (get_shell_wm (), window, tile_rect, tile_monitor);
}

static void
gnome_shell_plugin_hide_tile_preview (MetaPlugin *plugin)
{
  _shell_wm_hide_tile_preview (get_shell_wm ());
}

static void
gnome_shell_plugin_show_window_menu (MetaPlugin         *plugin,
                                     MetaWindow         *window,
                                     MetaWindowMenuType  menu,
                                     int                 x,
                                     int                 y)
{
  _shell_wm_show_window_menu (get_shell_wm (), window, menu, x, y);
}

static void
gnome_shell_plugin_show_window_menu_for_rect (MetaPlugin         *plugin,
                                              MetaWindow         *window,
                                              MetaWindowMenuType  menu,
                                              MtkRectangle       *rect)
{
  _shell_wm_show_window_menu_for_rect (get_shell_wm (), window, menu, rect);
}

static gboolean
gnome_shell_plugin_keybinding_filter (MetaPlugin     *plugin,
                                      MetaKeyBinding *binding)
{
  return _shell_wm_filter_keybinding (get_shell_wm (), binding);
}

static void
gnome_shell_plugin_confirm_display_change (MetaPlugin *plugin)
{
  _shell_wm_confirm_display_change (get_shell_wm ());
}

static MetaCloseDialog *
gnome_shell_plugin_create_close_dialog (MetaPlugin *plugin,
                                        MetaWindow *window)
{
  return _shell_wm_create_close_dialog (get_shell_wm (), window);
}

static MetaInhibitShortcutsDialog *
gnome_shell_plugin_create_inhibit_shortcuts_dialog (MetaPlugin *plugin,
                                                    MetaWindow *window)
{
  return _shell_wm_create_inhibit_shortcuts_dialog (get_shell_wm (), window);
}

static void
gnome_shell_plugin_locate_pointer (MetaPlugin *plugin)
{
  GnomeShellPlugin *shell_plugin = GNOME_SHELL_PLUGIN (plugin);
  _shell_global_locate_pointer (shell_plugin->global);
}

static void
gnome_shell_plugin_class_init (GnomeShellPluginClass *klass)
{
  MetaPluginClass *plugin_class  = META_PLUGIN_CLASS (klass);

  plugin_class->start            = gnome_shell_plugin_start;
  plugin_class->map              = gnome_shell_plugin_map;
  plugin_class->minimize         = gnome_shell_plugin_minimize;
  plugin_class->unminimize       = gnome_shell_plugin_unminimize;
  plugin_class->size_changed     = gnome_shell_plugin_size_changed;
  plugin_class->size_change      = gnome_shell_plugin_size_change;
  plugin_class->destroy          = gnome_shell_plugin_destroy;

  plugin_class->switch_workspace = gnome_shell_plugin_switch_workspace;

  plugin_class->kill_window_effects   = gnome_shell_plugin_kill_window_effects;
  plugin_class->kill_switch_workspace = gnome_shell_plugin_kill_switch_workspace;

  plugin_class->show_tile_preview = gnome_shell_plugin_show_tile_preview;
  plugin_class->hide_tile_preview = gnome_shell_plugin_hide_tile_preview;
  plugin_class->show_window_menu = gnome_shell_plugin_show_window_menu;
  plugin_class->show_window_menu_for_rect = gnome_shell_plugin_show_window_menu_for_rect;

  plugin_class->keybinding_filter = gnome_shell_plugin_keybinding_filter;

  plugin_class->confirm_display_change = gnome_shell_plugin_confirm_display_change;

  plugin_class->create_close_dialog = gnome_shell_plugin_create_close_dialog;
  plugin_class->create_inhibit_shortcuts_dialog = gnome_shell_plugin_create_inhibit_shortcuts_dialog;

  plugin_class->locate_pointer = gnome_shell_plugin_locate_pointer;
}

static void
gnome_shell_plugin_init (GnomeShellPlugin *shell_plugin)
{
}
