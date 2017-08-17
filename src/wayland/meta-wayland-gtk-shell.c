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

static GQuark quark_gtk_surface_data = 0;

typedef struct _MetaWaylandGtkSurface
{
  struct wl_resource *resource;
  MetaWaylandSurface *surface;
  gboolean is_modal;
  gulong configure_handler_id;
} MetaWaylandGtkSurface;

static void
gtk_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandGtkSurface *gtk_surface = wl_resource_get_user_data (resource);

  if (gtk_surface->surface)
    {
      g_object_steal_qdata (G_OBJECT (gtk_surface->surface),
                            quark_gtk_surface_data);
      g_signal_handler_disconnect (gtk_surface->surface,
                                   gtk_surface->configure_handler_id);
    }

  g_free (gtk_surface);
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
  MetaWaylandGtkSurface *gtk_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = gtk_surface->surface;

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
  MetaWaylandGtkSurface *gtk_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = gtk_surface->surface;

  if (gtk_surface->is_modal)
    return;

  gtk_surface->is_modal = TRUE;
  meta_window_set_type (surface->window, META_WINDOW_MODAL_DIALOG);
}

static void
gtk_surface_unset_modal (struct wl_client   *client,
                         struct wl_resource *resource)
{
  MetaWaylandGtkSurface *gtk_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = gtk_surface->surface;

  if (!gtk_surface->is_modal)
    return;

  gtk_surface->is_modal = FALSE;
  meta_window_set_type (surface->window, META_WINDOW_NORMAL);
}

static void
gtk_surface_present (struct wl_client   *client,
                     struct wl_resource *resource,
                     uint32_t            timestamp)
{
  MetaWaylandGtkSurface *gtk_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = gtk_surface->surface;
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
gtk_surface_surface_destroyed (MetaWaylandGtkSurface *gtk_surface)
{
  wl_resource_set_implementation (gtk_surface->resource,
                                  NULL, NULL, NULL);
  gtk_surface->surface = NULL;
}

static void
fill_edge_states (struct wl_array *states,
                  MetaWindow      *window)
{
  uint32_t *s;

  /* Top */
  if (window->edge_constraints[0] != META_EDGE_CONSTRAINT_MONITOR)
    {
      s = wl_array_add (states, sizeof *s);
      *s = GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_TOP;
    }

  /* Right */
  if (window->edge_constraints[1] != META_EDGE_CONSTRAINT_MONITOR)
    {
      s = wl_array_add (states, sizeof *s);
      *s = GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_RIGHT;
    }

  /* Bottom */
  if (window->edge_constraints[2] != META_EDGE_CONSTRAINT_MONITOR)
    {
      s = wl_array_add (states, sizeof *s);
      *s = GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_BOTTOM;
    }

  /* Left */
  if (window->edge_constraints[3] != META_EDGE_CONSTRAINT_MONITOR)
    {
      s = wl_array_add (states, sizeof *s);
      *s = GTK_SURFACE1_EDGE_CONSTRAINT_RESIZABLE_LEFT;
    }
}

static void
send_configure_edges (MetaWaylandGtkSurface *gtk_surface,
                      MetaWindow            *window)
{
  struct wl_array edge_states;

  wl_array_init (&edge_states);
  fill_edge_states (&edge_states, window);

  gtk_surface1_send_configure_edges (gtk_surface->resource, &edge_states);

  wl_array_release (&edge_states);
}

static void
fill_states (struct wl_array    *states,
             MetaWindow         *window,
             struct wl_resource *resource)
{
  uint32_t *s;
  guint version;

  version = wl_resource_get_version (resource);

  if (version < GTK_SURFACE1_CONFIGURE_EDGES_SINCE_VERSION &&
      (window->tile_mode == META_TILE_LEFT ||
       window->tile_mode == META_TILE_RIGHT))
    {
      s = wl_array_add (states, sizeof *s);
      *s = GTK_SURFACE1_STATE_TILED;
    }

  if (version >= GTK_SURFACE1_STATE_TILED_TOP_SINCE_VERSION &&
      window->edge_constraints[0] != META_EDGE_CONSTRAINT_NONE)
    {
      s = wl_array_add (states, sizeof *s);
      *s = GTK_SURFACE1_STATE_TILED_TOP;
    }

  if (version >= GTK_SURFACE1_STATE_TILED_RIGHT_SINCE_VERSION &&
      window->edge_constraints[1] != META_EDGE_CONSTRAINT_NONE)
    {
      s = wl_array_add (states, sizeof *s);
      *s = GTK_SURFACE1_STATE_TILED_RIGHT;
    }

  if (version >= GTK_SURFACE1_STATE_TILED_BOTTOM_SINCE_VERSION &&
      window->edge_constraints[2] != META_EDGE_CONSTRAINT_NONE)
    {
      s = wl_array_add (states, sizeof *s);
      *s = GTK_SURFACE1_STATE_TILED_BOTTOM;
    }

  if (version >= GTK_SURFACE1_STATE_TILED_LEFT_SINCE_VERSION &&
      window->edge_constraints[3] != META_EDGE_CONSTRAINT_NONE)
    {
      s = wl_array_add (states, sizeof *s);
      *s = GTK_SURFACE1_STATE_TILED_LEFT;
    }
}

static void
send_configure (MetaWaylandGtkSurface *gtk_surface,
                MetaWindow            *window)
{
  struct wl_array states;

  wl_array_init (&states);
  fill_states (&states, window, gtk_surface->resource);

  gtk_surface1_send_configure (gtk_surface->resource, &states);

  wl_array_release (&states);
}

static void
on_configure (MetaWaylandSurface    *surface,
              MetaWaylandGtkSurface *gtk_surface)
{
  send_configure (gtk_surface, surface->window);


  if (wl_resource_get_version (gtk_surface->resource) >= GTK_SURFACE1_CONFIGURE_EDGES_SINCE_VERSION)
    send_configure_edges (gtk_surface, surface->window);
}

static void
gtk_shell_get_gtk_surface (struct wl_client   *client,
                           struct wl_resource *resource,
                           guint32             id,
                           struct wl_resource *surface_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandGtkSurface *gtk_surface;

  gtk_surface = g_object_get_qdata (G_OBJECT (surface), quark_gtk_surface_data);
  if (gtk_surface)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "gtk_shell::get_gtk_surface already requested");
      return;
    }

  gtk_surface = g_new0 (MetaWaylandGtkSurface, 1);
  gtk_surface->surface = surface;
  gtk_surface->resource = wl_resource_create (client,
                                              &gtk_surface1_interface,
                                              wl_resource_get_version (resource),
                                              id);
  wl_resource_set_implementation (gtk_surface->resource,
                                  &meta_wayland_gtk_surface_interface,
                                  gtk_surface, gtk_surface_destructor);

  gtk_surface->configure_handler_id = g_signal_connect (surface,
                                                        "configure",
                                                        G_CALLBACK (on_configure),
                                                        gtk_surface);

  g_object_set_qdata_full (G_OBJECT (surface),
                           quark_gtk_surface_data,
                           gtk_surface,
                           (GDestroyNotify) gtk_surface_surface_destroyed);
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
      MetaWaylandGtkSurface *gtk_surface =
        wl_resource_get_user_data (gtk_surface_resource);
      MetaWaylandSurface *surface = gtk_surface->surface;

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
  quark_gtk_surface_data =
    g_quark_from_static_string ("-meta-wayland-gtk-shell-surface-data");

  if (wl_global_create (compositor->wayland_display,
                        &gtk_shell1_interface,
                        META_GTK_SHELL1_VERSION,
                        compositor, bind_gtk_shell) == NULL)
    g_error ("Failed to register a global gtk-shell object");
}
