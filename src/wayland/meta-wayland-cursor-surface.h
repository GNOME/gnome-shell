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

#ifndef META_WAYLAND_CURSOR_SURFACE_H
#define META_WAYLAND_CURSOR_SURFACE_H

#include "meta-wayland-surface.h"
#include "backends/meta-cursor-renderer.h"

struct _MetaWaylandCursorSurfaceClass
{
  MetaWaylandSurfaceRoleClass parent_class;
};

#define META_TYPE_WAYLAND_CURSOR_SURFACE (meta_wayland_cursor_surface_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaWaylandCursorSurface,
                          meta_wayland_cursor_surface,
                          META, WAYLAND_CURSOR_SURFACE,
                          MetaWaylandSurfaceRole);

MetaCursorSprite *   meta_wayland_cursor_surface_get_sprite   (MetaWaylandCursorSurface *cursor_surface);

void                 meta_wayland_cursor_surface_set_hotspot  (MetaWaylandCursorSurface *cursor_surface,
                                                               int                       hotspot_x,
                                                               int                       hotspot_y);
void                 meta_wayland_cursor_surface_get_hotspot  (MetaWaylandCursorSurface *cursor_surface,
                                                               int                       *hotspot_x,
                                                               int                       *hotspot_y);
void                 meta_wayland_cursor_surface_set_renderer (MetaWaylandCursorSurface *cursor_surface,
                                                               MetaCursorRenderer       *renderer);
MetaCursorRenderer * meta_wayland_cursor_surface_get_renderer (MetaWaylandCursorSurface *cursor_surface);


#endif /* META_WAYLAND_CURSOR_SURFACE_H */
