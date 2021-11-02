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

#define SHELL_BLUETOOTH_TYPE_CLIENT (shell_bluetooth_client_get_type())
#define SHELL_BLUETOOTH_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
					SHELL_BLUETOOTH_TYPE_CLIENT, ShellBluetoothClient))
#define SHELL_BLUETOOTH_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
					SHELL_BLUETOOTH_TYPE_CLIENT, ShellBluetoothClientClass))
#define SHELL_BLUETOOTH_IS_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
							SHELL_BLUETOOTH_TYPE_CLIENT))
#define SHELL_BLUETOOTH_IS_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
							SHELL_BLUETOOTH_TYPE_CLIENT))
#define SHELL_BLUETOOTH_GET_CLIENT_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
					SHELL_BLUETOOTH_TYPE_CLIENT, ShellBluetoothClientClass))

/**
 * ShellBluetoothClient:
 *
 * The <structname>ShellBluetoothClient</structname> struct contains
 * only private fields and should not be directly accessed.
 */
typedef struct _ShellBluetoothClient ShellBluetoothClient;
typedef struct _ShellBluetoothClientClass ShellBluetoothClientClass;

struct _ShellBluetoothClient {
	GObject parent;
};

struct _ShellBluetoothClientClass {
	GObjectClass parent_class;
};

GType shell_bluetooth_client_get_type(void);

ShellBluetoothClient *shell_bluetooth_client_new(void);

GtkTreeModel *shell_bluetooth_client_get_model(ShellBluetoothClient *client);

GtkTreeModel *shell_bluetooth_client_get_filter_model(ShellBluetoothClient *client,
				GtkTreeModelFilterVisibleFunc func,
				gpointer data, GDestroyNotify destroy);
GtkTreeModel *shell_bluetooth_client_get_adapter_model(ShellBluetoothClient *client);
GtkTreeModel *shell_bluetooth_client_get_device_model(ShellBluetoothClient *client);

void shell_bluetooth_client_connect_service (ShellBluetoothClient     *client,
				       const char          *path,
				       gboolean             connect,
				       GCancellable        *cancellable,
				       GAsyncReadyCallback  callback,
				       gpointer             user_data);

gboolean shell_bluetooth_client_connect_service_finish (ShellBluetoothClient *client,
						  GAsyncResult    *res,
						  GError         **error);
