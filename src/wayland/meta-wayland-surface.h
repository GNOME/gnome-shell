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

struct _MetaWaylandBuffer
{
  struct wl_resource *resource;
  struct wl_signal destroy_signal;
  struct wl_listener destroy_listener;

  CoglTexture *texture;
  int32_t width, height;
  uint32_t busy_count;
};

struct _MetaWaylandBufferReference
{
  MetaWaylandBuffer *buffer;
  struct wl_listener destroy_listener;
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
} MetaWaylandDoubleBufferedState;

typedef enum {
  META_WAYLAND_SURFACE_TOPLEVEL = 0,
  META_WAYLAND_SURFACE_MAXIMIZED,
  META_WAYLAND_SURFACE_FULLSCREEN
} MetaWaylandSurfaceType;

typedef struct
{
  MetaWaylandSurfaceType initial_type;
  struct wl_resource *transient_for;

  char *title;
  char *wm_class;

  char *gtk_application_id;
  char *gtk_unique_bus_name;
  char *gtk_app_menu_path;
  char *gtk_menubar_path;
  char *gtk_application_object_path;
  char *gtk_window_object_path;
} MetaWaylandSurfaceInitialState;

typedef struct
{
  MetaWaylandSurface *surface;
  struct wl_resource *resource;
  struct wl_listener surface_destroy_listener;
} MetaWaylandSurfaceExtension;

struct _MetaWaylandSurface
{
  struct wl_resource *resource;
  MetaWaylandCompositor *compositor;
  MetaWaylandBufferReference buffer_ref;
  MetaWindow *window;
  MetaWaylandSurfaceExtension *shell_surface;
  MetaWaylandSurfaceExtension *gtk_surface;

  /* All the pending state, that wl_surface.commit will apply. */
  MetaWaylandDoubleBufferedState pending;

  /* All the initial state, that wl_shell_surface.set_* will apply
     (through meta_window_new_for_wayland) */
  MetaWaylandSurfaceInitialState *initial_state;
};

void                meta_wayland_init_shell     (MetaWaylandCompositor *compositor);

MetaWaylandSurface *meta_wayland_surface_create (MetaWaylandCompositor *compositor,
						 struct wl_client      *client,
						 guint32                id,
						 guint32                version);
void                meta_wayland_surface_free   (MetaWaylandSurface    *surface);

void                meta_wayland_surface_set_initial_state (MetaWaylandSurface *surface,
							    MetaWindow         *window);

void                meta_wayland_surface_configure_notify (MetaWaylandSurface *surface,
							   int                 width,
							   int                 height,
							   int                 edges);

#endif
