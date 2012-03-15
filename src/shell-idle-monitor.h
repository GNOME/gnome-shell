/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Adapted from gnome-session/gnome-session/gs-idle-monitor.h
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#ifndef __SHELL_IDLE_MONITOR_H
#define __SHELL_IDLE_MONITOR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define SHELL_TYPE_IDLE_MONITOR         (shell_idle_monitor_get_type ())
#define SHELL_IDLE_MONITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), SHELL_TYPE_IDLE_MONITOR, ShellIdleMonitor))
#define SHELL_IDLE_MONITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), SHELL_TYPE_IDLE_MONITOR, ShellIdleMonitorClass))
#define SHELL_IS_IDLE_MONITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), SHELL_TYPE_IDLE_MONITOR))
#define SHELL_IS_IDLE_MONITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), SHELL_TYPE_IDLE_MONITOR))
#define SHELL_IDLE_MONITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), SHELL_TYPE_IDLE_MONITOR, ShellIdleMonitorClass))

typedef struct ShellIdleMonitorPrivate ShellIdleMonitorPrivate;

typedef struct
{
        GObject                  parent;
        ShellIdleMonitorPrivate *priv;
} ShellIdleMonitor;

typedef struct
{
        GObjectClass          parent_class;
} ShellIdleMonitorClass;

typedef void (*ShellIdleMonitorWatchFunc) (ShellIdleMonitor *monitor,
                                           guint             id,
                                           gboolean          condition,
                                           gpointer          user_data);

GType              shell_idle_monitor_get_type     (void);

ShellIdleMonitor * shell_idle_monitor_new          (void);

guint              shell_idle_monitor_add_watch    (ShellIdleMonitor         *monitor,
                                                    guint                     interval,
                                                    ShellIdleMonitorWatchFunc callback,
                                                    gpointer                  user_data,
                                                    GDestroyNotify            notify);

void               shell_idle_monitor_remove_watch (ShellIdleMonitor         *monitor,
                                                    guint                     id);

G_END_DECLS

#endif /* __SHELL_IDLE_MONITOR_H */
