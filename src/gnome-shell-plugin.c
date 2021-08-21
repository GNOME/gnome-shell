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
#include <meta/meta-x11-display.h>
#include <meta/util.h>

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

  int glx_error_base;
  int glx_event_base;
  guint have_swap_event : 1;
  CoglContext *cogl_context;

  ShellGlobal *global;
};

G_DEFINE_TYPE (GnomeShellPlugin, gnome_shell_plugin, META_TYPE_PLUGIN)

static gboolean
gnome_shell_plugin_has_swap_event (GnomeShellPlugin *shell_plugin)
{
  CoglDisplay *cogl_display =
    cogl_context_get_display (shell_plugin->cogl_context);
  CoglRenderer *renderer = cogl_display_get_renderer (cogl_display);
  const char * (* query_extensions_string) (Display *dpy, int screen);
  Bool (* query_extension) (Display *dpy, int *error, int *event);
  MetaDisplay *display = meta_plugin_get_display (META_PLUGIN (shell_plugin));
  MetaX11Display *x11_display = meta_display_get_x11_display (display);
  Display *xdisplay;
  int screen_number;
  const char *glx_extensions;

  /* We will only get swap events if Cogl is using GLX */
  if (cogl_renderer_get_winsys_id (renderer) != COGL_WINSYS_ID_GLX)
    return FALSE;

  xdisplay = meta_x11_display_get_xdisplay (x11_display);

  query_extensions_string =
    (void *) cogl_get_proc_address ("glXQueryExtensionsString");
  query_extension =
    (void *) cogl_get_proc_address ("glXQueryExtension");

  query_extension (xdisplay,
                   &shell_plugin->glx_error_base,
                   &shell_plugin->glx_event_base);

  screen_number = XDefaultScreen (xdisplay);
  glx_extensions = query_extensions_string (xdisplay, screen_number);

  return strstr (glx_extensions, "GLX_INTEL_swap_event") != NULL;
}

static void
gnome_shell_plugin_start (MetaPlugin *plugin)
{
  GnomeShellPlugin *shell_plugin = GNOME_SHELL_PLUGIN (plugin);
  GError *error = NULL;
  uint8_t status;
  GjsContext *gjs_context;
  ClutterBackend *backend;

  backend = clutter_get_default_backend ();
  shell_plugin->cogl_context = clutter_backend_get_cogl_context (backend);

  shell_plugin->have_swap_event =
    gnome_shell_plugin_has_swap_event (shell_plugin);

  shell_perf_log_define_event (shell_perf_log_get_default (),
                               "glx.swapComplete",
                               "GL buffer swap complete event received (with timestamp of completion)",
                               "x");

  shell_plugin->global = shell_global_get ();
  _shell_global_set_plugin (shell_plugin->global, META_PLUGIN (shell_plugin));

  gjs_context = _shell_global_get_gjs_context (shell_plugin->global);

  if (!gjs_context_eval_module_file (gjs_context,
                                     "resource:///org/gnome/shell/ui/init.js",
                                     &status,
                                     &error))
    {
      g_message ("Execution of main.js threw exception: %s", error->message);
      g_error_free (error);
      /* We just exit() here, since in a development environment you'll get the
       * error in your shell output, and it's way better than a busted WM,
       * which typically manifests as a white screen.
       *
       * In production, we shouldn't crash =)  But if we do, we should get
       * restarted by the session infrastructure, which is likely going
       * to be better than some undefined state.
       *
       * If there was a generic "hook into bug-buddy for non-C crashes"
       * infrastructure, here would be the place to put it.
       */
      g_object_unref (gjs_context);
      exit (1);
    }
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
                                MetaRectangle      *old_frame_rect,
                                MetaRectangle      *old_buffer_rect)
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
                                      MetaRectangle   *tile_rect,
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
                                              MetaRectangle      *rect)
{
  _shell_wm_show_window_menu_for_rect (get_shell_wm (), window, menu, rect);
}

static gboolean
gnome_shell_plugin_xevent_filter (MetaPlugin *plugin,
                                  XEvent     *xev)
{
#ifdef GLX_INTEL_swap_event
  GnomeShellPlugin *shell_plugin = GNOME_SHELL_PLUGIN (plugin);

  if (shell_plugin->have_swap_event &&
      xev->type == (shell_plugin->glx_event_base + GLX_BufferSwapComplete))
    {
      GLXBufferSwapComplete *swap_complete_event;
      swap_complete_event = (GLXBufferSwapComplete *)xev;

      /* Buggy early versions of the INTEL_swap_event implementation in Mesa
       * can send this with a ust of 0. Simplify life for consumers
       * by ignoring such events */
      if (swap_complete_event->ust != 0)
        {
          gboolean frame_timestamps;
          g_object_get (shell_plugin->global,
                        "frame-timestamps", &frame_timestamps,
                        NULL);

          if (frame_timestamps)
            shell_perf_log_event_x (shell_perf_log_get_default (),
                                    "glx.swapComplete",
                                    swap_complete_event->ust);
        }
    }
#endif

  return FALSE;
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

static const MetaPluginInfo *
gnome_shell_plugin_plugin_info (MetaPlugin *plugin)
{
  static const MetaPluginInfo info = {
    .name = "GNOME Shell",
    .version = "0.1",
    .author = "Various",
    .license = "GPLv2+",
    .description = "Provides GNOME Shell core functionality"
  };

  return &info;
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

  plugin_class->xevent_filter     = gnome_shell_plugin_xevent_filter;
  plugin_class->keybinding_filter = gnome_shell_plugin_keybinding_filter;

  plugin_class->confirm_display_change = gnome_shell_plugin_confirm_display_change;

  plugin_class->plugin_info       = gnome_shell_plugin_plugin_info;

  plugin_class->create_close_dialog = gnome_shell_plugin_create_close_dialog;
  plugin_class->create_inhibit_shortcuts_dialog = gnome_shell_plugin_create_inhibit_shortcuts_dialog;

  plugin_class->locate_pointer = gnome_shell_plugin_locate_pointer;
}

static void
gnome_shell_plugin_init (GnomeShellPlugin *shell_plugin)
{
}
