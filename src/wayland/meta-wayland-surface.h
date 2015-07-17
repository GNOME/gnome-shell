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
#include "backends/meta-monitor-manager-private.h"

typedef struct _MetaWaylandPendingState MetaWaylandPendingState;

#define META_TYPE_WAYLAND_SURFACE (meta_wayland_surface_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSurface,
                      meta_wayland_surface,
                      META, WAYLAND_SURFACE,
                      GObject);

#define META_TYPE_WAYLAND_SURFACE_ROLE (meta_wayland_surface_role_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaWaylandSurfaceRole, meta_wayland_surface_role,
                          META, WAYLAND_SURFACE_ROLE, GObject);

struct _MetaWaylandSurfaceRoleClass
{
  GObjectClass parent_class;

  void (*assigned) (MetaWaylandSurfaceRole *surface_role);
  void (*commit) (MetaWaylandSurfaceRole  *surface_role,
                  MetaWaylandPendingState *pending);
  gboolean (*is_on_output) (MetaWaylandSurfaceRole *surface_role,
                            MetaMonitorInfo        *monitor);
};

struct _MetaWaylandSerial {
  gboolean set;
  uint32_t value;
};

#define META_TYPE_WAYLAND_SURFACE_ROLE_SUBSURFACE (meta_wayland_surface_role_subsurface_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSurfaceRoleSubsurface,
                      meta_wayland_surface_role_subsurface,
                      META, WAYLAND_SURFACE_ROLE_SUBSURFACE,
                      MetaWaylandSurfaceRole);

#define META_TYPE_WAYLAND_SURFACE_ROLE_XDG_SURFACE (meta_wayland_surface_role_xdg_surface_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSurfaceRoleXdgSurface,
                      meta_wayland_surface_role_xdg_surface,
                      META, WAYLAND_SURFACE_ROLE_XDG_SURFACE,
                      MetaWaylandSurfaceRole);

#define META_TYPE_WAYLAND_SURFACE_ROLE_XDG_POPUP (meta_wayland_surface_role_xdg_popup_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSurfaceRoleXdgPopup,
                      meta_wayland_surface_role_xdg_popup,
                      META, WAYLAND_SURFACE_ROLE_XDG_POPUP,
                      MetaWaylandSurfaceRole);

#define META_TYPE_WAYLAND_SURFACE_ROLE_WL_SHELL_SURFACE (meta_wayland_surface_role_wl_shell_surface_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSurfaceRoleWlShellSurface,
                      meta_wayland_surface_role_wl_shell_surface,
                      META, WAYLAND_SURFACE_ROLE_WL_SHELL_SURFACE,
                      MetaWaylandSurfaceRole);

#define META_TYPE_WAYLAND_SURFACE_ROLE_DND (meta_wayland_surface_role_dnd_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSurfaceRoleDND,
                      meta_wayland_surface_role_dnd,
                      META, WAYLAND_SURFACE_ROLE_DND,
                      MetaWaylandSurfaceRole);

struct _MetaWaylandPendingState
{
  /* wl_surface.attach */
  gboolean newly_attached;
  MetaWaylandBuffer *buffer;
  struct wl_listener buffer_destroy_listener;
  int32_t dx;
  int32_t dy;

  int scale;

  /* wl_surface.damage */
  cairo_region_t *damage;

  cairo_region_t *input_region;
  gboolean input_region_set;
  cairo_region_t *opaque_region;
  gboolean opaque_region_set;

  /* wl_surface.frame */
  struct wl_list frame_callback_list;

  MetaRectangle new_geometry;
  gboolean has_new_geometry;
};

struct _MetaWaylandDragDestFuncs
{
  void (* focus_in)  (MetaWaylandDataDevice *data_device,
                      MetaWaylandSurface    *surface,
                      MetaWaylandDataOffer  *offer);
  void (* focus_out) (MetaWaylandDataDevice *data_device,
                      MetaWaylandSurface    *surface);
  void (* motion)    (MetaWaylandDataDevice *data_device,
                      MetaWaylandSurface    *surface,
                      const ClutterEvent    *event);
  void (* drop)      (MetaWaylandDataDevice *data_device,
                      MetaWaylandSurface    *surface);
};

struct _MetaWaylandSurface
{
  GObject parent;

  /* Generic stuff */
  struct wl_resource *resource;
  MetaWaylandCompositor *compositor;
  MetaSurfaceActor *surface_actor;
  MetaWaylandSurfaceRole *role;
  MetaWindow *window;
  MetaWaylandBuffer *buffer;
  struct wl_listener buffer_destroy_listener;
  cairo_region_t *input_region;
  cairo_region_t *opaque_region;
  int scale;
  int32_t offset_x, offset_y;
  GList *subsurfaces;
  GHashTable *outputs;

  /* List of pending frame callbacks that needs to stay queued longer than one
   * commit sequence, such as when it has not yet been assigned a role.
   */
  struct wl_list pending_frame_callback_list;

  struct {
    const MetaWaylandDragDestFuncs *funcs;
  } dnd;

  /* All the pending state that wl_surface.commit will apply. */
  MetaWaylandPendingState pending;

  /* Extension resources. */
  struct wl_resource *xdg_surface;
  struct wl_resource *xdg_popup;
  struct wl_resource *wl_shell_surface;
  struct wl_resource *gtk_surface;
  struct wl_resource *wl_subsurface;

  /* xdg_surface stuff */
  struct wl_resource *xdg_shell_resource;
  MetaWaylandSerial acked_configure_serial;
  gboolean has_set_geometry;
  gboolean is_modal;

  /* xdg_popup */
  struct {
    MetaWaylandSurface *parent;
    struct wl_listener parent_destroy_listener;

    MetaWaylandPopup *popup;
    struct wl_listener destroy_listener;
  } popup;

  /* wl_subsurface stuff. */
  struct {
    MetaWaylandSurface *parent;
    struct wl_listener parent_destroy_listener;

    int x;
    int y;

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
};

void                meta_wayland_shell_init     (MetaWaylandCompositor *compositor);

MetaWaylandSurface *meta_wayland_surface_create (MetaWaylandCompositor *compositor,
                                                 struct wl_client      *client,
                                                 struct wl_resource    *compositor_resource,
                                                 guint32                id);

gboolean            meta_wayland_surface_assign_role (MetaWaylandSurface *surface,
                                                      GType               role_type);

void                meta_wayland_surface_set_window (MetaWaylandSurface *surface,
                                                     MetaWindow         *window);

void                meta_wayland_surface_configure_notify (MetaWaylandSurface *surface,
                                                           int                 width,
                                                           int                 height,
                                                           MetaWaylandSerial  *sent_serial);

void                meta_wayland_surface_ping (MetaWaylandSurface *surface,
                                               guint32             serial);
void                meta_wayland_surface_delete (MetaWaylandSurface *surface);

void                meta_wayland_surface_popup_done (MetaWaylandSurface *surface);

/* Drag dest functions */
void                meta_wayland_surface_drag_dest_focus_in  (MetaWaylandSurface   *surface,
                                                              MetaWaylandDataOffer *offer);
void                meta_wayland_surface_drag_dest_motion    (MetaWaylandSurface   *surface,
                                                              const ClutterEvent   *event);
void                meta_wayland_surface_drag_dest_focus_out (MetaWaylandSurface   *surface);
void                meta_wayland_surface_drag_dest_drop      (MetaWaylandSurface   *surface);

void                meta_wayland_surface_update_outputs (MetaWaylandSurface *surface);

MetaWindow *        meta_wayland_surface_get_toplevel_window (MetaWaylandSurface *surface);

void                meta_wayland_surface_queue_pending_frame_callbacks (MetaWaylandSurface *surface);

void                meta_wayland_surface_queue_pending_state_frame_callbacks (MetaWaylandSurface      *surface,
                                                                              MetaWaylandPendingState *pending);

MetaWaylandSurface * meta_wayland_surface_role_get_surface (MetaWaylandSurfaceRole *role);

#endif
