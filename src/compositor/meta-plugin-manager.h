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

#ifndef META_PLUGIN_MANAGER_H_
#define META_PLUGIN_MANAGER_H_

#include <meta/types.h>
#include <meta/screen.h>
#include <meta/meta-plugin.h>

typedef enum {
  META_PLUGIN_NONE,
  META_PLUGIN_MINIMIZE,
  META_PLUGIN_MAP,
  META_PLUGIN_DESTROY,
  META_PLUGIN_SWITCH_WORKSPACE,
  META_PLUGIN_UNMINIMIZE,
  META_PLUGIN_SIZE_CHANGE,
} MetaPluginEffect;

/**
 * MetaPluginManager: (skip)
 *
 */
typedef struct MetaPluginManager MetaPluginManager;

MetaPluginManager * meta_plugin_manager_new (MetaCompositor *compositor);

void     meta_plugin_manager_load         (const gchar       *plugin_name);

gboolean meta_plugin_manager_event_simple (MetaPluginManager *mgr,
                                           MetaWindowActor   *actor,
                                           MetaPluginEffect   event);

void     meta_plugin_manager_event_size_changed   (MetaPluginManager *mgr,
                                                   MetaWindowActor   *actor);

gboolean meta_plugin_manager_event_size_change    (MetaPluginManager *mgr,
                                                   MetaWindowActor   *actor,
                                                   MetaSizeChange     which_change,
                                                   MetaRectangle     *old_frame_rect,
                                                   MetaRectangle     *old_buffer_rect);

gboolean meta_plugin_manager_switch_workspace (MetaPluginManager   *mgr,
                                               gint                 from,
                                               gint                 to,
                                               MetaMotionDirection  direction);

gboolean meta_plugin_manager_filter_keybinding (MetaPluginManager  *mgr,
                                                MetaKeyBinding     *binding);

gboolean meta_plugin_manager_xevent_filter (MetaPluginManager *mgr,
                                            XEvent            *xev);
gboolean _meta_plugin_xevent_filter (MetaPlugin *plugin,
                                     XEvent     *xev);

void     meta_plugin_manager_confirm_display_change (MetaPluginManager *mgr);

gboolean meta_plugin_manager_show_tile_preview (MetaPluginManager *mgr,
                                                MetaWindow        *window,
                                                MetaRectangle     *tile_rect,
                                                int                tile_monitor_number);
gboolean meta_plugin_manager_hide_tile_preview (MetaPluginManager *mgr);

void meta_plugin_manager_show_window_menu (MetaPluginManager  *mgr,
                                           MetaWindow         *window,
                                           MetaWindowMenuType  menu,
                                           int                 x,
                                           int                 y);

void meta_plugin_manager_show_window_menu_for_rect (MetaPluginManager  *mgr,
		                                    MetaWindow         *window,
						    MetaWindowMenuType  menu,
						    MetaRectangle      *rect);

MetaCloseDialog * meta_plugin_manager_create_close_dialog (MetaPluginManager *plugin_mgr,
                                                           MetaWindow        *window);

MetaInhibitShortcutsDialog *
  meta_plugin_manager_create_inhibit_shortcuts_dialog (MetaPluginManager *plugin_mgr,
                                                       MetaWindow        *window);

#endif
