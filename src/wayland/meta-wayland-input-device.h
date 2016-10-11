/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat
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
 *     Jonas Ådahl <jadahl@gmail.com>
 */

#ifndef META_WAYLAND_INPUT_DEVICE_H
#define META_WAYLAND_INPUT_DEVICE_H

#include <glib-object.h>
#include <stdint.h>

#include "wayland/meta-wayland-types.h"

#define META_TYPE_WAYLAND_INPUT_DEVICE (meta_wayland_input_device_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaWaylandInputDevice,
                          meta_wayland_input_device,
                          META, WAYLAND_INPUT_DEVICE,
                          GObject)

struct _MetaWaylandInputDeviceClass
{
  GObjectClass parent_class;
};

MetaWaylandSeat * meta_wayland_input_device_get_seat (MetaWaylandInputDevice *input_device);

uint32_t meta_wayland_input_device_next_serial (MetaWaylandInputDevice *input_device);

#endif /* META_WAYLAND_INPUT_DEVICE_H */
