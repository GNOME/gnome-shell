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

#ifndef __META_WAYLAND_POINTER_H__
#define __META_WAYLAND_POINTER_H__

#include <wayland-server.h>

#include <glib.h>

#include "meta-wayland-types.h"

struct _MetaWaylandPointerGrabInterface
{
  void (*focus) (MetaWaylandPointerGrab * grab,
                 MetaWaylandSurface * surface, wl_fixed_t x, wl_fixed_t y);
  void (*motion) (MetaWaylandPointerGrab * grab,
                  uint32_t time, wl_fixed_t x, wl_fixed_t y);
  void (*button) (MetaWaylandPointerGrab * grab,
                  uint32_t time, uint32_t button, uint32_t state);
};

struct _MetaWaylandPointerGrab
{
  const MetaWaylandPointerGrabInterface *interface;
  MetaWaylandPointer *pointer;
  MetaWaylandSurface *focus;
  wl_fixed_t x, y;
};

struct _MetaWaylandPointer
{
  struct wl_list resource_list;
  MetaWaylandSurface *focus;
  struct wl_resource *focus_resource;
  struct wl_listener focus_listener;
  guint32 focus_serial;
  struct wl_signal focus_signal;

  MetaWaylandPointerGrab *grab;
  MetaWaylandPointerGrab default_grab;
  wl_fixed_t grab_x, grab_y;
  guint32 grab_button;
  guint32 grab_serial;
  guint32 grab_time;

  wl_fixed_t x, y; /* TODO: remove, use ClutterInputDevice instead */
  MetaWaylandSurface *current;
  struct wl_listener current_listener;
  wl_fixed_t current_x, current_y;

  guint32 button_count;
};

void
meta_wayland_pointer_init (MetaWaylandPointer *pointer,
			   gboolean            is_native);

void
meta_wayland_pointer_release (MetaWaylandPointer *pointer);

void
meta_wayland_pointer_set_focus (MetaWaylandPointer *pointer,
                                MetaWaylandSurface *surface,
                                wl_fixed_t sx,
                                wl_fixed_t sy);

void
meta_wayland_pointer_destroy_focus (MetaWaylandPointer *pointer);

void
meta_wayland_pointer_start_grab (MetaWaylandPointer *pointer,
                                 MetaWaylandPointerGrab *grab);

void
meta_wayland_pointer_end_grab (MetaWaylandPointer *pointer);

gboolean
meta_wayland_pointer_begin_modal (MetaWaylandPointer *pointer);
void
meta_wayland_pointer_end_modal   (MetaWaylandPointer *pointer);

void
meta_wayland_pointer_set_current (MetaWaylandPointer *pointer,
                                  MetaWaylandSurface *surface);

#endif /* __META_WAYLAND_POINTER_H__ */
