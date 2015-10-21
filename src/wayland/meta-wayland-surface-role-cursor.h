/*
 * Wayland Support
 *
 * Copyright (C) 2015 Red Hat, Inc.
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
 */

#ifndef META_WAYLAND_SURFACE_ROLE_CURSOR_H
#define META_WAYLAND_SURFACE_ROLE_CURSOR_H

#include "meta-wayland-surface.h"
#include "backends/meta-cursor-renderer.h"

#define META_TYPE_WAYLAND_SURFACE_ROLE_CURSOR (meta_wayland_surface_role_cursor_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSurfaceRoleCursor,
                      meta_wayland_surface_role_cursor,
                      META, WAYLAND_SURFACE_ROLE_CURSOR,
                      MetaWaylandSurfaceRole);

MetaCursorSprite *   meta_wayland_surface_role_cursor_get_sprite   (MetaWaylandSurfaceRoleCursor *cursor_role);

void                 meta_wayland_surface_role_cursor_set_hotspot  (MetaWaylandSurfaceRoleCursor *cursor_role,
                                                                    gint                          hotspot_x,
                                                                    gint                          hotspot_y);
void                 meta_wayland_surface_role_cursor_get_hotspot  (MetaWaylandSurfaceRoleCursor *cursor_role,
                                                                    gint                         *hotspot_x,
                                                                    gint                         *hotspot_y);
void                 meta_wayland_surface_role_cursor_set_renderer (MetaWaylandSurfaceRoleCursor *cursor_role,
                                                                    MetaCursorRenderer           *renderer);
MetaCursorRenderer * meta_wayland_surface_role_cursor_get_renderer (MetaWaylandSurfaceRoleCursor *cursor_role);


#endif /* META_WAYLAND_SURFACE_ROLE_CURSOR_H */
