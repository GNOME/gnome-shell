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

#include "wayland/meta-wayland-shell-surface.h"

#define META_TYPE_WAYLAND_XDG_SURFACE (meta_wayland_xdg_surface_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaWaylandXdgSurface,
                          meta_wayland_xdg_surface,
                          META, WAYLAND_XDG_SURFACE,
                          MetaWaylandShellSurface)

struct _MetaWaylandXdgSurfaceClass
{
  MetaWaylandShellSurfaceClass parent_class;

  void (*shell_client_destroyed) (MetaWaylandXdgSurface *xdg_surface);
  void (*reset) (MetaWaylandXdgSurface *xdg_surface);
};

#define META_TYPE_WAYLAND_XDG_TOPLEVEL (meta_wayland_xdg_toplevel_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandXdgToplevel,
                      meta_wayland_xdg_toplevel,
                      META, WAYLAND_XDG_TOPLEVEL,
                      MetaWaylandXdgSurface);

#define META_TYPE_WAYLAND_XDG_POPUP (meta_wayland_xdg_popup_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandXdgPopup,
                      meta_wayland_xdg_popup,
                      META, WAYLAND_XDG_POPUP,
                      MetaWaylandXdgSurface);

void meta_wayland_xdg_shell_init (MetaWaylandCompositor *compositor);

#endif /* META_WAYLAND_XDG_SHELL_H */
