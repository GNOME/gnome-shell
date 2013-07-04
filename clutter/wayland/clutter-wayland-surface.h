/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 *
 */

#ifndef __CLUTTER_WAYLAND_SURFACE_H__
#define __CLUTTER_WAYLAND_SURFACE_H__

#include <glib.h>
#include <glib-object.h>
#include <clutter/clutter.h>

#include <wayland-server.h>

G_BEGIN_DECLS

#define CLUTTER_WAYLAND_TYPE_SURFACE                 (clutter_wayland_surface_get_type ())
#define CLUTTER_WAYLAND_SURFACE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_WAYLAND_TYPE_SURFACE, ClutterWaylandSurface))
#define CLUTTER_WAYLAND_SURFACE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_WAYLAND_TYPE_SURFACE, ClutterWaylandSurfaceClass))
#define CLUTTER_WAYLAND_IS_SURFACE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_WAYLAND_TYPE_SURFACE))
#define CLUTTER_WAYLAND_IS_SURFACE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_WAYLAND_TYPE_SURFACE))
#define CLUTTER_WAYLAND_SURFACE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_WAYLAND_TYPE_SURFACE, ClutterWaylandSurfaceClass))

typedef struct _ClutterWaylandSurface        ClutterWaylandSurface;
typedef struct _ClutterWaylandSurfaceClass   ClutterWaylandSurfaceClass;
typedef struct _ClutterWaylandSurfacePrivate ClutterWaylandSurfacePrivate;

/**
 * ClutterWaylandSurface:
 *
 * The #ClutterWaylandSurface structure contains only private data
 *
 * Since: 1.10
 * Stability: unstable
 */
struct _ClutterWaylandSurface
{
  /*< private >*/
  ClutterActor parent;

  ClutterWaylandSurfacePrivate *priv;
};

/**
 * ClutterWaylandSurfaceClass:
 * @queue_damage_redraw: class handler of the #ClutterWaylandSurface::queue-damage-redraw signal
 *
 * The #ClutterWaylandSurfaceClass structure contains only private data
 *
 * Since: 1.10
 * Stability: unstable
 */
struct _ClutterWaylandSurfaceClass
{
  /*< private >*/
  ClutterActorClass parent_class;

  /*< public >*/
  void (*queue_damage_redraw) (ClutterWaylandSurface *texture,
                               gint x,
                               gint y,
                               gint width,
                               gint height);

  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[8];
};

GType clutter_wayland_surface_get_type (void) G_GNUC_CONST;

ClutterActor *clutter_wayland_surface_new               (struct wl_surface *surface);
void          clutter_wayland_surface_set_surface       (ClutterWaylandSurface *self,
                                                         struct wl_surface *surface);
struct wl_surface *clutter_wayland_surface_get_surface  (ClutterWaylandSurface *self);
gboolean      clutter_wayland_surface_attach_buffer     (ClutterWaylandSurface *self,
                                                         struct wl_resource *buffer,
                                                         GError **error);
void          clutter_wayland_surface_damage_buffer     (ClutterWaylandSurface *self,
                                                         struct wl_resource *buffer,
                                                         gint32 x,
                                                         gint32 y,
                                                         gint32 width,
                                                         gint32 height);
CoglTexture  *clutter_wayland_surface_get_cogl_texture  (ClutterWaylandSurface *self);

G_END_DECLS

#endif
