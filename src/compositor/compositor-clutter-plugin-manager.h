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

#ifndef META_COMPOSITOR_CLUTTER_PLUGIN_MANAGER_H_
#define META_COMPOSITOR_CLUTTER_PLUGIN_MANAGER_H_

#include "types.h"
#include "screen.h"
#include "compositor-clutter-plugin.h"

typedef struct MetaCompositorClutterPluginManager MetaCompositorClutterPluginManager;

MetaCompositorClutterPluginManager * meta_compositor_clutter_plugin_manager_new (MetaScreen *screen, ClutterActor *stage);
gboolean meta_compositor_clutter_plugin_manager_event_simple (MetaCompositorClutterPluginManager *mgr,
                                                       MetaCompWindow  *actor,
                                                       unsigned long    event);

gboolean meta_compositor_clutter_plugin_manager_event_maximize (MetaCompositorClutterPluginManager *mgr,
                                                          MetaCompWindow  *actor,
                                                          unsigned long    event,
                                                          gint             target_x,
                                                          gint             target_y,
                                                          gint             target_width,
                                                          gint           target_height);
void meta_compositor_clutter_plugin_manager_update_workspaces (MetaCompositorClutterPluginManager *mgr);

void meta_compositor_clutter_plugin_manager_update_workspace (MetaCompositorClutterPluginManager *mgr, MetaWorkspace *w);

gboolean meta_compositor_clutter_plugin_manager_switch_workspace (MetaCompositorClutterPluginManager *mgr,
                                                           const GList **actors,
                                                           gint          from,
                                                           gint          to);

gboolean meta_compositor_clutter_plugin_manager_xevent_filter (MetaCompositorClutterPluginManager *mgr,
                                                                 XEvent *xev);
#endif
