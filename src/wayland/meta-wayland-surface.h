/*
 * Copyright (C) 2013 Red Hat, Inc.
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

#ifndef META_WAYLAND_SURFACE_H
#define META_WAYLAND_SURFACE_H

#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>
#include <clutter/clutter.h>

#include <glib.h>
#include <cairo.h>

#include <meta/meta-cursor-tracker.h>
#include "meta-wayland-types.h"
#include "meta-surface-actor.h"

struct _MetaWaylandBuffer
{
  struct wl_resource *resource;
  struct wl_signal destroy_signal;
  struct wl_listener destroy_listener;

  CoglTexture *texture;
  uint32_t ref_count;
};

typedef struct
{
  /* wl_surface.attach */
  gboolean newly_attached;
  MetaWaylandBuffer *buffer;
  struct wl_listener buffer_destroy_listener;
  int32_t dx;
  int32_t dy;

  /* wl_surface.damage */
  cairo_region_t *damage;

  cairo_region_t *input_region;
  cairo_region_t *opaque_region;

  /* wl_surface.frame */
  struct wl_list frame_callback_list;

  gboolean frame_extents_changed;
  GtkBorder frame_extents;
} MetaWaylandPendingState;

typedef struct
{
  struct wl_resource *resource;
} MetaWaylandSurfaceExtension;

struct _MetaWaylandSurface
{
  struct wl_resource *resource;
  MetaWaylandCompositor *compositor;
  MetaSurfaceActor *surface_actor;
  MetaWindow *window;
  MetaWaylandSurfaceExtension xdg_surface;
  MetaWaylandSurfaceExtension xdg_popup;
  MetaWaylandSurfaceExtension wl_shell_surface;
  MetaWaylandSurfaceExtension gtk_surface;
  MetaWaylandSurfaceExtension subsurface;

  MetaWaylandBuffer *buffer;
  struct wl_listener buffer_destroy_listener;

  GList *subsurfaces;

  struct {
    MetaWaylandSurface *parent;
    struct wl_listener parent_destroy_listener;

    /* When the surface is synchronous, its state will be applied
     * when the parent is committed. This is done by moving the
     * "real" pending state below to here when this surface is
     * committed and in synchronous mode.
     *
     * When the parent surface is committed, we apply the pending
     * state here.
     */
    gboolean synchronous;
    MetaWaylandPendingState pending;

    int32_t pending_x;
    int32_t pending_y;
    gboolean pending_pos;
    GSList *pending_placement_ops;
  } sub;

  uint32_t state_changed_serial;

  /* All the pending state that wl_surface.commit will apply. */
  MetaWaylandPendingState pending;
};

void                meta_wayland_shell_init     (MetaWaylandCompositor *compositor);

MetaWaylandSurface *meta_wayland_surface_create (MetaWaylandCompositor *compositor,
                                                 struct wl_client      *client,
                                                 struct wl_resource    *compositor_resource,
                                                 guint32                id);

void                meta_wayland_surface_set_window (MetaWaylandSurface *surface,
                                                     MetaWindow         *window);

void                meta_wayland_surface_configure_notify (MetaWaylandSurface *surface,
							   int                 width,
							   int                 height);
void                meta_wayland_surface_send_maximized (MetaWaylandSurface *surface);
void                meta_wayland_surface_send_unmaximized (MetaWaylandSurface *surface);
void                meta_wayland_surface_send_fullscreened (MetaWaylandSurface *surface);
void                meta_wayland_surface_send_unfullscreened (MetaWaylandSurface *surface);

void                meta_wayland_surface_activated (MetaWaylandSurface *surface);
void                meta_wayland_surface_deactivated (MetaWaylandSurface *surface);

void                meta_wayland_surface_ping (MetaWaylandSurface *surface,
                                               guint32             serial);
void                meta_wayland_surface_delete (MetaWaylandSurface *surface);

void                meta_wayland_surface_popup_done (MetaWaylandSurface *surface);

#endif
