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

#define  META_PLUGIN_FROM_MANAGER_
#include <meta/meta-plugin.h>
#undef   META_PLUGIN_FROM_MANAGER_

#define META_PLUGIN_MINIMIZE         (1<<0)
#define META_PLUGIN_MAXIMIZE         (1<<1)
#define META_PLUGIN_UNMAXIMIZE       (1<<2)
#define META_PLUGIN_MAP              (1<<3)
#define META_PLUGIN_DESTROY          (1<<4)
#define META_PLUGIN_SWITCH_WORKSPACE (1<<5)

#define META_PLUGIN_ALL_EFFECTS      (~0)

/**
 * MetaPluginManager: (skip)
 *
 */
typedef struct MetaPluginManager MetaPluginManager;

MetaPluginManager * meta_plugin_manager_new (MetaCompositor *compositor);

void     meta_plugin_manager_load         (const gchar       *plugin_name);

gboolean meta_plugin_manager_event_simple (MetaPluginManager *mgr,
                                           MetaWindowActor   *actor,
                                           unsigned long      event);

gboolean meta_plugin_manager_event_maximize    (MetaPluginManager *mgr,
                                                MetaWindowActor   *actor,
                                                unsigned long      event,
                                                gint               target_x,
                                                gint               target_y,
                                                gint               target_width,
                                                gint               target_height);

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


#endif
