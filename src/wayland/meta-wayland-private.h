/*
 * Copyright (C) 2012 Intel Corporation
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

#ifndef META_WAYLAND_PRIVATE_H
#define META_WAYLAND_PRIVATE_H

#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>
#include <clutter/clutter.h>

#include <glib.h>
#include <cairo.h>

#include "window-private.h"
#include "meta-weston-launch.h"
#include <meta/meta-cursor-tracker.h>

#include "meta-wayland-types.h"
#include "meta-wayland-surface.h"
#include "meta-wayland-seat.h"

typedef struct
{
  struct wl_resource *resource;
  cairo_region_t *region;
} MetaWaylandRegion;

typedef struct
{
  GSource source;
  GPollFD pfd;
  struct wl_display *display;
} WaylandEventSource;

typedef struct
{
  struct wl_list link;

  /* Pointer back to the compositor */
  MetaWaylandCompositor *compositor;

  struct wl_resource *resource;
} MetaWaylandFrameCallback;

struct _MetaWaylandCompositor
{
  struct wl_display *wayland_display;
  struct wl_event_loop *wayland_loop;
  GMainLoop *init_loop;
  ClutterActor *stage;
  GHashTable *outputs;
  GSource *wayland_event_source;
  GList *surfaces;
  struct wl_list frame_callbacks;

  int xwayland_display_index;
  char *xwayland_lockfile;
  int xwayland_abstract_fd;
  int xwayland_unix_fd;
  pid_t xwayland_pid;
  struct wl_client *xwayland_client;
  struct wl_resource *xserver_resource;

  MetaLauncher *launcher;
  int drm_fd;

  MetaWaylandSeat *seat;

  /* This surface is only used to keep drag of the implicit grab when
     synthesizing XEvents for Mutter */
  MetaWaylandSurface *implicit_grab_surface;
  /* Button that was pressed to initiate an implicit grab. The
     implicit grab will only be released when this button is
     released */
  guint32 implicit_grab_button;
};

void                    meta_wayland_init                       (void);
void                    meta_wayland_finalize                   (void);

/* We maintain a singleton MetaWaylandCompositor which can be got at via this
 * API after meta_wayland_init() has been called. */
MetaWaylandCompositor  *meta_wayland_compositor_get_default     (void);

void                    meta_wayland_compositor_repick          (MetaWaylandCompositor *compositor);

void                    meta_wayland_compositor_set_input_focus (MetaWaylandCompositor *compositor,
                                                                 MetaWindow            *window);

MetaLauncher           *meta_wayland_compositor_get_launcher    (MetaWaylandCompositor *compositor);

void                    meta_wayland_surface_free               (MetaWaylandSurface    *surface);

#endif /* META_WAYLAND_PRIVATE_H */
