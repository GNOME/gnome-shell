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
#include "mutter-plugin-manager.h"
#include "prefs.h"
#include "errors.h"
#include "workspace.h"
#include "mutter-module.h"

#include <string.h>

/*
 * There is only one instace of each module per the process.
 */
static GHashTable *plugin_modules = NULL;

static gboolean mutter_plugin_manager_reload (MutterPluginManager *plugin_mgr);

struct MutterPluginManager
{
  MetaScreen   *screen;

  GList /* MutterPluginPending */ *pending_plugin_modules; /* Plugins not yet fully loaded */
  GList /* MutterPlugin */       *plugins;  /* TODO -- maybe use hash table */
  GList                          *unload;  /* Plugins that are disabled and pending unload */

  guint         idle_unload_id;
};

typedef struct MutterPluginPending
{
  MutterModule *module;
  char *path;
  char *params;
} MutterPluginPending;

/*
 * Checks that the plugin is compatible with the WM and sets up the plugin
 * struct.
 */
static MutterPlugin *
mutter_plugin_load (MutterPluginManager *mgr,
                    MutterModule        *module,
                    const gchar         *params)
{
  MutterPlugin *plugin = NULL;
  GType         plugin_type = mutter_module_get_plugin_type (module);

  if (!plugin_type)
    {
      g_warning ("Plugin type not registered !!!");
      return NULL;
    }

  plugin = g_object_new (plugin_type,
                         "screen", mgr->screen,
                         "params", params,
                         NULL);

  return plugin;
}

/*
 * Attempst to unload a plugin; returns FALSE if plugin cannot be unloaded at
 * present (e.g., and effect is in progress) and should be scheduled for
 * removal later.
 */
static gboolean
mutter_plugin_unload (MutterPlugin *plugin)
{
  if (mutter_plugin_running (plugin))
    {
      g_object_set (plugin, "disabled", TRUE, NULL);
      return FALSE;
    }

  g_object_unref (plugin);

  return TRUE;
}

/*
 * Iddle callback to remove plugins that could not be removed directly and are
 * pending for removal.
 */
static gboolean
mutter_plugin_manager_idle_unload (MutterPluginManager *plugin_mgr)
{
  GList *l = plugin_mgr->unload;
  gboolean dont_remove = TRUE;

  while (l)
    {
      MutterPlugin *plugin = l->data;

      if (mutter_plugin_unload (plugin))
        {
          /* Remove from list */
          GList *p = l->prev;
          GList *n = l->next;

          if (!p)
            plugin_mgr->unload = n;
          else
            p->next = n;

          if (n)
            n->prev = p;

          g_list_free_1 (l);

          l = n;
        }
      else
        l = l->next;
    }

  if (!plugin_mgr->unload)
    {
      /* If no more unloads are pending, remove the handler as well */
      dont_remove = FALSE;
      plugin_mgr->idle_unload_id = 0;
    }

  return dont_remove;
}

/*
 * Unloads all plugins
 */
static void
mutter_plugin_manager_unload (MutterPluginManager *plugin_mgr)
{
  GList *plugins = plugin_mgr->plugins;

  while (plugins)
    {
      MutterPlugin *plugin = plugins->data;

      /* If the plugin could not be removed, move it to the unload list */
      if (!mutter_plugin_unload (plugin))
        {
          plugin_mgr->unload = g_list_prepend (plugin_mgr->unload, plugin);

          if (!plugin_mgr->idle_unload_id)
            {
              plugin_mgr->idle_unload_id = g_idle_add ((GSourceFunc)
                            mutter_plugin_manager_idle_unload,
                            plugin_mgr);
            }
        }

      plugins = plugins->next;
    }

  g_list_free (plugin_mgr->plugins);
  plugin_mgr->plugins = NULL;
}

static void
prefs_changed_callback (MetaPreference pref,
                        void          *data)
{
  MutterPluginManager *plugin_mgr = data;

  if (pref == META_PREF_CLUTTER_PLUGINS)
    {
      mutter_plugin_manager_reload (plugin_mgr);
    }
}

static MutterModule *
mutter_plugin_manager_get_module (const gchar *path)
{
  MutterModule *module = g_hash_table_lookup (plugin_modules, path);

  if (!module &&
      (module = g_object_new (MUTTER_TYPE_MODULE, "path", path, NULL)))
    {
      g_hash_table_insert (plugin_modules, g_strdup (path), module);
    }

  return module;
}

/*
 * Loads all plugins listed in gconf registry.
 */
gboolean
mutter_plugin_manager_load (MutterPluginManager *plugin_mgr)
{
  const gchar *dpath = MUTTER_PLUGIN_DIR "/";
  GSList      *plugins, *fallback = NULL;

  plugins = meta_prefs_get_clutter_plugins ();

  if (!plugins)
    {
      /*
       * If no plugins are specified, try to load the default plugin.
       */
      fallback = g_slist_append (fallback, "default");
      plugins = fallback;
    }

  while (plugins)
    {
      gchar   *plugin_string;
      gchar   *params;

      plugin_string = g_strdup (plugins->data);

      if (plugin_string)
        {
          MutterModule *module;
          gchar        *path;

          params = strchr (plugin_string, ':');

          if (params)
            {
              *params = 0;
              ++params;
            }

          if (g_path_is_absolute (plugin_string))
            path = g_strdup (plugin_string);
          else
            path = g_strconcat (dpath, plugin_string, ".so", NULL);

          module = mutter_plugin_manager_get_module (path);

          if (module)
            {
              gboolean      use_succeeded;

              /*
               * This dlopens the module and registers the plugin type with the
               * GType system, if the module is not already loaded.  When we
               * create a plugin, the type system also calls g_type_module_use()
               * to guarantee the module will not be unloaded during the plugin
               * life time. Consequently we can unuse() the module again.
               */
              use_succeeded = g_type_module_use (G_TYPE_MODULE (module));

              if (use_succeeded)
                {
                  MutterPluginPending *pending = g_new0 (MutterPluginPending, 1);
                  pending->module = module;
                  pending->path = g_strdup (path);
                  pending->params = g_strdup (params);
                  plugin_mgr->pending_plugin_modules =
                    g_list_prepend (plugin_mgr->pending_plugin_modules, pending);
                }
            }
          else
            g_warning ("Unable to load plugin module [%s]: %s",
                       path, g_module_error());

          g_free (path);
          g_free (plugin_string);
        }

      plugins = plugins->next;
    }


  if (fallback)
    g_slist_free (fallback);

  if (plugin_mgr->pending_plugin_modules != NULL)
    {
      meta_prefs_add_listener (prefs_changed_callback, plugin_mgr);
      return TRUE;
    }

  return FALSE;
}

gboolean
mutter_plugin_manager_initialize (MutterPluginManager *plugin_mgr)
{
  GList *iter;

  for (iter = plugin_mgr->pending_plugin_modules; iter; iter = iter->next)
    {
      MutterPluginPending *pending = (MutterPluginPending*) iter->data;
      MutterPlugin *p;

      if ((p = mutter_plugin_load (plugin_mgr, pending->module, pending->params)))
        {
          plugin_mgr->plugins = g_list_prepend (plugin_mgr->plugins, p);
        }
      else
        {
          g_warning ("Plugin load for [%s] failed", pending->path);
        }

      g_type_module_unuse (G_TYPE_MODULE (pending->module));
      g_free (pending->path);
      g_free (pending->params);
      g_free (pending);
    }
  g_list_free (plugin_mgr->pending_plugin_modules);
  plugin_mgr->pending_plugin_modules = NULL;
  return TRUE;
}

/*
 * Reloads all plugins
 */
static gboolean
mutter_plugin_manager_reload (MutterPluginManager *plugin_mgr)
{
  /* TODO -- brute force; should we build a list of plugins to load and list of
   * plugins to unload? We are probably not going to have large numbers of
   * plugins loaded at the same time, so it might not be worth it.
   */
  mutter_plugin_manager_unload (plugin_mgr);
  return mutter_plugin_manager_load (plugin_mgr);
}

MutterPluginManager *
mutter_plugin_manager_new (MetaScreen *screen)
{
  MutterPluginManager *plugin_mgr;

  if (!plugin_modules)
    {
      plugin_modules = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                              NULL);
    }

  plugin_mgr = g_new0 (MutterPluginManager, 1);

  plugin_mgr->screen        = screen;

  return plugin_mgr;
}

static void
mutter_plugin_manager_kill_effect (MutterPluginManager *plugin_mgr,
                                   MutterWindow        *actor,
                                   unsigned long        events)
{
  GList *l = plugin_mgr->plugins;

  while (l)
    {
      MutterPlugin        *plugin = l->data;
      MutterPluginClass   *klass = MUTTER_PLUGIN_GET_CLASS (plugin);

      if (!mutter_plugin_disabled (plugin)
	  && (mutter_plugin_features (plugin) & events)
	  && klass->kill_effect)
        klass->kill_effect (plugin, actor, events);

      l = l->next;
    }
}

#define ALL_BUT_SWITCH \
  MUTTER_PLUGIN_ALL_EFFECTS & \
  ~MUTTER_PLUGIN_SWITCH_WORKSPACE
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
mutter_plugin_manager_event_simple (MutterPluginManager *plugin_mgr,
                                    MutterWindow        *actor,
                                    unsigned long        event)
{
  GList *l = plugin_mgr->plugins;
  gboolean retval = FALSE;

  while (l)
    {
      MutterPlugin        *plugin = l->data;
      MutterPluginClass   *klass = MUTTER_PLUGIN_GET_CLASS (plugin);

      if (!mutter_plugin_disabled (plugin) &&
          (mutter_plugin_features (plugin) & event))
        {
          retval = TRUE;

          switch (event)
            {
            case MUTTER_PLUGIN_MINIMIZE:
              if (klass->minimize)
                {
                  mutter_plugin_manager_kill_effect (
		      plugin_mgr,
		      actor,
		      ALL_BUT_SWITCH);

                  _mutter_plugin_effect_started (plugin);
                  klass->minimize (plugin, actor);
                }
              break;
            case MUTTER_PLUGIN_MAP:
              if (klass->map)
                {
                  mutter_plugin_manager_kill_effect (
		      plugin_mgr,
		      actor,
		      ALL_BUT_SWITCH);

                  _mutter_plugin_effect_started (plugin);
                  klass->map (plugin, actor);
                }
              break;
            case MUTTER_PLUGIN_DESTROY:
              if (klass->destroy)
                {
                  _mutter_plugin_effect_started (plugin);
                  klass->destroy (plugin, actor);
                }
              break;
            default:
              g_warning ("Incorrect handler called for event %lu", event);
            }
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
mutter_plugin_manager_event_maximize (MutterPluginManager *plugin_mgr,
                                      MutterWindow        *actor,
                                      unsigned long        event,
                                      gint                 target_x,
                                      gint                 target_y,
                                      gint                 target_width,
                                      gint                 target_height)
{
  GList *l = plugin_mgr->plugins;
  gboolean retval = FALSE;

  while (l)
    {
      MutterPlugin        *plugin = l->data;
      MutterPluginClass   *klass = MUTTER_PLUGIN_GET_CLASS (plugin);

      if (!mutter_plugin_disabled (plugin) &&
          (mutter_plugin_features (plugin) & event))
        {
          retval = TRUE;

          switch (event)
            {
            case MUTTER_PLUGIN_MAXIMIZE:
              if (klass->maximize)
                {
                  mutter_plugin_manager_kill_effect (
		      plugin_mgr,
		      actor,
		      ALL_BUT_SWITCH);

                  _mutter_plugin_effect_started (plugin);
                  klass->maximize (plugin, actor,
                                   target_x, target_y,
                                   target_width, target_height);
                }
              break;
            case MUTTER_PLUGIN_UNMAXIMIZE:
              if (klass->unmaximize)
                {
                  mutter_plugin_manager_kill_effect (
		      plugin_mgr,
		      actor,
		      ALL_BUT_SWITCH);

                  _mutter_plugin_effect_started (plugin);
                  klass->unmaximize (plugin, actor,
                                     target_x, target_y,
                                     target_width, target_height);
                }
              break;
            default:
              g_warning ("Incorrect handler called for event %lu", event);
            }
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
mutter_plugin_manager_switch_workspace (MutterPluginManager *plugin_mgr,
                                        const GList        **actors,
                                        gint                 from,
                                        gint                 to,
                                        MetaMotionDirection  direction)
{
  GList *l = plugin_mgr->plugins;
  gboolean retval = FALSE;

  while (l)
    {
      MutterPlugin        *plugin = l->data;
      MutterPluginClass   *klass = MUTTER_PLUGIN_GET_CLASS (plugin);

      if (!mutter_plugin_disabled (plugin) &&
          (mutter_plugin_features (plugin) & MUTTER_PLUGIN_SWITCH_WORKSPACE) &&
          (actors && *actors))
        {
          if (klass->switch_workspace)
            {
              retval = TRUE;
              mutter_plugin_manager_kill_effect (
		  plugin_mgr,
		  MUTTER_WINDOW ((*actors)->data),
		  MUTTER_PLUGIN_SWITCH_WORKSPACE);

              _mutter_plugin_effect_started (plugin);
              klass->switch_workspace (plugin, actors, from, to, direction);
            }
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
mutter_plugin_manager_xevent_filter (MutterPluginManager *plugin_mgr,
                                     XEvent              *xev)
{
  GList *l;

  if (!plugin_mgr)
    return FALSE;

  l = plugin_mgr->plugins;

  while (l)
    {
      MutterPlugin      *plugin = l->data;
      MutterPluginClass *klass = MUTTER_PLUGIN_GET_CLASS (plugin);

      if (klass->xevent_filter)
        {
          if (klass->xevent_filter (plugin, xev) == TRUE)
            return TRUE;
        }

      l = l->next;
    }

  return FALSE;
}
