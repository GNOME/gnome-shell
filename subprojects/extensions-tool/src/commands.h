/* commands.h
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

#include <glib.h>

G_BEGIN_DECLS

int handle_enable     (int argc, char *argv[], gboolean do_help);
int handle_disable    (int argc, char *argv[], gboolean do_help);
int handle_reset      (int argc, char *argv[], gboolean do_help);
int handle_list       (int argc, char *argv[], gboolean do_help);
int handle_info       (int argc, char *argv[], gboolean do_help);
int handle_prefs      (int argc, char *argv[], gboolean do_help);
int handle_create     (int argc, char *argv[], gboolean do_help);
int handle_pack       (int argc, char *argv[], gboolean do_help);
int handle_install    (int argc, char *argv[], gboolean do_help);
int handle_uninstall  (int argc, char *argv[], gboolean do_help);

G_END_DECLS
