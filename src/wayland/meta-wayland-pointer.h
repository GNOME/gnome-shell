/*
 * Wayland Support
 *
 * Copyright (C) 2013 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
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
 */

#ifndef META_WAYLAND_POINTER_H
#define META_WAYLAND_POINTER_H

#include <wayland-server.h>

#include <glib.h>

#include "meta-wayland-types.h"
#include "meta-wayland-pointer-gesture-swipe.h"
#include "meta-wayland-pointer-gesture-pinch.h"
#include "meta-wayland-surface.h"

#include <meta/meta-cursor-tracker.h>

#define META_TYPE_WAYLAND_SURFACE_ROLE_CURSOR (meta_wayland_surface_role_cursor_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSurfaceRoleCursor,
                      meta_wayland_surface_role_cursor,
                      META, WAYLAND_SURFACE_ROLE_CURSOR,
                      MetaWaylandSurfaceRole);

struct _MetaWaylandPointerGrabInterface
{
  void (*focus) (MetaWaylandPointerGrab *grab,
                 MetaWaylandSurface     *surface);
  void (*motion) (MetaWaylandPointerGrab *grab,
		  const ClutterEvent     *event);
  void (*button) (MetaWaylandPointerGrab *grab,
		  const ClutterEvent     *event);
};

struct _MetaWaylandPointerGrab
{
  const MetaWaylandPointerGrabInterface *interface;
  MetaWaylandPointer *pointer;
};

struct _MetaWaylandPointerClient
{
  struct wl_list pointer_resources;
  struct wl_list swipe_gesture_resources;
  struct wl_list pinch_gesture_resources;
};

struct _MetaWaylandPointer
{
  struct wl_display *display;

  MetaWaylandPointerClient *focus_client;
  GHashTable *pointer_clients;

  MetaWaylandSurface *focus_surface;
  struct wl_listener focus_surface_listener;
  guint32 focus_serial;
  guint32 click_serial;

  MetaWaylandSurface *cursor_surface;
  struct wl_listener cursor_surface_destroy_listener;
  int hotspot_x, hotspot_y;

  MetaWaylandPointerGrab *grab;
  MetaWaylandPointerGrab default_grab;
  guint32 grab_button;
  guint32 grab_serial;
  guint32 grab_time;
  float grab_x, grab_y;

  ClutterInputDevice *device;
  MetaWaylandSurface *current;

  guint32 button_count;
};

void meta_wayland_pointer_init (MetaWaylandPointer *pointer,
                                struct wl_display  *display);

void meta_wayland_pointer_release (MetaWaylandPointer *pointer);

void meta_wayland_pointer_update (MetaWaylandPointer *pointer,
                                  const ClutterEvent *event);

gboolean meta_wayland_pointer_handle_event (MetaWaylandPointer *pointer,
                                            const ClutterEvent *event);

void meta_wayland_pointer_send_motion (MetaWaylandPointer *pointer,
                                       const ClutterEvent *event);

void meta_wayland_pointer_send_button (MetaWaylandPointer *pointer,
                                       const ClutterEvent *event);

void meta_wayland_pointer_set_focus (MetaWaylandPointer *pointer,
                                     MetaWaylandSurface *surface);

void meta_wayland_pointer_start_grab (MetaWaylandPointer *pointer,
                                      MetaWaylandPointerGrab *grab);

void meta_wayland_pointer_end_grab (MetaWaylandPointer *pointer);

MetaWaylandPopup *meta_wayland_pointer_start_popup_grab (MetaWaylandPointer *pointer,
                                                         MetaWaylandSurface *popup);

void meta_wayland_pointer_end_popup_grab (MetaWaylandPointer *pointer);

void meta_wayland_pointer_repick (MetaWaylandPointer *pointer);

void meta_wayland_pointer_get_relative_coordinates (MetaWaylandPointer *pointer,
                                                    MetaWaylandSurface *surface,
                                                    wl_fixed_t         *x,
                                                    wl_fixed_t         *y);

void meta_wayland_pointer_update_cursor_surface (MetaWaylandPointer *pointer);

void meta_wayland_pointer_create_new_resource (MetaWaylandPointer *pointer,
                                               struct wl_client   *client,
                                               struct wl_resource *seat_resource,
                                               uint32_t id);

gboolean meta_wayland_pointer_can_grab_surface (MetaWaylandPointer *pointer,
                                                MetaWaylandSurface *surface,
                                                uint32_t            serial);

gboolean meta_wayland_pointer_can_popup (MetaWaylandPointer *pointer,
                                         uint32_t            serial);

MetaWaylandSurface *meta_wayland_pointer_get_top_popup (MetaWaylandPointer *pointer);

MetaWaylandPointerClient * meta_wayland_pointer_get_pointer_client (MetaWaylandPointer *pointer,
                                                                    struct wl_client   *client);
void meta_wayland_pointer_unbind_pointer_client_resource (struct wl_resource *resource);

#endif /* META_WAYLAND_POINTER_H */
