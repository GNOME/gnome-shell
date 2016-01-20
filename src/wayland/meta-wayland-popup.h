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

MetaWaylandPopupGrab *meta_wayland_popup_grab_create (MetaWaylandPointer *pointer,
                                                      MetaWaylandSurface *surface);

void meta_wayland_popup_grab_destroy (MetaWaylandPopupGrab *grab);

MetaWaylandSurface *meta_wayland_popup_grab_get_top_popup (MetaWaylandPopupGrab *grab);

gboolean meta_wayland_pointer_grab_is_popup_grab (MetaWaylandPointerGrab *grab);

MetaWaylandPopup *meta_wayland_popup_create (MetaWaylandSurface   *surface,
                                             MetaWaylandPopupGrab *grab);

void meta_wayland_popup_destroy (MetaWaylandPopup *popup);

void meta_wayland_popup_dismiss (MetaWaylandPopup *popup);

MetaWaylandSurface *meta_wayland_popup_get_top_popup (MetaWaylandPopup *popup);

struct wl_signal *meta_wayland_popup_get_destroy_signal (MetaWaylandPopup *popup);

#endif /* META_WAYLAND_POPUP_H */
