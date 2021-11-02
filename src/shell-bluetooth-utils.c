/*
 *
 *  BlueZ - ShellBluetooth protocol stack for Linux
 *
 *  Copyright (C) 2009-2011  Bastien Nocera <hadess@hadess.net>
 *  Copyright (C) 2010       Giovanni Campagna <scampa.giovanni@gmail.com>
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
 * SECTION:bluetooth-utils
 * @short_description: ShellBluetooth utility functions
 * @stability: Stable
 * @include: bluetooth-utils.h
 *
 * Those helper functions are used throughout the ShellBluetooth
 * management utilities.
 **/

#include <config.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "shell-bluetooth-utils.h"
#include "shell-enum-types.h"

/**
 * shell_bluetooth_type_to_string:
 * @type: a #ShellBluetoothType
 *
 * Returns a human-readable string representation of @type usable for display to users. Do not free the return value.
 * The returned string is already translated with gettext().
 *
 * Return value: a string.
 **/
const gchar *
shell_bluetooth_type_to_string (ShellBluetoothType type)
{
	switch (type) {
	case SHELL_BLUETOOTH_TYPE_PHONE:
		return _("Phone");
	case SHELL_BLUETOOTH_TYPE_MODEM:
		return _("Modem");
	case SHELL_BLUETOOTH_TYPE_COMPUTER:
		return _("Computer");
	case SHELL_BLUETOOTH_TYPE_NETWORK:
		return _("Network");
	case SHELL_BLUETOOTH_TYPE_HEADSET:
		/* translators: a hands-free headset, a combination of a single speaker with a microphone */
		return _("Headset");
	case SHELL_BLUETOOTH_TYPE_HEADPHONES:
		return _("Headphones");
	case SHELL_BLUETOOTH_TYPE_OTHER_AUDIO:
		return _("Audio device");
	case SHELL_BLUETOOTH_TYPE_KEYBOARD:
		return _("Keyboard");
	case SHELL_BLUETOOTH_TYPE_MOUSE:
		return _("Mouse");
	case SHELL_BLUETOOTH_TYPE_CAMERA:
		return _("Camera");
	case SHELL_BLUETOOTH_TYPE_PRINTER:
		return _("Printer");
	case SHELL_BLUETOOTH_TYPE_JOYPAD:
		return _("Joypad");
	case SHELL_BLUETOOTH_TYPE_TABLET:
		return _("Tablet");
	case SHELL_BLUETOOTH_TYPE_VIDEO:
		return _("Video device");
	case SHELL_BLUETOOTH_TYPE_REMOTE_CONTROL:
		return _("Remote control");
	case SHELL_BLUETOOTH_TYPE_SCANNER:
		return _("Scanner");
	case SHELL_BLUETOOTH_TYPE_DISPLAY:
		return _("Display");
	case SHELL_BLUETOOTH_TYPE_WEARABLE:
		return _("Wearable");
	case SHELL_BLUETOOTH_TYPE_TOY:
		return _("Toy");
	}

	return _("Unknown");
}

/**
 * shell_bluetooth_type_to_filter_string:
 * @type: a #ShellBluetoothType
 *
 * Returns a human-readable string representation of @type usable for display to users,
 * when type filters are displayed. Do not free the return value.
 * The returned string is already translated with gettext().
 *
 * Return value: a string.
 **/
const gchar *
shell_bluetooth_type_to_filter_string (ShellBluetoothType type)
{
	switch (type) {
	case SHELL_BLUETOOTH_TYPE_ANY:
		return _("All types");
	default:
		return shell_bluetooth_type_to_string (type);
	}

	g_assert_not_reached ();
}

/**
 * shell_bluetooth_verify_address:
 * @bdaddr: a string representing a ShellBluetooth address
 *
 * Returns whether the string is a valid ShellBluetooth address. This does not contact the device in any way.
 *
 * Return value: %TRUE if the address is valid, %FALSE if not.
 **/
gboolean
shell_bluetooth_verify_address (const char *bdaddr)
{
	guint i;

	g_return_val_if_fail (bdaddr != NULL, FALSE);

	if (strlen (bdaddr) != 17)
		return FALSE;

	for (i = 0; i < 17; i++) {
		if (((i + 1) % 3) == 0) {
			if (bdaddr[i] != ':')
				return FALSE;
			continue;
		}
		if (g_ascii_isxdigit (bdaddr[i]) == FALSE)
			return FALSE;
	}

	return TRUE;
}

/**
 * shell_bluetooth_class_to_type:
 * @class: a ShellBluetooth device class
 *
 * Returns the type of device corresponding to the given @class value.
 *
 * Return value: a #ShellBluetoothType.
 **/
ShellBluetoothType
shell_bluetooth_class_to_type (guint32 class)
{
	switch ((class & 0x1f00) >> 8) {
	case 0x01:
		return SHELL_BLUETOOTH_TYPE_COMPUTER;
	case 0x02:
		switch ((class & 0xfc) >> 2) {
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x05:
			return SHELL_BLUETOOTH_TYPE_PHONE;
		case 0x04:
			return SHELL_BLUETOOTH_TYPE_MODEM;
		}
		break;
	case 0x03:
		return SHELL_BLUETOOTH_TYPE_NETWORK;
	case 0x04:
		switch ((class & 0xfc) >> 2) {
		case 0x01:
		case 0x02:
			return SHELL_BLUETOOTH_TYPE_HEADSET;
		case 0x05:
			return SHELL_BLUETOOTH_TYPE_SPEAKERS;
		case 0x06:
			return SHELL_BLUETOOTH_TYPE_HEADPHONES;
		case 0x0b: /* VCR */
		case 0x0c: /* Video Camera */
		case 0x0d: /* Camcorder */
			return SHELL_BLUETOOTH_TYPE_VIDEO;
		default:
			return SHELL_BLUETOOTH_TYPE_OTHER_AUDIO;
		}
		break;
	case 0x05:
		switch ((class & 0xc0) >> 6) {
		case 0x00:
			switch ((class & 0x1e) >> 2) {
			case 0x01:
			case 0x02:
				return SHELL_BLUETOOTH_TYPE_JOYPAD;
			case 0x03:
				return SHELL_BLUETOOTH_TYPE_REMOTE_CONTROL;
			}
			break;
		case 0x01:
			return SHELL_BLUETOOTH_TYPE_KEYBOARD;
		case 0x02:
			switch ((class & 0x1e) >> 2) {
			case 0x05:
				return SHELL_BLUETOOTH_TYPE_TABLET;
			default:
				return SHELL_BLUETOOTH_TYPE_MOUSE;
			}
		}
		break;
	case 0x06:
		if (class & 0x80)
			return SHELL_BLUETOOTH_TYPE_PRINTER;
		if (class & 0x40)
			return SHELL_BLUETOOTH_TYPE_SCANNER;
		if (class & 0x20)
			return SHELL_BLUETOOTH_TYPE_CAMERA;
		if (class & 0x10)
			return SHELL_BLUETOOTH_TYPE_DISPLAY;
		break;
	case 0x07:
		return SHELL_BLUETOOTH_TYPE_WEARABLE;
	case 0x08:
		return SHELL_BLUETOOTH_TYPE_TOY;
	}

	return 0;
}

/**
 * shell_bluetooth_appearance_to_type:
 * @appearance: a ShellBluetooth device appearance
 *
 * Returns the type of device corresponding to the given @appearance value,
 * as usually found in the GAP service.
 *
 * Return value: a #ShellBluetoothType.
 **/
ShellBluetoothType
shell_bluetooth_appearance_to_type (guint16 appearance)
{
	switch ((appearance & 0xffc0) >> 6) {
	case 0x01:
		return SHELL_BLUETOOTH_TYPE_PHONE;
	case 0x02:
		return SHELL_BLUETOOTH_TYPE_COMPUTER;
	case 0x05:
		return SHELL_BLUETOOTH_TYPE_DISPLAY;
	case 0x0a:
		return SHELL_BLUETOOTH_TYPE_OTHER_AUDIO;
	case 0x0b:
		return SHELL_BLUETOOTH_TYPE_SCANNER;
	case 0x0f: /* HID Generic */
		switch (appearance & 0x3f) {
		case 0x01:
			return SHELL_BLUETOOTH_TYPE_KEYBOARD;
		case 0x02:
			return SHELL_BLUETOOTH_TYPE_MOUSE;
		case 0x03:
		case 0x04:
			return SHELL_BLUETOOTH_TYPE_JOYPAD;
		case 0x05:
			return SHELL_BLUETOOTH_TYPE_TABLET;
		case 0x08:
			return SHELL_BLUETOOTH_TYPE_SCANNER;
		}
		break;
	}

	return 0;

}

static const char *
uuid16_custom_to_string (guint uuid16, const char *uuid)
{
	switch (uuid16) {
	case 0x2:
		return "SyncMLClient";
	case 0x5601:
		return "Nokia SyncML Server";
	default:
		g_debug ("Unhandled custom UUID %s (0x%x)", uuid, uuid16);
		return NULL;
	}
}

/* Short names from Table 2 at:
 * https://www.bluetooth.org/Technical/AssignedNumbers/service_discovery.htm */
static const char *
uuid16_to_string (guint uuid16, const char *uuid)
{
	switch (uuid16) {
	case SHELL_BLUETOOTH_UUID_SPP:
		return "SerialPort";
	case SHELL_BLUETOOTH_UUID_DUN:
		return "DialupNetworking";
	case SHELL_BLUETOOTH_UUID_IRMC:
		return "IrMCSync";
	case SHELL_BLUETOOTH_UUID_OPP:
		return "OBEXObjectPush";
	case SHELL_BLUETOOTH_UUID_FTP:
		return "OBEXFileTransfer";
	case SHELL_BLUETOOTH_UUID_HSP:
		return "HSP";
	case SHELL_BLUETOOTH_UUID_A2DP_SOURCE:
		return "AudioSource";
	case SHELL_BLUETOOTH_UUID_A2DP_SINK:
		return "AudioSink";
	case SHELL_BLUETOOTH_UUID_AVRCP_TARGET:
		return "A/V_RemoteControlTarget";
	case SHELL_BLUETOOTH_UUID_A2DP:
		return "AdvancedAudioDistribution";
	case SHELL_BLUETOOTH_UUID_AVRCP_CONTROL:
		return "A/V_RemoteControl";
	case SHELL_BLUETOOTH_UUID_HSP_AG:
		return "Headset_-_AG";
	case SHELL_BLUETOOTH_UUID_PAN_PANU:
		return "PANU";
	case SHELL_BLUETOOTH_UUID_PAN_NAP:
		return "NAP";
	case SHELL_BLUETOOTH_UUID_PAN_GN:
		return "GN";
	case SHELL_BLUETOOTH_UUID_HFP_HF:
		return "Handsfree";
	case SHELL_BLUETOOTH_UUID_HFP_AG:
		return "HandsfreeAudioGateway";
	case SHELL_BLUETOOTH_UUID_HID:
	case 0x1812:
		return "HumanInterfaceDeviceService";
	case SHELL_BLUETOOTH_UUID_SAP:
		return "SIM_Access";
	case SHELL_BLUETOOTH_UUID_PBAP:
		return "Phonebook_Access_-_PSE";
	case SHELL_BLUETOOTH_UUID_GENERIC_AUDIO:
		return "GenericAudio";
	case SHELL_BLUETOOTH_UUID_SDP: /* ServiceDiscoveryServerServiceClassID */
	case SHELL_BLUETOOTH_UUID_PNP: /* PnPInformation */
		/* Those are ignored */
		return NULL;
	case SHELL_BLUETOOTH_UUID_GENERIC_NET:
		return "GenericNetworking";
	case SHELL_BLUETOOTH_UUID_VDP_SOURCE:
		return "VideoSource";
	case 0x8e771303:
	case 0x8e771301:
		return "SEMC HLA";
	case 0x8e771401:
		return "SEMC Watch Phone";
	default:
		g_debug ("Unhandled UUID %s (0x%x)", uuid, uuid16);
		return NULL;
	}
}

/**
 * shell_bluetooth_uuid_to_string:
 * @uuid: a string representing a ShellBluetooth UUID
 *
 * Returns a string representing a human-readable (but not usable for display to users) version of the @uuid. Do not free the return value.
 *
 * Return value: a string.
 **/
const char *
shell_bluetooth_uuid_to_string (const char *uuid)
{
	char **parts;
	guint uuid16;
	gboolean is_custom = FALSE;

	if (g_str_has_suffix (uuid, "-0000-1000-8000-0002ee000002") != FALSE)
		is_custom = TRUE;

	parts = g_strsplit (uuid, "-", -1);
	if (parts == NULL || parts[0] == NULL) {
		g_strfreev (parts);
		return NULL;
	}

	uuid16 = g_ascii_strtoull (parts[0], NULL, 16);
	g_strfreev (parts);
	if (uuid16 == 0)
		return NULL;

	if (is_custom == FALSE)
		return uuid16_to_string (uuid16, uuid);
	return uuid16_custom_to_string (uuid16, uuid);
}
