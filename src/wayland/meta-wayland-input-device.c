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

#include "config.h"

#include "wayland/meta-wayland-input-device.h"

#include <wayland-server.h>

#include "wayland/meta-wayland-seat.h"

enum
{
  PROP_0,

  PROP_SEAT
};

typedef struct _MetaWaylandInputDevicePrivate
{
  MetaWaylandSeat *seat;
} MetaWaylandInputDevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaWaylandInputDevice,
                            meta_wayland_input_device,
                            G_TYPE_OBJECT)

MetaWaylandSeat *
meta_wayland_input_device_get_seat (MetaWaylandInputDevice *input_device)
{
  MetaWaylandInputDevicePrivate *priv =
    meta_wayland_input_device_get_instance_private (input_device);

  return priv->seat;
}

uint32_t
meta_wayland_input_device_next_serial (MetaWaylandInputDevice *input_device)
{
  MetaWaylandSeat *seat = meta_wayland_input_device_get_seat (input_device);

  return wl_display_next_serial (seat->wl_display);
}

static void
meta_wayland_input_device_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  MetaWaylandInputDevice *input_device = META_WAYLAND_INPUT_DEVICE (object);
  MetaWaylandInputDevicePrivate *priv =
    meta_wayland_input_device_get_instance_private (input_device);

  switch (prop_id)
    {
    case PROP_SEAT:
      priv->seat = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_wayland_input_device_get_property (GObject      *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  MetaWaylandInputDevice *input_device = META_WAYLAND_INPUT_DEVICE (object);
  MetaWaylandInputDevicePrivate *priv =
    meta_wayland_input_device_get_instance_private (input_device);

  switch (prop_id)
    {
    case PROP_SEAT:
      g_value_set_pointer (value, priv->seat);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_wayland_input_device_init (MetaWaylandInputDevice *input_device)
{
}

static void
meta_wayland_input_device_class_init (MetaWaylandInputDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  object_class->set_property = meta_wayland_input_device_set_property;
  object_class->get_property = meta_wayland_input_device_get_property;

  pspec = g_param_spec_pointer ("seat",
                                "MetaWaylandSeat",
                                "The seat",
                                G_PARAM_READWRITE |
                                G_PARAM_STATIC_STRINGS |
                                G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_SEAT, pspec);
}
