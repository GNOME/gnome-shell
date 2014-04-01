/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
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

#ifndef META_MONITOR_MANAGER_DUMMY_H
#define META_MONITOR_MANAGER_DUMMY_H

#include "monitor-private.h"

#define META_TYPE_MONITOR_MANAGER_DUMMY            (meta_monitor_manager_dummy_get_type ())
#define META_MONITOR_MANAGER_DUMMY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_MONITOR_MANAGER_DUMMY, MetaMonitorManagerDummy))
#define META_MONITOR_MANAGER_DUMMY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_MONITOR_MANAGER_DUMMY, MetaMonitorManagerDummyClass))
#define META_IS_MONITOR_MANAGER_DUMMY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_MONITOR_MANAGER_DUMMY))
#define META_IS_MONITOR_MANAGER_DUMMY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_MONITOR_MANAGER_DUMMY))
#define META_MONITOR_MANAGER_DUMMY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_MONITOR_MANAGER_DUMMY, MetaMonitorManagerDummyClass))

typedef struct _MetaMonitorManagerDummyClass    MetaMonitorManagerDummyClass;
typedef struct _MetaMonitorManagerDummy         MetaMonitorManagerDummy;

GType meta_monitor_manager_dummy_get_type (void);

#endif /* META_MONITOR_MANAGER_DUMMY_H */
