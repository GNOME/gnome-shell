/*
 * Copyright (C) 2013 Red Hat
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
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#ifndef __SHELL_MENU_TRACKER_H__
#define __SHELL_MENU_TRACKER_H__

#include <gio/gio.h>

#include "gtkmenutrackeritem.h"

typedef struct _ShellMenuTracker ShellMenuTracker;

GType shell_menu_tracker_get_type (void) G_GNUC_CONST;

typedef void         (* ShellMenuTrackerInsertFunc)       (GtkMenuTrackerItem       *item,
                                                           gint                      position,
                                                           gpointer                  user_data);

typedef void         (* ShellMenuTrackerRemoveFunc)       (gint                      position,
                                                           gpointer                  user_data);

ShellMenuTracker * shell_menu_tracker_new (GtkActionObservable        *observable,
                                           GMenuModel                 *model,
                                           const gchar                *action_namespace,
                                           ShellMenuTrackerInsertFunc  insert_func,
                                           gpointer                    insert_user_data,
                                           GDestroyNotify              insert_notify,
                                           ShellMenuTrackerRemoveFunc  remove_func,
                                           gpointer                    remove_user_data,
                                           GDestroyNotify              remove_notify);
ShellMenuTracker * shell_menu_tracker_new_for_item_submenu (GtkMenuTrackerItem         *item,
                                                            ShellMenuTrackerInsertFunc  insert_func,
                                                            gpointer                    insert_user_data,
                                                            GDestroyNotify              insert_notify,
                                                            ShellMenuTrackerRemoveFunc  remove_func,
                                                            gpointer                    remove_user_data,
                                                            GDestroyNotify              remove_notify);

ShellMenuTracker * shell_menu_tracker_ref (ShellMenuTracker *tracker);
void shell_menu_tracker_unref (ShellMenuTracker *tracker);
void shell_menu_tracker_destroy (ShellMenuTracker *tracker);

#endif /* __SHELL_MENU_TRACKER_H__ */
