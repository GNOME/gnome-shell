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
  void (*focus) (MetaWaylandPointerGrab *grab,
                 MetaWaylandSurface     *surface,
		 const ClutterEvent     *event);
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

struct _MetaWaylandPointer
{
  struct wl_list resource_list;

  MetaWaylandSurface *focus_surface;
  struct wl_listener focus_surface_listener;
  struct wl_resource *focus_resource;
  struct wl_listener focus_resource_listener;
  guint32 focus_serial;
  guint32 click_serial;

  MetaWaylandPointerGrab *grab;
  MetaWaylandPointerGrab default_grab;
  wl_fixed_t grab_x, grab_y;
  wl_fixed_t focus_x, focus_y;
  guint32 grab_button;
  guint32 grab_serial;
  guint32 grab_time;

  wl_fixed_t x, y; /* TODO: remove, use ClutterInputDevice instead */
  MetaWaylandSurface *current;

  guint32 button_count;
};

void
meta_wayland_pointer_init (MetaWaylandPointer *pointer);

void
meta_wayland_pointer_release (MetaWaylandPointer *pointer);

void
meta_wayland_pointer_set_focus (MetaWaylandPointer *pointer,
                                MetaWaylandSurface *surface);

void
meta_wayland_pointer_start_grab (MetaWaylandPointer *pointer,
                                 MetaWaylandPointerGrab *grab);

void
meta_wayland_pointer_end_grab (MetaWaylandPointer *pointer);

gboolean
meta_wayland_pointer_start_popup_grab (MetaWaylandPointer *pointer,
				       MetaWaylandSurface *popup);

void
meta_wayland_pointer_get_relative_coordinates (MetaWaylandPointer *pointer,
					       MetaWaylandSurface *surface,
					       wl_fixed_t         *x,
					       wl_fixed_t         *y);

#endif /* __META_WAYLAND_POINTER_H__ */
