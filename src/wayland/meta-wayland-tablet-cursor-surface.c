/*
 * Wayland Support
 *
 * Copyright (C) 2016 Red Hat, Inc.
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

#include "config.h"

#include "meta-wayland-tablet-cursor-surface.h"

struct _MetaWaylandTabletCursorSurface
{
  MetaWaylandCursorSurface parent;
};

G_DEFINE_TYPE (MetaWaylandTabletCursorSurface,
               meta_wayland_tablet_cursor_surface,
               META_TYPE_WAYLAND_CURSOR_SURFACE)

static void
meta_wayland_tablet_cursor_surface_init (MetaWaylandTabletCursorSurface *role)
{
}

static void
meta_wayland_tablet_cursor_surface_class_init (MetaWaylandTabletCursorSurfaceClass *klass)
{
}
