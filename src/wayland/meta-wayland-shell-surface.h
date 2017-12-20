/*
 * Copyright (C) 2012,2013 Intel Corporation
 * Copyright (C) 2013-2017 Red Hat, Inc.
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

#ifndef META_WAYLAND_SHELL_SURFACE_H
#define META_WAYLAND_SHELL_SURFACE_H

#include "wayland/meta-wayland-actor-surface.h"

#define META_TYPE_WAYLAND_SHELL_SURFACE (meta_wayland_shell_surface_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaWaylandShellSurface,
                          meta_wayland_shell_surface,
                          META, WAYLAND_SHELL_SURFACE,
                          MetaWaylandActorSurface)

struct _MetaWaylandShellSurfaceClass
{
  MetaWaylandActorSurfaceClass parent_class;

  void (*configure) (MetaWaylandShellSurface *shell_surface,
                     int                      new_x,
                     int                      new_y,
                     int                      new_width,
                     int                      new_height,
                     MetaWaylandSerial       *sent_serial);
  void (*managed) (MetaWaylandShellSurface *shell_surface,
                   MetaWindow              *window);
  void (*ping) (MetaWaylandShellSurface *shell_surface,
                uint32_t                 serial);
  void (*close) (MetaWaylandShellSurface *shell_surface);
};

void meta_wayland_shell_surface_configure (MetaWaylandShellSurface *shell_surface,
                                           int                      new_x,
                                           int                      new_y,
                                           int                      new_width,
                                           int                      new_height,
                                           MetaWaylandSerial       *sent_serial);

void meta_wayland_shell_surface_ping (MetaWaylandShellSurface *shell_surface,
                                      uint32_t                 serial);

void meta_wayland_shell_surface_close (MetaWaylandShellSurface *shell_surface);

void meta_wayland_shell_surface_managed (MetaWaylandShellSurface *shell_surface,
                                         MetaWindow              *window);

#endif /* META_WAYLAND_SHELL_SURFACE_H */
