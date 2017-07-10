/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015 Red Hat
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

#ifndef META_MONITOR_MANAGER_H
#define META_MONITOR_MANAGER_H

#include <glib-object.h>

typedef enum
{
  META_MONITOR_SWITCH_CONFIG_ALL_MIRROR,
  META_MONITOR_SWITCH_CONFIG_ALL_LINEAR,
  META_MONITOR_SWITCH_CONFIG_EXTERNAL,
  META_MONITOR_SWITCH_CONFIG_BUILTIN,
  META_MONITOR_SWITCH_CONFIG_UNKNOWN,
} MetaMonitorSwitchConfigType;

typedef struct _MetaMonitorManagerClass    MetaMonitorManagerClass;
typedef struct _MetaMonitorManager         MetaMonitorManager;

GType meta_monitor_manager_get_type (void);

MetaMonitorManager *meta_monitor_manager_get  (void);

gint meta_monitor_manager_get_monitor_for_connector (MetaMonitorManager *manager,
                                                     const char         *connector);

gboolean meta_monitor_manager_get_is_builtin_display_on (MetaMonitorManager *manager);

void meta_monitor_manager_switch_config (MetaMonitorManager          *manager,
                                         MetaMonitorSwitchConfigType  config_type);

gboolean meta_monitor_manager_can_switch_config (MetaMonitorManager *manager);

MetaMonitorSwitchConfigType meta_monitor_manager_get_switch_config (MetaMonitorManager *manager);

gint meta_monitor_manager_get_display_configuration_timeout (void);

#endif /* META_MONITOR_MANAGER_H */
