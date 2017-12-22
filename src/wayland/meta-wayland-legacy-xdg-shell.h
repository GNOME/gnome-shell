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

#ifndef META_WAYLAND_LEGACY_XDG_SHELL_H
#define META_WAYLAND_LEGACY_XDG_SHELL_H

#include "wayland/meta-wayland-surface.h"
#include "wayland/meta-wayland-shell-surface.h"

#define META_TYPE_WAYLAND_ZXDG_SURFACE_V6 (meta_wayland_zxdg_surface_v6_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaWaylandZxdgSurfaceV6,
                          meta_wayland_zxdg_surface_v6,
                          META, WAYLAND_ZXDG_SURFACE_V6,
                          MetaWaylandShellSurface)

struct _MetaWaylandZxdgSurfaceV6Class
{
  MetaWaylandShellSurfaceClass parent_class;

  void (*shell_client_destroyed) (MetaWaylandZxdgSurfaceV6 *xdg_surface);
};

#define META_TYPE_WAYLAND_ZXDG_TOPLEVEL_V6 (meta_wayland_zxdg_toplevel_v6_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandZxdgToplevelV6,
                      meta_wayland_zxdg_toplevel_v6,
                      META, WAYLAND_ZXDG_TOPLEVEL_V6,
                      MetaWaylandZxdgSurfaceV6);

#define META_TYPE_WAYLAND_ZXDG_POPUP_V6 (meta_wayland_zxdg_popup_v6_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandZxdgPopupV6,
                      meta_wayland_zxdg_popup_v6,
                      META, WAYLAND_ZXDG_POPUP_V6,
                      MetaWaylandZxdgSurfaceV6);

void meta_wayland_legacy_xdg_shell_init (MetaWaylandCompositor *compositor);

#endif /* META_WAYLAND_LEGACY_XDG_SHELL_H */
