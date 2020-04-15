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

#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#include "shew-external-window-wayland.h"

static GdkDisplay *wayland_display;

struct _ShewExternalWindowWayland
{
  ShewExternalWindow parent;

  char *handle_str;
};

G_DEFINE_TYPE (ShewExternalWindowWayland, shew_external_window_wayland,
               SHEW_TYPE_EXTERNAL_WINDOW)

static GdkDisplay *
get_wayland_display (void)
{
  if (wayland_display)
    return wayland_display;

  gdk_set_allowed_backends ("wayland");
  wayland_display = gdk_display_open (NULL);
  gdk_set_allowed_backends (NULL);

  if (!wayland_display)
    g_warning ("Failed to open Wayland display");

  return wayland_display;
}

ShewExternalWindowWayland *
shew_external_window_wayland_new (const char *handle_str)
{
  ShewExternalWindowWayland *external_window_wayland;
  GdkDisplay *display;

  display = get_wayland_display ();
  if (!display)
    {
      g_warning ("No Wayland display connection, ignoring Wayland parent");
      return NULL;
    }

  external_window_wayland = g_object_new (SHEW_TYPE_EXTERNAL_WINDOW_WAYLAND,
                                          "display", display,
                                          NULL);
  external_window_wayland->handle_str = g_strdup (handle_str);

  return external_window_wayland;
}

static void
shew_external_window_wayland_set_parent_of (ShewExternalWindow *external_window,
                                            GdkSurface         *child_surface)
{
  ShewExternalWindowWayland *external_window_wayland =
    SHEW_EXTERNAL_WINDOW_WAYLAND (external_window);
  char *handle_str = external_window_wayland->handle_str;

#ifdef GDK_WINDOWING_WAYLAND
  if (!gdk_wayland_toplevel_set_transient_for_exported (GDK_WAYLAND_TOPLEVEL (child_surface), handle_str))
    g_warning ("Failed to set portal window transient for external parent");
#endif
}

static void
shew_external_window_wayland_dispose (GObject *object)
{
  ShewExternalWindowWayland *external_window_wayland =
    SHEW_EXTERNAL_WINDOW_WAYLAND (object);

  g_free (external_window_wayland->handle_str);

  G_OBJECT_CLASS (shew_external_window_wayland_parent_class)->dispose (object);
}

static void
shew_external_window_wayland_init (ShewExternalWindowWayland *external_window_wayland)
{
}

static void
shew_external_window_wayland_class_init (ShewExternalWindowWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ShewExternalWindowClass *external_window_class = SHEW_EXTERNAL_WINDOW_CLASS (klass);

  object_class->dispose = shew_external_window_wayland_dispose;

  external_window_class->set_parent_of = shew_external_window_wayland_set_parent_of;
}
