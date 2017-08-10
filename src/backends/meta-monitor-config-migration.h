/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat
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

#ifndef META_MONITOR_CONFIG_MIGRATION_H
#define META_MONITOR_CONFIG_MIGRATION_H

#include "backends/meta-monitor-manager-private.h"

gboolean meta_migrate_old_monitors_config (MetaMonitorConfigStore *config_store,
                                           GFile                  *in_file,
                                           GError                **error);

gboolean meta_migrate_old_user_monitors_config (MetaMonitorConfigStore *config_store,
                                                GError                **error);

gboolean meta_finish_monitors_config_migration (MetaMonitorManager *monitor_manager,
                                                MetaMonitorsConfig *config,
                                                GError            **error);

#endif /* META_MONITOR_CONFIG_MIGRATION_H */
