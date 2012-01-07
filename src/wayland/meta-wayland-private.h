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

typedef struct _MetaWaylandCompositor MetaWaylandCompositor;

typedef struct
{
  struct wl_resource *resource;
  struct wl_signal destroy_signal;
  struct wl_listener destroy_listener;

  union
  {
    struct wl_shm_buffer *shm_buffer;
    struct wl_buffer *legacy_buffer;
  };

  int32_t width, height;
  uint32_t busy_count;
} MetaWaylandBuffer;

typedef struct
{
  MetaWaylandBuffer *buffer;
  struct wl_listener destroy_listener;
} MetaWaylandBufferReference;

typedef struct
{
  struct wl_resource *resource;
  cairo_region_t *region;
} MetaWaylandRegion;

struct _MetaWaylandSurface
{
  struct wl_resource *resource;
  MetaWaylandCompositor *compositor;
  guint32 xid;
  int x;
  int y;
  MetaWaylandBufferReference buffer_ref;
  MetaWindow *window;
  gboolean has_shell_surface;

  /* All the pending state, that wl_surface.commit will apply. */
  struct
  {
    /* wl_surface.attach */
    gboolean newly_attached;
    MetaWaylandBuffer *buffer;
    struct wl_listener buffer_destroy_listener;
    int32_t sx;
    int32_t sy;

    /* wl_surface.damage */
    cairo_region_t *damage;

    /* wl_surface.frame */
    struct wl_list frame_callback_list;
  } pending;
};

#ifndef HAVE_META_WAYLAND_SURFACE_TYPE
typedef struct _MetaWaylandSurface MetaWaylandSurface;
#endif

typedef struct
{
  MetaWaylandSurface *surface;
  struct wl_resource *resource;
  struct wl_listener surface_destroy_listener;
} MetaWaylandShellSurface;

typedef struct
{
  guint32 flags;
  int width;
  int height;
  int refresh;
} MetaWaylandMode;

typedef struct
{
  struct wl_object wayland_output;
  int x;
  int y;
  int width_mm;
  int height_mm;
  /* XXX: with sliced stages we'd reference a CoglFramebuffer here. */

  GList *modes;
} MetaWaylandOutput;

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
  GList *outputs;
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
  GHashTable *window_surfaces;
};

void                    meta_wayland_init                   (void);
void                    meta_wayland_finalize               (void);

/* We maintain a singleton MetaWaylandCompositor which can be got at via this
 * API after meta_wayland_init() has been called. */
MetaWaylandCompositor  *meta_wayland_compositor_get_default (void);

void                    meta_wayland_handle_sig_child       (void);

MetaWaylandSurface     *meta_wayland_lookup_surface_for_xid (guint32 xid);

#endif /* META_WAYLAND_PRIVATE_H */
