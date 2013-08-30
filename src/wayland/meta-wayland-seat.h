/*
 * Wayland Support
 *
 * Copyright (C) 2012 Intel Corporation
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

#ifndef __META_WAYLAND_SEAT_H__
#define __META_WAYLAND_SEAT_H__

#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>
#include <clutter/clutter.h>
#include <glib.h>

#include <meta/meta-cursor-tracker.h>
#include "meta-wayland-types.h"
#include "meta-wayland-keyboard.h"
#include "meta-wayland-pointer.h"

struct _MetaWaylandDataOffer
{
  struct wl_resource *resource;
  MetaWaylandDataSource *source;
  struct wl_listener source_destroy_listener;
};

struct _MetaWaylandDataSource
{
  struct wl_resource *resource;
  struct wl_array mime_types;

  void (*accept) (MetaWaylandDataSource * source,
                  uint32_t serial, const char *mime_type);
  void (*send) (MetaWaylandDataSource * source,
                const char *mime_type, int32_t fd);
  void (*cancel) (MetaWaylandDataSource * source);
};

struct _MetaWaylandSeat
{
  struct wl_list base_resource_list;
  struct wl_signal destroy_signal;

  uint32_t selection_serial;
  MetaWaylandDataSource *selection_data_source;
  struct wl_listener selection_data_source_listener;
  struct wl_signal selection_signal;

  struct wl_list drag_resource_list;
  struct wl_client *drag_client;
  MetaWaylandDataSource *drag_data_source;
  struct wl_listener drag_data_source_listener;
  MetaWaylandSurface *drag_focus;
  struct wl_resource *drag_focus_resource;
  struct wl_listener drag_focus_listener;
  MetaWaylandPointerGrab drag_grab;
  MetaWaylandSurface *drag_surface;
  struct wl_listener drag_icon_listener;
  struct wl_signal drag_icon_signal;

  MetaWaylandPointer pointer;
  MetaWaylandKeyboard keyboard;

  struct wl_display *display;

  MetaCursorTracker *cursor_tracker;
  MetaWaylandSurface *sprite;
  int hotspot_x, hotspot_y;
  struct wl_listener sprite_destroy_listener;

  ClutterActor *current_stage;
};

MetaWaylandSeat *
meta_wayland_seat_new (struct wl_display *display,
		       gboolean           is_native);

void
meta_wayland_seat_handle_event (MetaWaylandSeat *seat,
                                const ClutterEvent *event);

void
meta_wayland_seat_repick (MetaWaylandSeat *seat,
                          uint32_t time,
                          ClutterActor *actor);

void
meta_wayland_seat_update_sprite (MetaWaylandSeat *seat);

void
meta_wayland_seat_free (MetaWaylandSeat *seat);

#endif /* __META_WAYLAND_SEAT_H__ */
