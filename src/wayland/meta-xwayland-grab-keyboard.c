/*
 * Copyright (C) 2017 Red Hat
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
 *
 * Written by:
 *     Olivier Fourdan <ofourdan@redhat.com>
 */

#include "config.h"

#include <wayland-server.h>

#include "meta/meta-backend.h"
#include "backends/meta-settings-private.h"
#include "wayland/meta-wayland-private.h"
#include "wayland/meta-wayland-versions.h"
#include "wayland/meta-wayland-seat.h"
#include "wayland/meta-window-wayland.h"
#include "wayland/meta-xwayland-grab-keyboard.h"

struct _MetaXwaylandKeyboardActiveGrab
{
  MetaWaylandSurface *surface;
  MetaWaylandSeat *seat;
  MetaWaylandKeyboardGrab keyboard_grab;
  gulong surface_destroyed_handler;
  gulong shortcuts_restored_handler;
  gulong window_associate_handler;
  struct wl_resource *resource;
};

static gboolean
meta_xwayland_keyboard_grab_key (MetaWaylandKeyboardGrab *grab,
                                 const ClutterEvent      *event)
{
  MetaXwaylandKeyboardActiveGrab *active_grab;
  MetaWaylandKeyboard *keyboard;

  active_grab = wl_container_of (grab, active_grab, keyboard_grab);
  keyboard = active_grab->keyboard_grab.keyboard;

  /* Force focus onto the surface who has the active grab on the keyboard */
  if (active_grab->surface != NULL && keyboard->focus_surface != active_grab->surface)
    meta_wayland_keyboard_set_focus (keyboard, active_grab->surface);

  /* Chain-up with default keyboard handler */
  return keyboard->default_grab.interface->key (&keyboard->default_grab, event);
}

static void
meta_xwayland_keyboard_grab_modifiers (MetaWaylandKeyboardGrab *grab,
                                       ClutterModifierType      modifiers)
{
  MetaXwaylandKeyboardActiveGrab *active_grab;
  MetaWaylandKeyboard *keyboard;

  active_grab = wl_container_of (grab, active_grab, keyboard_grab);
  keyboard = active_grab->keyboard_grab.keyboard;

  /* Force focus onto the surface who has the active grab on the keyboard */
  if (active_grab->surface != NULL && keyboard->focus_surface != active_grab->surface)
    meta_wayland_keyboard_set_focus (keyboard, active_grab->surface);

  /* Chain-up with default keyboard handler */
 return keyboard->default_grab.interface->modifiers (&keyboard->default_grab,
                                                     modifiers);
}

static void
meta_xwayland_keyboard_grab_end (MetaXwaylandKeyboardActiveGrab *active_grab)
{
  MetaWaylandSeat *seat = active_grab->seat;

  if (!active_grab->surface)
    return;

  g_signal_handler_disconnect (active_grab->surface,
                               active_grab->surface_destroyed_handler);

  g_signal_handler_disconnect (active_grab->surface,
                               active_grab->shortcuts_restored_handler);

  meta_wayland_surface_restore_shortcuts (active_grab->surface,
                                          active_grab->seat);

  if (active_grab->window_associate_handler)
    {
      g_signal_handler_disconnect (active_grab->surface->role,
                                   active_grab->window_associate_handler);
      active_grab->window_associate_handler = 0;
    }

  if (seat->keyboard->grab->interface->key == meta_xwayland_keyboard_grab_key)
    {
      meta_wayland_keyboard_end_grab (active_grab->keyboard_grab.keyboard);
      meta_wayland_keyboard_set_focus (active_grab->keyboard_grab.keyboard, NULL);
      meta_display_sync_wayland_input_focus (meta_get_display ());
    }

  active_grab->surface = NULL;
}

static const MetaWaylandKeyboardGrabInterface
  keyboard_grab_interface = {
    meta_xwayland_keyboard_grab_key,
    meta_xwayland_keyboard_grab_modifiers
  };

static void
zwp_xwayland_keyboard_grab_destructor (struct wl_resource *resource)
{
  MetaXwaylandKeyboardActiveGrab *active_grab;

  active_grab = wl_resource_get_user_data (resource);
  meta_xwayland_keyboard_grab_end (active_grab);

  g_free (active_grab);
}

static void
zwp_xwayland_keyboard_grab_destroy (struct wl_client   *client,
                                    struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct zwp_xwayland_keyboard_grab_v1_interface
  xwayland_keyboard_grab_interface = {
    zwp_xwayland_keyboard_grab_destroy,
  };

static void
surface_destroyed_cb (MetaWaylandSurface             *surface,
                      MetaXwaylandKeyboardActiveGrab *active_grab)
{
  active_grab->surface = NULL;
}

static void
shortcuts_restored_cb (MetaWaylandSurface             *surface,
                       MetaXwaylandKeyboardActiveGrab *active_grab)
{
  meta_xwayland_keyboard_grab_end (active_grab);
}

static void
zwp_xwayland_keyboard_grab_manager_destroy (struct wl_client   *client,
                                            struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static gboolean
application_is_in_pattern_array (MetaWindow *window,
                                 GPtrArray  *pattern_array)
{
  guint i;

  for (i = 0; pattern_array && i < pattern_array->len; i++)
    {
      GPatternSpec *pattern = (GPatternSpec *) g_ptr_array_index (pattern_array, i);

      if ((window->res_class && g_pattern_match_string (pattern, window->res_class)) ||
          (window->res_name && g_pattern_match_string (pattern, window->res_name)))
        return TRUE;
    }

   return FALSE;
}

static gboolean
meta_xwayland_grab_is_granted (MetaWindow *window)
{
  MetaBackend *backend;
  MetaSettings *settings;
  GPtrArray *whitelist;
  GPtrArray *blacklist;
  gboolean may_grab;

  backend = meta_get_backend ();
  settings = meta_backend_get_settings (backend);
  if (!meta_settings_are_xwayland_grabs_allowed (settings))
    return FALSE;

  /* Check whether the window is blacklisted */
  meta_settings_get_xwayland_grab_patterns (settings, &whitelist, &blacklist);

  if (blacklist && application_is_in_pattern_array (window, blacklist))
    return FALSE;

  /* Check if we are dealing with good citizen Xwayland client whitelisting itself. */
  g_object_get (G_OBJECT (window), "xwayland-may-grab-keyboard", &may_grab, NULL);
  if (may_grab)
    return TRUE;

  /* Last resort, is it white listed. */
  if (whitelist && application_is_in_pattern_array (window, whitelist))
    return TRUE;

  return FALSE;
}

static void
meta_xwayland_keyboard_grab_activate (MetaXwaylandKeyboardActiveGrab *active_grab)
{
  MetaWaylandSurface *surface = active_grab->surface;
  MetaWindow *window = surface->window;
  MetaWaylandSeat *seat = active_grab->seat;

  if (meta_xwayland_grab_is_granted (window))
    {
      meta_verbose ("XWayland window %s has a grab granted", window->desc);
      meta_wayland_surface_inhibit_shortcuts (surface, seat);
      /* Use a grab for O-R windows which never receive keyboard focus otherwise */
      if (window->override_redirect)
        meta_wayland_keyboard_start_grab (seat->keyboard, &active_grab->keyboard_grab);
    }
  if (active_grab->window_associate_handler)
    {
      g_signal_handler_disconnect (active_grab->surface->role,
                                   active_grab->window_associate_handler);
      active_grab->window_associate_handler = 0;
    }
}

static void
meta_xwayland_keyboard_window_associated (MetaWaylandSurfaceRole         *surface_role,
                                          MetaXwaylandKeyboardActiveGrab *active_grab)
{
  meta_xwayland_keyboard_grab_activate (active_grab);
}

static void
zwp_xwayland_keyboard_grab_manager_grab (struct wl_client   *client,
                                         struct wl_resource *resource,
                                         uint32_t            id,
                                         struct wl_resource *surface_resource,
                                         struct wl_resource *seat_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWindow *window = surface->window;
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaXwaylandKeyboardActiveGrab *active_grab;
  struct wl_resource *grab_resource;

  grab_resource = wl_resource_create (client,
                                      &zwp_xwayland_keyboard_grab_manager_v1_interface,
                                      wl_resource_get_version (resource),
                                      id);

  active_grab = g_new0 (MetaXwaylandKeyboardActiveGrab, 1);
  active_grab->surface = surface;
  active_grab->resource = grab_resource;
  active_grab->seat = seat;
  active_grab->keyboard_grab.interface = &keyboard_grab_interface;
  active_grab->surface_destroyed_handler =
    g_signal_connect (surface, "destroy",
                      G_CALLBACK (surface_destroyed_cb),
                      active_grab);
  active_grab->shortcuts_restored_handler =
    g_signal_connect (surface, "shortcuts-restored",
                      G_CALLBACK (shortcuts_restored_cb),
                      active_grab);

  if (window)
    meta_xwayland_keyboard_grab_activate (active_grab);
  else if (surface->role)
    active_grab->window_associate_handler =
      g_signal_connect (surface->role, "window-associated",
                        G_CALLBACK (meta_xwayland_keyboard_window_associated),
                        active_grab);
  else
    g_warning ("Cannot grant Xwayland grab to surface %p", surface);

  wl_resource_set_implementation (grab_resource,
                                  &xwayland_keyboard_grab_interface,
                                  active_grab,
                                  zwp_xwayland_keyboard_grab_destructor);
}

static const struct zwp_xwayland_keyboard_grab_manager_v1_interface
  meta_keyboard_grab_manager_interface = {
    zwp_xwayland_keyboard_grab_manager_destroy,
    zwp_xwayland_keyboard_grab_manager_grab,
  };

static void
bind_keyboard_grab (struct wl_client *client,
                    void             *data,
                    uint32_t          version,
                    uint32_t          id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client,
                                 &zwp_xwayland_keyboard_grab_manager_v1_interface,
                                 MIN (META_ZWP_XWAYLAND_KEYBOARD_GRAB_V1_VERSION, version),
                                 id);

  wl_resource_set_implementation (resource,
                                  &meta_keyboard_grab_manager_interface,
                                  NULL, NULL);
}

gboolean
meta_xwayland_grab_keyboard_init (MetaWaylandCompositor *compositor)
{
  return (wl_global_create (compositor->wayland_display,
                            &zwp_xwayland_keyboard_grab_manager_v1_interface,
                            META_ZWP_XWAYLAND_KEYBOARD_GRAB_V1_VERSION,
                            NULL,
                            bind_keyboard_grab) != NULL);
}
