/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Endless Mobile
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
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#ifndef META_WAYLAND_REGION_H
#define META_WAYLAND_REGION_H

#include <glib.h>
#include <cairo.h>
#include <wayland-server.h>
#include "meta-wayland-types.h"

MetaWaylandRegion * meta_wayland_region_create (MetaWaylandCompositor *compositor,
                                                struct wl_client      *client,
                                                struct wl_resource    *compositor_resource,
                                                guint32                id);

cairo_region_t * meta_wayland_region_peek_cairo_region (MetaWaylandRegion *region);

#endif /* META_WAYLAND_REGION_H */
