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

#include "compositor-clutter-plugin-manager.h"
#include "compositor-clutter.h"
#include "prefs.h"
#include "errors.h"
#include "workspace.h"

#include <gmodule.h>
#include <string.h>

static gboolean meta_compositor_clutter_plugin_manager_reload (MetaCompositorClutterPluginManager *mgr);

struct MetaCompositorClutterPluginManager
{
  MetaScreen   *screen;

  GList        *plugins; /* TODO -- maybe use hash table */
  GList        *unload;  /* Plugins that are disabled and pending unload */

  guint         idle_unload_id;
};

typedef struct MetaCompositorClutterPluginPrivate MetaCompositorClutterPluginPrivate;

struct MetaCompositorClutterPluginPrivate
{
  MetaCompositorClutterPluginManager *self;
  GModule                            *module;

  gboolean disabled : 1;
};

/*
 * This function gets called when an effect completes. It is responsible for
 * any post-effect cleanup.
 */
static void
meta_compositor_clutter_effect_completed (MetaCompositorClutterPlugin *plugin,
                                          MetaCompWindow              *actor,
                                          unsigned long                event)
{
  if (!actor)
    {
      g_warning ("Plugin [%s] passed NULL for actor!",
                 (plugin && plugin->name) ? plugin->name : "unknown");
    }

  meta_compositor_clutter_window_effect_completed (actor, event);
}

static void
free_plugin_workspaces (MetaCompositorClutterPlugin *plg)
{
  GList *l;

  l = plg->work_areas;

  while (l)
    {
      g_free (l->data);
      l = l->next;
    }

  if (plg->work_areas)
    g_list_free (plg->work_areas);

  plg->work_areas = NULL;
}

/*
 * Gets work area geometry and stores it in list in the plugin.
 *
 * If the plg list is already populated, we simply replace it (we are dealing
 * with a small number of items in the list and unfrequent changes).
 */
static void
update_plugin_workspaces (MetaScreen                  *screen,
                          MetaCompositorClutterPlugin *plg)
{
  GList *l, *l2 = NULL;

  l = meta_screen_get_workspaces (screen);

  while (l)
    {
      MetaWorkspace  *w = l->data;
      PluginWorkspaceRectangle *r;

      r = g_new0 (PluginWorkspaceRectangle, 1);

      meta_workspace_get_work_area_all_xineramas (w, (MetaRectangle*)r);

      l2 = g_list_append (l2, r);

      l = l->next;
    }

  free_plugin_workspaces (plg);

  plg->work_areas = l2;
}

/*
 * Checks that the plugin is compatible with the WM and sets up the plugin
 * struct.
 */
static MetaCompositorClutterPlugin *
meta_compositor_clutter_plugin_load (MetaCompositorClutterPluginManager *mgr,
                                     GModule                            *module,
                                     const gchar                        *params)
{
  MetaCompositorClutterPlugin *plg;

  if (g_module_symbol (module,
                       META_COMPOSITOR_CLUTTER_PLUGIN_STRUCT_NAME,
                       (gpointer *)&plg))
    {
      if (plg->version_api == METACITY_CLUTTER_PLUGIN_API_VERSION)
        {
          MetaCompositorClutterPluginPrivate *priv;
          gboolean (*init_func) (void);

          priv                 = g_new0 (MetaCompositorClutterPluginPrivate, 1);
          plg->params          = g_strdup (params);
          plg->completed       = meta_compositor_clutter_effect_completed;
          plg->manager_private = priv;
          priv->module         = module;
          priv->self           = mgr;

          meta_screen_get_size (mgr->screen,
                                &plg->screen_width, &plg->screen_height);

          update_plugin_workspaces (mgr->screen, plg);

          /*
           * Check for and run the plugin init function.
           */
          if (g_module_symbol (module,
                                META_COMPOSITOR_CLUTTER_PLUGIN_INIT_FUNC_NAME,
                                (gpointer *)&init_func) &&
              !init_func())
            {
              g_free (plg->params);
              g_free (priv);

              free_plugin_workspaces (plg);

              return NULL;
            }

          meta_verbose ("Loaded plugin [%s]\n", plg->name);

          return plg;
        }
    }

  return NULL;
}

/*
 * Attempst to unload a plugin; returns FALSE if plugin cannot be unloaded at
 * present (e.g., and effect is in progress) and should be scheduled for
 * removal later.
 */
static gboolean
meta_compositor_clutter_plugin_unload (MetaCompositorClutterPlugin *plg)
{
  MetaCompositorClutterPluginPrivate *priv;
  GModule *module;

  priv = plg->manager_private;
  module = priv->module;

  if (plg->running)
    {
      priv->disabled = TRUE;
      return FALSE;
    }

  g_free (plg->params);
  plg->params = NULL;

  g_free (priv);
  plg->manager_private = NULL;

  g_module_close (module);

  return TRUE;
}

/*
 * Iddle callback to remove plugins that could not be removed directly and are
 * pending for removal.
 */
static gboolean
meta_compositor_clutter_plugin_manager_idle_unload (MetaCompositorClutterPluginManager *mgr)
{
  GList *l = mgr->unload;
  gboolean dont_remove = TRUE;

  while (l)
    {
      MetaCompositorClutterPlugin *plg = l->data;

      if (meta_compositor_clutter_plugin_unload (plg))
        {
          /* Remove from list */
          GList *p = l->prev;
          GList *n = l->next;

          if (!p)
            mgr->unload = n;
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

  if (!mgr->unload)
    {
      /* If no more unloads are pending, remove the handler as well */
      dont_remove = FALSE;
      mgr->idle_unload_id = 0;
    }

  return dont_remove;
}

/*
 * Unloads all plugins
 */
static void
meta_compositor_clutter_plugin_manager_unload (MetaCompositorClutterPluginManager *mgr)
{
  GList *plugins = mgr->plugins;

  while (plugins)
    {
      MetaCompositorClutterPlugin *plg = plugins->data;

      /* If the plugin could not be removed, move it to the unload list */
      if (!meta_compositor_clutter_plugin_unload (plg))
        {
          mgr->unload = g_list_prepend (mgr->unload, plg);

          if (!mgr->idle_unload_id)
            {
              mgr->idle_unload_id = g_idle_add ((GSourceFunc)
                            meta_compositor_clutter_plugin_manager_idle_unload,
                            mgr);
            }
        }

      plugins = plugins->next;
    }

  g_list_free (mgr->plugins);
  mgr->plugins = NULL;
}

static void
prefs_changed_callback (MetaPreference pref,
                        void          *data)
{
  MetaCompositorClutterPluginManager *mgr = data;

  if (pref == META_PREF_CLUTTER_PLUGINS)
    {
      meta_compositor_clutter_plugin_manager_reload (mgr);
    }
  else if (pref == META_PREF_NUM_WORKSPACES)
    {
      meta_compositor_clutter_plugin_manager_update_workspaces (mgr);
    }
}

/*
 * Loads all plugins listed in gconf registry.
 */
static gboolean
meta_compositor_clutter_plugin_manager_load (MetaCompositorClutterPluginManager *mgr)
{
  const gchar *dpath = METACITY_PKGLIBDIR "/plugins/clutter/";
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
      gchar   *plg_string;
      gchar   *params;

      plg_string = g_strdup (plugins->data);

      if (plg_string)
        {
          GModule *plg;
          gchar   *path;

          params = strchr (plg_string, ':');

          if (params)
            {
              *params = 0;
              ++params;
            }

          path = g_strconcat (dpath, plg_string, ".so", NULL);

          if ((plg = g_module_open (path, 0)))
            {
              MetaCompositorClutterPlugin *p;

              if ((p = meta_compositor_clutter_plugin_load (mgr,
                                                            plg, params)))
                mgr->plugins = g_list_prepend (mgr->plugins, p);
              else
                {
                  g_message ("Plugin load for [%s] failed\n", path);
                  g_module_close (plg);
                }
            }
          else
            g_message ("Unable to load plugin [%s]\n", path);

          g_free (path);
          g_free (plg_string);
        }

      plugins = plugins->next;
    }


  if (fallback)
    g_slist_free (fallback);

  if (mgr->plugins != NULL)
    {
      meta_prefs_add_listener (prefs_changed_callback, mgr);
      return TRUE;
    }

  return FALSE;
}

/*
 * Reloads all plugins
 */
static gboolean
meta_compositor_clutter_plugin_manager_reload (MetaCompositorClutterPluginManager *mgr)
{
  /* TODO -- brute force; should we build a list of plugins to load and list of
   * plugins to unload? We are probably not going to have large numbers of
   * plugins loaded at the same time, so it might not be worth it.
   */
  meta_compositor_clutter_plugin_manager_unload (mgr);
  return meta_compositor_clutter_plugin_manager_load (mgr);
}

static gboolean
meta_compositor_clutter_plugin_manager_init (MetaCompositorClutterPluginManager *mgr)
{
  return meta_compositor_clutter_plugin_manager_load (mgr);
}

void
meta_compositor_clutter_plugin_manager_update_workspace (MetaCompositorClutterPluginManager *mgr, MetaWorkspace *w)
{
  GList *l;
  gint   n;

  n = meta_workspace_index (w);
  l = mgr->plugins;

  while (l)
    {
      MetaCompositorClutterPlugin *plg = l->data;
      PluginWorkspaceRectangle    *r = g_list_nth_data (plg->work_areas, n);

      if (r)
        {
          meta_workspace_get_work_area_all_xineramas (w, (MetaRectangle*)r);
        }
      else
        {
          /* Something not entirely right; redo the whole thing */
          update_plugin_workspaces (mgr->screen, plg);
          return;
        }

      l = l->next;
    }
}

void
meta_compositor_clutter_plugin_manager_update_workspaces (MetaCompositorClutterPluginManager *mgr)
{
  GList *l;

  l = mgr->plugins;
  while (l)
    {
      MetaCompositorClutterPlugin *plg = l->data;

      update_plugin_workspaces (mgr->screen, plg);

      l = l->next;
    }
}

MetaCompositorClutterPluginManager *
meta_compositor_clutter_plugin_manager_new (MetaScreen *screen)
{
  MetaCompositorClutterPluginManager *mgr;

  mgr = g_new0 (MetaCompositorClutterPluginManager, 1);

  mgr->screen        = screen;

  if (!meta_compositor_clutter_plugin_manager_init (mgr))
    {
      g_free (mgr);
      mgr = NULL;
    }

  return mgr;
}

static void
meta_compositor_clutter_plugin_manager_kill_effect (MetaCompositorClutterPluginManager *mgr,
                                                    MetaCompWindow *actor,
                                                    unsigned long   events)
{
  GList *l = mgr->plugins;

  while (l)
    {
      MetaCompositorClutterPlugin        *plg = l->data;
      MetaCompositorClutterPluginPrivate *priv = plg->manager_private;

      if (!priv->disabled && (plg->features & events) && plg->kill_effect)
        plg->kill_effect (actor, events);

      l = l->next;
    }
}

#define ALL_BUT_SWITCH \
  META_COMPOSITOR_CLUTTER_PLUGIN_ALL_EFFECTS & \
  ~META_COMPOSITOR_CLUTTER_PLUGIN_SWITCH_WORKSPACE
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
meta_compositor_clutter_plugin_manager_event_simple (MetaCompositorClutterPluginManager *mgr,
                                                     MetaCompWindow  *actor,
                                                     unsigned long    event)
{
  GList *l = mgr->plugins;
  gboolean retval = FALSE;

  while (l)
    {
      MetaCompositorClutterPlugin        *plg = l->data;
      MetaCompositorClutterPluginPrivate *priv = plg->manager_private;

      if (!priv->disabled && (plg->features & event))
        {
          retval = TRUE;

          switch (event)
            {
            case META_COMPOSITOR_CLUTTER_PLUGIN_MINIMIZE:
              if (plg->minimize)
                {
                  meta_compositor_clutter_plugin_manager_kill_effect (mgr,
                                                                actor,
                                                                ALL_BUT_SWITCH);
                  plg->minimize (actor);
                }
              break;
            case META_COMPOSITOR_CLUTTER_PLUGIN_MAP:
              if (plg->map)
                {
                  meta_compositor_clutter_plugin_manager_kill_effect (mgr,
                                                                actor,
                                                                ALL_BUT_SWITCH);
                  plg->map (actor);
                }
              break;
            case META_COMPOSITOR_CLUTTER_PLUGIN_DESTROY:
              if (plg->destroy)
                {
                  plg->destroy (actor);
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
meta_compositor_clutter_plugin_manager_event_maximize (MetaCompositorClutterPluginManager *mgr,
                                                 MetaCompWindow  *actor,
                                                 unsigned long    event,
                                                 gint             target_x,
                                                 gint             target_y,
                                                 gint             target_width,
                                                 gint             target_height)
{
  GList *l = mgr->plugins;
  gboolean retval = FALSE;

  while (l)
    {
      MetaCompositorClutterPlugin        *plg = l->data;
      MetaCompositorClutterPluginPrivate *priv = plg->manager_private;

      if (!priv->disabled && (plg->features & event))
        {
          retval = TRUE;

          switch (event)
            {
            case META_COMPOSITOR_CLUTTER_PLUGIN_MAXIMIZE:
              if (plg->maximize)
                {
                  meta_compositor_clutter_plugin_manager_kill_effect (mgr,
                                                                actor,
                                                                ALL_BUT_SWITCH);
                  plg->maximize (actor,
                                 target_x, target_y,
                                 target_width, target_height);
                }
              break;
            case META_COMPOSITOR_CLUTTER_PLUGIN_UNMAXIMIZE:
              if (plg->unmaximize)
                {
                  meta_compositor_clutter_plugin_manager_kill_effect (mgr,
                                                                actor,
                                                                ALL_BUT_SWITCH);
                  plg->unmaximize (actor,
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
meta_compositor_clutter_plugin_manager_switch_workspace (MetaCompositorClutterPluginManager *mgr,
                                                         const GList **actors,
                                                         gint          from,
                                                         gint          to,
                                                         MetaMotionDirection direction)
{
  GList *l = mgr->plugins;
  gboolean retval = FALSE;

  while (l)
    {
      MetaCompositorClutterPlugin        *plg = l->data;
      MetaCompositorClutterPluginPrivate *priv = plg->manager_private;

      if (!priv->disabled &&
          (plg->features & META_COMPOSITOR_CLUTTER_PLUGIN_SWITCH_WORKSPACE) &&
          (actors && *actors))
        {
          if (plg->switch_workspace)
            {
              retval = TRUE;
              meta_compositor_clutter_plugin_manager_kill_effect (mgr,
                                            META_COMP_WINDOW ((*actors)->data),
                            META_COMPOSITOR_CLUTTER_PLUGIN_SWITCH_WORKSPACE);
              plg->switch_workspace (actors, from, to, direction);
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
meta_compositor_clutter_plugin_manager_xevent_filter
                        (MetaCompositorClutterPluginManager *mgr, XEvent *xev)
{
  GList *l;

  if (!mgr)
    return FALSE;

  l = mgr->plugins;

  while (l)
    {
      MetaCompositorClutterPlugin        *plg = l->data;

      if (plg->xevent_filter)
        {
          if (plg->xevent_filter (xev) == TRUE)
            return TRUE;
        }

      l = l->next;
    }

  return FALSE;
}

/*
 * Public accessors for plugins, exposed from compositor-clutter-plugin.h
 */
ClutterActor *
meta_comp_clutter_plugin_get_overlay_group (MetaCompositorClutterPlugin *plugin)
{
  MetaCompositorClutterPluginPrivate *priv = plugin->manager_private;
  MetaCompositorClutterPluginManager *mgr  = priv->self;

  return meta_compositor_clutter_get_overlay_group_for_screen (mgr->screen);
}

ClutterActor *
meta_comp_clutter_plugin_get_stage (MetaCompositorClutterPlugin *plugin)
{
  MetaCompositorClutterPluginPrivate *priv = plugin->manager_private;
  MetaCompositorClutterPluginManager *mgr  = priv->self;

  return meta_compositor_clutter_get_stage_for_screen (mgr->screen);
}
