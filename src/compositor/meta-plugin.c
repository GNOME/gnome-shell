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

#include "meta-plugin.h"
#include "screen.h"
#include "display.h"

#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <clutter/x11/clutter-x11.h>

#include "compositor-private.h"
#include "meta-window-actor-private.h"

G_DEFINE_ABSTRACT_TYPE (MetaPlugin, meta_plugin, G_TYPE_OBJECT);

#define META_PLUGIN_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), META_TYPE_PLUGIN, MetaPluginPrivate))

enum
{
  PROP_0,
  PROP_SCREEN,
  PROP_PARAMS,
  PROP_FEATURES,
  PROP_DISABLED,
  PROP_DEBUG_MODE,
};

struct _MetaPluginPrivate
{
  MetaScreen   *screen;
  gchar        *params;
  gulong        features;

  gint          running;

  gboolean      disabled : 1;
  gboolean      debug    : 1;
};

static void
meta_plugin_dispose (GObject *object)
{
  G_OBJECT_CLASS (meta_plugin_parent_class)->dispose (object);
}

static void
meta_plugin_finalize (GObject *object)
{
  MetaPluginPrivate *priv = META_PLUGIN (object)->priv;

  g_free (priv->params);
  priv->params = NULL;

  G_OBJECT_CLASS (meta_plugin_parent_class)->finalize (object);
}

static void
meta_plugin_parse_params (MetaPlugin *plugin)
{
  char                  *p;
  gulong                features = 0;
  MetaPluginPrivate  *priv     = plugin->priv;
  MetaPluginClass    *klass    = META_PLUGIN_GET_CLASS (plugin);

/*
 * Feature flags: identify events that the plugin can handle; a plugin can
 * handle one or more events.
 */
  if (klass->minimize)
    features |= META_PLUGIN_MINIMIZE;

  if (klass->maximize)
    features |= META_PLUGIN_MAXIMIZE;

  if (klass->unmaximize)
    features |= META_PLUGIN_UNMAXIMIZE;

  if (klass->map)
    features |= META_PLUGIN_MAP;

  if (klass->destroy)
    features |= META_PLUGIN_DESTROY;

  if (klass->switch_workspace)
    features |= META_PLUGIN_SWITCH_WORKSPACE;

  if (priv->params)
    {
      gboolean debug = FALSE;

      if ((p = strstr (priv->params, "disable:")))
        {
          gchar *d = g_strdup (p+8);

          p = strchr (d, ';');

          if (p)
            *p = 0;

          if (strstr (d, "minimize"))
            features &= ~ META_PLUGIN_MINIMIZE;

          if (strstr (d, "maximize"))
            features &= ~ META_PLUGIN_MAXIMIZE;

          if (strstr (d, "unmaximize"))
            features &= ~ META_PLUGIN_UNMAXIMIZE;

          if (strstr (d, "map"))
            features &= ~ META_PLUGIN_MAP;

          if (strstr (d, "destroy"))
            features &= ~ META_PLUGIN_DESTROY;

          if (strstr (d, "switch-workspace"))
            features &= ~META_PLUGIN_SWITCH_WORKSPACE;

          g_free (d);
        }

      if (strstr (priv->params, "debug"))
        debug = TRUE;

      if (debug != priv->debug)
        {
          priv->debug = debug;

          g_object_notify (G_OBJECT (plugin), "debug-mode");
        }
    }

  if (features != priv->features)
    {
      priv->features = features;

      g_object_notify (G_OBJECT (plugin), "features");
    }
}

static void
meta_plugin_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  MetaPluginPrivate *priv = META_PLUGIN (object)->priv;

  switch (prop_id)
    {
    case PROP_SCREEN:
      priv->screen = g_value_get_object (value);
      break;
    case PROP_PARAMS:
      priv->params = g_value_dup_string (value);
      meta_plugin_parse_params (META_PLUGIN (object));
      break;
    case PROP_DISABLED:
      priv->disabled = g_value_get_boolean (value);
      break;
    case PROP_DEBUG_MODE:
      priv->debug = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_plugin_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MetaPluginPrivate *priv = META_PLUGIN (object)->priv;

  switch (prop_id)
    {
    case PROP_SCREEN:
      g_value_set_object (value, priv->screen);
      break;
    case PROP_PARAMS:
      g_value_set_string (value, priv->params);
      break;
    case PROP_DISABLED:
      g_value_set_boolean (value, priv->disabled);
      break;
    case PROP_DEBUG_MODE:
      g_value_set_boolean (value, priv->debug);
      break;
    case PROP_FEATURES:
      g_value_set_ulong (value, priv->features);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
meta_plugin_class_init (MetaPluginClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize        = meta_plugin_finalize;
  gobject_class->dispose         = meta_plugin_dispose;
  gobject_class->set_property    = meta_plugin_set_property;
  gobject_class->get_property    = meta_plugin_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_SCREEN,
                                   g_param_spec_object ("screen",
                                                        "MetaScreen",
                                                        "MetaScreen",
                                                        META_TYPE_SCREEN,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_PARAMS,
				   g_param_spec_string ("params",
							"Parameters",
							"Plugin Parameters",
							NULL,
							G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
				   PROP_FEATURES,
				   g_param_spec_ulong ("features",
                                                       "Features",
                                                       "Plugin Features",
                                                       0 , G_MAXULONG, 0,
                                                       G_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
				   PROP_DISABLED,
				   g_param_spec_boolean ("disabled",
                                                      "Plugin disabled",
                                                      "Plugin disabled",
                                                      FALSE,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_DEBUG_MODE,
				   g_param_spec_boolean ("debug-mode",
                                                      "Debug Mode",
                                                      "Debug Mode",
                                                      FALSE,
                                                      G_PARAM_READABLE));

  g_type_class_add_private (gobject_class, sizeof (MetaPluginPrivate));
}

static void
meta_plugin_init (MetaPlugin *self)
{
  MetaPluginPrivate *priv;

  self->priv = priv = META_PLUGIN_GET_PRIVATE (self);
}

gulong
meta_plugin_features (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  return priv->features;
}

gboolean
meta_plugin_disabled (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  return priv->disabled;
}

gboolean
meta_plugin_running  (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  return (priv->running > 0);
}

gboolean
meta_plugin_debug_mode (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  return priv->debug;
}

const MetaPluginInfo *
meta_plugin_get_info (MetaPlugin *plugin)
{
  MetaPluginClass  *klass = META_PLUGIN_GET_CLASS (plugin);

  if (klass && klass->plugin_info)
    return klass->plugin_info (plugin);

  return NULL;
}

ClutterActor *
meta_plugin_get_overlay_group (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  return meta_get_overlay_group_for_screen (priv->screen);
}

ClutterActor *
meta_plugin_get_stage (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  return meta_get_stage_for_screen (priv->screen);
}

ClutterActor *
meta_plugin_get_window_group (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  return meta_get_window_group_for_screen (priv->screen);
}

/**
 * _meta_plugin_effect_started:
 * @plugin: the plugin
 *
 * Mark that an effect has started for the plugin. This is called
 * internally by MetaPluginManager.
 */
void
_meta_plugin_effect_started (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  priv->running++;
}

void
meta_plugin_switch_workspace_completed (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  MetaScreen *screen = meta_plugin_get_screen (plugin);

  if (priv->running-- < 0)
    {
      g_warning ("Error in running effect accounting, adjusting.");
      priv->running = 0;
    }

  meta_switch_workspace_completed (screen);
}

static void
meta_plugin_window_effect_completed (MetaPlugin      *plugin,
                                     MetaWindowActor *actor,
                                     unsigned long    event)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  if (priv->running-- < 0)
    {
      g_warning ("Error in running effect accounting, adjusting.");
      priv->running = 0;
    }

  if (!actor)
    {
      const MetaPluginInfo *info;
      const gchar            *name = NULL;

      if (plugin && (info = meta_plugin_get_info (plugin)))
        name = info->name;

      g_warning ("Plugin [%s] passed NULL for actor!",
                 name ? name : "unknown");
    }

  meta_window_actor_effect_completed (actor, event);
}

void
meta_plugin_minimize_completed (MetaPlugin      *plugin,
                                MetaWindowActor *actor)
{
  meta_plugin_window_effect_completed (plugin, actor, META_PLUGIN_MINIMIZE);
}

void
meta_plugin_maximize_completed (MetaPlugin      *plugin,
                                MetaWindowActor *actor)
{
  meta_plugin_window_effect_completed (plugin, actor, META_PLUGIN_MAXIMIZE);
}

void
meta_plugin_unmaximize_completed (MetaPlugin      *plugin,
                                  MetaWindowActor *actor)
{
  meta_plugin_window_effect_completed (plugin, actor, META_PLUGIN_UNMAXIMIZE);
}

void
meta_plugin_map_completed (MetaPlugin      *plugin,
                           MetaWindowActor *actor)
{
  meta_plugin_window_effect_completed (plugin, actor, META_PLUGIN_MAP);
}

void
meta_plugin_destroy_completed (MetaPlugin      *plugin,
                               MetaWindowActor *actor)
{
  meta_plugin_window_effect_completed (plugin, actor, META_PLUGIN_DESTROY);
}

void
meta_plugin_query_screen_size (MetaPlugin *plugin,
                               int        *width,
                               int        *height)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  meta_screen_get_size (priv->screen, width, height);
}

void
meta_plugin_set_stage_reactive (MetaPlugin *plugin,
                                gboolean    reactive)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;
  MetaScreen  *screen  = priv->screen;

  if (reactive)
    meta_set_stage_input_region (screen, None);
  else
    meta_empty_stage_input_region (screen);
}

void
meta_plugin_set_stage_input_area (MetaPlugin *plugin,
                                  gint x, gint y, gint width, gint height)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;
  MetaScreen   *screen  = priv->screen;
  MetaDisplay  *display = meta_screen_get_display (screen);
  Display      *xdpy    = meta_display_get_xdisplay (display);
  XRectangle    rect;
  XserverRegion region;

  rect.x = x;
  rect.y = y;
  rect.width = width;
  rect.height = height;

  region = XFixesCreateRegion (xdpy, &rect, 1);
  meta_set_stage_input_region (screen, region);
  XFixesDestroyRegion (xdpy, region);
}

void
meta_plugin_set_stage_input_region (MetaPlugin   *plugin,
                                    XserverRegion region)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;
  MetaScreen  *screen  = priv->screen;

  meta_set_stage_input_region (screen, region);
}

/**
 * meta_plugin_get_window_actors:
 * @plugin: A #MetaPlugin
 *
 * This function returns all of the #MetaWindowActor objects referenced by Mutter, including
 * override-redirect windows.  The returned list is a snapshot of Mutter's current
 * stacking order, with the topmost window last.
 *
 * The 'restacked' signal of #MetaScreen signals when this value has changed.
 *
 * Returns: (transfer none) (element-type MetaWindowActor): Windows in stacking order, topmost last
 */
GList *
meta_plugin_get_window_actors (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  return meta_get_window_actors (priv->screen);
}

/**
 * meta_plugin_begin_modal:
 * @plugin: a #MetaPlugin
 * @grab_window: the X window to grab the keyboard and mouse on
 * @cursor: the cursor to use for the pointer grab, or None,
 *          to use the normal cursor for the grab window and
 *          its descendants.
 * @options: flags that modify the behavior of the modal grab
 * @timestamp: the timestamp used for establishing grabs
 *
 * This function is used to grab the keyboard and mouse for the exclusive
 * use of the plugin. Correct operation requires that both the keyboard
 * and mouse are grabbed, or thing will break. (In particular, other
 * passive X grabs in Meta can trigger but not be handled by the normal
 * keybinding handling code.) However, the plugin can establish the keyboard
 * and/or mouse grabs ahead of time and pass in the
 * %META_MODAL_POINTER_ALREADY_GRABBED and/or %META_MODAL_KEYBOARD_ALREADY_GRABBED
 * options. This facility is provided for two reasons: first to allow using
 * this function to establish modality after a passive grab, and second to
 * allow using obscure features of XGrabPointer() and XGrabKeyboard() without
 * having to add them to this API.
 *
 * Return value: whether we successfully grabbed the keyboard and
 *  mouse and made the plugin modal.
 */
gboolean
meta_plugin_begin_modal (MetaPlugin       *plugin,
                         Window            grab_window,
                         Cursor            cursor,
                         MetaModalOptions  options,
                         guint32           timestamp)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  return meta_begin_modal_for_plugin (priv->screen, plugin,
                                      grab_window, cursor, options, timestamp);
}

/**
 * meta_plugin_end_modal
 * @plugin: a #MetaPlugin
 * @timestamp: the time used for releasing grabs
 *
 * Ends the modal operation begun with meta_plugin_begin_modal(). This
 * ungrabs both the mouse and keyboard even when
 * %META_MODAL_POINTER_ALREADY_GRABBED or
 * %META_MODAL_KEYBOARD_ALREADY_GRABBED were provided as options
 * when beginnning the modal operation.
 */
void
meta_plugin_end_modal (MetaPlugin *plugin,
                       guint32     timestamp)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  meta_end_modal_for_plugin (priv->screen, plugin, timestamp);
}

Display *
meta_plugin_get_xdisplay (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv    = META_PLUGIN (plugin)->priv;
  MetaDisplay       *display = meta_screen_get_display (priv->screen);
  Display           *xdpy    = meta_display_get_xdisplay (display);

  return xdpy;
}

/**
 * meta_plugin_get_screen:
 * @plugin: a #MetaPlugin
 *
 * Gets the #MetaScreen corresponding to a plugin. Each plugin instance
 * is associated with exactly one screen; if Metacity is managing
 * multiple screens, multiple plugin instances will be created.
 *
 * Return value: (transfer none): the #MetaScreen for the plugin
 */
MetaScreen *
meta_plugin_get_screen (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  return priv->screen;
}

