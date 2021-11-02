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

#include <glib.h>

/**
 * SECTION:shell-bluetooth-enums
 * @short_description: ShellBluetooth related enumerations
 * @stability: Stable
 * @include: shell-bluetooth-enums.h
 *
 * Enumerations related to ShellBluetooth.
 **/

/**
 * ShellBluetoothType:
 * @SHELL_BLUETOOTH_TYPE_ANY: any device, or a device of an unknown type
 * @SHELL_BLUETOOTH_TYPE_PHONE: a telephone (usually a cell/mobile phone)
 * @SHELL_BLUETOOTH_TYPE_MODEM: a modem
 * @SHELL_BLUETOOTH_TYPE_COMPUTER: a computer, can be a laptop, a wearable computer, etc.
 * @SHELL_BLUETOOTH_TYPE_NETWORK: a network device, such as a router
 * @SHELL_BLUETOOTH_TYPE_HEADSET: a headset (usually a hands-free device)
 * @SHELL_BLUETOOTH_TYPE_HEADPHONES: headphones (covers two ears)
 * @SHELL_BLUETOOTH_TYPE_OTHER_AUDIO: another type of audio device
 * @SHELL_BLUETOOTH_TYPE_KEYBOARD: a keyboard
 * @SHELL_BLUETOOTH_TYPE_MOUSE: a mouse
 * @SHELL_BLUETOOTH_TYPE_CAMERA: a camera (still or moving)
 * @SHELL_BLUETOOTH_TYPE_PRINTER: a printer
 * @SHELL_BLUETOOTH_TYPE_JOYPAD: a joypad, joystick, or other game controller
 * @SHELL_BLUETOOTH_TYPE_TABLET: a drawing tablet
 * @SHELL_BLUETOOTH_TYPE_VIDEO: a video device, such as a webcam
 * @SHELL_BLUETOOTH_TYPE_REMOTE_CONTROL: a remote control
 * @SHELL_BLUETOOTH_TYPE_SCANNER: a scanner
 * @SHELL_BLUETOOTH_TYPE_DISPLAY: a display
 * @SHELL_BLUETOOTH_TYPE_WEARABLE: a wearable computer
 * @SHELL_BLUETOOTH_TYPE_TOY: a toy or game
 * @SHELL_BLUETOOTH_TYPE_SPEAKERS: audio speaker or speakers
 *
 * The type of a ShellBluetooth device. See also %SHELL_BLUETOOTH_TYPE_INPUT and %SHELL_BLUETOOTH_TYPE_AUDIO
 **/
typedef enum {
	SHELL_BLUETOOTH_TYPE_ANY		= 1 << 0,
	SHELL_BLUETOOTH_TYPE_PHONE		= 1 << 1,
	SHELL_BLUETOOTH_TYPE_MODEM		= 1 << 2,
	SHELL_BLUETOOTH_TYPE_COMPUTER		= 1 << 3,
	SHELL_BLUETOOTH_TYPE_NETWORK		= 1 << 4,
	SHELL_BLUETOOTH_TYPE_HEADSET		= 1 << 5,
	SHELL_BLUETOOTH_TYPE_HEADPHONES	= 1 << 6,
	SHELL_BLUETOOTH_TYPE_OTHER_AUDIO	= 1 << 7,
	SHELL_BLUETOOTH_TYPE_KEYBOARD		= 1 << 8,
	SHELL_BLUETOOTH_TYPE_MOUSE		= 1 << 9,
	SHELL_BLUETOOTH_TYPE_CAMERA		= 1 << 10,
	SHELL_BLUETOOTH_TYPE_PRINTER		= 1 << 11,
	SHELL_BLUETOOTH_TYPE_JOYPAD		= 1 << 12,
	SHELL_BLUETOOTH_TYPE_TABLET		= 1 << 13,
	SHELL_BLUETOOTH_TYPE_VIDEO		= 1 << 14,
	SHELL_BLUETOOTH_TYPE_REMOTE_CONTROL	= 1 << 15,
	SHELL_BLUETOOTH_TYPE_SCANNER		= 1 << 16,
	SHELL_BLUETOOTH_TYPE_DISPLAY		= 1 << 17,
	SHELL_BLUETOOTH_TYPE_WEARABLE		= 1 << 18,
	SHELL_BLUETOOTH_TYPE_TOY		= 1 << 19,
	SHELL_BLUETOOTH_TYPE_SPEAKERS		= 1 << 20,
} ShellBluetoothType;

#define _SHELL_BLUETOOTH_TYPE_NUM_TYPES 21

/**
 * SHELL_BLUETOOTH_TYPE_INPUT:
 *
 * Use this value to select any ShellBluetooth input device where a #ShellBluetoothType enum is required.
 */
#define SHELL_BLUETOOTH_TYPE_INPUT (SHELL_BLUETOOTH_TYPE_KEYBOARD | SHELL_BLUETOOTH_TYPE_MOUSE | SHELL_BLUETOOTH_TYPE_TABLET | SHELL_BLUETOOTH_TYPE_JOYPAD)
/**
 * SHELL_BLUETOOTH_TYPE_AUDIO:
 *
 * Use this value to select any ShellBluetooth audio device where a #ShellBluetoothType enum is required.
 */
#define SHELL_BLUETOOTH_TYPE_AUDIO (SHELL_BLUETOOTH_TYPE_HEADSET | SHELL_BLUETOOTH_TYPE_HEADPHONES | SHELL_BLUETOOTH_TYPE_OTHER_AUDIO | SHELL_BLUETOOTH_TYPE_SPEAKERS)

/**
 * ShellBluetoothColumn:
 * @SHELL_BLUETOOTH_COLUMN_PROXY: a #GDBusProxy object
 * @SHELL_BLUETOOTH_COLUMN_PROPERTIES: Used to be #GDBusProxy object for DBus.Properties, now always %NULL
 * @SHELL_BLUETOOTH_COLUMN_ADDRESS: a string representing a ShellBluetooth address
 * @SHELL_BLUETOOTH_COLUMN_ALIAS: a string to use for display (the name of the device, or its address if the name is not known). Only available for devices.
 * @SHELL_BLUETOOTH_COLUMN_NAME: a string representing the device or adapter's name
 * @SHELL_BLUETOOTH_COLUMN_TYPE: the #ShellBluetoothType of the device. Only available for devices.
 * @SHELL_BLUETOOTH_COLUMN_ICON: a string representing the icon name for the device. Only available for devices.
 * @SHELL_BLUETOOTH_COLUMN_DEFAULT: whether the adapter is the default one. Only available for adapters.
 * @SHELL_BLUETOOTH_COLUMN_PAIRED: whether the device is paired to its parent adapter. Only available for devices.
 * @SHELL_BLUETOOTH_COLUMN_TRUSTED: whether the device is trusted. Only available for devices.
 * @SHELL_BLUETOOTH_COLUMN_CONNECTED: whether the device is connected. Only available for devices.
 * @SHELL_BLUETOOTH_COLUMN_DISCOVERABLE: whether the adapter is discoverable/visible. Only available for adapters.
 * @SHELL_BLUETOOTH_COLUMN_DISCOVERING: whether the adapter is discovering. Only available for adapters.
 * @SHELL_BLUETOOTH_COLUMN_LEGACYPAIRING: whether the device does not support ShellBluetooth 2.1 Simple Secure Pairing. Only available for devices.
 * @SHELL_BLUETOOTH_COLUMN_POWERED: whether the adapter is powered. Only available for adapters.
 * @SHELL_BLUETOOTH_COLUMN_SERVICES: an array of service names and #ShellBluetoothStatus connection statuses.
 * @SHELL_BLUETOOTH_COLUMN_UUIDS: a string array of human-readable UUIDs.
 *
 * A column identifier to pass to shell_bluetooth_chooser_get_selected_device_info().
 **/
typedef enum {
	SHELL_BLUETOOTH_COLUMN_PROXY,
	SHELL_BLUETOOTH_COLUMN_PROPERTIES,
	SHELL_BLUETOOTH_COLUMN_ADDRESS,
	SHELL_BLUETOOTH_COLUMN_ALIAS,
	SHELL_BLUETOOTH_COLUMN_NAME,
	SHELL_BLUETOOTH_COLUMN_TYPE,
	SHELL_BLUETOOTH_COLUMN_ICON,
	SHELL_BLUETOOTH_COLUMN_DEFAULT,
	SHELL_BLUETOOTH_COLUMN_PAIRED,
	SHELL_BLUETOOTH_COLUMN_TRUSTED,
	SHELL_BLUETOOTH_COLUMN_CONNECTED,
	SHELL_BLUETOOTH_COLUMN_DISCOVERABLE,
	SHELL_BLUETOOTH_COLUMN_DISCOVERING,
	SHELL_BLUETOOTH_COLUMN_LEGACYPAIRING,
	SHELL_BLUETOOTH_COLUMN_POWERED,
	SHELL_BLUETOOTH_COLUMN_SERVICES,
	SHELL_BLUETOOTH_COLUMN_UUIDS,
} ShellBluetoothColumn;

#define _SHELL_BLUETOOTH_NUM_COLUMNS (SHELL_BLUETOOTH_COLUMN_UUIDS + 1)

/**
 * ShellBluetoothStatus:
 * @SHELL_BLUETOOTH_STATUS_INVALID: whether the status has been set yet
 * @SHELL_BLUETOOTH_STATUS_DISCONNECTED: whether the service is disconnected
 * @SHELL_BLUETOOTH_STATUS_CONNECTED: whether the service is connected
 * @SHELL_BLUETOOTH_STATUS_CONNECTING: whether the service is connecting
 * @SHELL_BLUETOOTH_STATUS_PLAYING: whether the service is playing (only used by the audio service)
 *
 * The connection status of a service on a particular device. Note that @SHELL_BLUETOOTH_STATUS_CONNECTING and @SHELL_BLUETOOTH_STATUS_PLAYING might not be available for all services.
 **/
typedef enum {
	SHELL_BLUETOOTH_STATUS_INVALID = 0,
	SHELL_BLUETOOTH_STATUS_DISCONNECTED,
	SHELL_BLUETOOTH_STATUS_CONNECTED,
	SHELL_BLUETOOTH_STATUS_CONNECTING,
	SHELL_BLUETOOTH_STATUS_PLAYING
} ShellBluetoothStatus;
