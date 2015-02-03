/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
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

#ifndef META_WAYLAND_OUTPUTS_H
#define META_WAYLAND_OUTPUTS_H

#include "backends/meta-monitor-manager-private.h"
#include "meta-wayland-private.h"

#define META_TYPE_WAYLAND_OUTPUT            (meta_wayland_output_get_type ())
#define META_WAYLAND_OUTPUT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_WAYLAND_OUTPUT, MetaWaylandOutput))
#define META_WAYLAND_OUTPUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_WAYLAND_OUTPUT, MetaWaylandOutputClass))
#define META_IS_WAYLAND_OUTPUT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_WAYLAND_OUTPUT))
#define META_IS_WAYLAND_OUTPUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_WAYLAND_OUTPUT))
#define META_WAYLAND_OUTPUT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_WAYLAND_OUTPUT, MetaWaylandOutputClass))

typedef struct _MetaWaylandOutputClass  MetaWaylandOutputClass;

struct _MetaWaylandOutput
{
  GObject                   parent;

  MetaMonitorInfo          *monitor_info;
  struct wl_global         *global;
  int                       x, y;
  enum wl_output_transform  transform;

  GList                    *resources;
};

struct _MetaWaylandOutputClass
{
  GObjectClass parent_class;
};

GType meta_wayland_output_get_type (void) G_GNUC_CONST;

void meta_wayland_outputs_init (MetaWaylandCompositor *compositor);

#endif /* META_WAYLAND_OUTPUTS_H */
