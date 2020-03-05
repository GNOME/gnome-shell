/*
 * Copyright © 2020 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Florian Müllner <fmuellner@gnome.org>
 */

#pragma once

#include <gtk/gtk.h>

#define SHEW_TYPE_WINDOW_EXPORTER (shew_window_exporter_get_type ())
G_DECLARE_FINAL_TYPE (ShewWindowExporter, shew_window_exporter, SHEW, WINDOW_EXPORTER, GObject)

ShewWindowExporter *shew_window_exporter_new (GtkWindow *window);

void shew_window_exporter_export (ShewWindowExporter  *exporter,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data);

char *shew_window_exporter_export_finish (ShewWindowExporter  *exporter,
                                          GAsyncResult        *result,
                                          GError             **error);

void shew_window_exporter_unexport (ShewWindowExporter *exporter);
