/* common.h
 *
 * Copyright 2018 Florian Müllner <fmuellner@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum {
  TYPE_SYSTEM = 1,
  TYPE_USER
} ExtensionType;

typedef enum {
  STATE_ENABLED = 1,
  STATE_DISABLED,
  STATE_ERROR,
  STATE_OUT_OF_DATE,
  STATE_DOWNLOADING,
  STATE_INITIALIZED,
  STATE_DISABLING,
  STATE_ENABLING,

  STATE_UNINSTALLED = 99
} ExtensionState;

typedef enum {
  DISPLAY_ONELINE,
  DISPLAY_DETAILED
} DisplayFormat;

GOptionGroup *get_option_group (void);

void show_help (GOptionContext *context,
                const char     *message);

void print_extension_info (GVariantDict  *info,
                           DisplayFormat  format);

GDBusProxy *get_shell_proxy (GError **error);
GVariant   *get_extension_property (GDBusProxy *proxy,
                                    const char *uuid,
                                    const char *property);

GSettings  *get_shell_settings (void);

gboolean settings_list_add (GSettings  *settings,
                            const char *key,
                            const char *value);
gboolean settings_list_remove (GSettings  *settings,
                               const char *key,
                               const char *value);

gboolean file_delete_recursively (GFile   *file,
                                  GError **error);

G_END_DECLS
