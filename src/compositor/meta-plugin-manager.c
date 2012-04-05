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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
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

static GSList *plugin_types;

/*
 * We have one "default plugin manager" that acts for the first screen,
 * but also can be used before we open any screens, and additional
 * plugin managers for each screen. (This is ugly. Probably we should
 * have one plugin manager and only make the plugins per-screen.)
 */
static MetaPluginManager *default_plugin_manager;

struct MetaPluginManager
{
  MetaScreen   *screen;

  GList /* MetaPlugin */ *plugins;  /* TODO -- maybe use hash table */
};

/*
 * Loads the given plugin.
 */
void
meta_plugin_manager_load (MetaPluginManager *plugin_mgr,
                          const gchar       *plugin_name)
{
  const gchar *dpath = MUTTER_PLUGIN_DIR "/";
  gchar       *path;
  MetaModule  *module;
  GType        plugin_type;

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

  plugin_type = meta_module_get_plugin_type (module);
  meta_plugin_manager_register (plugin_mgr, plugin_type);

  g_type_module_unuse (G_TYPE_MODULE (module));
  g_free (path);
}

/*
 * Registers the given plugin type
 */
void
meta_plugin_manager_register (MetaPluginManager *plugin_mgr,
                              GType              plugin_type)
{
  MetaPlugin  *plugin;

  plugin_types = g_slist_prepend (plugin_types, GSIZE_TO_POINTER (plugin_type));

  plugin = g_object_new (plugin_type, NULL);
  plugin_mgr->plugins = g_list_prepend (plugin_mgr->plugins, plugin);
}

void
meta_plugin_manager_initialize (MetaPluginManager *plugin_mgr)
{
  GList *iter;

  if (!plugin_mgr->plugins)
    {
      /*
       * If no plugins are specified, load the default plugin.
       */
      meta_plugin_manager_load (plugin_mgr, "default");
    }

  for (iter = plugin_mgr->plugins; iter; iter = iter->next)
    {
      MetaPlugin *plugin = (MetaPlugin*) iter->data;
      MetaPluginClass *klass = META_PLUGIN_GET_CLASS (plugin);

      g_object_set (plugin,
                    "screen", plugin_mgr->screen,
                    NULL);

      if (klass->start)
        klass->start (plugin);
    }
}

static MetaPluginManager *
meta_plugin_manager_new (MetaScreen *screen)
{
  MetaPluginManager *plugin_mgr;

  plugin_mgr = g_new0 (MetaPluginManager, 1);
  plugin_mgr->screen = screen;

  if (screen)
    g_object_set_data (G_OBJECT (screen), "meta-plugin-manager", plugin_mgr);

  return plugin_mgr;
}

MetaPluginManager *
meta_plugin_manager_get_default (void)
{
  if (!default_plugin_manager)
    {
      default_plugin_manager = meta_plugin_manager_new (NULL);
    }

  return default_plugin_manager;
}

MetaPluginManager *
meta_plugin_manager_get (MetaScreen *screen)
{
  MetaPluginManager *plugin_mgr;

  plugin_mgr = g_object_get_data (G_OBJECT (screen), "meta-plugin-manager");
  if (plugin_mgr)
    return plugin_mgr;

  if (!default_plugin_manager)
    meta_plugin_manager_get_default ();

  if (!default_plugin_manager->screen)
    {
      /* The default plugin manager is so far unused, we can recycle it */
      default_plugin_manager->screen = screen;
      g_object_set_data (G_OBJECT (screen), "meta-plugin-manager", default_plugin_manager);

      return default_plugin_manager;
    }
  else
    {
      GSList *iter;
      GType plugin_type;
      MetaPlugin *plugin;

      plugin_mgr = meta_plugin_manager_new (screen);

      for (iter = plugin_types; iter; iter = iter->next)
        {
          plugin_type = (GType)GPOINTER_TO_SIZE (iter->data);
          plugin = g_object_new (plugin_type, "screen", screen,  NULL);
          plugin_mgr->plugins = g_list_prepend (plugin_mgr->plugins, plugin);
        }

      return plugin_mgr;
    }
}

static void
meta_plugin_manager_kill_window_effects (MetaPluginManager *plugin_mgr,
                                         MetaWindowActor   *actor)
{
  GList *l = plugin_mgr->plugins;

  while (l)
    {
      MetaPlugin        *plugin = l->data;
      MetaPluginClass   *klass = META_PLUGIN_GET_CLASS (plugin);

      if (klass->kill_window_effects)
        klass->kill_window_effects (plugin, actor);

      l = l->next;
    }
}

static void
meta_plugin_manager_kill_switch_workspace (MetaPluginManager *plugin_mgr)
{
  GList *l = plugin_mgr->plugins;

  while (l)
    {
      MetaPlugin        *plugin = l->data;
      MetaPluginClass   *klass = META_PLUGIN_GET_CLASS (plugin);

      if (klass->kill_switch_workspace)
        klass->kill_switch_workspace (plugin);

      l = l->next;
    }
}

/*
 * Public method that the compositor hooks into for events that require
 * no additional parameters.
 *
 * Returns TRUE if at least one of the plugins handled the event type (i.e.,
 * if the return value is FALSE, there will be no subsequent call to the
 * manager completed() callback, and the compositor must ensure that any
 * appropriate post-effect cleanup is carried out.
 */
gboolean
meta_plugin_manager_event_simple (MetaPluginManager *plugin_mgr,
                                  MetaWindowActor   *actor,
                                  unsigned long      event)
{
  GList *l = plugin_mgr->plugins;
  gboolean retval = FALSE;
  MetaDisplay *display  = meta_screen_get_display (plugin_mgr->screen);

  if (display->display_opening)
    return FALSE;

  while (l)
    {
      MetaPlugin        *plugin = l->data;
      MetaPluginClass   *klass = META_PLUGIN_GET_CLASS (plugin);

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

      l = l->next;
    }

  return retval;
}

/*
 * The public method that the compositor hooks into for maximize and unmaximize
 * events.
 *
 * Returns TRUE if at least one of the plugins handled the event type (i.e.,
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
  GList *l = plugin_mgr->plugins;
  gboolean retval = FALSE;
  MetaDisplay *display  = meta_screen_get_display (plugin_mgr->screen);

  if (display->display_opening)
    return FALSE;

  while (l)
    {
      MetaPlugin        *plugin = l->data;
      MetaPluginClass   *klass = META_PLUGIN_GET_CLASS (plugin);

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

      l = l->next;
    }

  return retval;
}

/*
 * The public method that the compositor hooks into for desktop switching.
 *
 * Returns TRUE if at least one of the plugins handled the event type (i.e.,
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
  GList *l = plugin_mgr->plugins;
  gboolean retval = FALSE;
  MetaDisplay *display  = meta_screen_get_display (plugin_mgr->screen);

  if (display->display_opening)
    return FALSE;

  while (l)
    {
      MetaPlugin        *plugin = l->data;
      MetaPluginClass   *klass = META_PLUGIN_GET_CLASS (plugin);

      if (klass->switch_workspace)
        {
          retval = TRUE;
          meta_plugin_manager_kill_switch_workspace (plugin_mgr);

          _meta_plugin_effect_started (plugin);
          klass->switch_workspace (plugin, from, to, direction);
        }

      l = l->next;
    }

  return retval;
}

/*
 * The public method that the compositor hooks into for desktop switching.
 *
 * Returns TRUE if at least one of the plugins handled the event type (i.e.,
 * if the return value is FALSE, there will be no subsequent call to the
 * manager completed() callback, and the compositor must ensure that any
 * appropriate post-effect cleanup is carried out.
 */
gboolean
meta_plugin_manager_xevent_filter (MetaPluginManager *plugin_mgr,
                                   XEvent            *xev)
{
  GList *l;
  gboolean have_plugin_xevent_func;

  if (!plugin_mgr)
    return FALSE;

  l = plugin_mgr->plugins;

  /* We need to make sure that clutter gets certain events, like
   * ConfigureNotify on the stage window. If there is a plugin that
   * provides an xevent_filter function, then it's the responsibility
   * of that plugin to pass events to Clutter. Otherwise, we send the
   * event directly to Clutter ourselves.
   *
   * What happens if there are two plugins with xevent_filter functions
   * is undefined; in general, multiple competing plugins are something
   * we don't support well or care much about.
   *
   * FIXME: Really, we should just always handle sending the event to
   *  clutter if a plugin doesn't report the event as handled by
   *  returning TRUE, but it doesn't seem worth breaking compatibility
   *  of the plugin interface right now to achieve this; the way it is
   *  now works fine in practice.
   */
  have_plugin_xevent_func = FALSE;

  while (l)
    {
      MetaPlugin      *plugin = l->data;
      MetaPluginClass *klass = META_PLUGIN_GET_CLASS (plugin);

      if (klass->xevent_filter)
        {
          have_plugin_xevent_func = TRUE;
          if (klass->xevent_filter (plugin, xev) == TRUE)
            return TRUE;
        }

      l = l->next;
    }

  if (!have_plugin_xevent_func)
    return clutter_x11_handle_event (xev) != CLUTTER_X11_FILTER_CONTINUE;

  return FALSE;
}
