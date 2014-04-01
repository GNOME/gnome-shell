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
#include <clutter/clutter.h>

#include <glib.h>
#include <cairo.h>

#include "window-private.h"
#include <meta/meta-cursor-tracker.h>

#include "meta-wayland.h"
#include "meta-wayland-versions.h"
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

typedef struct
{
  int display_index;
  char *lockfile;
  int abstract_fd;
  int unix_fd;
  pid_t pid;
  struct wl_client *client;
  struct wl_resource *xserver_resource;
  char *display_name;

  GMainLoop *init_loop;
} MetaXWaylandManager;

struct _MetaWaylandCompositor
{
  struct wl_display *wayland_display;
  char *display_name;
  struct wl_event_loop *wayland_loop;
  ClutterActor *stage;
  GHashTable *outputs;
  GSource *wayland_event_source;
  GList *surfaces;
  struct wl_list frame_callbacks;

  MetaXWaylandManager xwayland_manager;

  MetaWaylandSeat *seat;
};

MetaWaylandBuffer *     meta_wayland_buffer_from_resource       (struct wl_resource    *resource);
void                    meta_wayland_buffer_ref                 (MetaWaylandBuffer     *buffer);
void                    meta_wayland_buffer_unref               (MetaWaylandBuffer     *buffer);

#endif /* META_WAYLAND_PRIVATE_H */
