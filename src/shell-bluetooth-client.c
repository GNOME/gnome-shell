/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2010       Giovanni Campagna <scampa.giovanni@gmail.com>
 *  Copyright (C) 2013       Intel Corporation.
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

/**
 * SECTION:shell-bluetooth-client
 * @short_description: ShellBluetooth client object
 * @stability: Stable
 * @include: shell-bluetooth-client.h
 *
 * The #ShellBluetoothClient object is used to query the state of Bluetooth
 * devices and adapters.
 **/

#include <config.h>

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "shell-bluetooth-client.h"
#include "shell-bluetooth-client-glue.h"
#include "shell-bluetooth-utils.h"
#include "shell-enum-types.h"

#define BLUEZ_SERVICE			"org.bluez"
#define BLUEZ_MANAGER_PATH		"/"
#define BLUEZ_ADAPTER_INTERFACE		"org.bluez.Adapter1"
#define BLUEZ_DEVICE_INTERFACE		"org.bluez.Device1"

#define SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(obj) shell_bluetooth_client_get_instance_private (obj)

typedef struct _ShellBluetoothClientPrivate ShellBluetoothClientPrivate;

struct _ShellBluetoothClientPrivate {
	GDBusObjectManager *manager;
	GCancellable *cancellable;
	GtkTreeStore *store;
	GtkTreeRowReference *default_adapter;
	/* Discoverable during discovery? */
	gboolean disco_during_disco;
	gboolean discovery_started;
};

enum {
	PROP_0,
	PROP_DEFAULT_ADAPTER,
	PROP_DEFAULT_ADAPTER_POWERED,
	PROP_DEFAULT_ADAPTER_DISCOVERABLE,
	PROP_DEFAULT_ADAPTER_NAME,
	PROP_DEFAULT_ADAPTER_DISCOVERING
};

enum {
	DEVICE_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE(ShellBluetoothClient, shell_bluetooth_client, G_TYPE_OBJECT)

typedef gboolean (*IterSearchFunc) (GtkTreeStore *store,
				GtkTreeIter *iter, gpointer user_data);

static gboolean iter_search(GtkTreeStore *store,
				GtkTreeIter *iter, GtkTreeIter *parent,
				IterSearchFunc func, gpointer user_data)
{
	gboolean cont, found = FALSE;

	if (parent == NULL)
		cont = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store),
									iter);
	else
		cont = gtk_tree_model_iter_children(GTK_TREE_MODEL(store),
								iter, parent);

	while (cont == TRUE) {
		GtkTreeIter child;

		found = func(store, iter, user_data);
		if (found == TRUE)
			break;

		found = iter_search(store, &child, iter, func, user_data);
		if (found == TRUE) {
			*iter = child;
			break;
		}

		cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), iter);
	}

	return found;
}

static gboolean
compare_path (GtkTreeStore *store,
	      GtkTreeIter *iter,
	      gpointer user_data)
{
	const gchar *path = user_data;
	g_autoptr(GDBusProxy) object = NULL;

	gtk_tree_model_get (GTK_TREE_MODEL(store), iter,
			    SHELL_BLUETOOTH_COLUMN_PROXY, &object,
			    -1);

	return (object != NULL &&
		g_str_equal (path, g_dbus_proxy_get_object_path (object)));
}

static gboolean
compare_address (GtkTreeStore *store,
		 GtkTreeIter *iter,
		 gpointer user_data)
{
	const char *address = user_data;
	g_autofree char *tmp_address = NULL;

	gtk_tree_model_get (GTK_TREE_MODEL(store), iter,
			    SHELL_BLUETOOTH_COLUMN_ADDRESS, &tmp_address, -1);
	return (g_strcmp0 (address, tmp_address) == 0);
}

static gboolean
get_iter_from_path (GtkTreeStore *store,
		    GtkTreeIter  *iter,
		    const char   *path)
{
	g_return_val_if_fail (path != NULL, FALSE);
	return iter_search(store, iter, NULL, compare_path, (gpointer) path);
}

static gboolean
get_iter_from_proxy(GtkTreeStore *store,
		    GtkTreeIter *iter,
		    GDBusProxy *proxy)
{
	g_return_val_if_fail (proxy != NULL, FALSE);
	return iter_search(store, iter, NULL, compare_path,
			   (gpointer) g_dbus_proxy_get_object_path (proxy));
}

static gboolean
get_iter_from_address (GtkTreeStore *store,
		       GtkTreeIter  *iter,
		       const char   *address,
		       GDBusProxy   *adapter)
{
	GtkTreeIter parent_iter;

	g_return_val_if_fail (address != NULL, FALSE);
	g_return_val_if_fail (adapter != NULL, FALSE);

	if (get_iter_from_proxy (store, &parent_iter, adapter) == FALSE)
		return FALSE;

	return iter_search (store, iter, &parent_iter, compare_address, (gpointer) address);
}

static char **
device_list_uuids (const gchar * const *uuids)
{
	GPtrArray *ret;
	guint i;

	if (uuids == NULL)
		return NULL;

	ret = g_ptr_array_new ();

	for (i = 0; uuids[i] != NULL; i++) {
		const char *uuid;

		uuid = shell_bluetooth_uuid_to_string (uuids[i]);
		if (uuid == NULL)
			continue;
		g_ptr_array_add (ret, g_strdup (uuid));
	}

	if (ret->len == 0) {
		g_ptr_array_free (ret, TRUE);
		return NULL;
	}

	g_ptr_array_add (ret, NULL);

	return (char **) g_ptr_array_free (ret, FALSE);
}

static const char *
icon_override (const char    *bdaddr,
	       ShellBluetoothType  type)
{
	/* audio-card, you're ugly */
	switch (type) {
	case SHELL_BLUETOOTH_TYPE_HEADSET:
		return "audio-headset";
	case SHELL_BLUETOOTH_TYPE_HEADPHONES:
		return "audio-headphones";
	case SHELL_BLUETOOTH_TYPE_OTHER_AUDIO:
		return "audio-speakers";
	case SHELL_BLUETOOTH_TYPE_DISPLAY:
		return "video-display";
	case SHELL_BLUETOOTH_TYPE_SCANNER:
		return "scanner";
	case SHELL_BLUETOOTH_TYPE_REMOTE_CONTROL:
	case SHELL_BLUETOOTH_TYPE_WEARABLE:
	case SHELL_BLUETOOTH_TYPE_TOY:
		/* FIXME */
	default:
		return NULL;
	}
}

static void
device_resolve_type_and_icon (Device1 *device, ShellBluetoothType *type, const char **icon)
{
	g_return_if_fail (type);
	g_return_if_fail (icon);

	if (g_strcmp0 (device1_get_name (device), "ION iCade Game Controller") == 0 ||
	    g_strcmp0 (device1_get_name (device), "8Bitdo Zero GamePad") == 0) {
		*type = SHELL_BLUETOOTH_TYPE_JOYPAD;
		*icon = "input-gaming";
		return;
	}

	if (*type == 0 || *type == SHELL_BLUETOOTH_TYPE_ANY)
		*type = shell_bluetooth_appearance_to_type (device1_get_appearance (device));
	if (*type == 0 || *type == SHELL_BLUETOOTH_TYPE_ANY)
		*type = shell_bluetooth_class_to_type (device1_get_class (device));

	*icon = icon_override (device1_get_address (device), *type);

	if (!*icon)
		*icon = device1_get_icon (device);

	if (!*icon)
		*icon = "bluetooth";
}

static void
device_notify_cb (Device1         *device,
		  GParamSpec      *pspec,
		  ShellBluetoothClient *client)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	const char *property = g_param_spec_get_name (pspec);
	GtkTreeIter iter;

	if (get_iter_from_proxy (priv->store, &iter, G_DBUS_PROXY (device)) == FALSE)
		return;

	if (g_strcmp0 (property, "name") == 0) {
		const gchar *name = device1_get_name (device);

		gtk_tree_store_set (priv->store, &iter,
				    SHELL_BLUETOOTH_COLUMN_NAME, name, -1);
	} else if (g_strcmp0 (property, "alias") == 0) {
		const gchar *alias = device1_get_alias (device);

		gtk_tree_store_set (priv->store, &iter,
				    SHELL_BLUETOOTH_COLUMN_ALIAS, alias, -1);
	} else if (g_strcmp0 (property, "paired") == 0) {
		gboolean paired = device1_get_paired (device);

		gtk_tree_store_set (priv->store, &iter,
				    SHELL_BLUETOOTH_COLUMN_PAIRED, paired, -1);
	} else if (g_strcmp0 (property, "trusted") == 0) {
		gboolean trusted = device1_get_trusted (device);

		gtk_tree_store_set (priv->store, &iter,
				    SHELL_BLUETOOTH_COLUMN_TRUSTED, trusted, -1);
	} else if (g_strcmp0 (property, "connected") == 0) {
		gboolean connected = device1_get_connected (device);

		gtk_tree_store_set (priv->store, &iter,
				    SHELL_BLUETOOTH_COLUMN_CONNECTED, connected, -1);
	} else if (g_strcmp0 (property, "uuids") == 0) {
		char **uuids;

		uuids = device_list_uuids (device1_get_uuids (device));

		gtk_tree_store_set (priv->store, &iter,
				    SHELL_BLUETOOTH_COLUMN_UUIDS, uuids, -1);
		g_strfreev (uuids);
	} else if (g_strcmp0 (property, "legacy-pairing") == 0) {
		gboolean legacypairing = device1_get_legacy_pairing (device);

		gtk_tree_store_set (priv->store, &iter,
				    SHELL_BLUETOOTH_COLUMN_LEGACYPAIRING, legacypairing,
				    -1);
	} else if (g_strcmp0 (property, "icon") == 0 ||
		   g_strcmp0 (property, "class") == 0 ||
		   g_strcmp0 (property, "appearance") == 0) {
		ShellBluetoothType type = SHELL_BLUETOOTH_TYPE_ANY;
		const char *icon = NULL;

		device_resolve_type_and_icon (device, &type, &icon);

		gtk_tree_store_set (priv->store, &iter,
				    SHELL_BLUETOOTH_COLUMN_TYPE, type,
				    SHELL_BLUETOOTH_COLUMN_ICON, icon,
				    -1);
	} else {
		g_debug ("Unhandled property: %s", property);
	}
}

static void
device_added (GDBusObjectManager   *manager,
	      Device1              *device,
	      ShellBluetoothClient      *client)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GDBusProxy *adapter;
	const char *adapter_path, *address, *alias, *name, *icon;
	char **uuids;
	gboolean paired, trusted, connected;
	int legacypairing;
	ShellBluetoothType type = SHELL_BLUETOOTH_TYPE_ANY;
	GtkTreeIter iter, parent;

	g_signal_connect_object (G_OBJECT (device), "notify",
				 G_CALLBACK (device_notify_cb), client, 0);

	adapter_path = device1_get_adapter (device);
	address = device1_get_address (device);
	alias = device1_get_alias (device);
	name = device1_get_name (device);
	paired = device1_get_paired (device);
	trusted = device1_get_trusted (device);
	connected = device1_get_connected (device);
	uuids = device_list_uuids (device1_get_uuids (device));
	legacypairing = device1_get_legacy_pairing (device);

	device_resolve_type_and_icon (device, &type, &icon);

	if (get_iter_from_path (priv->store, &parent, adapter_path) == FALSE)
		return;

	gtk_tree_model_get (GTK_TREE_MODEL(priv->store), &parent,
			    SHELL_BLUETOOTH_COLUMN_PROXY, &adapter, -1);

	if (get_iter_from_address (priv->store, &iter, address, adapter) == FALSE) {
		gtk_tree_store_insert_with_values (priv->store, &iter, &parent, -1,
						   SHELL_BLUETOOTH_COLUMN_ADDRESS, address,
						   SHELL_BLUETOOTH_COLUMN_ALIAS, alias,
						   SHELL_BLUETOOTH_COLUMN_NAME, name,
						   SHELL_BLUETOOTH_COLUMN_TYPE, type,
						   SHELL_BLUETOOTH_COLUMN_ICON, icon,
						   SHELL_BLUETOOTH_COLUMN_LEGACYPAIRING, legacypairing,
						   SHELL_BLUETOOTH_COLUMN_UUIDS, uuids,
						   SHELL_BLUETOOTH_COLUMN_PAIRED, paired,
						   SHELL_BLUETOOTH_COLUMN_CONNECTED, connected,
						   SHELL_BLUETOOTH_COLUMN_TRUSTED, trusted,
						   SHELL_BLUETOOTH_COLUMN_PROXY, device,
						   -1);
	} else {
		gtk_tree_store_set(priv->store, &iter,
				   SHELL_BLUETOOTH_COLUMN_ADDRESS, address,
				   SHELL_BLUETOOTH_COLUMN_ALIAS, alias,
				   SHELL_BLUETOOTH_COLUMN_NAME, name,
				   SHELL_BLUETOOTH_COLUMN_TYPE, type,
				   SHELL_BLUETOOTH_COLUMN_ICON, icon,
				   SHELL_BLUETOOTH_COLUMN_LEGACYPAIRING, legacypairing,
				   SHELL_BLUETOOTH_COLUMN_UUIDS, uuids,
				   SHELL_BLUETOOTH_COLUMN_PAIRED, paired,
				   SHELL_BLUETOOTH_COLUMN_CONNECTED, connected,
				   SHELL_BLUETOOTH_COLUMN_TRUSTED, trusted,
				   SHELL_BLUETOOTH_COLUMN_PROXY, device,
				   -1);
	}
	g_strfreev (uuids);
	g_object_unref (adapter);
}

static void
device_removed (const char      *path,
		ShellBluetoothClient *client)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;

	if (get_iter_from_path(priv->store, &iter, path) == TRUE) {
		/* Note that removal can also happen from adapter_removed. */
		g_signal_emit (G_OBJECT (client), signals[DEVICE_REMOVED], 0, path);
		gtk_tree_store_remove(priv->store, &iter);
	}
}

static void
adapter_set_powered_cb (GDBusProxy *proxy,
			GAsyncResult *res,
			gpointer      user_data)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GVariant) ret = NULL;

	ret = g_dbus_proxy_call_finish (proxy, res, &error);
	if (!ret) {
		g_debug ("Error setting property 'Powered' on interface org.bluez.Adapter1: %s (%s, %d)",
			 error->message, g_quark_to_string (error->domain), error->code);
	}
}

static void
adapter_set_powered (ShellBluetoothClient *client,
		     const char *path,
		     gboolean powered)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	g_autoptr(GObject) adapter = NULL;
	GtkTreeIter iter;
	GVariant *variant;

	g_return_if_fail (SHELL_BLUETOOTH_IS_CLIENT (client));

	if (get_iter_from_path (priv->store, &iter, path) == FALSE)
		return;

	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
			    SHELL_BLUETOOTH_COLUMN_PROXY, &adapter, -1);

	if (adapter == NULL)
		return;

	variant = g_variant_new_boolean (powered);
	g_dbus_proxy_call (G_DBUS_PROXY (adapter),
			   "org.freedesktop.DBus.Properties.Set",
			   g_variant_new ("(ssv)", "org.bluez.Adapter1", "Powered", variant),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   NULL, (GAsyncReadyCallback) adapter_set_powered_cb, client);
}

static void
default_adapter_changed (GDBusObjectManager   *manager,
			 const char           *path,
			 ShellBluetoothClient      *client)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	GtkTreePath *tree_path;
	gboolean powered;

	g_assert (!priv->default_adapter);

	if (get_iter_from_path (priv->store, &iter, path) == FALSE)
		return;

	tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->store), &iter);
	priv->default_adapter = gtk_tree_row_reference_new (GTK_TREE_MODEL (priv->store), tree_path);
	gtk_tree_path_free (tree_path);

	gtk_tree_store_set (priv->store, &iter,
			    SHELL_BLUETOOTH_COLUMN_DEFAULT, TRUE, -1);

	gtk_tree_model_get (GTK_TREE_MODEL(priv->store), &iter,
			   SHELL_BLUETOOTH_COLUMN_POWERED, &powered, -1);

	if (powered) {
		g_object_notify (G_OBJECT (client), "default-adapter");
		g_object_notify (G_OBJECT (client), "default-adapter-powered");
		g_object_notify (G_OBJECT (client), "default-adapter-discoverable");
		g_object_notify (G_OBJECT (client), "default-adapter-discovering");
		g_object_notify (G_OBJECT (client), "default-adapter-name");
		return;
	}

	/*
	 * If the adapter is turn off (Powered = False in bluetooth) object
	 * notifications will be sent only when a Powered = True signal arrives
	 * from bluetoothd
	 */
	adapter_set_powered (client, path, TRUE);
}

static void
adapter_notify_cb (Adapter1       *adapter,
		   GParamSpec     *pspec,
		   ShellBluetoothClient *client)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	const char *property = g_param_spec_get_name (pspec);
	GtkTreeIter iter;
	gboolean notify = TRUE;
	gboolean is_default;

	if (get_iter_from_proxy (priv->store, &iter, G_DBUS_PROXY (adapter)) == FALSE)
		return;

	gtk_tree_model_get (GTK_TREE_MODEL(priv->store), &iter,
			    SHELL_BLUETOOTH_COLUMN_DEFAULT, &is_default, -1);

	if (g_strcmp0 (property, "alias") == 0) {
		const gchar *alias = adapter1_get_alias (adapter);

		gtk_tree_store_set (priv->store, &iter,
				    SHELL_BLUETOOTH_COLUMN_ALIAS, alias, -1);

		if (is_default) {
			g_object_notify (G_OBJECT (client), "default-adapter-powered");
			g_object_notify (G_OBJECT (client), "default-adapter-name");
		}
	} else if (g_strcmp0 (property, "discovering") == 0) {
		gboolean discovering = adapter1_get_discovering (adapter);

		gtk_tree_store_set (priv->store, &iter,
				    SHELL_BLUETOOTH_COLUMN_DISCOVERING, discovering, -1);

		if (is_default)
			g_object_notify (G_OBJECT (client), "default-adapter-discovering");
	} else if (g_strcmp0 (property, "powered") == 0) {
		gboolean powered = adapter1_get_powered (adapter);

		gtk_tree_store_set (priv->store, &iter,
				    SHELL_BLUETOOTH_COLUMN_POWERED, powered, -1);

		if (is_default && powered) {
			g_object_notify (G_OBJECT (client), "default-adapter");
			g_object_notify (G_OBJECT (client), "default-adapter-discoverable");
			g_object_notify (G_OBJECT (client), "default-adapter-discovering");
			g_object_notify (G_OBJECT (client), "default-adapter-name");
		}
		g_object_notify (G_OBJECT (client), "default-adapter-powered");
	} else if (g_strcmp0 (property, "discoverable") == 0) {
		gboolean discoverable = adapter1_get_discoverable (adapter);

		gtk_tree_store_set (priv->store, &iter,
				    SHELL_BLUETOOTH_COLUMN_DISCOVERABLE, discoverable, -1);

		if (is_default)
			g_object_notify (G_OBJECT (client), "default-adapter-discoverable");
	} else {
		notify = FALSE;
	}

	if (notify != FALSE) {
		GtkTreePath *path;

		/* Tell the world */
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->store), &iter);
		gtk_tree_model_row_changed (GTK_TREE_MODEL (priv->store), path, &iter);
		gtk_tree_path_free (path);
	}
}

static void
adapter_added (GDBusObjectManager   *manager,
	       Adapter1             *adapter,
	       ShellBluetoothClient      *client)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	const gchar *address, *name, *alias;
	gboolean discovering, discoverable, powered;

	g_signal_connect_object (G_OBJECT (adapter), "notify",
				 G_CALLBACK (adapter_notify_cb), client, 0);

	address = adapter1_get_address (adapter);
	name = adapter1_get_name (adapter);
	alias = adapter1_get_alias (adapter);
	discovering = adapter1_get_discovering (adapter);
	powered = adapter1_get_powered (adapter);
	discoverable = adapter1_get_discoverable (adapter);

	gtk_tree_store_insert_with_values(priv->store, &iter, NULL, -1,
					  SHELL_BLUETOOTH_COLUMN_PROXY, adapter,
					  SHELL_BLUETOOTH_COLUMN_ADDRESS, address,
					  SHELL_BLUETOOTH_COLUMN_NAME, name,
					  SHELL_BLUETOOTH_COLUMN_ALIAS, alias,
					  SHELL_BLUETOOTH_COLUMN_DISCOVERING, discovering,
					  SHELL_BLUETOOTH_COLUMN_DISCOVERABLE, discoverable,
					  SHELL_BLUETOOTH_COLUMN_POWERED, powered,
					  -1);

	if (!priv->default_adapter) {
		default_adapter_changed (manager,
					 g_dbus_object_get_object_path (g_dbus_interface_get_object (G_DBUS_INTERFACE (adapter))),
					 client);
	}
}

static void
adapter_removed (GDBusObjectManager   *manager,
		 const char           *path,
		 ShellBluetoothClient      *client)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter, childiter;
	gboolean was_default;
	gboolean have_child;

	if (get_iter_from_path (priv->store, &iter, path) == FALSE)
		return;

	gtk_tree_model_get (GTK_TREE_MODEL(priv->store), &iter,
			   SHELL_BLUETOOTH_COLUMN_DEFAULT, &was_default, -1);

	if (!was_default)
		return;

	/* Ensure that all devices are removed. This can happen if bluetoothd
	 * crashes as the "object-removed" signal is emitted in an undefined
	 * order. */
	have_child = gtk_tree_model_iter_children (GTK_TREE_MODEL (priv->store), &childiter, &iter);
	while (have_child) {
		GDBusProxy *object;

		gtk_tree_model_get (GTK_TREE_MODEL(priv->store), &childiter,
				    SHELL_BLUETOOTH_COLUMN_PROXY, &object, -1);

		g_signal_emit (G_OBJECT (client), signals[DEVICE_REMOVED], 0, g_dbus_proxy_get_object_path (object));
		g_object_unref (object);

		have_child = gtk_tree_store_remove (priv->store, &childiter);
	}

	g_clear_pointer (&priv->default_adapter, gtk_tree_row_reference_free);
	gtk_tree_store_remove (priv->store, &iter);

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL(priv->store),
					   &iter)) {
		GDBusProxy *adapter;
		const char *adapter_path;

		gtk_tree_model_get (GTK_TREE_MODEL(priv->store), &iter,
				   SHELL_BLUETOOTH_COLUMN_PROXY, &adapter, -1);

		adapter_path = g_dbus_proxy_get_object_path (adapter);
		default_adapter_changed (manager, adapter_path, client);

		g_object_unref(adapter);
	} else {
		g_object_notify (G_OBJECT (client), "default-adapter");
		g_object_notify (G_OBJECT (client), "default-adapter-powered");
		g_object_notify (G_OBJECT (client), "default-adapter-discoverable");
		g_object_notify (G_OBJECT (client), "default-adapter-discovering");
	}
}

static GType
object_manager_get_proxy_type_func (GDBusObjectManagerClient *manager,
				    const gchar              *object_path,
				    const gchar              *interface_name,
				    gpointer                  user_data)
{
	if (interface_name == NULL)
		return G_TYPE_DBUS_OBJECT_PROXY;

	if (g_str_equal (interface_name, BLUEZ_DEVICE_INTERFACE))
		return TYPE_DEVICE1_PROXY;
	if (g_str_equal (interface_name, BLUEZ_ADAPTER_INTERFACE))
		return TYPE_ADAPTER1_PROXY;

	return G_TYPE_DBUS_PROXY;
}

static void
interface_added (GDBusObjectManager *manager,
		 GDBusObject        *object,
		 GDBusInterface     *interface,
		 gpointer            user_data)
{
	ShellBluetoothClient *client = user_data;

	if (IS_ADAPTER1 (interface)) {
		adapter_added (manager,
			       ADAPTER1 (interface),
			       client);
	} else if (IS_DEVICE1 (interface)) {
		device_added (manager,
			      DEVICE1 (interface),
			      client);
	}
}

static void
interface_removed (GDBusObjectManager *manager,
		   GDBusObject        *object,
		   GDBusInterface     *interface,
		   gpointer            user_data)
{
	ShellBluetoothClient *client = user_data;

	if (IS_ADAPTER1 (interface)) {
		adapter_removed (manager,
				 g_dbus_object_get_object_path (object),
				 client);
	} else if (IS_DEVICE1 (interface)) {
		device_removed (g_dbus_object_get_object_path (object),
				client);
	}
}

static void
object_added (GDBusObjectManager *manager,
	      GDBusObject        *object,
	      ShellBluetoothClient    *client)
{
	GList *interfaces, *l;

	interfaces = g_dbus_object_get_interfaces (object);

	for (l = interfaces; l != NULL; l = l->next)
		interface_added (manager, object, G_DBUS_INTERFACE (l->data), client);

	g_list_free_full (interfaces, g_object_unref);
}

static void
object_removed (GDBusObjectManager *manager,
	        GDBusObject        *object,
	        ShellBluetoothClient    *client)
{
	GList *interfaces, *l;

	interfaces = g_dbus_object_get_interfaces (object);

	for (l = interfaces; l != NULL; l = l->next)
		interface_removed (manager, object, G_DBUS_INTERFACE (l->data), client);

	g_list_free_full (interfaces, g_object_unref);
}

static void
object_manager_new_callback(GObject      *source_object,
			    GAsyncResult *res,
			    void         *user_data)
{
	ShellBluetoothClient *client;
	ShellBluetoothClientPrivate *priv;
	GDBusObjectManager *manager;
	GList *object_list, *l;
	GError *error = NULL;

	manager = g_dbus_object_manager_client_new_for_bus_finish (res, &error);
	if (!manager) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Could not create bluez object manager: %s", error->message);
		g_error_free (error);
		return;
	}

	client = SHELL_BLUETOOTH_CLIENT (user_data);
	priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	priv->manager = manager;

	g_signal_connect_object (G_OBJECT (priv->manager), "interface-added", (GCallback) interface_added, client, 0);
	g_signal_connect_object (G_OBJECT (priv->manager), "interface-removed", (GCallback) interface_removed, client, 0);

	g_signal_connect_object (G_OBJECT (priv->manager), "object-added", (GCallback) object_added, client, 0);
	g_signal_connect_object (G_OBJECT (priv->manager), "object-removed", (GCallback) object_removed, client, 0);

	object_list = g_dbus_object_manager_get_objects (priv->manager);

	/* We need to add the adapters first, otherwise the devices will
	 * be dropped to the floor, as they wouldn't have a parent in
	 * the treestore */
	for (l = object_list; l != NULL; l = l->next) {
		GDBusObject *object = l->data;
		GDBusInterface *iface;

		iface = g_dbus_object_get_interface (object, BLUEZ_ADAPTER_INTERFACE);
		if (!iface)
			continue;

		adapter_added (priv->manager,
			       ADAPTER1 (iface),
			       client);
	}

	for (l = object_list; l != NULL; l = l->next) {
		GDBusObject *object = l->data;
		GDBusInterface *iface;

		iface = g_dbus_object_get_interface (object, BLUEZ_DEVICE_INTERFACE);
		if (!iface)
			continue;

		device_added (priv->manager,
			      DEVICE1 (iface),
			      client);
	}
	g_list_free_full (object_list, g_object_unref);
}

static void shell_bluetooth_client_init(ShellBluetoothClient *client)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);

	priv->cancellable = g_cancellable_new ();
	priv->store = gtk_tree_store_new(_SHELL_BLUETOOTH_NUM_COLUMNS,
					 G_TYPE_OBJECT,     /* SHELL_BLUETOOTH_COLUMN_PROXY */
					 G_TYPE_OBJECT,     /* SHELL_BLUETOOTH_COLUMN_PROPERTIES */
					 G_TYPE_STRING,     /* SHELL_BLUETOOTH_COLUMN_ADDRESS */
					 G_TYPE_STRING,     /* SHELL_BLUETOOTH_COLUMN_ALIAS */
					 G_TYPE_STRING,     /* SHELL_BLUETOOTH_COLUMN_NAME */
					 G_TYPE_UINT,       /* SHELL_BLUETOOTH_COLUMN_TYPE */
					 G_TYPE_STRING,     /* SHELL_BLUETOOTH_COLUMN_ICON */
					 G_TYPE_BOOLEAN,    /* SHELL_BLUETOOTH_COLUMN_DEFAULT */
					 G_TYPE_BOOLEAN,    /* SHELL_BLUETOOTH_COLUMN_PAIRED */
					 G_TYPE_BOOLEAN,    /* SHELL_BLUETOOTH_COLUMN_TRUSTED */
					 G_TYPE_BOOLEAN,    /* SHELL_BLUETOOTH_COLUMN_CONNECTED */
					 G_TYPE_BOOLEAN,    /* SHELL_BLUETOOTH_COLUMN_DISCOVERABLE */
					 G_TYPE_BOOLEAN,    /* SHELL_BLUETOOTH_COLUMN_DISCOVERING */
					 G_TYPE_INT,        /* SHELL_BLUETOOTH_COLUMN_LEGACYPAIRING */
					 G_TYPE_BOOLEAN,    /* SHELL_BLUETOOTH_COLUMN_POWERED */
					 G_TYPE_HASH_TABLE, /* SHELL_BLUETOOTH_COLUMN_SERVICES */
					 G_TYPE_STRV);      /* SHELL_BLUETOOTH_COLUMN_UUIDS */

	g_dbus_object_manager_client_new_for_bus (G_BUS_TYPE_SYSTEM,
						  G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START,
						  BLUEZ_SERVICE,
						  BLUEZ_MANAGER_PATH,
						  object_manager_get_proxy_type_func,
						  NULL, NULL,
						  priv->cancellable,
						  object_manager_new_callback, client);
}

static GDBusProxy *
_shell_bluetooth_client_get_default_adapter(ShellBluetoothClient *client)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreePath *path;
	GtkTreeIter iter;
	GDBusProxy *adapter;

	g_return_val_if_fail (SHELL_BLUETOOTH_IS_CLIENT (client), NULL);

	if (priv->default_adapter == NULL)
		return NULL;

	path = gtk_tree_row_reference_get_path (priv->default_adapter);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
			    SHELL_BLUETOOTH_COLUMN_PROXY, &adapter, -1);
	gtk_tree_path_free (path);

	return adapter;
}

static const char*
_shell_bluetooth_client_get_default_adapter_path (ShellBluetoothClient *self)
{
	GDBusProxy *adapter = _shell_bluetooth_client_get_default_adapter (self);

	if (adapter != NULL) {
		const char *ret = g_dbus_proxy_get_object_path (adapter);
		g_object_unref (adapter);
		return ret;
	}
	return NULL;
}

static gboolean
_shell_bluetooth_client_get_default_adapter_powered (ShellBluetoothClient *self)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE (self);
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean ret;

	if (priv->default_adapter == NULL)
		return FALSE;

	path = gtk_tree_row_reference_get_path (priv->default_adapter);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, SHELL_BLUETOOTH_COLUMN_POWERED, &ret, -1);
	gtk_tree_path_free (path);

	return ret;
}

static char *
_shell_bluetooth_client_get_default_adapter_name (ShellBluetoothClient *self)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE (self);
	GtkTreePath *path;
	GtkTreeIter iter;
	char *ret;

	if (priv->default_adapter == NULL)
		return NULL;

	path = gtk_tree_row_reference_get_path (priv->default_adapter);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, SHELL_BLUETOOTH_COLUMN_ALIAS, &ret, -1);
	gtk_tree_path_free (path);

	return ret;
}

static void
_shell_bluetooth_client_set_default_adapter_discovering (ShellBluetoothClient *client,
						   gboolean         discovering,
						   gboolean         discoverable)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE (client);
	g_autoptr(GDBusProxy) adapter = NULL;
	GVariantBuilder builder;

	adapter = _shell_bluetooth_client_get_default_adapter (client);
	if (adapter == NULL)
		return;

	if (discovering) {
		g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
		g_variant_builder_add (&builder, "{sv}",
				       "Discoverable", g_variant_new_boolean (discoverable));
		adapter1_call_set_discovery_filter_sync (ADAPTER1 (adapter),
							 g_variant_builder_end (&builder), NULL, NULL);
	}

	priv->discovery_started = discovering;
	if (discovering)
		adapter1_call_start_discovery (ADAPTER1 (adapter), NULL, NULL, NULL);
	else
		adapter1_call_stop_discovery (ADAPTER1 (adapter), NULL, NULL, NULL);
}

static gboolean
_shell_bluetooth_client_get_default_adapter_discovering (ShellBluetoothClient *self)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE (self);
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean ret;

	if (priv->default_adapter == NULL)
		return FALSE;

	path = gtk_tree_row_reference_get_path (priv->default_adapter);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, SHELL_BLUETOOTH_COLUMN_DISCOVERING, &ret, -1);
	gtk_tree_path_free (path);

	return ret;
}

static void
shell_bluetooth_client_get_property (GObject        *object,
			       guint           property_id,
			       GValue         *value,
			       GParamSpec     *pspec)
{
	ShellBluetoothClient *self = SHELL_BLUETOOTH_CLIENT (object);
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE (self);

	switch (property_id) {
	case PROP_DEFAULT_ADAPTER:
		g_value_set_string (value, _shell_bluetooth_client_get_default_adapter_path (self));
		break;
	case PROP_DEFAULT_ADAPTER_POWERED:
		g_value_set_boolean (value, _shell_bluetooth_client_get_default_adapter_powered (self));
		break;
	case PROP_DEFAULT_ADAPTER_NAME:
		g_value_take_string (value, _shell_bluetooth_client_get_default_adapter_name (self));
		break;
	case PROP_DEFAULT_ADAPTER_DISCOVERABLE:
		g_value_set_boolean (value, priv->disco_during_disco);
		break;
	case PROP_DEFAULT_ADAPTER_DISCOVERING:
		g_value_set_boolean (value, _shell_bluetooth_client_get_default_adapter_discovering (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
shell_bluetooth_client_set_property (GObject        *object,
			       guint           property_id,
			       const GValue   *value,
			       GParamSpec     *pspec)
{
	ShellBluetoothClient *self = SHELL_BLUETOOTH_CLIENT (object);
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE (self);

	switch (property_id) {
	case PROP_DEFAULT_ADAPTER_DISCOVERABLE:
		priv->disco_during_disco = g_value_get_boolean (value);
		_shell_bluetooth_client_set_default_adapter_discovering (self, priv->discovery_started, priv->disco_during_disco);
		break;
	case PROP_DEFAULT_ADAPTER_DISCOVERING:
		_shell_bluetooth_client_set_default_adapter_discovering (self, g_value_get_boolean (value), priv->disco_during_disco);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void shell_bluetooth_client_finalize(GObject *object)
{
	ShellBluetoothClient *client = SHELL_BLUETOOTH_CLIENT (object);
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE (client);

	if (priv->cancellable != NULL) {
		g_cancellable_cancel (priv->cancellable);
		g_clear_object (&priv->cancellable);
	}
	g_clear_object (&priv->manager);
	g_object_unref (priv->store);

	g_clear_pointer (&priv->default_adapter, gtk_tree_row_reference_free);

	G_OBJECT_CLASS(shell_bluetooth_client_parent_class)->finalize (object);
}

static void shell_bluetooth_client_class_init(ShellBluetoothClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = shell_bluetooth_client_finalize;
	object_class->get_property = shell_bluetooth_client_get_property;
	object_class->set_property = shell_bluetooth_client_set_property;

	/**
	 * ShellBluetoothClient::device-removed:
	 * @client: a #ShellBluetoothClient object which received the signal
	 * @device: the D-Bus object path for the now-removed device
	 *
	 * The #ShellBluetoothClient::device-removed signal is launched when a
	 * device gets removed from the model.
	 **/
	signals[DEVICE_REMOVED] =
		g_signal_new ("device-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	/**
	 * ShellBluetoothClient:default-adapter:
	 *
	 * The D-Bus path of the default ShellBluetooth adapter or %NULL.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER,
					 g_param_spec_string ("default-adapter", NULL,
							      "The D-Bus path of the default adapter",
							      NULL, G_PARAM_READABLE));
	/**
	 * ShellBluetoothClient:default-adapter-powered:
	 *
	 * %TRUE if the default ShellBluetooth adapter is powered.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_POWERED,
					 g_param_spec_boolean ("default-adapter-powered", NULL,
							      "Whether the default adapter is powered",
							       FALSE, G_PARAM_READABLE));
	/**
	 * ShellBluetoothClient:default-adapter-discoverable:
	 *
	 * %TRUE if the default ShellBluetooth adapter is discoverable during discovery.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_DISCOVERABLE,
					 g_param_spec_boolean ("default-adapter-discoverable", NULL,
							      "Whether the default adapter is visible by other devices during discovery",
							       FALSE, G_PARAM_READWRITE));
	/**
	 * ShellBluetoothClient:default-adapter-name:
	 *
	 * The name of the default ShellBluetooth adapter or %NULL.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_NAME,
					 g_param_spec_string ("default-adapter-name", NULL,
							      "The human readable name of the default adapter",
							      NULL, G_PARAM_READABLE));
	/**
	 * ShellBluetoothClient:default-adapter-discovering:
	 *
	 * %TRUE if the default ShellBluetooth adapter is discovering.
	 */
	g_object_class_install_property (object_class, PROP_DEFAULT_ADAPTER_DISCOVERING,
					 g_param_spec_boolean ("default-adapter-discovering", NULL,
							      "Whether the default adapter is searching for devices",
							       FALSE, G_PARAM_READWRITE));
}

/**
 * shell_bluetooth_client_new:
 *
 * Returns a reference to the #ShellBluetoothClient singleton. Use g_object_unref() when done with the object.
 *
 * Return value: (transfer full): a #ShellBluetoothClient object.
 **/
ShellBluetoothClient *shell_bluetooth_client_new(void)
{
	static ShellBluetoothClient *shell_bluetooth_client = NULL;

	if (shell_bluetooth_client != NULL)
		return g_object_ref(shell_bluetooth_client);

	shell_bluetooth_client = SHELL_BLUETOOTH_CLIENT (g_object_new (SHELL_BLUETOOTH_TYPE_CLIENT, NULL));
	g_object_add_weak_pointer (G_OBJECT (shell_bluetooth_client),
				   (gpointer) &shell_bluetooth_client);

	return shell_bluetooth_client;
}

/**
 * shell_bluetooth_client_get_model:
 * @client: a #ShellBluetoothClient object
 *
 * Returns an unfiltered #GtkTreeModel representing the adapter and devices available on the system.
 *
 * Return value: (transfer full): a #GtkTreeModel object.
 **/
GtkTreeModel *shell_bluetooth_client_get_model (ShellBluetoothClient *client)
{
	ShellBluetoothClientPrivate *priv;
	GtkTreeModel *model;

	g_return_val_if_fail (SHELL_BLUETOOTH_IS_CLIENT (client), NULL);

	priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	model = GTK_TREE_MODEL (g_object_ref(priv->store));

	return model;
}

/**
 * shell_bluetooth_client_get_filter_model:
 * @client: a #ShellBluetoothClient object
 * @func: a #GtkTreeModelFilterVisibleFunc
 * @data: user data to pass to gtk_tree_model_filter_set_visible_func()
 * @destroy: a destroy function for gtk_tree_model_filter_set_visible_func()
 *
 * Returns a #GtkTreeModelFilter of devices filtered using the @func, @data and @destroy arguments to pass to gtk_tree_model_filter_set_visible_func().
 *
 * Return value: (transfer full): a #GtkTreeModel object.
 **/
GtkTreeModel *shell_bluetooth_client_get_filter_model (ShellBluetoothClient *client,
						 GtkTreeModelFilterVisibleFunc func,
						 gpointer data, GDestroyNotify destroy)
{
	ShellBluetoothClientPrivate *priv;
	GtkTreeModel *model;

	g_return_val_if_fail (SHELL_BLUETOOTH_IS_CLIENT (client), NULL);

	priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	model = gtk_tree_model_filter_new(GTK_TREE_MODEL(priv->store), NULL);

	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(model),
							func, data, destroy);

	return model;
}

static gboolean adapter_filter(GtkTreeModel *model,
					GtkTreeIter *iter, gpointer user_data)
{
	GDBusProxy *proxy;
	gboolean active;

	gtk_tree_model_get(model, iter, SHELL_BLUETOOTH_COLUMN_PROXY, &proxy, -1);

	if (proxy == NULL)
		return FALSE;

	active = g_str_equal(BLUEZ_ADAPTER_INTERFACE,
					g_dbus_proxy_get_interface_name(proxy));

	g_object_unref(proxy);

	return active;
}

/**
 * shell_bluetooth_client_get_adapter_model:
 * @client: a #ShellBluetoothClient object
 *
 * Returns a #GtkTreeModelFilter with only adapters present.
 *
 * Return value: (transfer full): a #GtkTreeModel object.
 **/
GtkTreeModel *shell_bluetooth_client_get_adapter_model (ShellBluetoothClient *client)
{
	return shell_bluetooth_client_get_filter_model (client, adapter_filter,
						  NULL, NULL);
}

/**
 * shell_bluetooth_client_get_device_model:
 * @client: a #ShellBluetoothClient object
 *
 * Returns a #GtkTreeModelFilter with only devices belonging to the default adapter listed.
 * Note that the model will follow a specific adapter, and will not follow the default adapter.
 * Also note that due to the way #GtkTreeModelFilter works, you will probably want to
 * monitor signals on the "child-model" #GtkTreeModel to monitor for changes.
 *
 * Return value: (transfer full): a #GtkTreeModel object.
 **/
GtkTreeModel *shell_bluetooth_client_get_device_model (ShellBluetoothClient *client)
{
	ShellBluetoothClientPrivate *priv;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean cont, found = FALSE;

	g_return_val_if_fail (SHELL_BLUETOOTH_IS_CLIENT (client), NULL);

	priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(priv->store), &iter);

	while (cont == TRUE) {
		gboolean is_default;

		gtk_tree_model_get (GTK_TREE_MODEL(priv->store), &iter,
				    SHELL_BLUETOOTH_COLUMN_DEFAULT, &is_default, -1);

		if (is_default == TRUE) {
			found = TRUE;
			break;
		}

		cont = gtk_tree_model_iter_next (GTK_TREE_MODEL(priv->store), &iter);
	}

	if (found == TRUE) {
		path = gtk_tree_model_get_path (GTK_TREE_MODEL(priv->store), &iter);
		model = gtk_tree_model_filter_new (GTK_TREE_MODEL(priv->store), path);
		gtk_tree_path_free (path);
	} else
		model = NULL;

	return model;
}

static void
connect_callback (GDBusProxy   *proxy,
		  GAsyncResult *res,
		  GTask        *task)
{
	gboolean retval;
	GError *error = NULL;

	retval = device1_call_connect_finish (DEVICE1 (proxy), res, &error);
	if (retval == FALSE) {
		g_debug ("Connect failed for %s: %s",
			 g_dbus_proxy_get_object_path (proxy), error->message);
		g_task_return_error (task, error);
	} else {
		g_debug ("Connect succeeded for %s",
			 g_dbus_proxy_get_object_path (proxy));
		g_task_return_boolean (task, retval);
	}

	g_object_unref (task);
}

static void
disconnect_callback (GDBusProxy   *proxy,
		     GAsyncResult *res,
		     GTask        *task)
{
	gboolean retval;
	GError *error = NULL;

	retval = device1_call_disconnect_finish (DEVICE1 (proxy), res, &error);
	if (retval == FALSE) {
		g_debug ("Disconnect failed for %s: %s",
			 g_dbus_proxy_get_object_path (proxy),
			 error->message);
		g_task_return_error (task, error);
	} else {
		g_debug ("Disconnect succeeded for %s",
			 g_dbus_proxy_get_object_path (proxy));
		g_task_return_boolean (task, retval);
	}

	g_object_unref (task);
}

/**
 * shell_bluetooth_client_connect_service:
 * @client: a #ShellBluetoothClient
 * @path: the object path on which to operate
 * @connect: Whether try to connect or disconnect from services on a device
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the connection is complete
 * @user_data: the data to pass to callback function
 *
 * When the connection operation is finished, @callback will be called. You can
 * then call shell_bluetooth_client_connect_service_finish() to get the result of the
 * operation.
 **/
void
shell_bluetooth_client_connect_service (ShellBluetoothClient     *client,
				  const char          *path,
				  gboolean             connect,
				  GCancellable        *cancellable,
				  GAsyncReadyCallback  callback,
				  gpointer             user_data)
{
	ShellBluetoothClientPrivate *priv = SHELL_BLUETOOTH_CLIENT_GET_PRIVATE(client);
	GtkTreeIter iter;
	GTask *task;
	g_autoptr(GDBusProxy) device = NULL;

	g_return_if_fail (SHELL_BLUETOOTH_IS_CLIENT (client));
	g_return_if_fail (path != NULL);

	task = g_task_new (G_OBJECT (client),
			   cancellable,
			   callback,
			   user_data);
	g_task_set_source_tag (task, shell_bluetooth_client_connect_service);

	if (get_iter_from_path (priv->store, &iter, path) == FALSE) {
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
					 "Device with object path %s does not exist",
					 path);
		g_object_unref (task);
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL(priv->store), &iter,
			    SHELL_BLUETOOTH_COLUMN_PROXY, &device,
			    -1);

	if (connect) {
		device1_call_connect (DEVICE1(device),
				      cancellable,
				      (GAsyncReadyCallback) connect_callback,
				      task);
	} else {
		device1_call_disconnect (DEVICE1(device),
					 cancellable,
					 (GAsyncReadyCallback) disconnect_callback,
					 task);
	}
}

/**
 * shell_bluetooth_client_connect_service_finish:
 * @client: a #ShellBluetoothClient
 * @res: a #GAsyncResult
 * @error: a #GError
 *
 * Finishes the connection operation. See shell_bluetooth_client_connect_service().
 *
 * Returns: %TRUE if the connection operation succeeded, %FALSE otherwise.
 **/
gboolean
shell_bluetooth_client_connect_service_finish (ShellBluetoothClient *client,
					 GAsyncResult    *res,
					 GError         **error)
{
	GTask *task;

	task = G_TASK (res);

	g_warn_if_fail (g_task_get_source_tag (task) == shell_bluetooth_client_connect_service);

	return g_task_propagate_boolean (task, error);
}
