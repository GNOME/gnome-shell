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

#pragma once

#include <gio/gio.h>
#include <shell-bluetooth-enums.h>

/*
 * The profile UUID list is provided by the ShellBluetooth SIG:
 * https://www.bluetooth.com/specifications/assigned-numbers/service-discovery
 */
#define SHELL_BLUETOOTH_UUID_SPP		0x1101
#define SHELL_BLUETOOTH_UUID_DUN		0x1103
#define SHELL_BLUETOOTH_UUID_IRMC		0x1104
#define SHELL_BLUETOOTH_UUID_OPP		0x1105
#define SHELL_BLUETOOTH_UUID_FTP		0x1106
#define SHELL_BLUETOOTH_UUID_HSP		0x1108
#define SHELL_BLUETOOTH_UUID_A2DP_SOURCE	0x110A
#define SHELL_BLUETOOTH_UUID_A2DP_SINK	0x110B
#define SHELL_BLUETOOTH_UUID_AVRCP_TARGET	0x110C
#define SHELL_BLUETOOTH_UUID_A2DP		0x110D
#define SHELL_BLUETOOTH_UUID_AVRCP_CONTROL	0x110E
#define SHELL_BLUETOOTH_UUID_HSP_AG		0x1112
#define SHELL_BLUETOOTH_UUID_PAN_PANU		0x1115
#define SHELL_BLUETOOTH_UUID_PAN_NAP		0x1116
#define SHELL_BLUETOOTH_UUID_PAN_GN		0x1117
#define SHELL_BLUETOOTH_UUID_HFP_HF		0x111E
#define SHELL_BLUETOOTH_UUID_HFP_AG		0x111F
#define SHELL_BLUETOOTH_UUID_HID		0x1124
#define SHELL_BLUETOOTH_UUID_SAP		0x112d
#define SHELL_BLUETOOTH_UUID_PBAP		0x112F
#define SHELL_BLUETOOTH_UUID_GENERIC_AUDIO	0x1203
#define SHELL_BLUETOOTH_UUID_SDP		0x1000
#define SHELL_BLUETOOTH_UUID_PNP		0x1200
#define SHELL_BLUETOOTH_UUID_GENERIC_NET	0x1201
#define SHELL_BLUETOOTH_UUID_VDP_SOURCE	0x1303

ShellBluetoothType  shell_bluetooth_class_to_type         (guint32 class);
ShellBluetoothType  shell_bluetooth_appearance_to_type    (guint16 appearance);
const gchar   *shell_bluetooth_type_to_string        (guint type);
const char    *shell_bluetooth_uuid_to_string        (const char *uuid);
