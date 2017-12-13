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
#include "meta-wayland-pointer-constraints.h"

typedef struct _MetaWaylandPendingState MetaWaylandPendingState;

#define META_TYPE_WAYLAND_SURFACE (meta_wayland_surface_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSurface,
                      meta_wayland_surface,
                      META, WAYLAND_SURFACE,
                      GObject);

#define META_TYPE_WAYLAND_SURFACE_ROLE (meta_wayland_surface_role_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaWaylandSurfaceRole, meta_wayland_surface_role,
                          META, WAYLAND_SURFACE_ROLE, GObject);

#define META_TYPE_WAYLAND_PENDING_STATE (meta_wayland_pending_state_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandPendingState,
                      meta_wayland_pending_state,
                      META, WAYLAND_PENDING_STATE,
                      GObject);

struct _MetaWaylandSurfaceRoleClass
{
  GObjectClass parent_class;

  void (*assigned) (MetaWaylandSurfaceRole *surface_role);
  void (*pre_commit) (MetaWaylandSurfaceRole  *surface_role,
                      MetaWaylandPendingState *pending);
  void (*commit) (MetaWaylandSurfaceRole  *surface_role,
                  MetaWaylandPendingState *pending);
  gboolean (*is_on_logical_monitor) (MetaWaylandSurfaceRole *surface_role,
                                     MetaLogicalMonitor     *logical_monitor);
  MetaWaylandSurface * (*get_toplevel) (MetaWaylandSurfaceRole *surface_role);
};

struct _MetaWaylandSerial {
  gboolean set;
  uint32_t value;
};

#define META_TYPE_WAYLAND_SURFACE_ROLE_ACTOR_SURFACE (meta_wayland_surface_role_actor_surface_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaWaylandSurfaceRoleActorSurface,
                          meta_wayland_surface_role_actor_surface,
                          META, WAYLAND_SURFACE_ROLE_ACTOR_SURFACE,
                          MetaWaylandSurfaceRole);

struct _MetaWaylandSurfaceRoleActorSurfaceClass
{
  MetaWaylandSurfaceRoleClass parent_class;
};

#define META_TYPE_WAYLAND_SHELL_SURFACE (meta_wayland_shell_surface_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaWaylandShellSurface,
                          meta_wayland_shell_surface,
                          META, WAYLAND_SHELL_SURFACE,
                          MetaWaylandSurfaceRoleActorSurface);

struct _MetaWaylandShellSurfaceClass
{
  MetaWaylandSurfaceRoleActorSurfaceClass parent_class;

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

#define META_TYPE_WAYLAND_SURFACE_ROLE_SUBSURFACE (meta_wayland_surface_role_subsurface_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSurfaceRoleSubsurface,
                      meta_wayland_surface_role_subsurface,
                      META, WAYLAND_SURFACE_ROLE_SUBSURFACE,
                      MetaWaylandSurfaceRoleActorSurface);

#define META_TYPE_WAYLAND_SURFACE_ROLE_DND (meta_wayland_surface_role_dnd_get_type ())
G_DECLARE_FINAL_TYPE (MetaWaylandSurfaceRoleDND,
                      meta_wayland_surface_role_dnd,
                      META, WAYLAND_SURFACE_ROLE_DND,
                      MetaWaylandSurfaceRole);

struct _MetaWaylandPendingState
{
  GObject parent;

  /* wl_surface.attach */
  gboolean newly_attached;
  MetaWaylandBuffer *buffer;
  gulong buffer_destroy_handler_id;
  int32_t dx;
  int32_t dy;

  int scale;

  /* wl_surface.damage */
  cairo_region_t *surface_damage;
  /* wl_surface.damage_buffer */
  cairo_region_t *buffer_damage;

  cairo_region_t *input_region;
  gboolean input_region_set;
  cairo_region_t *opaque_region;
  gboolean opaque_region_set;

  /* wl_surface.frame */
  struct wl_list frame_callback_list;

  MetaRectangle new_geometry;
  gboolean has_new_geometry;

  /* pending min/max size in window geometry coordinates */
  gboolean has_new_min_size;
  int new_min_width;
  int new_min_height;
  gboolean has_new_max_size;
  int new_max_width;
  int new_max_height;
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
  void (* update)    (MetaWaylandDataDevice *data_device,
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
  cairo_region_t *input_region;
  cairo_region_t *opaque_region;
  int scale;
  int32_t offset_x, offset_y;
  GList *subsurfaces;
  GHashTable *outputs_to_destroy_notify_id;

  /* Buffer reference state. */
  struct {
    MetaWaylandBuffer *buffer;
    unsigned int use_count;
  } buffer_ref;

  /* Buffer renderer state. */
  gboolean buffer_held;

  /* List of pending frame callbacks that needs to stay queued longer than one
   * commit sequence, such as when it has not yet been assigned a role.
   */
  struct wl_list pending_frame_callback_list;

  /* Intermediate state for when no role has been assigned. */
  struct {
    MetaWaylandBuffer *buffer;
  } unassigned;

  struct {
    const MetaWaylandDragDestFuncs *funcs;
  } dnd;

  /* All the pending state that wl_surface.commit will apply. */
  MetaWaylandPendingState *pending;

  /* Extension resources. */
  struct wl_resource *wl_subsurface;

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
    MetaWaylandPendingState *pending;

    int32_t pending_x;
    int32_t pending_y;
    gboolean pending_pos;
    GSList *pending_placement_ops;
  } sub;

  /* table of seats for which shortcuts are inhibited */
  GHashTable *shortcut_inhibited_seats;
};

void                meta_wayland_shell_init     (MetaWaylandCompositor *compositor);

MetaWaylandSurface *meta_wayland_surface_create (MetaWaylandCompositor *compositor,
                                                 struct wl_client      *client,
                                                 struct wl_resource    *compositor_resource,
                                                 guint32                id);

gboolean            meta_wayland_surface_assign_role (MetaWaylandSurface *surface,
                                                      GType               role_type,
                                                      const char         *first_property_name,
                                                      ...);

MetaWaylandBuffer  *meta_wayland_surface_get_buffer (MetaWaylandSurface *surface);

void                meta_wayland_surface_ref_buffer_use_count (MetaWaylandSurface *surface);

void                meta_wayland_surface_unref_buffer_use_count (MetaWaylandSurface *surface);

void                meta_wayland_surface_set_window (MetaWaylandSurface *surface,
                                                     MetaWindow         *window);

void                meta_wayland_surface_configure_notify (MetaWaylandSurface *surface,
                                                           int                 new_x,
                                                           int                 new_y,
                                                           int                 width,
                                                           int                 height,
                                                           MetaWaylandSerial  *sent_serial);

void                meta_wayland_surface_ping (MetaWaylandSurface *surface,
                                               guint32             serial);
void                meta_wayland_surface_delete (MetaWaylandSurface *surface);

/* Drag dest functions */
void                meta_wayland_surface_drag_dest_focus_in  (MetaWaylandSurface   *surface,
                                                              MetaWaylandDataOffer *offer);
void                meta_wayland_surface_drag_dest_motion    (MetaWaylandSurface   *surface,
                                                              const ClutterEvent   *event);
void                meta_wayland_surface_drag_dest_focus_out (MetaWaylandSurface   *surface);
void                meta_wayland_surface_drag_dest_drop      (MetaWaylandSurface   *surface);
void                meta_wayland_surface_drag_dest_update    (MetaWaylandSurface   *surface);

void                meta_wayland_surface_update_outputs (MetaWaylandSurface *surface);

MetaWaylandSurface *meta_wayland_surface_get_toplevel (MetaWaylandSurface *surface);

MetaWindow *        meta_wayland_surface_get_toplevel_window (MetaWaylandSurface *surface);

void                meta_wayland_surface_queue_pending_frame_callbacks (MetaWaylandSurface *surface);

void                meta_wayland_surface_queue_pending_state_frame_callbacks (MetaWaylandSurface      *surface,
                                                                              MetaWaylandPendingState *pending);

void                meta_wayland_surface_get_relative_coordinates (MetaWaylandSurface *surface,
                                                                   float               abs_x,
                                                                   float               abs_y,
                                                                   float              *sx,
                                                                   float              *sy);

void                meta_wayland_surface_get_absolute_coordinates (MetaWaylandSurface *surface,
                                                                   float               sx,
                                                                   float               sy,
                                                                   float              *x,
                                                                   float              *y);

MetaWaylandSurface * meta_wayland_surface_role_get_surface (MetaWaylandSurfaceRole *role);

cairo_region_t *    meta_wayland_surface_calculate_input_region (MetaWaylandSurface *surface);

void                meta_wayland_surface_calculate_window_geometry (MetaWaylandSurface *surface,
                                                                    MetaRectangle      *total_geometry,
                                                                    float               parent_x,
                                                                    float               parent_y);

void                meta_wayland_surface_destroy_window (MetaWaylandSurface *surface);

gboolean            meta_wayland_surface_begin_grab_op (MetaWaylandSurface *surface,
                                                        MetaWaylandSeat    *seat,
                                                        MetaGrabOp          grab_op,
                                                        gfloat              x,
                                                        gfloat              y);

void                meta_wayland_surface_window_managed (MetaWaylandSurface *surface,
                                                         MetaWindow         *window);

void                meta_wayland_surface_inhibit_shortcuts (MetaWaylandSurface *surface,
                                                            MetaWaylandSeat    *seat);

void                meta_wayland_surface_restore_shortcuts (MetaWaylandSurface *surface,
                                                            MetaWaylandSeat    *seat);

gboolean            meta_wayland_surface_is_shortcuts_inhibited (MetaWaylandSurface *surface,
                                                                 MetaWaylandSeat    *seat);

#endif
