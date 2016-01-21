/*
 * Wayland Support
 *
 * Copyright (C) 2013 Intel Corporation
 * Copyright (C) 2015 Red Hat, Inc.
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

#ifndef META_WAYLAND_POPUP_H
#define META_WAYLAND_POPUP_H

#include <glib.h>
#include <wayland-server.h>

#include "meta-wayland-types.h"
#include "meta-wayland-pointer.h"

#define META_TYPE_WAYLAND_POPUP_SURFACE (meta_wayland_popup_surface_get_type ())
G_DECLARE_INTERFACE (MetaWaylandPopupSurface, meta_wayland_popup_surface,
                     META, WAYLAND_POPUP_SURFACE,
                     GObject);

struct _MetaWaylandPopupSurfaceInterface
{
  GTypeInterface parent_iface;

  void (*done) (MetaWaylandPopupSurface *popup_surface);
  void (*dismiss) (MetaWaylandPopupSurface *popup_surface);
  MetaWaylandSurface *(*get_surface) (MetaWaylandPopupSurface *popup_surface);
};

MetaWaylandPopupGrab *meta_wayland_popup_grab_create (MetaWaylandPointer      *pointer,
                                                      MetaWaylandPopupSurface *popup_surface);

void meta_wayland_popup_grab_destroy (MetaWaylandPopupGrab *grab);

MetaWaylandSurface *meta_wayland_popup_grab_get_top_popup (MetaWaylandPopupGrab *grab);

gboolean meta_wayland_pointer_grab_is_popup_grab (MetaWaylandPointerGrab *grab);

MetaWaylandPopup *meta_wayland_popup_create (MetaWaylandPopupSurface *surface,
                                             MetaWaylandPopupGrab    *grab);

void meta_wayland_popup_destroy (MetaWaylandPopup *popup);

void meta_wayland_popup_dismiss (MetaWaylandPopup *popup);

MetaWaylandSurface *meta_wayland_popup_get_top_popup (MetaWaylandPopup *popup);

#endif /* META_WAYLAND_POPUP_H */
