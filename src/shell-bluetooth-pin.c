/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2009  Bastien Nocera <hadess@hadess.net>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libudev.h>
#include <shell-bluetooth-enums.h>
#include <shell-bluetooth-utils.h>

#include "shell-bluetooth-pin.h"

#define PIN_CODE_DB "pin-code-database.xml"
#define MAX_DIGITS_PIN_PREFIX "max:"

char *
oui_to_vendor (const char *oui)
{
	struct udev *udev = NULL;
	struct udev_hwdb *hwdb = NULL;
	struct udev_list_entry *list, *l;
	char *modalias = NULL;
	char *vendor = NULL;

	if (oui == NULL ||
	    strlen (oui) < 8)
		return NULL;

	udev = udev_new ();
	if (udev == NULL)
		goto bail;

	hwdb = udev_hwdb_new (udev);
	if (hwdb == NULL)
		goto bail;

	modalias = g_strdup_printf ("OUI:%c%c%c%c%c%c",
				    g_ascii_toupper (oui[0]),
				    g_ascii_toupper (oui[1]),
				    g_ascii_toupper (oui[3]),
				    g_ascii_toupper (oui[4]),
				    g_ascii_toupper (oui[6]),
				    g_ascii_toupper (oui[7]));

	list = udev_hwdb_get_properties_list_entry (hwdb, modalias, 0);

	udev_list_entry_foreach (l, list) {
		const char *name = udev_list_entry_get_name (l);

		if (g_strcmp0 (name, "ID_OUI_FROM_DATABASE") == 0) {
			vendor = g_strdup (udev_list_entry_get_value (l));
			break;
		}
	}

bail:
	g_clear_pointer (&modalias, g_free);
	g_clear_pointer (&hwdb, udev_hwdb_unref);
	g_clear_pointer (&udev, udev_unref);

	return vendor;
}

#define TYPE_IS(x, r) {				\
	if (g_str_equal(type, x)) return r;	\
}

static guint string_to_type(const char *type)
{
	TYPE_IS ("any", SHELL_BLUETOOTH_TYPE_ANY);
	TYPE_IS ("mouse", SHELL_BLUETOOTH_TYPE_MOUSE);
	TYPE_IS ("tablet", SHELL_BLUETOOTH_TYPE_TABLET);
	TYPE_IS ("keyboard", SHELL_BLUETOOTH_TYPE_KEYBOARD);
	TYPE_IS ("headset", SHELL_BLUETOOTH_TYPE_HEADSET);
	TYPE_IS ("headphones", SHELL_BLUETOOTH_TYPE_HEADPHONES);
	TYPE_IS ("audio", SHELL_BLUETOOTH_TYPE_OTHER_AUDIO);
	TYPE_IS ("printer", SHELL_BLUETOOTH_TYPE_PRINTER);
	TYPE_IS ("network", SHELL_BLUETOOTH_TYPE_NETWORK);
	TYPE_IS ("joypad", SHELL_BLUETOOTH_TYPE_JOYPAD);

	g_warning ("unhandled type '%s'", type);
	return SHELL_BLUETOOTH_TYPE_ANY;
}

typedef struct {
	char *ret_pin;
	guint max_digits;
	guint type;
	const char *address;
	const char *name;
	char *vendor;
	gboolean confirm;
} PinParseData;

static void
pin_db_parse_start_tag (GMarkupParseContext *ctx,
			const gchar         *element_name,
			const gchar        **attr_names,
			const gchar        **attr_values,
			gpointer             data,
			GError             **error)
{
	PinParseData *pdata = (PinParseData *) data;

	if (pdata->ret_pin != NULL || pdata->max_digits != 0)
		return;
	if (g_str_equal (element_name, "device") == FALSE)
		return;

	while (*attr_names && *attr_values) {
		if (g_str_equal (*attr_names, "type")) {
			guint type;

			type = string_to_type (*attr_values);
			if (type != SHELL_BLUETOOTH_TYPE_ANY && type != pdata->type)
				return;
		} else if (g_str_equal (*attr_names, "oui")) {
			if (g_str_has_prefix (pdata->address, *attr_values) == FALSE)
				return;
		} else if (g_str_equal (*attr_names, "vendor")) {
			if (*attr_values == NULL || pdata->vendor == NULL)
				return;
			if (strstr (pdata->vendor, *attr_values) == NULL)
				return;
		} else if (g_str_equal (*attr_names, "name")) {
			if (*attr_values == NULL || pdata->name == NULL)
				return;
			if (strstr (pdata->name, *attr_values) == NULL)
				return;
			pdata->confirm = FALSE;
		} else if (g_str_equal (*attr_names, "pin")) {
			if (g_str_has_prefix (*attr_values, MAX_DIGITS_PIN_PREFIX) != FALSE) {
				pdata->max_digits = strtoul (*attr_values + strlen (MAX_DIGITS_PIN_PREFIX), NULL, 0);
				g_assert (pdata->max_digits > 0 && pdata->max_digits < PIN_NUM_DIGITS);
			} else {
				pdata->ret_pin = g_strdup (*attr_values);
			}
			return;
		}

		++attr_names;
		++attr_values;
	}
}

char *
get_pincode_for_device (guint       type,
			const char *address,
			const char *name,
			guint      *max_digits,
			gboolean   *confirm)
{
	GMarkupParseContext *ctx;
	GMarkupParser parser = { pin_db_parse_start_tag, NULL, NULL, NULL, NULL };
	PinParseData *data;
	char *buf;
	gsize buf_len;
	GError *err = NULL;
	char *tmp_vendor, *ret_pin;

	g_return_val_if_fail (address != NULL, NULL);

	g_debug ("Getting pincode for device '%s' (type: %s address: %s)",
		 name ? name : "", shell_bluetooth_type_to_string (type), address);

	/* Load the PIN database and split it in lines */
	if (!g_file_get_contents(PIN_CODE_DB, &buf, &buf_len, NULL)) {
		char *filename;

		filename = g_build_filename(GNOME_SHELL_DATADIR, PIN_CODE_DB, NULL);
		if (!g_file_get_contents(filename, &buf, &buf_len, NULL)) {
			g_warning("Could not load "PIN_CODE_DB);
			g_free (filename);
			return NULL;
		}
		g_free (filename);
	}

	data = g_new0 (PinParseData, 1);
	data->type = type;
	data->address = address;
	data->name = name;
	data->confirm = TRUE;

	tmp_vendor = oui_to_vendor (address);
	if (tmp_vendor)
		data->vendor = g_ascii_strdown (tmp_vendor, -1);
	g_free (tmp_vendor);

	ctx = g_markup_parse_context_new (&parser, 0, data, NULL);

	if (!g_markup_parse_context_parse (ctx, buf, buf_len, &err)) {
		g_warning ("Failed to parse '%s': %s\n", PIN_CODE_DB, err->message);
		g_error_free (err);
	}

	g_markup_parse_context_free (ctx);
	g_free (buf);

	if (max_digits != NULL)
		*max_digits = data->max_digits;
	if (confirm != NULL)
		*confirm = data->confirm;

	g_debug ("Got pin '%s' (max digits: %d, confirm: %d) for device '%s' (type: %s address: %s, vendor: %s)",
		 data->ret_pin, data->max_digits, data->confirm,
		 name ? name : "", shell_bluetooth_type_to_string (type), address, data->vendor);

	g_free (data->vendor);
	ret_pin = data->ret_pin;
	g_free (data);

	return ret_pin;
}

