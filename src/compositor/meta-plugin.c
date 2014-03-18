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

/**
 * SECTION:meta-plugin
 * @title: MetaPlugin
 * @short_description: Entry point for plugins
 *
 */

#include <meta/meta-plugin.h>
#include "meta-plugin-manager.h"
#include <meta/screen.h>
#include <meta/display.h>
#include <meta/util.h>

#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <clutter/x11/clutter-x11.h>

#include "compositor-private.h"
#include "meta-window-actor-private.h"
#include "monitor-private.h"

G_DEFINE_ABSTRACT_TYPE (MetaPlugin, meta_plugin, G_TYPE_OBJECT);

#define META_PLUGIN_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), META_TYPE_PLUGIN, MetaPluginPrivate))

struct _MetaPluginPrivate
{
  MetaScreen *screen;
};

static void
meta_plugin_class_init (MetaPluginClass *klass)
{
  g_type_class_add_private (klass, sizeof (MetaPluginPrivate));
}

static void
meta_plugin_init (MetaPlugin *self)
{
  self->priv = META_PLUGIN_GET_PRIVATE (self);
}

const MetaPluginInfo *
meta_plugin_get_info (MetaPlugin *plugin)
{
  MetaPluginClass  *klass = META_PLUGIN_GET_CLASS (plugin);

  if (klass && klass->plugin_info)
    return klass->plugin_info (plugin);

  return NULL;
}

gboolean
_meta_plugin_xevent_filter (MetaPlugin *plugin,
                            XEvent     *xev)
{
  MetaPluginClass *klass = META_PLUGIN_GET_CLASS (plugin);

  /* When mutter is running as a wayland compositor, things like input
   * events just come directly from clutter so it won't have disabled
   * clutter's event retrieval and won't need to forward it events (if
   * it did it would lead to recursion). Also when running as a
   * wayland compositor we shouldn't be assuming that we're running
   * with the clutter x11 backend.
   */

  if (klass->xevent_filter && klass->xevent_filter (plugin, xev))
    return TRUE;
  else if (!meta_is_wayland_compositor ())
    return clutter_x11_handle_event (xev) != CLUTTER_X11_FILTER_CONTINUE;
  else
    return FALSE;
}

void
meta_plugin_switch_workspace_completed (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;
  MetaScreen *screen = priv->screen;

  meta_switch_workspace_completed (screen);
}

static void
meta_plugin_window_effect_completed (MetaPlugin      *plugin,
                                     MetaWindowActor *actor,
                                     unsigned long    event)
{
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

/**
 * meta_plugin_begin_modal:
 * @plugin: a #MetaPlugin
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
                         MetaModalOptions  options,
                         guint32           timestamp)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  return meta_begin_modal_for_plugin (priv->screen, plugin,
                                      options, timestamp);
}

/**
 * meta_plugin_end_modal:
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

/**
 * meta_plugin_get_screen:
 * @plugin: a #MetaPlugin
 *
 * Gets the #MetaScreen corresponding to a plugin.
 *
 * Return value: (transfer none): the #MetaScreen for the plugin
 */
MetaScreen *
meta_plugin_get_screen (MetaPlugin *plugin)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  return priv->screen;
}

void
_meta_plugin_set_screen (MetaPlugin *plugin,
                         MetaScreen *screen)
{
  MetaPluginPrivate *priv = META_PLUGIN (plugin)->priv;

  priv->screen = screen;
}

void
meta_plugin_complete_display_change (MetaPlugin *plugin,
                                     gboolean    ok)
{
  MetaMonitorManager *manager;

  manager = meta_monitor_manager_get ();
  meta_monitor_manager_confirm_configuration (manager, ok);
}
