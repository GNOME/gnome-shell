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

#ifndef META_WAYLAND_H
#define META_WAYLAND_H

#include <clutter/clutter.h>
#include <meta/types.h>
#include "meta-wayland-types.h"

void                    meta_wayland_pre_clutter_init           (void);
void                    meta_wayland_init                       (void);
void                    meta_wayland_finalize                   (void);

/* We maintain a singleton MetaWaylandCompositor which can be got at via this
 * API after meta_wayland_init() has been called. */
MetaWaylandCompositor  *meta_wayland_compositor_get_default     (void);

void                    meta_wayland_compositor_update          (MetaWaylandCompositor *compositor,
                                                                 const ClutterEvent    *event);
gboolean                meta_wayland_compositor_handle_event    (MetaWaylandCompositor *compositor,
                                                                 const ClutterEvent    *event);
void                    meta_wayland_compositor_update_key_state (MetaWaylandCompositor *compositor,
                                                                 char                  *key_vector,
                                                                  int                    key_vector_len,
                                                                  int                    offset);
void                    meta_wayland_compositor_repick          (MetaWaylandCompositor *compositor);

void                    meta_wayland_compositor_set_input_focus (MetaWaylandCompositor *compositor,
                                                                 MetaWindow            *window);

void                    meta_wayland_compositor_paint_finished  (MetaWaylandCompositor *compositor);

void                    meta_wayland_compositor_destroy_frame_callbacks (MetaWaylandCompositor *compositor,
                                                                         MetaWaylandSurface    *surface);

const char             *meta_wayland_get_wayland_display_name   (MetaWaylandCompositor *compositor);
const char             *meta_wayland_get_xwayland_display_name  (MetaWaylandCompositor *compositor);

#endif

