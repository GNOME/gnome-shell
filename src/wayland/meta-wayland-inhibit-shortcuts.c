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

#include "keyboard-shortcuts-inhibit-unstable-v1-server-protocol.h"
#include "wayland/meta-wayland-private.h"
#include "wayland/meta-wayland-versions.h"
#include "wayland/meta-wayland-inhibit-shortcuts.h"
#include "wayland/meta-wayland-inhibit-shortcuts-dialog.h"

struct _MetaWaylandKeyboardShotscutsInhibit
{
  MetaWaylandSurface      *surface;
  MetaWaylandSeat         *seat;
  gulong                   inhibit_shortcut_handler;
  gulong                   restore_shortcut_handler;
  gulong                   surface_destroyed_handler;
  struct wl_resource      *resource;
};

static void
zwp_keyboard_shortcuts_inhibit_destructor (struct wl_resource *resource)
{
  MetaWaylandKeyboardShotscutsInhibit *shortcut_inhibit;

  shortcut_inhibit = wl_resource_get_user_data (resource);
  if (shortcut_inhibit->surface)
    {
      meta_wayland_surface_cancel_inhibit_shortcuts_dialog (shortcut_inhibit->surface);

      g_signal_handler_disconnect (shortcut_inhibit->surface,
                                   shortcut_inhibit->surface_destroyed_handler);

      g_signal_handler_disconnect (shortcut_inhibit->surface,
                                   shortcut_inhibit->inhibit_shortcut_handler);

      g_signal_handler_disconnect (shortcut_inhibit->surface,
                                   shortcut_inhibit->restore_shortcut_handler);

      meta_wayland_surface_restore_shortcuts (shortcut_inhibit->surface,
                                              shortcut_inhibit->seat);
    }
  g_free (shortcut_inhibit);
}

static void
zwp_keyboard_shortcuts_inhibit_destroy (struct wl_client   *client,
                                        struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct zwp_keyboard_shortcuts_inhibit_manager_v1_interface
  meta_keyboard_shortcuts_inhibit_interface = {
    zwp_keyboard_shortcuts_inhibit_destroy,
  };

static void
surface_destroyed_cb (MetaWaylandSurface                  *surface,
                      MetaWaylandKeyboardShotscutsInhibit *shortcut_inhibit)
{
  shortcut_inhibit->surface = NULL;
  shortcut_inhibit->seat = NULL;
}

static void
shortcuts_inhibited_cb (MetaWaylandSurface                  *surface,
                        MetaWaylandKeyboardShotscutsInhibit *shortcut_inhibit)
{
  MetaWaylandKeyboard *keyboard = shortcut_inhibit->seat->keyboard;

  /* Send active event only if the surface has keyboard focus */
  if (keyboard->focus_surface == surface)
    zwp_keyboard_shortcuts_inhibitor_v1_send_active (shortcut_inhibit->resource);
}

static void
shortcuts_restored_cb (MetaWaylandSurface *surface,
                       gpointer user_data)
{
  MetaWaylandKeyboardShotscutsInhibit *shortcut_inhibit = user_data;

  zwp_keyboard_shortcuts_inhibitor_v1_send_inactive (shortcut_inhibit->resource);
}

static void
zwp_keyboard_shortcuts_inhibit_manager_destroy (struct wl_client   *client,
                                                struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
zwp_keyboard_shortcuts_inhibit_manager_inhibit_shortcuts (struct wl_client   *client,
                                                          struct wl_resource *resource,
                                                          uint32_t            id,
                                                          struct wl_resource *surface_resource,
                                                          struct wl_resource *seat_resource)
{
  MetaWaylandKeyboardShotscutsInhibit *shortcut_inhibit;
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  struct wl_resource *keyboard_shortcuts_inhibit_resource;

  keyboard_shortcuts_inhibit_resource =
      wl_resource_create (client,
                          &zwp_keyboard_shortcuts_inhibitor_v1_interface,
                          META_ZWP_KEYBOARD_SHORTCUTS_INHIBIT_V1_VERSION,
                          id);

  shortcut_inhibit = g_new0 (MetaWaylandKeyboardShotscutsInhibit, 1);
  shortcut_inhibit->surface = surface;
  shortcut_inhibit->seat = seat;
  shortcut_inhibit->resource = keyboard_shortcuts_inhibit_resource;

  shortcut_inhibit->inhibit_shortcut_handler =
    g_signal_connect (surface, "shortcuts-inhibited",
                      G_CALLBACK (shortcuts_inhibited_cb),
                      shortcut_inhibit);
  shortcut_inhibit->restore_shortcut_handler =
    g_signal_connect (surface, "shortcuts-restored",
                      G_CALLBACK (shortcuts_restored_cb),
                      shortcut_inhibit);
  shortcut_inhibit->surface_destroyed_handler =
    g_signal_connect (surface, "destroy",
                      G_CALLBACK (surface_destroyed_cb),
                      shortcut_inhibit);

  /* Cannot grant shortcuts to a surface without any window */
  if (meta_wayland_surface_get_toplevel_window (surface))
    meta_wayland_surface_show_inhibit_shortcuts_dialog (surface, seat);

  wl_resource_set_implementation (keyboard_shortcuts_inhibit_resource,
                                  &meta_keyboard_shortcuts_inhibit_interface,
                                  shortcut_inhibit,
                                  zwp_keyboard_shortcuts_inhibit_destructor);
}

static const struct zwp_keyboard_shortcuts_inhibit_manager_v1_interface
  meta_keyboard_shortcuts_inhibit_manager_interface = {
    zwp_keyboard_shortcuts_inhibit_manager_destroy,
    zwp_keyboard_shortcuts_inhibit_manager_inhibit_shortcuts,
  };

static void
bind_keyboard_shortcuts_inhibit (struct wl_client *client,
                                 void             *data,
                                 uint32_t          version,
                                 uint32_t          id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client,
                                 &zwp_keyboard_shortcuts_inhibit_manager_v1_interface,
                                 META_ZWP_KEYBOARD_SHORTCUTS_INHIBIT_V1_VERSION,
                                 id);

  wl_resource_set_implementation (resource,
                                  &meta_keyboard_shortcuts_inhibit_manager_interface,
                                  NULL, NULL);
}

gboolean
meta_wayland_keyboard_shortcuts_inhibit_init (MetaWaylandCompositor *compositor)
{
  return (wl_global_create (compositor->wayland_display,
                            &zwp_keyboard_shortcuts_inhibit_manager_v1_interface,
                            META_ZWP_KEYBOARD_SHORTCUTS_INHIBIT_V1_VERSION,
                            NULL,
                            bind_keyboard_shortcuts_inhibit) != NULL);
}
