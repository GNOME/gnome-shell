/*
 * Wayland Support
 *
 * Copyright (C) 2012,2013 Intel Corporation
 *               2013-2016 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include "wayland/meta-wayland-gtk-shell.h"

#include "core/bell.h"
#include "core/window-private.h"
#include "wayland/meta-wayland-private.h"
#include "wayland/meta-wayland-surface.h"
#include "wayland/meta-wayland-versions.h"
#include "wayland/meta-window-wayland.h"

#include "gtk-shell-server-protocol.h"

static void
gtk_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  surface->gtk_surface = NULL;
}

static void
gtk_surface_set_dbus_properties (struct wl_client   *client,
                                 struct wl_resource *resource,
                                 const char         *application_id,
                                 const char         *app_menu_path,
                                 const char         *menubar_path,
                                 const char         *window_object_path,
                                 const char         *application_object_path,
                                 const char         *unique_bus_name)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  /* Broken client, let it die instead of us */
  if (!surface->window)
    {
      meta_warning ("meta-wayland-surface: set_dbus_properties called with invalid window!\n");
      return;
    }

  meta_window_set_gtk_dbus_properties (surface->window,
                                       application_id,
                                       unique_bus_name,
                                       app_menu_path,
                                       menubar_path,
                                       application_object_path,
                                       window_object_path);
}

static void
gtk_surface_set_modal (struct wl_client   *client,
                       struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  if (surface->is_modal)
    return;

  surface->is_modal = TRUE;
  meta_window_set_type (surface->window, META_WINDOW_MODAL_DIALOG);
}

static void
gtk_surface_unset_modal (struct wl_client   *client,
                         struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  if (!surface->is_modal)
    return;

  surface->is_modal = FALSE;
  meta_window_set_type (surface->window, META_WINDOW_NORMAL);
}

static void
gtk_surface_present (struct wl_client   *client,
                     struct wl_resource *resource,
                     uint32_t            timestamp)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWindow *window = surface->window;

  if (!window)
    return;

  meta_window_activate_full (window, timestamp,
                             META_CLIENT_TYPE_APPLICATION, NULL);
}

static const struct gtk_surface1_interface meta_wayland_gtk_surface_interface = {
  gtk_surface_set_dbus_properties,
  gtk_surface_set_modal,
  gtk_surface_unset_modal,
  gtk_surface_present,
};

static void
gtk_shell_get_gtk_surface (struct wl_client   *client,
                           struct wl_resource *resource,
                           guint32             id,
                           struct wl_resource *surface_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  if (surface->gtk_surface != NULL)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "gtk_shell::get_gtk_surface already requested");
      return;
    }

  surface->gtk_surface = wl_resource_create (client,
                                             &gtk_surface1_interface,
                                             wl_resource_get_version (resource),
                                             id);
  wl_resource_set_implementation (surface->gtk_surface,
                                  &meta_wayland_gtk_surface_interface,
                                  surface, gtk_surface_destructor);
}

static void
gtk_shell_set_startup_id (struct wl_client   *client,
                          struct wl_resource *resource,
                          const char         *startup_id)
{
  MetaDisplay *display;

  display = meta_get_display ();
  meta_startup_notification_remove_sequence (display->startup_notification,
                                             startup_id);
}

static void
gtk_shell_system_bell (struct wl_client   *client,
                       struct wl_resource *resource,
                       struct wl_resource *gtk_surface_resource)
{
  MetaDisplay *display = meta_get_display ();

  if (gtk_surface_resource)
    {
      MetaWaylandSurface *surface =
        wl_resource_get_user_data (gtk_surface_resource);

      if (!surface->window)
        return;

      meta_bell_notify (display, surface->window);
    }
  else
    {
      meta_bell_notify (display, NULL);
    }
}

static const struct gtk_shell1_interface meta_wayland_gtk_shell_interface = {
  gtk_shell_get_gtk_surface,
  gtk_shell_set_startup_id,
  gtk_shell_system_bell,
};

static void
bind_gtk_shell (struct wl_client *client,
                void             *data,
                guint32           version,
                guint32           id)
{
  struct wl_resource *resource;
  uint32_t capabilities = 0;

  resource = wl_resource_create (client, &gtk_shell1_interface, version, id);
  wl_resource_set_implementation (resource, &meta_wayland_gtk_shell_interface,
                                  data, NULL);

  if (!meta_prefs_get_show_fallback_app_menu ())
    capabilities = GTK_SHELL1_CAPABILITY_GLOBAL_APP_MENU;

  gtk_shell1_send_capabilities (resource, capabilities);
}

void
meta_wayland_gtk_shell_init (MetaWaylandCompositor *compositor)
{
  if (wl_global_create (compositor->wayland_display,
                        &gtk_shell1_interface,
                        META_GTK_SHELL1_VERSION,
                        compositor, bind_gtk_shell) == NULL)
    g_error ("Failed to register a global gtk-shell object");
}
