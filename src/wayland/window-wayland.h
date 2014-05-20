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

#ifndef META_WINDOW_WAYLAND_H
#define META_WINDOW_WAYLAND_H

#include <meta/window.h>
#include "wayland/meta-wayland-types.h"

G_BEGIN_DECLS

#define META_TYPE_WINDOW_WAYLAND            (meta_window_wayland_get_type())
#define META_WINDOW_WAYLAND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_WINDOW_WAYLAND, MetaWindowWayland))
#define META_WINDOW_WAYLAND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_WINDOW_WAYLAND, MetaWindowWaylandClass))
#define META_IS_WINDOW_WAYLAND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_WINDOW_WAYLAND))
#define META_IS_WINDOW_WAYLAND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_WINDOW_WAYLAND))
#define META_WINDOW_WAYLAND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_WINDOW_WAYLAND, MetaWindowWaylandClass))

GType meta_window_wayland_get_type (void);

typedef struct _MetaWindowWayland      MetaWindowWayland;
typedef struct _MetaWindowWaylandClass MetaWindowWaylandClass;

MetaWindow * meta_window_wayland_new       (MetaDisplay        *display,
                                            MetaWaylandSurface *surface);

void meta_window_wayland_move_resize (MetaWindow *window,
                                      int         width,
                                      int         height,
                                      int         dx,
                                      int         dy);

#endif
