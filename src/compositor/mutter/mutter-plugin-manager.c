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

#include <gmodule.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <clutter/x11/clutter-x11.h>

static gboolean mutter_plugin_manager_reload (
		      MutterPluginManager *plugin_mgr);

struct MutterPluginManager
{
  MetaScreen   *screen;

  GList        *plugins; /* TODO -- maybe use hash table */
  GList        *unload;  /* Plugins that are disabled and pending unload */

  guint         idle_unload_id;
};

typedef struct MutterPluginPrivate
{
  char				     *name;
  MutterPluginManager *self;
  GModule                            *module;
  gulong			      features;
  /* We use this to track the number of effects currently being managed
   * by a plugin. Currently this is used to block unloading while effects
   * are in progress. */
  gint				      running;

  gboolean disabled : 1;
} MutterPluginPrivate;


static void
free_plugin_workspaces (MutterPlugin *plugin)
{
  GList *l;

  l = plugin->work_areas;

  while (l)
    {
      g_free (l->data);
      l = l->next;
    }

  if (plugin->work_areas)
    g_list_free (plugin->work_areas);

  plugin->work_areas = NULL;
}

/*
 * Gets work area geometry and stores it in list in the plugin.
 *
 * If the plugin list is already populated, we simply replace it (we are
 * dealing with a small number of items in the list and unfrequent changes).
 */
static void
update_plugin_workspaces (MetaScreen                  *screen,
                          MutterPlugin *plugin)
{
  GList *l, *l2 = NULL;

  l = meta_screen_get_workspaces (screen);

  while (l)
    {
      MetaWorkspace  *w = l->data;
      MetaRectangle *r;

      r = g_new0 (MetaRectangle, 1);

      meta_workspace_get_work_area_all_xineramas (w, (MetaRectangle*)r);

      l2 = g_list_append (l2, r);

      l = l->next;
    }

  free_plugin_workspaces (plugin);

  plugin->work_areas = l2;
}

/**
 * parse_disable_params:
 * @params: as read from gconf, a ':' seperated list of plugin options
 * @features: The mask of features the plugin advertises
 *
 * This function returns a new mask of features removing anything that
 * the user has disabled.
 */
static gulong
parse_disable_params (const char *params, MutterPlugin *plugin)
{
  char  *p;
  gulong features = 0;

/*
 * Feature flags: identify events that the plugin can handle; a plugin can
 * handle one or more events.
 */
  if (plugin->minimize)
    features |= MUTTER_PLUGIN_MINIMIZE;

  if (plugin->maximize)
    features |= MUTTER_PLUGIN_MAXIMIZE;

  if (plugin->unmaximize)
    features |= MUTTER_PLUGIN_UNMAXIMIZE;

  if (plugin->map)
    features |= MUTTER_PLUGIN_MAP;

  if (plugin->destroy)
    features |= MUTTER_PLUGIN_DESTROY;

  if (plugin->switch_workspace)
    features |= MUTTER_PLUGIN_SWITCH_WORKSPACE;

  if (!params)
    return features;

  if ((p = strstr (params, "disable:")))
    {
      gchar *d = g_strdup (p+8);

      p = strchr (d, ';');

      if (p)
	*p = 0;

      if (strstr (d, "minimize"))
	features &= ~ MUTTER_PLUGIN_MINIMIZE;

      if (strstr (d, "maximize"))
	features &= ~ MUTTER_PLUGIN_MAXIMIZE;

      if (strstr (d, "unmaximize"))
	features &= ~ MUTTER_PLUGIN_UNMAXIMIZE;

      if (strstr (d, "map"))
	features &= ~ MUTTER_PLUGIN_MAP;

      if (strstr (d, "destroy"))
	features &= ~ MUTTER_PLUGIN_DESTROY;

      if (strstr (d, "switch-workspace"))
	features &= ~MUTTER_PLUGIN_SWITCH_WORKSPACE;

      g_free (d);
    }
  return features;
}

/*
 * Checks that the plugin is compatible with the WM and sets up the plugin
 * struct.
 */
static MutterPlugin *
mutter_plugin_load (MutterPluginManager *plugin_mgr,
                    GModule             *module,
                    const gchar         *params)
{
  MutterPlugin *plugin;

  if (g_module_symbol (module, "mutter_plugin", (gpointer *)&plugin))
    {
      if (plugin->version_api == METACITY_CLUTTER_PLUGIN_API_VERSION)
        {
          MutterPluginPrivate *priv;

          priv		= g_new0 (MutterPluginPrivate, 1);
	  priv->name	= _(plugin->name);
          priv->module  = module;
          priv->self	= plugin_mgr;

	  /* FIXME: instead of hanging private data of the plugin descriptor
	   * we could make the descriptor const if we were to hang it off
	   * a plugin manager structure */
          plugin->manager_private = priv;

          update_plugin_workspaces (plugin_mgr->screen, plugin);

	  priv->features = parse_disable_params (params, plugin);

          /*
           * Check for and run the plugin init function.
           */
          if (!plugin->do_init || !(plugin->do_init (params)))
            {
              g_free (priv);

              free_plugin_workspaces (plugin);

              return NULL;
            }

          meta_verbose ("Loaded plugin [%s]\n", priv->name);

          return plugin;
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
mutter_plugin_unload (MutterPlugin *plugin)
{
  MutterPluginPrivate *priv;
  GModule *module;

  priv = plugin->manager_private;
  module = priv->module;

  if (priv->running)
    {
      priv->disabled = TRUE;
      return FALSE;
    }

  g_free (priv);
  plugin->manager_private = NULL;

  g_module_close (module);

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
  else if (pref == META_PREF_NUM_WORKSPACES)
    {
      mutter_plugin_manager_update_workspaces (plugin_mgr);
    }
}

/*
 * Loads all plugins listed in gconf registry.
 */
static gboolean
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
          GModule *plugin;
          gchar   *path;

          params = strchr (plugin_string, ':');

          if (params)
            {
              *params = 0;
              ++params;
            }

          path = g_strconcat (dpath, plugin_string, ".so", NULL);

          if ((plugin = g_module_open (path, G_MODULE_BIND_LOCAL)))
            {
              MutterPlugin *p;

              if ((p = mutter_plugin_load (plugin_mgr, plugin, params)))
                plugin_mgr->plugins = g_list_prepend (plugin_mgr->plugins, p);
              else
                {
                  g_message ("Plugin load for [%s] failed\n", path);
                  g_module_close (plugin);
                }
            }
          else
            g_message ("Unable to load plugin [%s]\n", path);

              g_message ("got this far with [%s]\n", path);

          g_free (path);
          g_free (plugin_string);
        }

      plugins = plugins->next;
    }


  if (fallback)
    g_slist_free (fallback);

  if (plugin_mgr->plugins != NULL)
    {
      meta_prefs_add_listener (prefs_changed_callback, plugin_mgr);
      return TRUE;
    }

  return FALSE;
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

static gboolean
mutter_plugin_manager_init (MutterPluginManager *plugin_mgr)
{
  return mutter_plugin_manager_load (plugin_mgr);
}

void
mutter_plugin_manager_update_workspace (MutterPluginManager *plugin_mgr,
                                        MetaWorkspace *workspace)
{
  GList *l;
  gint   index;

  index = meta_workspace_index (workspace);
  l = plugin_mgr->plugins;

  while (l)
    {
      MutterPlugin *plugin = l->data;
      MetaRectangle *rect = g_list_nth_data (plugin->work_areas, index);

      if (rect)
        {
          meta_workspace_get_work_area_all_xineramas (workspace, rect);
        }
      else
        {
          /* Something not entirely right; redo the whole thing */
          update_plugin_workspaces (plugin_mgr->screen, plugin);
          return;
        }

      l = l->next;
    }
}

void
mutter_plugin_manager_update_workspaces (MutterPluginManager *plugin_mgr)
{
  GList *l;

  l = plugin_mgr->plugins;
  while (l)
    {
      MutterPlugin *plugin = l->data;

      update_plugin_workspaces (plugin_mgr->screen, plugin);

      l = l->next;
    }
}

MutterPluginManager *
mutter_plugin_manager_new (MetaScreen *screen)
{
  MutterPluginManager *plugin_mgr;

  plugin_mgr = g_new0 (MutterPluginManager, 1);

  plugin_mgr->screen        = screen;

  if (!mutter_plugin_manager_init (plugin_mgr))
    {
      g_free (plugin_mgr);
      plugin_mgr = NULL;
    }

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
      MutterPluginPrivate *priv = plugin->manager_private;

      if (!priv->disabled
	  && (priv->features & events)
	  && plugin->kill_effect)
        plugin->kill_effect (actor, events);

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
      MutterPluginPrivate *priv = plugin->manager_private;

      if (!priv->disabled && (priv->features & event))
        {
          retval = TRUE;

          switch (event)
            {
            case MUTTER_PLUGIN_MINIMIZE:
              if (plugin->minimize)
                {
                  mutter_plugin_manager_kill_effect (
		      plugin_mgr,
		      actor,
		      ALL_BUT_SWITCH);

		  priv->running++;
                  plugin->minimize (actor);
                }
              break;
            case MUTTER_PLUGIN_MAP:
              if (plugin->map)
                {
                  mutter_plugin_manager_kill_effect (
		      plugin_mgr,
		      actor,
		      ALL_BUT_SWITCH);

		  priv->running++;
                  plugin->map (actor);
                }
              break;
            case MUTTER_PLUGIN_DESTROY:
              if (plugin->destroy)
                {
		  priv->running++;
                  plugin->destroy (actor);
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
      MutterPluginPrivate *priv = plugin->manager_private;

      if (!priv->disabled && (priv->features & event))
        {
          retval = TRUE;

          switch (event)
            {
            case MUTTER_PLUGIN_MAXIMIZE:
              if (plugin->maximize)
                {
                  mutter_plugin_manager_kill_effect (
		      plugin_mgr,
		      actor,
		      ALL_BUT_SWITCH);

                  plugin->maximize (actor,
                                 target_x, target_y,
                                 target_width, target_height);
                }
              break;
            case MUTTER_PLUGIN_UNMAXIMIZE:
              if (plugin->unmaximize)
                {
                  mutter_plugin_manager_kill_effect (
		      plugin_mgr,
		      actor,
		      ALL_BUT_SWITCH);
                  plugin->unmaximize (actor,
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
      MutterPluginPrivate *priv = plugin->manager_private;

      if (!priv->disabled &&
          (priv->features & MUTTER_PLUGIN_SWITCH_WORKSPACE) &&
          (actors && *actors))
        {
          if (plugin->switch_workspace)
            {
              retval = TRUE;
              mutter_plugin_manager_kill_effect (
		  plugin_mgr,
		  MUTTER_WINDOW ((*actors)->data),
		  MUTTER_PLUGIN_SWITCH_WORKSPACE);

              plugin->switch_workspace (actors, from, to, direction);
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
      MutterPlugin *plugin = l->data;

      if (plugin->xevent_filter)
        {
          if (plugin->xevent_filter (xev) == TRUE)
            return TRUE;
        }

      l = l->next;
    }

  return FALSE;
}

/*
 * Public accessors for plugins, exposed from mutter-plugin.h
 */
ClutterActor *
mutter_plugin_get_overlay_group (MutterPlugin *plugin)
{
  MutterPluginPrivate *priv = plugin->manager_private;
  MutterPluginManager *plugin_mgr = priv->self;

  return mutter_get_overlay_group_for_screen (plugin_mgr->screen);
}

ClutterActor *
mutter_plugin_get_stage (MutterPlugin *plugin)
{
  MutterPluginPrivate *priv = plugin->manager_private;
  MutterPluginManager *plugin_mgr  = priv->self;

  return mutter_get_stage_for_screen (plugin_mgr->screen);
}

ClutterActor *
mutter_plugin_get_window_group (MutterPlugin *plugin)
{
  MutterPluginPrivate *priv = plugin->manager_private;
  MutterPluginManager *plugin_mgr  = priv->self;

  return mutter_get_window_group_for_screen (plugin_mgr->screen);
}

void
mutter_plugin_effect_completed (MutterPlugin *plugin,
                                MutterWindow *actor,
                                unsigned long event)
{
  MutterPluginPrivate *priv = plugin->manager_private;

  priv->running--;

  if (!actor)
    {
      g_warning ("Plugin [%s] passed NULL for actor!",
                 (plugin && plugin->name) ? plugin->name : "unknown");
    }

  mutter_window_effect_completed (actor, event);
}

void
mutter_plugin_query_screen_size (MutterPlugin *plugin,
                                 int          *width,
                                 int          *height)
{
  MutterPluginPrivate *priv = plugin->manager_private;
  MutterPluginManager *plugin_mgr  = priv->self;

  meta_screen_get_size (plugin_mgr->screen, width, height);
}

void
mutter_plugin_set_stage_reactive (MutterPlugin *plugin,
                                  gboolean      reactive)
{
  MutterPluginPrivate *priv = plugin->manager_private;
  MutterPluginManager *mgr  = priv->self;
  MetaDisplay *display = meta_screen_get_display (mgr->screen);
  Display     *xdpy    = meta_display_get_xdisplay (display);
  Window       xstage, xoverlay;
  ClutterActor *stage;

  stage = mutter_get_stage_for_screen (mgr->screen);
  xstage = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));
  xoverlay = mutter_get_overlay_window (mgr->screen);

  static XserverRegion region = None;

  if (region == None)
    region = XFixesCreateRegion (xdpy, NULL, 0);

  if (reactive)
    {
      XFixesSetWindowShapeRegion (xdpy, xstage,
                                  ShapeInput, 0, 0, None);
      XFixesSetWindowShapeRegion (xdpy, xoverlay,
                                  ShapeInput, 0, 0, None);
    }
  else
    {
      XFixesSetWindowShapeRegion (xdpy, xstage,
                                  ShapeInput, 0, 0, region);
      XFixesSetWindowShapeRegion (xdpy, xoverlay,
                                  ShapeInput, 0, 0, region);
    }
}

void
mutter_plugin_set_stage_input_area (MutterPlugin *plugin,
                                    gint x, gint y, gint width, gint height)
{
  MutterPluginPrivate *priv = plugin->manager_private;
  MutterPluginManager *mgr  = priv->self;
  MetaDisplay  *display = meta_screen_get_display (mgr->screen);
  Display      *xdpy    = meta_display_get_xdisplay (display);
  Window        xstage, xoverlay;
  ClutterActor *stage;
  XRectangle    rect;
  XserverRegion region;

  stage = mutter_get_stage_for_screen (mgr->screen);
  xstage = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));
  xoverlay = mutter_get_overlay_window (mgr->screen);

  rect.x = x;
  rect.y = y;
  rect.width = width;
  rect.height = height;

  region = XFixesCreateRegion (xdpy, &rect, 1);

  XFixesSetWindowShapeRegion (xdpy, xstage, ShapeInput, 0, 0, region);
  XFixesSetWindowShapeRegion (xdpy, xoverlay, ShapeInput, 0, 0, region);

  XFixesDestroyRegion (xdpy, region);
}

void
mutter_plugin_set_stage_input_region (MutterPlugin *plugin,
                                      XserverRegion region)
{
  MutterPluginPrivate *priv = plugin->manager_private;
  MutterPluginManager *mgr  = priv->self;
  MetaDisplay  *display = meta_screen_get_display (mgr->screen);
  Display      *xdpy    = meta_display_get_xdisplay (display);
  Window        xstage, xoverlay;
  ClutterActor *stage;

  stage = mutter_get_stage_for_screen (mgr->screen);
  xstage = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));
  xoverlay = mutter_get_overlay_window (mgr->screen);

  XFixesSetWindowShapeRegion (xdpy, xstage, ShapeInput, 0, 0, region);
  XFixesSetWindowShapeRegion (xdpy, xoverlay, ShapeInput, 0, 0, region);
}

GList *
mutter_plugin_get_windows (MutterPlugin *plugin)
{
  MutterPluginPrivate *priv = plugin->manager_private;
  MutterPluginManager *plugin_mgr  = priv->self;

  return mutter_get_windows (plugin_mgr->screen);
}

Display *
mutter_plugin_get_xdisplay (MutterPlugin *plugin)
{
  MutterPluginPrivate *priv    = plugin->manager_private;
  MutterPluginManager *mgr     = priv->self;
  MetaDisplay         *display = meta_screen_get_display (mgr->screen);
  Display             *xdpy    = meta_display_get_xdisplay (display);

  return xdpy;
}
