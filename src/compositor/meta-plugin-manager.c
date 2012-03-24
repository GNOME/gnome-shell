/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (c) 2008 Intel Corp.
 *
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

#include "config.h"
#include "compositor-private.h"
#include "meta-plugin-manager.h"
#include <meta/prefs.h>
#include <meta/errors.h>
#include <meta/workspace.h>
#include "meta-module.h"
#include "window-private.h"

#include <string.h>
#include <stdlib.h>

#include <clutter/x11/clutter-x11.h>

static GType plugin_type = G_TYPE_NONE;

struct MetaPluginManager
{
  MetaScreen *screen;
  MetaPlugin *plugin;
};

void
meta_plugin_manager_set_plugin_type (GType gtype)
{
  if (plugin_type != G_TYPE_NONE)
    meta_fatal ("Mutter plugin already set: %s", g_type_name (plugin_type));

  plugin_type = gtype;
}

/*
 * Loads the given plugin.
 */
void
meta_plugin_manager_load (const gchar       *plugin_name)
{
  const gchar *dpath = MUTTER_PLUGIN_DIR "/";
  gchar       *path;
  MetaModule  *module;

  if (g_path_is_absolute (plugin_name))
    path = g_strdup (plugin_name);
  else
    path = g_strconcat (dpath, plugin_name, ".so", NULL);

  module = g_object_new (META_TYPE_MODULE, "path", path, NULL);
  if (!module || !g_type_module_use (G_TYPE_MODULE (module)))
    {
      /* This is fatal under the assumption that a monitoring
       * process like gnome-session will take over and handle
       * our untimely exit.
       */
      g_printerr ("Unable to load plugin module [%s]: %s",
                  path, g_module_error());
      exit (1);
    }

  meta_plugin_manager_set_plugin_type (meta_module_get_plugin_type (module));

  g_type_module_unuse (G_TYPE_MODULE (module));
  g_free (path);
}

static void
on_confirm_display_change (MetaMonitorManager *monitors,
                           MetaPluginManager  *plugin_mgr)
{
  meta_plugin_manager_confirm_display_change (plugin_mgr);
}

MetaPluginManager *
meta_plugin_manager_new (MetaScreen *screen)
{
  MetaPluginManager *plugin_mgr;
  MetaPluginClass *klass;
  MetaPlugin *plugin;
  MetaMonitorManager *monitors;

  plugin_mgr = g_new0 (MetaPluginManager, 1);
  plugin_mgr->screen = screen;
  plugin_mgr->plugin = plugin = g_object_new (plugin_type, "screen", screen, NULL);

  klass = META_PLUGIN_GET_CLASS (plugin);

  if (klass->start)
    klass->start (plugin);

  monitors = meta_monitor_manager_get ();
  g_signal_connect (monitors, "confirm-display-change",
                    G_CALLBACK (on_confirm_display_change), plugin_mgr);

  return plugin_mgr;
}

static void
meta_plugin_manager_kill_window_effects (MetaPluginManager *plugin_mgr,
                                         MetaWindowActor   *actor)
{
  MetaPlugin        *plugin = plugin_mgr->plugin;
  MetaPluginClass   *klass = META_PLUGIN_GET_CLASS (plugin);

  if (klass->kill_window_effects)
    klass->kill_window_effects (plugin, actor);
}

static void
meta_plugin_manager_kill_switch_workspace (MetaPluginManager *plugin_mgr)
{
  MetaPlugin        *plugin = plugin_mgr->plugin;
  MetaPluginClass   *klass = META_PLUGIN_GET_CLASS (plugin);

  if (klass->kill_switch_workspace)
    klass->kill_switch_workspace (plugin);
}

/*
 * Public method that the compositor hooks into for events that require
 * no additional parameters.
 *
 * Returns TRUE if the plugin handled the event type (i.e.,
 * if the return value is FALSE, there will be no subsequent call to the
 * manager completed() callback, and the compositor must ensure that any
 * appropriate post-effect cleanup is carried out.
 */
gboolean
meta_plugin_manager_event_simple (MetaPluginManager *plugin_mgr,
                                  MetaWindowActor   *actor,
                                  unsigned long      event)
{
  MetaPlugin *plugin = plugin_mgr->plugin;
  MetaPluginClass *klass = META_PLUGIN_GET_CLASS (plugin);
  MetaDisplay *display  = meta_screen_get_display (plugin_mgr->screen);
  gboolean retval = FALSE;

  if (display->display_opening)
    return FALSE;

  switch (event)
    {
    case META_PLUGIN_MINIMIZE:
      if (klass->minimize)
        {
          retval = TRUE;
          meta_plugin_manager_kill_window_effects (plugin_mgr,
                                                   actor);

          _meta_plugin_effect_started (plugin);
          klass->minimize (plugin, actor);
        }
      break;
    case META_PLUGIN_MAP:
      if (klass->map)
        {
          retval = TRUE;
          meta_plugin_manager_kill_window_effects (plugin_mgr,
                                                   actor);

          _meta_plugin_effect_started (plugin);
          klass->map (plugin, actor);
        }
      break;
    case META_PLUGIN_DESTROY:
      if (klass->destroy)
        {
          retval = TRUE;
          _meta_plugin_effect_started (plugin);
          klass->destroy (plugin, actor);
        }
      break;
    default:
      g_warning ("Incorrect handler called for event %lu", event);
    }

  return retval;
}

/*
 * The public method that the compositor hooks into for maximize and unmaximize
 * events.
 *
 * Returns TRUE if the plugin handled the event type (i.e.,
 * if the return value is FALSE, there will be no subsequent call to the
 * manager completed() callback, and the compositor must ensure that any
 * appropriate post-effect cleanup is carried out.
 */
gboolean
meta_plugin_manager_event_maximize (MetaPluginManager *plugin_mgr,
                                    MetaWindowActor   *actor,
                                    unsigned long      event,
                                    gint               target_x,
                                    gint               target_y,
                                    gint               target_width,
                                    gint               target_height)
{
  MetaPlugin *plugin = plugin_mgr->plugin;
  MetaPluginClass *klass = META_PLUGIN_GET_CLASS (plugin);
  MetaDisplay *display = meta_screen_get_display (plugin_mgr->screen);
  gboolean retval = FALSE;

  if (display->display_opening)
    return FALSE;

  switch (event)
    {
    case META_PLUGIN_MAXIMIZE:
      if (klass->maximize)
        {
          retval = TRUE;
          meta_plugin_manager_kill_window_effects (plugin_mgr,
                                                   actor);

          _meta_plugin_effect_started (plugin);
          klass->maximize (plugin, actor,
                           target_x, target_y,
                           target_width, target_height);
        }
      break;
    case META_PLUGIN_UNMAXIMIZE:
      if (klass->unmaximize)
        {
          retval = TRUE;
          meta_plugin_manager_kill_window_effects (plugin_mgr,
                                                   actor);

          _meta_plugin_effect_started (plugin);
          klass->unmaximize (plugin, actor,
                             target_x, target_y,
                             target_width, target_height);
        }
      break;
    default:
      g_warning ("Incorrect handler called for event %lu", event);
    }

  return retval;
}

/*
 * The public method that the compositor hooks into for desktop switching.
 *
 * Returns TRUE if the plugin handled the event type (i.e.,
 * if the return value is FALSE, there will be no subsequent call to the
 * manager completed() callback, and the compositor must ensure that any
 * appropriate post-effect cleanup is carried out.
 */
gboolean
meta_plugin_manager_switch_workspace (MetaPluginManager   *plugin_mgr,
                                      gint                 from,
                                      gint                 to,
                                      MetaMotionDirection  direction)
{
  MetaPlugin *plugin = plugin_mgr->plugin;
  MetaPluginClass *klass = META_PLUGIN_GET_CLASS (plugin);
  MetaDisplay *display = meta_screen_get_display (plugin_mgr->screen);
  gboolean retval = FALSE;

  if (display->display_opening)
    return FALSE;

  if (klass->switch_workspace)
    {
      retval = TRUE;
      meta_plugin_manager_kill_switch_workspace (plugin_mgr);

      _meta_plugin_effect_started (plugin);
      klass->switch_workspace (plugin, from, to, direction);
    }

  return retval;
}

gboolean
meta_plugin_manager_filter_keybinding (MetaPluginManager *plugin_mgr,
                                       MetaKeyBinding    *binding)
{
  MetaPlugin *plugin = plugin_mgr->plugin;
  MetaPluginClass *klass = META_PLUGIN_GET_CLASS (plugin);

  if (klass->keybinding_filter)
    return klass->keybinding_filter (plugin, binding);

  return FALSE;
}

gboolean
meta_plugin_manager_xevent_filter (MetaPluginManager *plugin_mgr,
                                   XEvent            *xev)
{
  MetaPlugin *plugin = plugin_mgr->plugin;

  return _meta_plugin_xevent_filter (plugin, xev);
}

void
meta_plugin_manager_confirm_display_change (MetaPluginManager *plugin_mgr)
{
  MetaPlugin *plugin = plugin_mgr->plugin;
  MetaPluginClass *klass = META_PLUGIN_GET_CLASS (plugin);

  if (klass->confirm_display_change)
    return klass->confirm_display_change (plugin);
  else
    return meta_plugin_complete_display_change (plugin, TRUE);
}

gboolean
meta_plugin_manager_show_tile_preview (MetaPluginManager *plugin_mgr,
                                       MetaWindow        *window,
                                       MetaRectangle     *tile_rect,
                                       int                tile_monitor_number)
{
  MetaPlugin *plugin = plugin_mgr->plugin;
  MetaPluginClass *klass = META_PLUGIN_GET_CLASS (plugin);
  MetaDisplay *display  = meta_screen_get_display (plugin_mgr->screen);

  if (display->display_opening)
    return FALSE;

  if (klass->show_tile_preview)
    {
      klass->show_tile_preview (plugin, window, tile_rect, tile_monitor_number);
      return TRUE;
    }

  return FALSE;
}

gboolean
meta_plugin_manager_hide_tile_preview (MetaPluginManager *plugin_mgr)
{
  MetaPlugin *plugin = plugin_mgr->plugin;
  MetaPluginClass *klass = META_PLUGIN_GET_CLASS (plugin);
  MetaDisplay *display  = meta_screen_get_display (plugin_mgr->screen);

  if (display->display_opening)
    return FALSE;

  if (klass->hide_tile_preview)
    {
      klass->hide_tile_preview (plugin);
      return TRUE;
    }

  return FALSE;
}
