/*
 * Copyright (C) 2013-2015 Red Hat, Inc.
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

#ifndef META_WAYLAND_XDG_SHELL_H
#define META_WAYLAND_XDG_SHELL_H

#include "wayland/meta-wayland-surface.h"

#define META_TYPE_WAYLAND_SURFACE_ROLE_XDG_SURFACE (meta_wayland_surface_role_xdg_surface_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSurfaceRoleXdgSurface,
                      meta_wayland_surface_role_xdg_surface,
                      META, WAYLAND_SURFACE_ROLE_XDG_SURFACE,
                      MetaWaylandSurfaceRoleShellSurface);

#define META_TYPE_WAYLAND_SURFACE_ROLE_XDG_POPUP (meta_wayland_surface_role_xdg_popup_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSurfaceRoleXdgPopup,
                      meta_wayland_surface_role_xdg_popup,
                      META, WAYLAND_SURFACE_ROLE_XDG_POPUP,
                      MetaWaylandSurfaceRoleShellSurface);

void meta_wayland_xdg_shell_init (MetaWaylandCompositor *compositor);

#endif /* META_WAYLAND_XDG_SHELL_H */
