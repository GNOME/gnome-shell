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

#ifndef MUTTER_PLUGIN_MANAGER_H_
#define MUTTER_PLUGIN_MANAGER_H_

#include "types.h"
#include "screen.h"

#define  MUTTER_PLUGIN_FROM_MANAGER_
#include "mutter-plugin.h"
#undef   MUTTER_PLUGIN_FROM_MANAGER_

typedef struct MutterPluginManager MutterPluginManager;

MutterPluginManager * mutter_plugin_manager_new (MetaScreen *screen);
gboolean mutter_plugin_manager_load (MutterPluginManager *mgr);
gboolean mutter_plugin_manager_initialize (MutterPluginManager *plugin_mgr);
gboolean mutter_plugin_manager_event_simple (MutterPluginManager *mgr,
					     MutterWindow  *actor,
					     unsigned long    event);

gboolean mutter_plugin_manager_event_maximize (MutterPluginManager *mgr,
					       MutterWindow  *actor,
					       unsigned long    event,
					       gint             target_x,
					       gint             target_y,
					       gint             target_width,
					       gint		target_height);
void mutter_plugin_manager_update_workspaces (MutterPluginManager *mgr);

void mutter_plugin_manager_update_workspace (MutterPluginManager *mgr, MetaWorkspace *w);

gboolean mutter_plugin_manager_switch_workspace (MutterPluginManager *mgr,
						 const GList **actors,
						 gint          from,
						 gint          to,
						 MetaMotionDirection direction);

gboolean mutter_plugin_manager_xevent_filter (MutterPluginManager *mgr,
					      XEvent *xev);

#endif
