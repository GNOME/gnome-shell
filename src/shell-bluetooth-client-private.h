/*
 *
 *  BlueZ - ShellBluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include <shell-bluetooth-enums.h>

typedef void (*ShellBluetoothClientSetupFunc) (ShellBluetoothClient *client,
					  const GError    *error,
					  const char      *device_path);

void shell_bluetooth_client_setup_device (ShellBluetoothClient          *client,
				    const char               *path,
				    gboolean                  pair,
				    GCancellable             *cancellable,
				    GAsyncReadyCallback       callback,
				    gpointer                  user_data);
gboolean shell_bluetooth_client_setup_device_finish (ShellBluetoothClient  *client,
					       GAsyncResult     *res,
					       char            **path,
					       GError          **error);

void shell_bluetooth_client_cancel_setup_device (ShellBluetoothClient     *client,
					   const char          *path,
					   GCancellable        *cancellable,
					   GAsyncReadyCallback  callback,
					   gpointer             user_data);
gboolean shell_bluetooth_client_cancel_setup_device_finish (ShellBluetoothClient *client,
						      GAsyncResult     *res,
						      char            **path,
						      GError          **error);

gboolean shell_bluetooth_client_set_trusted(ShellBluetoothClient *client,
					const char *device, gboolean trusted);

GDBusProxy *shell_bluetooth_client_get_device (ShellBluetoothClient *client,
					 const char      *path);

void shell_bluetooth_client_dump_device (GtkTreeModel *model,
				   GtkTreeIter *iter);

gboolean shell_bluetooth_client_get_connectable(const char **uuids);

GDBusProxy *_shell_bluetooth_client_get_default_adapter (ShellBluetoothClient *client);
