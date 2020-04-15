/*
 * Copyright © 2016 Red Hat, Inc
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
 *       Jonas Ådahl <jadahl@redhat.com>
 */

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>


#define SHEW_TYPE_EXTERNAL_WINDOW (shew_external_window_get_type ())
G_DECLARE_DERIVABLE_TYPE (ShewExternalWindow, shew_external_window, SHEW, EXTERNAL_WINDOW, GObject)

struct _ShewExternalWindowClass
{
  GObjectClass parent_class;

  void (*set_parent_of) (ShewExternalWindow *external_window,
                         GdkSurface         *child_surface);
};

ShewExternalWindow *shew_external_window_new_from_handle (const char *handle_str);

void shew_external_window_set_parent_of (ShewExternalWindow *external_window,
                                         GdkSurface         *child_surface);

GdkDisplay *shew_external_window_get_display (ShewExternalWindow *external_window);
