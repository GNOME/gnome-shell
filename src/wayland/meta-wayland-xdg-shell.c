/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2012-2013 Intel Corporation
 * Copyright (C) 2013-2015 Red Hat Inc.
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
 *
 */

#include "config.h"

#include "wayland/meta-wayland-xdg-shell.h"

#include "backends/meta-logical-monitor.h"
#include "core/window-private.h"
#include "wayland/meta-wayland.h"
#include "wayland/meta-wayland-outputs.h"
#include "wayland/meta-wayland-popup.h"
#include "wayland/meta-wayland-private.h"
#include "wayland/meta-wayland-seat.h"
#include "wayland/meta-wayland-shell-surface.h"
#include "wayland/meta-wayland-surface.h"
#include "wayland/meta-wayland-versions.h"
#include "wayland/meta-window-wayland.h"

#include "xdg-shell-server-protocol.h"

enum
{
  XDG_SURFACE_PROP_0,

  XDG_SURFACE_PROP_SHELL_CLIENT,
  XDG_SURFACE_PROP_RESOURCE,
};

typedef struct _MetaWaylandXdgShellClient
{
  struct wl_resource *resource;
  GList *surfaces;
  GList *surface_constructors;
} MetaWaylandXdgShellClient;

typedef struct _MetaWaylandXdgPositioner
{
  MetaRectangle anchor_rect;
  int32_t width;
  int32_t height;
  uint32_t gravity;
  uint32_t anchor;
  uint32_t constraint_adjustment;
  int32_t offset_x;
  int32_t offset_y;
} MetaWaylandXdgPositioner;

typedef struct _MetaWaylandXdgSurfaceConstructor
{
  MetaWaylandSurface *surface;
  struct wl_resource *resource;
  MetaWaylandXdgShellClient *shell_client;
} MetaWaylandXdgSurfaceConstructor;

typedef struct _MetaWaylandXdgSurfacePrivate
{
  struct wl_resource *resource;
  MetaWaylandXdgShellClient *shell_client;
  MetaWaylandSerial acked_configure_serial;
  MetaRectangle geometry;

  guint configure_sent : 1;
  guint first_buffer_attached : 1;
  guint has_set_geometry : 1;
} MetaWaylandXdgSurfacePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaWaylandXdgSurface,
                            meta_wayland_xdg_surface,
                            META_TYPE_WAYLAND_SHELL_SURFACE);

struct _MetaWaylandXdgToplevel
{
  MetaWaylandXdgSurface parent;

  struct wl_resource *resource;
};

G_DEFINE_TYPE (MetaWaylandXdgToplevel,
               meta_wayland_xdg_toplevel,
               META_TYPE_WAYLAND_XDG_SURFACE);

struct _MetaWaylandXdgPopup
{
  MetaWaylandXdgSurface parent;

  struct wl_resource *resource;

  MetaWaylandSurface *parent_surface;
  gulong parent_surface_unmapped_handler_id;

  MetaWaylandPopup *popup;

  gboolean dismissed_by_client;

  struct {
    MetaWaylandSurface *parent_surface;

    /*
     * The coordinates/dimensions in the placement rule are in logical pixel
     * coordinate space, i.e. not scaled given what monitor the popup is on.
     */
    MetaPlacementRule placement_rule;

    MetaWaylandSeat *grab_seat;
    uint32_t grab_serial;
  } setup;
};

static void
popup_surface_iface_init (MetaWaylandPopupSurfaceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaWaylandXdgPopup,
                         meta_wayland_xdg_popup,
                         META_TYPE_WAYLAND_XDG_SURFACE,
                         G_IMPLEMENT_INTERFACE (META_TYPE_WAYLAND_POPUP_SURFACE,
                                                popup_surface_iface_init));

static MetaPlacementRule
meta_wayland_xdg_positioner_to_placement (MetaWaylandXdgPositioner *xdg_positioner);

static struct wl_resource *
meta_wayland_xdg_surface_get_wm_base_resource (MetaWaylandXdgSurface *xdg_surface);

static MetaRectangle
meta_wayland_xdg_surface_get_window_geometry (MetaWaylandXdgSurface *xdg_surface);

static uint32_t
meta_wayland_xdg_surface_send_configure (MetaWaylandXdgSurface *xdg_surface);

static MetaWaylandSurface *
surface_from_xdg_surface_resource (struct wl_resource *resource)
{
  MetaWaylandSurfaceRole *surface_role = wl_resource_get_user_data (resource);
  return meta_wayland_surface_role_get_surface (surface_role);
}

static MetaWaylandSurface *
surface_from_xdg_toplevel_resource (struct wl_resource *resource)
{
  return surface_from_xdg_surface_resource (resource);
}

static void
meta_wayland_xdg_surface_reset (MetaWaylandXdgSurface *xdg_surface)
{
  META_WAYLAND_XDG_SURFACE_GET_CLASS (xdg_surface)->reset (xdg_surface);
}

static void
xdg_toplevel_destructor (struct wl_resource *resource)
{
  MetaWaylandXdgToplevel *xdg_toplevel = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  meta_wayland_surface_destroy_window (surface);
  xdg_toplevel->resource = NULL;
}

static void
xdg_toplevel_destroy (struct wl_client   *client,
                      struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
xdg_toplevel_set_parent (struct wl_client   *client,
                         struct wl_resource *resource,
                         struct wl_resource *parent_resource)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);
  MetaWindow *transient_for = NULL;

  if (parent_resource)
    {
      MetaWaylandSurface *parent_surface =
        surface_from_xdg_surface_resource (parent_resource);

      transient_for = parent_surface->window;
    }

  meta_window_set_transient_for (surface->window, transient_for);
}

static void
xdg_toplevel_set_title (struct wl_client   *client,
                        struct wl_resource *resource,
                        const char         *title)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  if (!g_utf8_validate (title, -1, NULL))
    title = "";

  meta_window_set_title (surface->window, title);
}

static void
xdg_toplevel_set_app_id (struct wl_client   *client,
                         struct wl_resource *resource,
                         const char         *app_id)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  if (!g_utf8_validate (app_id, -1, NULL))
    app_id = "";

  meta_window_set_wm_class (surface->window, app_id, app_id);
}

static void
xdg_toplevel_show_window_menu (struct wl_client   *client,
                               struct wl_resource *resource,
                               struct wl_resource *seat_resource,
                               uint32_t            serial,
                               int32_t             x,
                               int32_t             y)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);
  int monitor_scale;

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, FALSE, NULL, NULL))
    return;

  monitor_scale = surface->window->monitor->scale;
  meta_window_show_menu (surface->window, META_WINDOW_MENU_WM,
                         surface->window->buffer_rect.x + (x * monitor_scale),
                         surface->window->buffer_rect.y + (y * monitor_scale));
}

static void
xdg_toplevel_move (struct wl_client   *client,
                   struct wl_resource *resource,
                   struct wl_resource *seat_resource,
                   uint32_t            serial)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);
  float x, y;

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, TRUE, &x, &y))
    return;

  meta_wayland_surface_begin_grab_op (surface, seat, META_GRAB_OP_MOVING, x, y);
}

static MetaGrabOp
grab_op_for_xdg_toplevel_resize_edge (int edge)
{
  MetaGrabOp op = META_GRAB_OP_WINDOW_BASE;

  if (edge & XDG_TOPLEVEL_RESIZE_EDGE_TOP)
    op |= META_GRAB_OP_WINDOW_DIR_NORTH;
  if (edge & XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM)
    op |= META_GRAB_OP_WINDOW_DIR_SOUTH;
  if (edge & XDG_TOPLEVEL_RESIZE_EDGE_LEFT)
    op |= META_GRAB_OP_WINDOW_DIR_WEST;
  if (edge & XDG_TOPLEVEL_RESIZE_EDGE_RIGHT)
    op |= META_GRAB_OP_WINDOW_DIR_EAST;

  if (op == META_GRAB_OP_WINDOW_BASE)
    {
      g_warning ("invalid edge: %d", edge);
      return META_GRAB_OP_NONE;
    }

  return op;
}

static void
xdg_toplevel_resize (struct wl_client   *client,
                     struct wl_resource *resource,
                     struct wl_resource *seat_resource,
                     uint32_t            serial,
                     uint32_t            edges)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);
  gfloat x, y;
  MetaGrabOp grab_op;

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, TRUE, &x, &y))
    return;

  grab_op = grab_op_for_xdg_toplevel_resize_edge (edges);
  meta_wayland_surface_begin_grab_op (surface, seat, grab_op, x, y);
}

static void
xdg_toplevel_set_max_size (struct wl_client   *client,
                           struct wl_resource *resource,
                           int32_t             width,
                           int32_t             height)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  if (width < 0 || height < 0)
    {
      wl_resource_post_error (resource,
                              XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE,
                              "invalid negative max size requested %i x %i",
                              width, height);
      return;
    }

  surface->pending->has_new_max_size = TRUE;
  surface->pending->new_max_width = width;
  surface->pending->new_max_height = height;
}

static void
xdg_toplevel_set_min_size (struct wl_client   *client,
                           struct wl_resource *resource,
                           int32_t             width,
                           int32_t             height)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  if (width < 0 || height < 0)
    {
      wl_resource_post_error (resource,
                              XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE,
                              "invalid negative min size requested %i x %i",
                              width, height);
      return;
    }

  surface->pending->has_new_min_size = TRUE;
  surface->pending->new_min_width = width;
  surface->pending->new_min_height = height;
}

static void
xdg_toplevel_set_maximized (struct wl_client   *client,
                            struct wl_resource *resource)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);
  MetaWindow *window = surface->window;

  meta_window_force_placement (window, TRUE);
  meta_window_maximize (window, META_MAXIMIZE_BOTH);
}

static void
xdg_toplevel_unset_maximized (struct wl_client   *client,
                              struct wl_resource *resource)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  meta_window_unmaximize (surface->window, META_MAXIMIZE_BOTH);
}

static void
xdg_toplevel_set_fullscreen (struct wl_client   *client,
                             struct wl_resource *resource,
                             struct wl_resource *output_resource)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  if (output_resource)
    {
      MetaWaylandOutput *output = wl_resource_get_user_data (output_resource);

      if (output)
        {
          meta_window_move_to_monitor (surface->window,
                                       output->logical_monitor->number);
        }
    }

  meta_window_make_fullscreen (surface->window);
}

static void
xdg_toplevel_unset_fullscreen (struct wl_client   *client,
                               struct wl_resource *resource)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  meta_window_unmake_fullscreen (surface->window);
}

static void
xdg_toplevel_set_minimized (struct wl_client   *client,
                            struct wl_resource *resource)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  meta_window_minimize (surface->window);
}

static const struct xdg_toplevel_interface meta_wayland_xdg_toplevel_interface = {
  xdg_toplevel_destroy,
  xdg_toplevel_set_parent,
  xdg_toplevel_set_title,
  xdg_toplevel_set_app_id,
  xdg_toplevel_show_window_menu,
  xdg_toplevel_move,
  xdg_toplevel_resize,
  xdg_toplevel_set_max_size,
  xdg_toplevel_set_min_size,
  xdg_toplevel_set_maximized,
  xdg_toplevel_unset_maximized,
  xdg_toplevel_set_fullscreen,
  xdg_toplevel_unset_fullscreen,
  xdg_toplevel_set_minimized,
};

static void
meta_wayland_xdg_popup_unmap (MetaWaylandXdgPopup *xdg_popup)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (xdg_popup);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  g_assert (!xdg_popup->popup);

  if (xdg_popup->parent_surface)
    {
      g_signal_handler_disconnect (xdg_popup->parent_surface,
                                   xdg_popup->parent_surface_unmapped_handler_id);
      xdg_popup->parent_surface = NULL;
    }

  meta_wayland_surface_destroy_window (surface);
}

static void
xdg_popup_destructor (struct wl_resource *resource)
{
  MetaWaylandXdgPopup *xdg_popup =
    META_WAYLAND_XDG_POPUP (wl_resource_get_user_data (resource));

  if (xdg_popup->popup)
    meta_wayland_popup_dismiss (xdg_popup->popup);
  else
    meta_wayland_xdg_popup_unmap (xdg_popup);

  xdg_popup->resource = NULL;
}

static void
xdg_popup_destroy (struct wl_client   *client,
                   struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
xdg_popup_grab (struct wl_client   *client,
                struct wl_resource *resource,
                struct wl_resource *seat_resource,
                uint32_t            serial)
{
  MetaWaylandXdgPopup *xdg_popup =
    META_WAYLAND_XDG_POPUP (wl_resource_get_user_data (resource));
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *parent_surface;

  parent_surface = xdg_popup->setup.parent_surface;
  if (!parent_surface)
    {
      wl_resource_post_error (resource,
                              XDG_POPUP_ERROR_INVALID_GRAB,
                              "tried to grab after popup was mapped");
      return;
    }

  xdg_popup->setup.grab_seat = seat;
  xdg_popup->setup.grab_serial = serial;
}

static const struct xdg_popup_interface meta_wayland_xdg_popup_interface = {
  xdg_popup_destroy,
  xdg_popup_grab,
};

static void
on_parent_surface_unmapped (MetaWaylandSurface  *parent_surface,
                            MetaWaylandXdgPopup *xdg_popup)
{
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (xdg_popup);
  struct wl_resource *xdg_wm_base_resource =
    meta_wayland_xdg_surface_get_wm_base_resource (xdg_surface);
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (xdg_popup);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  wl_resource_post_error (xdg_wm_base_resource,
                          XDG_WM_BASE_ERROR_NOT_THE_TOPMOST_POPUP,
                          "destroyed popup not top most popup");
  xdg_popup->parent_surface = NULL;

  meta_wayland_surface_destroy_window (surface);
}

static void
fill_states (struct wl_array *states,
             MetaWindow      *window)
{
  uint32_t *s;

  if (META_WINDOW_MAXIMIZED (window))
    {
      s = wl_array_add (states, sizeof *s);
      *s = XDG_TOPLEVEL_STATE_MAXIMIZED;
    }
  if (meta_window_is_fullscreen (window))
    {
      s = wl_array_add (states, sizeof *s);
      *s = XDG_TOPLEVEL_STATE_FULLSCREEN;
    }
  if (meta_grab_op_is_resizing (window->display->grab_op))
    {
      s = wl_array_add (states, sizeof *s);
      *s = XDG_TOPLEVEL_STATE_RESIZING;
    }
  if (meta_window_appears_focused (window))
    {
      s = wl_array_add (states, sizeof *s);
      *s = XDG_TOPLEVEL_STATE_ACTIVATED;
    }
}

static void
meta_wayland_xdg_toplevel_send_configure (MetaWaylandXdgToplevel *xdg_toplevel,
                                          int                     new_width,
                                          int                     new_height,
                                          MetaWaylandSerial      *sent_serial)
{
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (xdg_toplevel);
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (xdg_toplevel);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  struct wl_array states;
  uint32_t serial;

  wl_array_init (&states);
  fill_states (&states, surface->window);

  xdg_toplevel_send_configure (xdg_toplevel->resource,
                               new_width, new_height,
                               &states);
  wl_array_release (&states);

  serial = meta_wayland_xdg_surface_send_configure (xdg_surface);

  if (sent_serial)
    {
      sent_serial->set = TRUE;
      sent_serial->value = serial;
    }
}

static gboolean
is_new_size_hints_valid (MetaWindow              *window,
                         MetaWaylandPendingState *pending)
{
  int new_min_width, new_min_height;
  int new_max_width, new_max_height;

  if (pending->has_new_min_size)
    {
      new_min_width = pending->new_min_width;
      new_min_height = pending->new_min_height;
    }
  else
    {
      meta_window_wayland_get_min_size (window, &new_min_width, &new_min_height);
    }

  if (pending->has_new_max_size)
    {
      new_max_width = pending->new_max_width;
      new_max_height = pending->new_max_height;
    }
  else
    {
      meta_window_wayland_get_max_size (window, &new_max_width, &new_max_height);
    }
  /* Zero means unlimited */
  return ((new_max_width == 0 || new_min_width <= new_max_width) &&
          (new_max_height == 0 || new_min_height <= new_max_height));
}

static void
meta_wayland_xdg_toplevel_commit (MetaWaylandSurfaceRole  *surface_role,
                                  MetaWaylandPendingState *pending)
{
  MetaWaylandXdgToplevel *xdg_toplevel = META_WAYLAND_XDG_TOPLEVEL (surface_role);
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (xdg_toplevel);
  MetaWaylandXdgSurfacePrivate *xdg_surface_priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWindow *window;
  MetaRectangle window_geometry;

  if (!surface->buffer_ref.buffer && xdg_surface_priv->first_buffer_attached)
    {
      meta_wayland_xdg_surface_reset (xdg_surface);
      return;
    }

  window = surface->window;

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_xdg_toplevel_parent_class);
  surface_role_class->commit (surface_role, pending);

  if (!xdg_surface_priv->configure_sent)
    {
      meta_wayland_xdg_toplevel_send_configure (xdg_toplevel, 0, 0, NULL);
      return;
    }

  if (!pending->newly_attached)
    return;

  if (pending->has_new_geometry)
    {
      window_geometry = meta_wayland_xdg_surface_get_window_geometry (xdg_surface);
      meta_window_wayland_move_resize (window,
                                       &xdg_surface_priv->acked_configure_serial,
                                       window_geometry,
                                       pending->dx, pending->dy);
    }
  else if (pending->dx != 0 || pending->dx != 0)
    {
      g_warning ("XXX: Attach-initiated move without a new geometry. "
                 "This is unimplemented right now.");
    }

  /* When we get to this point, we ought to have valid size hints */
  if (pending->has_new_min_size || pending->has_new_max_size)
    {
      if (is_new_size_hints_valid (window, pending))
        {
          if (pending->has_new_min_size)
            meta_window_wayland_set_min_size (window,
                                              pending->new_min_width,
                                              pending->new_min_height);

          if (pending->has_new_max_size)
            meta_window_wayland_set_max_size (window,
                                              pending->new_max_width,
                                              pending->new_max_height);

          meta_window_recalc_features (window);
        }
      else
        {
          wl_resource_post_error (surface->resource,
                                  XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE,
                                  "Invalid min/max size");

        }
    }

  xdg_surface_priv->acked_configure_serial.set = FALSE;
}

static MetaWaylandSurface *
meta_wayland_xdg_toplevel_get_toplevel (MetaWaylandSurfaceRole *surface_role)
{
  return meta_wayland_surface_role_get_surface (surface_role);
}

static void
meta_wayland_xdg_toplevel_reset (MetaWaylandXdgSurface *xdg_surface)
{
  MetaWaylandShellSurface *shell_surface =
    META_WAYLAND_SHELL_SURFACE (xdg_surface);
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (xdg_surface);
  MetaWaylandXdgSurfaceClass *xdg_surface_class =
    META_WAYLAND_XDG_SURFACE_CLASS (meta_wayland_xdg_toplevel_parent_class);
  MetaWaylandSurface *surface;
  MetaWindow *window;

  surface = meta_wayland_surface_role_get_surface (surface_role);

  meta_wayland_surface_destroy_window (surface);

  meta_wayland_actor_surface_reset_actor (META_WAYLAND_ACTOR_SURFACE (surface_role));
  window = meta_window_wayland_new (meta_get_display (), surface);
  meta_wayland_shell_surface_set_window (shell_surface, window);

  xdg_surface_class->reset (xdg_surface);
}

static void
meta_wayland_xdg_toplevel_configure (MetaWaylandShellSurface *shell_surface,
                                     int                      new_x,
                                     int                      new_y,
                                     int                      new_width,
                                     int                      new_height,
                                     MetaWaylandSerial       *sent_serial)
{
  MetaWaylandXdgToplevel *xdg_toplevel =
    META_WAYLAND_XDG_TOPLEVEL (shell_surface);
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (xdg_toplevel);
  MetaWaylandXdgSurfacePrivate *xdg_surface_priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);

  if (!xdg_surface_priv->resource)
    return;

  if (!xdg_toplevel->resource)
    return;

  meta_wayland_xdg_toplevel_send_configure (xdg_toplevel,
                                            new_width, new_height,
                                            sent_serial);
}

static void
meta_wayland_xdg_toplevel_managed (MetaWaylandShellSurface *shell_surface,
                                   MetaWindow              *window)
{
}

static void
meta_wayland_xdg_toplevel_close (MetaWaylandShellSurface *shell_surface)
{
  MetaWaylandXdgToplevel *xdg_toplevel =
    META_WAYLAND_XDG_TOPLEVEL (shell_surface);

  xdg_toplevel_send_close (xdg_toplevel->resource);
}

static void
meta_wayland_xdg_toplevel_shell_client_destroyed (MetaWaylandXdgSurface *xdg_surface)
{
  MetaWaylandXdgToplevel *xdg_toplevel =
    META_WAYLAND_XDG_TOPLEVEL (xdg_surface);
  struct wl_resource *xdg_wm_base_resource =
    meta_wayland_xdg_surface_get_wm_base_resource (xdg_surface);
  MetaWaylandXdgSurfaceClass *xdg_surface_class =
    META_WAYLAND_XDG_SURFACE_CLASS (meta_wayland_xdg_toplevel_parent_class);

  xdg_surface_class->shell_client_destroyed (xdg_surface);

  if (xdg_toplevel->resource)
    {
      wl_resource_post_error (xdg_wm_base_resource,
                              XDG_WM_BASE_ERROR_DEFUNCT_SURFACES,
                              "xdg_wm_base of xdg_toplevel@%d was destroyed",
                              wl_resource_get_id (xdg_toplevel->resource));

      wl_resource_destroy (xdg_toplevel->resource);
    }
}

static void
meta_wayland_xdg_toplevel_finalize (GObject *object)
{
  MetaWaylandXdgToplevel *xdg_toplevel = META_WAYLAND_XDG_TOPLEVEL (object);

  g_clear_pointer (&xdg_toplevel->resource, wl_resource_destroy);

  G_OBJECT_CLASS (meta_wayland_xdg_toplevel_parent_class)->finalize (object);
}

static void
meta_wayland_xdg_toplevel_init (MetaWaylandXdgToplevel *role)
{
}

static void
meta_wayland_xdg_toplevel_class_init (MetaWaylandXdgToplevelClass *klass)
{
  GObjectClass *object_class;
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandShellSurfaceClass *shell_surface_class;
  MetaWaylandXdgSurfaceClass *xdg_surface_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = meta_wayland_xdg_toplevel_finalize;

  surface_role_class = META_WAYLAND_SURFACE_ROLE_CLASS (klass);
  surface_role_class->commit = meta_wayland_xdg_toplevel_commit;
  surface_role_class->get_toplevel = meta_wayland_xdg_toplevel_get_toplevel;

  shell_surface_class = META_WAYLAND_SHELL_SURFACE_CLASS (klass);
  shell_surface_class->configure = meta_wayland_xdg_toplevel_configure;
  shell_surface_class->managed = meta_wayland_xdg_toplevel_managed;
  shell_surface_class->close = meta_wayland_xdg_toplevel_close;

  xdg_surface_class = META_WAYLAND_XDG_SURFACE_CLASS (klass);
  xdg_surface_class->shell_client_destroyed =
    meta_wayland_xdg_toplevel_shell_client_destroyed;
  xdg_surface_class->reset = meta_wayland_xdg_toplevel_reset;
}

static void
scale_placement_rule (MetaPlacementRule  *placement_rule,
                      MetaWaylandSurface *surface)
{
  int geometry_scale;

  geometry_scale = meta_window_wayland_get_geometry_scale (surface->window);

  placement_rule->anchor_rect.x *= geometry_scale;
  placement_rule->anchor_rect.y *= geometry_scale;
  placement_rule->anchor_rect.width *= geometry_scale;
  placement_rule->anchor_rect.height *= geometry_scale;
  placement_rule->offset_x *= geometry_scale;
  placement_rule->offset_y *= geometry_scale;
  placement_rule->width *= geometry_scale;
  placement_rule->height *= geometry_scale;
}

static void
finish_popup_setup (MetaWaylandXdgPopup *xdg_popup)
{
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (xdg_popup);
  MetaWaylandShellSurface *shell_surface =
    META_WAYLAND_SHELL_SURFACE (xdg_surface);
  MetaWaylandSurfaceRole *surface_role = META_WAYLAND_SURFACE_ROLE (xdg_popup);
  struct wl_resource *xdg_wm_base_resource =
    meta_wayland_xdg_surface_get_wm_base_resource (xdg_surface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandSurface *parent_surface;
  MetaPlacementRule scaled_placement_rule;
  MetaWaylandSeat *seat;
  uint32_t serial;
  MetaDisplay *display = meta_get_display ();
  MetaWindow *window;

  parent_surface = xdg_popup->setup.parent_surface;
  seat = xdg_popup->setup.grab_seat;
  serial = xdg_popup->setup.grab_serial;

  xdg_popup->setup.parent_surface = NULL;
  xdg_popup->setup.grab_seat = NULL;

  if (!parent_surface->window)
    {
      xdg_popup_send_popup_done (xdg_popup->resource);
      return;
    }

  if (seat)
    {
      MetaWaylandSurface *top_popup;

      if (!meta_wayland_seat_can_popup (seat, serial))
        {
          xdg_popup_send_popup_done (xdg_popup->resource);
          return;
        }

      top_popup = meta_wayland_pointer_get_top_popup (seat->pointer);
      if (top_popup && parent_surface != top_popup)
        {
          wl_resource_post_error (xdg_wm_base_resource,
                                  XDG_WM_BASE_ERROR_NOT_THE_TOPMOST_POPUP,
                                  "parent not top most surface");
          return;
        }
    }

  xdg_popup->parent_surface = parent_surface;
  xdg_popup->parent_surface_unmapped_handler_id =
    g_signal_connect (parent_surface, "unmapped",
                      G_CALLBACK (on_parent_surface_unmapped),
                      xdg_popup);

  window = meta_window_wayland_new (display, surface);
  meta_wayland_shell_surface_set_window (shell_surface, window);

  scaled_placement_rule = xdg_popup->setup.placement_rule;
  scale_placement_rule (&scaled_placement_rule, surface);
  meta_window_place_with_placement_rule (window, &scaled_placement_rule);

  if (seat)
    {
      MetaWaylandPopupSurface *popup_surface;
      MetaWaylandPopup *popup;

      meta_window_focus (window, meta_display_get_current_time (display));
      popup_surface = META_WAYLAND_POPUP_SURFACE (surface->role);
      popup = meta_wayland_pointer_start_popup_grab (seat->pointer,
                                                     popup_surface);
      if (popup == NULL)
        {
          xdg_popup_send_popup_done (xdg_popup->resource);
          meta_wayland_surface_destroy_window (surface);
          return;
        }

      xdg_popup->popup = popup;
    }
  else
    {
      /* The keyboard focus semantics for non-grabbing xdg_wm_base popups
       * is pretty undefined. Same applies for subsurfaces, but in practice,
       * subsurfaces never receive keyboard focus, so it makes sense to
       * do the same for non-grabbing popups.
       *
       * See https://bugzilla.gnome.org/show_bug.cgi?id=771694#c24
       */
      window->input = FALSE;
    }
}

static void
meta_wayland_xdg_popup_commit (MetaWaylandSurfaceRole  *surface_role,
                               MetaWaylandPendingState *pending)
{
  MetaWaylandXdgPopup *xdg_popup = META_WAYLAND_XDG_POPUP (surface_role);
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (surface_role);
  MetaWaylandXdgSurfacePrivate *xdg_surface_priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaRectangle window_geometry;

  if (xdg_popup->setup.parent_surface)
    finish_popup_setup (xdg_popup);

  if (!surface->buffer_ref.buffer && xdg_surface_priv->first_buffer_attached)
    {
      meta_wayland_xdg_surface_reset (xdg_surface);
      return;
    }

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_xdg_popup_parent_class);
  surface_role_class->commit (surface_role, pending);

  if (xdg_popup->dismissed_by_client && surface->buffer_ref.buffer)
    {
      wl_resource_post_error (xdg_popup->resource,
                              XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE,
                              "Can't commit buffer to dismissed popup");
      return;
    }

  /* If the window disappeared the surface is not coming back. */
  if (!surface->window)
    return;

  if (!pending->newly_attached)
    return;

  if (!surface->buffer_ref.buffer)
    return;

  window_geometry = meta_wayland_xdg_surface_get_window_geometry (xdg_surface);
  meta_window_wayland_move_resize (surface->window,
                                   NULL,
                                   window_geometry,
                                   pending->dx, pending->dy);
}

static MetaWaylandSurface *
meta_wayland_xdg_popup_get_toplevel (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandXdgPopup *xdg_popup = META_WAYLAND_XDG_POPUP (surface_role);

  if (xdg_popup->parent_surface)
    return meta_wayland_surface_get_toplevel (xdg_popup->parent_surface);
  else
    return NULL;
}

static void
meta_wayland_xdg_popup_reset (MetaWaylandXdgSurface *xdg_surface)
{
  MetaWaylandXdgPopup *xdg_popup = META_WAYLAND_XDG_POPUP (xdg_surface);
  MetaWaylandXdgSurfaceClass *xdg_surface_class =
    META_WAYLAND_XDG_SURFACE_CLASS (meta_wayland_xdg_popup_parent_class);

  if (xdg_popup->popup)
    meta_wayland_popup_dismiss (xdg_popup->popup);
  else
    meta_wayland_xdg_popup_unmap (xdg_popup);

  xdg_popup->dismissed_by_client = TRUE;

  xdg_surface_class->reset (xdg_surface);
}

static void
meta_wayland_xdg_popup_configure (MetaWaylandShellSurface *shell_surface,
                                  int                      new_x,
                                  int                      new_y,
                                  int                      new_width,
                                  int                      new_height,
                                  MetaWaylandSerial       *sent_serial)
{
  MetaWaylandXdgPopup *xdg_popup = META_WAYLAND_XDG_POPUP (shell_surface);
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (xdg_popup);
  MetaWindow *parent_window = xdg_popup->parent_surface->window;
  int geometry_scale;
  int x, y;

  /* If the parent surface was destroyed, its window will be destroyed
   * before the popup receives the parent-destroy signal. This means that
   * the popup may potentially get temporary focus until itself is destroyed.
   * If this happen, don't try to configure the xdg_popup surface.
   *
   * FIXME: Could maybe add a signal that is emitted before the window is
   * created so that we can avoid incorrect intermediate foci.
   */
  if (!parent_window)
    return;

  geometry_scale = meta_window_wayland_get_geometry_scale (parent_window);
  x = (new_x - parent_window->rect.x) / geometry_scale;
  y = (new_y - parent_window->rect.y) / geometry_scale;
  xdg_popup_send_configure (xdg_popup->resource,
                            x, y, new_width, new_height);
  meta_wayland_xdg_surface_send_configure (xdg_surface);
}

static void
meta_wayland_xdg_popup_managed (MetaWaylandShellSurface *shell_surface,
                                MetaWindow              *window)
{
  MetaWaylandXdgPopup *xdg_popup = META_WAYLAND_XDG_POPUP (shell_surface);
  MetaWaylandSurface *parent = xdg_popup->parent_surface;

  g_assert (parent);

  meta_window_set_transient_for (window, parent->window);
  meta_window_set_type (window, META_WINDOW_DROPDOWN_MENU);
}

static void
meta_wayland_xdg_popup_shell_client_destroyed (MetaWaylandXdgSurface *xdg_surface)
{
  MetaWaylandXdgPopup *xdg_popup = META_WAYLAND_XDG_POPUP (xdg_surface);
  struct wl_resource *xdg_wm_base_resource =
    meta_wayland_xdg_surface_get_wm_base_resource (xdg_surface);
  MetaWaylandXdgSurfaceClass *xdg_surface_class =
    META_WAYLAND_XDG_SURFACE_CLASS (meta_wayland_xdg_popup_parent_class);

  xdg_surface_class->shell_client_destroyed (xdg_surface);

  if (xdg_popup->resource)
    {
      wl_resource_post_error (xdg_wm_base_resource,
                              XDG_WM_BASE_ERROR_DEFUNCT_SURFACES,
                              "xdg_wm_base of xdg_popup@%d was destroyed",
                              wl_resource_get_id (xdg_popup->resource));

      wl_resource_destroy (xdg_popup->resource);
    }
}

static void
meta_wayland_xdg_popup_done (MetaWaylandPopupSurface *popup_surface)
{
  MetaWaylandXdgPopup *xdg_popup = META_WAYLAND_XDG_POPUP (popup_surface);

  xdg_popup_send_popup_done (xdg_popup->resource);
}

static void
meta_wayland_xdg_popup_dismiss (MetaWaylandPopupSurface *popup_surface)
{
  MetaWaylandXdgPopup *xdg_popup = META_WAYLAND_XDG_POPUP (popup_surface);
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (xdg_popup);
  struct wl_resource *xdg_wm_base_resource =
    meta_wayland_xdg_surface_get_wm_base_resource (xdg_surface);
  MetaWaylandSurfaceRole *surface_role = META_WAYLAND_SURFACE_ROLE (xdg_popup);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandSurface *top_popup;

  top_popup = meta_wayland_popup_get_top_popup (xdg_popup->popup);
  if (surface != top_popup)
    {
      wl_resource_post_error (xdg_wm_base_resource,
                              XDG_WM_BASE_ERROR_NOT_THE_TOPMOST_POPUP,
                              "destroyed popup not top most popup");
    }

  xdg_popup->popup = NULL;
  meta_wayland_xdg_popup_unmap (xdg_popup);
}

static MetaWaylandSurface *
meta_wayland_xdg_popup_get_surface (MetaWaylandPopupSurface *popup_surface)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (popup_surface);

  return meta_wayland_surface_role_get_surface (surface_role);
}

static void
popup_surface_iface_init (MetaWaylandPopupSurfaceInterface *iface)
{
  iface->done = meta_wayland_xdg_popup_done;
  iface->dismiss = meta_wayland_xdg_popup_dismiss;
  iface->get_surface = meta_wayland_xdg_popup_get_surface;
}

static void
meta_wayland_xdg_popup_finalize (GObject *object)
{
  MetaWaylandXdgPopup *xdg_popup = META_WAYLAND_XDG_POPUP (object);

  g_clear_pointer (&xdg_popup->resource, wl_resource_destroy);

  G_OBJECT_CLASS (meta_wayland_xdg_popup_parent_class)->finalize (object);
}

static void
meta_wayland_xdg_popup_init (MetaWaylandXdgPopup *role)
{
}

static void
meta_wayland_xdg_popup_class_init (MetaWaylandXdgPopupClass *klass)
{
  GObjectClass *object_class;
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandShellSurfaceClass *shell_surface_class;
  MetaWaylandXdgSurfaceClass *xdg_surface_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = meta_wayland_xdg_popup_finalize;

  surface_role_class = META_WAYLAND_SURFACE_ROLE_CLASS (klass);
  surface_role_class->commit = meta_wayland_xdg_popup_commit;
  surface_role_class->get_toplevel = meta_wayland_xdg_popup_get_toplevel;

  shell_surface_class = META_WAYLAND_SHELL_SURFACE_CLASS (klass);
  shell_surface_class->configure = meta_wayland_xdg_popup_configure;
  shell_surface_class->managed = meta_wayland_xdg_popup_managed;

  xdg_surface_class = META_WAYLAND_XDG_SURFACE_CLASS (klass);
  xdg_surface_class->shell_client_destroyed =
    meta_wayland_xdg_popup_shell_client_destroyed;
  xdg_surface_class->reset = meta_wayland_xdg_popup_reset;
}

static struct wl_resource *
meta_wayland_xdg_surface_get_wm_base_resource (MetaWaylandXdgSurface *xdg_surface)
{
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);

  return priv->shell_client->resource;
}

static MetaRectangle
meta_wayland_xdg_surface_get_window_geometry (MetaWaylandXdgSurface *xdg_surface)
{
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);

  return priv->geometry;
}

static gboolean
meta_wayland_xdg_surface_is_assigned (MetaWaylandXdgSurface *xdg_surface)
{
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);

  return priv->resource != NULL;
}

static uint32_t
meta_wayland_xdg_surface_send_configure (MetaWaylandXdgSurface *xdg_surface)
{
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);
  struct wl_display *display;
  uint32_t serial;

  display = wl_client_get_display (wl_resource_get_client (priv->resource));
  serial = wl_display_next_serial (display);
  xdg_surface_send_configure (priv->resource, serial);

  priv->configure_sent = TRUE;

  return serial;
}

static void
xdg_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = surface_from_xdg_surface_resource (resource);
  MetaWaylandXdgSurface *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);

  meta_wayland_compositor_destroy_frame_callbacks (surface->compositor,
                                                   surface);

  priv->shell_client->surfaces = g_list_remove (priv->shell_client->surfaces,
                                                xdg_surface);

  priv->resource = NULL;
  priv->first_buffer_attached = FALSE;
}

static void
xdg_surface_destroy (struct wl_client   *client,
                     struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
xdg_surface_get_toplevel (struct wl_client   *client,
                          struct wl_resource *resource,
                          uint32_t            id)
{
  MetaWaylandXdgSurface *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = surface_from_xdg_surface_resource (resource);
  struct wl_resource *xdg_wm_base_resource =
    meta_wayland_xdg_surface_get_wm_base_resource (xdg_surface);

  wl_resource_post_error (xdg_wm_base_resource, XDG_WM_BASE_ERROR_ROLE,
                          "wl_surface@%d already has a role assigned",
                          wl_resource_get_id (surface->resource));
}

static void
xdg_surface_get_popup (struct wl_client   *client,
                       struct wl_resource *resource,
                       uint32_t            id,
                       struct wl_resource *parent_resource,
                       struct wl_resource *positioner_resource)
{
  MetaWaylandXdgSurface *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);
  MetaWaylandSurface *surface = surface_from_xdg_surface_resource (resource);

  wl_resource_post_error (priv->shell_client->resource,
                          XDG_WM_BASE_ERROR_ROLE,
                          "wl_surface@%d already has a role assigned",
                          wl_resource_get_id (surface->resource));
}

static void
xdg_surface_set_window_geometry (struct wl_client   *client,
                                 struct wl_resource *resource,
                                 int32_t             x,
                                 int32_t             y,
                                 int32_t             width,
                                 int32_t             height)
{
  MetaWaylandSurface *surface = surface_from_xdg_surface_resource (resource);

  surface->pending->has_new_geometry = TRUE;
  surface->pending->new_geometry.x = x;
  surface->pending->new_geometry.y = y;
  surface->pending->new_geometry.width = width;
  surface->pending->new_geometry.height = height;
}

static void
xdg_surface_ack_configure (struct wl_client   *client,
                           struct wl_resource *resource,
                           uint32_t            serial)
{
  MetaWaylandXdgSurface *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);

  priv->acked_configure_serial.set = TRUE;
  priv->acked_configure_serial.value = serial;
}

static const struct xdg_surface_interface meta_wayland_xdg_surface_interface = {
  xdg_surface_destroy,
  xdg_surface_get_toplevel,
  xdg_surface_get_popup,
  xdg_surface_set_window_geometry,
  xdg_surface_ack_configure,
};

static void
meta_wayland_xdg_surface_finalize (GObject *object)
{
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (object);
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);

  g_clear_pointer (&priv->resource, wl_resource_destroy);

  G_OBJECT_CLASS (meta_wayland_xdg_surface_parent_class)->finalize (object);
}

static void
meta_wayland_xdg_surface_real_reset (MetaWaylandXdgSurface *xdg_surface)
{
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);

  priv->first_buffer_attached = FALSE;
  priv->configure_sent = FALSE;
  priv->geometry = (MetaRectangle) { 0 };
  priv->has_set_geometry = FALSE;
}

static void
meta_wayland_xdg_surface_commit (MetaWaylandSurfaceRole  *surface_role,
                                 MetaWaylandPendingState *pending)
{
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (surface_role);
  MetaWaylandShellSurface *shell_surface =
    META_WAYLAND_SHELL_SURFACE (xdg_surface);
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWindow *window = surface->window;
  MetaWaylandSurfaceRoleClass *surface_role_class;

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_xdg_surface_parent_class);
  surface_role_class->commit (surface_role, pending);

  /* Ignore commits when unassigned. */
  if (!priv->resource)
    return;

  if (!window)
    return;

  if (surface->buffer_ref.buffer)
    priv->first_buffer_attached = TRUE;
  else
    return;

  if (pending->has_new_geometry)
    {
      /* If we have new geometry, use it. */
      priv->geometry = pending->new_geometry;
      priv->has_set_geometry = TRUE;
    }
  else if (!priv->has_set_geometry)
    {
      MetaRectangle new_geometry = { 0 };

      /* If the surface has never set any geometry, calculate
       * a default one unioning the surface and all subsurfaces together. */

      meta_wayland_shell_surface_calculate_geometry (shell_surface,
                                                     &new_geometry);
      if (!meta_rectangle_equal (&new_geometry, &priv->geometry))
        {
          pending->has_new_geometry = TRUE;
          priv->geometry = new_geometry;
        }
    }
}

static void
meta_wayland_xdg_surface_assigned (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (surface_role);
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  struct wl_resource *xdg_wm_base_resource =
    meta_wayland_xdg_surface_get_wm_base_resource (xdg_surface);
  MetaWaylandSurfaceRoleClass *surface_role_class;

  priv->configure_sent = FALSE;
  priv->first_buffer_attached = FALSE;

  if (surface->buffer_ref.buffer)
    {
      wl_resource_post_error (xdg_wm_base_resource,
                              XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE,
                              "wl_surface@%d already has a buffer committed",
                              wl_resource_get_id (surface->resource));
      return;
    }

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_xdg_surface_parent_class);
  surface_role_class->assigned (surface_role);
}

static void
meta_wayland_xdg_surface_ping (MetaWaylandShellSurface *shell_surface,
                               uint32_t                 serial)
{
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (shell_surface);
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);

  xdg_wm_base_send_ping (priv->shell_client->resource, serial);
}

static void
meta_wayland_xdg_surface_real_shell_client_destroyed (MetaWaylandXdgSurface *xdg_surface)
{
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);

  if (priv->resource)
    {
      wl_resource_post_error (priv->shell_client->resource,
                              XDG_WM_BASE_ERROR_DEFUNCT_SURFACES,
                              "xdg_wm_base of xdg_surface@%d was destroyed",
                              wl_resource_get_id (priv->resource));

      wl_resource_destroy (priv->resource);
    }
}

static void
meta_wayland_xdg_surface_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (object);
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);

  switch (prop_id)
    {
    case XDG_SURFACE_PROP_SHELL_CLIENT:
      priv->shell_client  = g_value_get_pointer (value);
      break;

    case XDG_SURFACE_PROP_RESOURCE:
      priv->resource  = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_wayland_xdg_surface_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  MetaWaylandXdgSurface *xdg_surface = META_WAYLAND_XDG_SURFACE (object);
  MetaWaylandXdgSurfacePrivate *priv =
    meta_wayland_xdg_surface_get_instance_private (xdg_surface);

  switch (prop_id)
    {
    case XDG_SURFACE_PROP_SHELL_CLIENT:
      g_value_set_pointer (value, priv->shell_client);
      break;

    case XDG_SURFACE_PROP_RESOURCE:
      g_value_set_pointer (value, priv->resource);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_wayland_xdg_surface_init (MetaWaylandXdgSurface *xdg_surface)
{
}

static void
meta_wayland_xdg_surface_class_init (MetaWaylandXdgSurfaceClass *klass)
{
  GObjectClass *object_class;
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandShellSurfaceClass *shell_surface_class;
  GParamSpec *pspec;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = meta_wayland_xdg_surface_finalize;
  object_class->set_property = meta_wayland_xdg_surface_set_property;
  object_class->get_property = meta_wayland_xdg_surface_get_property;

  surface_role_class = META_WAYLAND_SURFACE_ROLE_CLASS (klass);
  surface_role_class->commit = meta_wayland_xdg_surface_commit;
  surface_role_class->assigned = meta_wayland_xdg_surface_assigned;

  shell_surface_class = META_WAYLAND_SHELL_SURFACE_CLASS (klass);
  shell_surface_class->ping = meta_wayland_xdg_surface_ping;

  klass->shell_client_destroyed =
    meta_wayland_xdg_surface_real_shell_client_destroyed;
  klass->reset = meta_wayland_xdg_surface_real_reset;

  pspec = g_param_spec_pointer ("shell-client",
                                "MetaWaylandXdgShellClient",
                                "The shell client instance",
                                G_PARAM_READWRITE |
                                G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
                                   XDG_SURFACE_PROP_SHELL_CLIENT,
                                   pspec);
  pspec = g_param_spec_pointer ("xdg-surface-resource",
                                "xdg_surface wl_resource",
                                "The xdg_surface wl_resource instance",
                                G_PARAM_READWRITE |
                                G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
                                   XDG_SURFACE_PROP_RESOURCE,
                                   pspec);
}

static void
meta_wayland_xdg_surface_shell_client_destroyed (MetaWaylandXdgSurface *xdg_surface)
{
  MetaWaylandXdgSurfaceClass *xdg_surface_class =
    META_WAYLAND_XDG_SURFACE_GET_CLASS (xdg_surface);

  xdg_surface_class->shell_client_destroyed (xdg_surface);
}

static void
meta_wayland_xdg_surface_constructor_finalize (MetaWaylandXdgSurfaceConstructor *constructor,
                                               MetaWaylandXdgSurface            *xdg_surface)
{
  MetaWaylandXdgShellClient *shell_client = constructor->shell_client;

  shell_client->surface_constructors =
    g_list_remove (shell_client->surface_constructors, constructor);
  shell_client->surfaces = g_list_append (shell_client->surfaces, xdg_surface);

  wl_resource_set_implementation (constructor->resource,
                                  &meta_wayland_xdg_surface_interface,
                                  xdg_surface,
                                  xdg_surface_destructor);

  g_free (constructor);
}

static void
xdg_surface_constructor_destroy (struct wl_client   *client,
                                 struct wl_resource *resource)
{
  wl_resource_post_error (resource,
                          XDG_SURFACE_ERROR_NOT_CONSTRUCTED,
                          "xdg_surface destroyed before constructed");
  wl_resource_destroy (resource);
}

static void
xdg_surface_constructor_get_toplevel (struct wl_client   *client,
                                      struct wl_resource *resource,
                                      uint32_t            id)
{
  MetaWaylandXdgSurfaceConstructor *constructor =
    wl_resource_get_user_data (resource);
  MetaWaylandXdgShellClient *shell_client = constructor->shell_client;
  struct wl_resource *xdg_surface_resource = constructor->resource;
  MetaWaylandSurface *surface = constructor->surface;
  MetaWaylandXdgToplevel *xdg_toplevel;
  MetaWaylandXdgSurface *xdg_surface;
  MetaWaylandShellSurface *shell_surface;
  MetaWindow *window;

  if (!meta_wayland_surface_assign_role (surface,
                                         META_TYPE_WAYLAND_XDG_TOPLEVEL,
                                         "shell-client", shell_client,
                                         "xdg-surface-resource", xdg_surface_resource,
                                         NULL))
    {
      wl_resource_post_error (resource, XDG_WM_BASE_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  xdg_toplevel = META_WAYLAND_XDG_TOPLEVEL (surface->role);
  xdg_toplevel->resource = wl_resource_create (client,
                                               &xdg_toplevel_interface,
                                               wl_resource_get_version (resource),
                                               id);
  wl_resource_set_implementation (xdg_toplevel->resource,
                                  &meta_wayland_xdg_toplevel_interface,
                                  xdg_toplevel,
                                  xdg_toplevel_destructor);

  xdg_surface = META_WAYLAND_XDG_SURFACE (xdg_toplevel);
  meta_wayland_xdg_surface_constructor_finalize (constructor, xdg_surface);

  window = meta_window_wayland_new (meta_get_display (), surface);
  shell_surface = META_WAYLAND_SHELL_SURFACE (xdg_surface);
  meta_wayland_shell_surface_set_window (shell_surface, window);
}

static void
xdg_surface_constructor_get_popup (struct wl_client   *client,
                                   struct wl_resource *resource,
                                   uint32_t            id,
                                   struct wl_resource *parent_resource,
                                   struct wl_resource *positioner_resource)
{
  MetaWaylandXdgSurfaceConstructor *constructor =
    wl_resource_get_user_data (resource);
  MetaWaylandXdgShellClient *shell_client = constructor->shell_client;
  MetaWaylandSurface *surface = constructor->surface;
  struct wl_resource *xdg_wm_base_resource = constructor->shell_client->resource;
  struct wl_resource *xdg_surface_resource = constructor->resource;
  MetaWaylandSurface *parent_surface =
    surface_from_xdg_surface_resource (parent_resource);
  MetaWaylandXdgPositioner *xdg_positioner;
  MetaWaylandXdgPopup *xdg_popup;
  MetaWaylandXdgSurface *xdg_surface;

  if (!meta_wayland_surface_assign_role (surface,
                                         META_TYPE_WAYLAND_XDG_POPUP,
                                         "shell-client", shell_client,
                                         "xdg-surface-resource", xdg_surface_resource,
                                         NULL))
    {
      wl_resource_post_error (xdg_wm_base_resource, XDG_WM_BASE_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  if (!META_IS_WAYLAND_XDG_SURFACE (parent_surface->role))
    {
      wl_resource_post_error (xdg_wm_base_resource,
                              XDG_WM_BASE_ERROR_INVALID_POPUP_PARENT,
                              "Invalid popup parent role");
      return;
    }

  xdg_popup = META_WAYLAND_XDG_POPUP (surface->role);

  xdg_popup->resource = wl_resource_create (client,
                                            &xdg_popup_interface,
                                            wl_resource_get_version (resource),
                                            id);
  wl_resource_set_implementation (xdg_popup->resource,
                                  &meta_wayland_xdg_popup_interface,
                                  xdg_popup,
                                  xdg_popup_destructor);

  xdg_surface = META_WAYLAND_XDG_SURFACE (xdg_popup);
  meta_wayland_xdg_surface_constructor_finalize (constructor, xdg_surface);

  xdg_positioner = wl_resource_get_user_data (positioner_resource);
  xdg_popup->setup.placement_rule =
    meta_wayland_xdg_positioner_to_placement (xdg_positioner);
  xdg_popup->setup.parent_surface = parent_surface;
}

static void
xdg_surface_constructor_set_window_geometry (struct wl_client   *client,
                                             struct wl_resource *resource,
                                             int32_t             x,
                                             int32_t             y,
                                             int32_t             width,
                                             int32_t             height)
{
  wl_resource_post_error (resource,
                          XDG_SURFACE_ERROR_NOT_CONSTRUCTED,
                          "xdg_surface::set_window_geometry called before constructed");
}

static void
xdg_surface_constructor_ack_configure (struct wl_client   *client,
                                       struct wl_resource *resource,
                                       uint32_t            serial)
{
  wl_resource_post_error (resource,
                          XDG_SURFACE_ERROR_NOT_CONSTRUCTED,
                          "xdg_surface::ack_configure called before constructed");
}

static const struct xdg_surface_interface meta_wayland_xdg_surface_constructor_interface = {
  xdg_surface_constructor_destroy,
  xdg_surface_constructor_get_toplevel,
  xdg_surface_constructor_get_popup,
  xdg_surface_constructor_set_window_geometry,
  xdg_surface_constructor_ack_configure,
};

static void
xdg_surface_constructor_destructor (struct wl_resource *resource)
{
  MetaWaylandXdgSurfaceConstructor *constructor =
    wl_resource_get_user_data (resource);

  constructor->shell_client->surface_constructors =
    g_list_remove (constructor->shell_client->surface_constructors,
                   constructor);

  g_free (constructor);
}

static MetaPlacementAnchor
positioner_anchor_to_placement_anchor (uint32_t anchor)
{
  switch (anchor)
    {
    case XDG_POSITIONER_ANCHOR_NONE:
      return META_PLACEMENT_ANCHOR_NONE;
    case XDG_POSITIONER_ANCHOR_TOP:
      return META_PLACEMENT_ANCHOR_TOP;
    case XDG_POSITIONER_ANCHOR_BOTTOM:
      return META_PLACEMENT_ANCHOR_BOTTOM;
    case XDG_POSITIONER_ANCHOR_LEFT:
      return META_PLACEMENT_ANCHOR_LEFT;
    case XDG_POSITIONER_ANCHOR_RIGHT:
      return META_PLACEMENT_ANCHOR_RIGHT;
    case XDG_POSITIONER_ANCHOR_TOP_LEFT:
      return (META_PLACEMENT_ANCHOR_TOP | META_PLACEMENT_ANCHOR_LEFT);
    case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT:
      return (META_PLACEMENT_ANCHOR_BOTTOM | META_PLACEMENT_ANCHOR_LEFT);
    case XDG_POSITIONER_ANCHOR_TOP_RIGHT:
      return (META_PLACEMENT_ANCHOR_TOP | META_PLACEMENT_ANCHOR_RIGHT);
    case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
      return (META_PLACEMENT_ANCHOR_BOTTOM | META_PLACEMENT_ANCHOR_RIGHT);
    default:
      g_assert_not_reached ();
    }
}

static MetaPlacementGravity
positioner_gravity_to_placement_gravity (uint32_t gravity)
{
  switch (gravity)
    {
    case XDG_POSITIONER_GRAVITY_NONE:
      return META_PLACEMENT_GRAVITY_NONE;
    case XDG_POSITIONER_GRAVITY_TOP:
      return META_PLACEMENT_GRAVITY_TOP;
    case XDG_POSITIONER_GRAVITY_BOTTOM:
      return META_PLACEMENT_GRAVITY_BOTTOM;
    case XDG_POSITIONER_GRAVITY_LEFT:
      return META_PLACEMENT_GRAVITY_LEFT;
    case XDG_POSITIONER_GRAVITY_RIGHT:
      return META_PLACEMENT_GRAVITY_RIGHT;
    case XDG_POSITIONER_GRAVITY_TOP_LEFT:
      return (META_PLACEMENT_GRAVITY_TOP | META_PLACEMENT_GRAVITY_LEFT);
    case XDG_POSITIONER_GRAVITY_BOTTOM_LEFT:
      return (META_PLACEMENT_GRAVITY_BOTTOM | META_PLACEMENT_GRAVITY_LEFT);
    case XDG_POSITIONER_GRAVITY_TOP_RIGHT:
      return (META_PLACEMENT_GRAVITY_TOP | META_PLACEMENT_GRAVITY_RIGHT);
    case XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT:
      return (META_PLACEMENT_GRAVITY_BOTTOM | META_PLACEMENT_GRAVITY_RIGHT);
    default:
      g_assert_not_reached ();
    }
}

static MetaPlacementRule
meta_wayland_xdg_positioner_to_placement (MetaWaylandXdgPositioner *xdg_positioner)
{
  return (MetaPlacementRule) {
    .anchor_rect = xdg_positioner->anchor_rect,
    .gravity = positioner_gravity_to_placement_gravity (xdg_positioner->gravity),
    .anchor = positioner_anchor_to_placement_anchor (xdg_positioner->anchor),
    .constraint_adjustment = xdg_positioner->constraint_adjustment,
    .offset_x = xdg_positioner->offset_x,
    .offset_y = xdg_positioner->offset_y,
    .width = xdg_positioner->width,
    .height = xdg_positioner->height,
  };
}

static void
xdg_positioner_destroy (struct wl_client   *client,
                        struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
xdg_positioner_set_size (struct wl_client   *client,
                         struct wl_resource *resource,
                         int32_t             width,
                         int32_t             height)
{
  MetaWaylandXdgPositioner *positioner = wl_resource_get_user_data (resource);

  if (width <= 0 || height <= 0)
    {
      wl_resource_post_error (resource, XDG_POSITIONER_ERROR_INVALID_INPUT,
                              "Invalid size");
      return;
    }

  positioner->width = width;
  positioner->height = height;
}

static void
xdg_positioner_set_anchor_rect (struct wl_client   *client,
                                struct wl_resource *resource,
                                int32_t             x,
                                int32_t             y,
                                int32_t             width,
                                int32_t             height)
{
  MetaWaylandXdgPositioner *positioner = wl_resource_get_user_data (resource);

  if (width <= 0 || height <= 0)
    {
      wl_resource_post_error (resource, XDG_POSITIONER_ERROR_INVALID_INPUT,
                              "Invalid anchor rectangle size");
      return;
    }

  positioner->anchor_rect = (MetaRectangle) {
    .x = x,
    .y = y,
    .width = width,
    .height = height,
  };
}

static void
xdg_positioner_set_anchor (struct wl_client   *client,
                           struct wl_resource *resource,
                           uint32_t            anchor)
{
  MetaWaylandXdgPositioner *positioner = wl_resource_get_user_data (resource);

  if (anchor > XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT)
    {
      wl_resource_post_error (resource, XDG_POSITIONER_ERROR_INVALID_INPUT,
                              "Invalid anchor");
      return;
    }

  positioner->anchor = anchor;
}

static void
xdg_positioner_set_gravity (struct wl_client   *client,
                            struct wl_resource *resource,
                            uint32_t            gravity)
{
  MetaWaylandXdgPositioner *positioner = wl_resource_get_user_data (resource);

  if (gravity > XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
    {
      wl_resource_post_error (resource, XDG_POSITIONER_ERROR_INVALID_INPUT,
                              "Invalid gravity");
      return;
    }

  positioner->gravity = gravity;
}

static void
xdg_positioner_set_constraint_adjustment (struct wl_client   *client,
                                          struct wl_resource *resource,
                                          uint32_t            constraint_adjustment)
{
  MetaWaylandXdgPositioner *positioner = wl_resource_get_user_data (resource);
  uint32_t all_adjustments = (XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X |
                              XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X |
                              XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y |
                              XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y |
                              XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X |
                              XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y);

  if ((constraint_adjustment & ~all_adjustments) != 0)
    {
      wl_resource_post_error (resource, XDG_POSITIONER_ERROR_INVALID_INPUT,
                              "Invalid constraint action");
      return;
    }

  positioner->constraint_adjustment = constraint_adjustment;
}

static void
xdg_positioner_set_offset (struct wl_client   *client,
                           struct wl_resource *resource,
                           int32_t             x,
                           int32_t             y)
{
  MetaWaylandXdgPositioner *positioner = wl_resource_get_user_data (resource);

  positioner->offset_x = x;
  positioner->offset_y = y;
}

static const struct xdg_positioner_interface meta_wayland_xdg_positioner_interface = {
  xdg_positioner_destroy,
  xdg_positioner_set_size,
  xdg_positioner_set_anchor_rect,
  xdg_positioner_set_anchor,
  xdg_positioner_set_gravity,
  xdg_positioner_set_constraint_adjustment,
  xdg_positioner_set_offset,
};

static void
xdg_positioner_destructor (struct wl_resource *resource)
{
  MetaWaylandXdgPositioner *positioner = wl_resource_get_user_data (resource);

  g_free (positioner);
}

static void
xdg_wm_base_destroy (struct wl_client   *client,
                     struct wl_resource *resource)
{
  MetaWaylandXdgShellClient *shell_client = wl_resource_get_user_data (resource);

  if (shell_client->surfaces || shell_client->surface_constructors)
    wl_resource_post_error (resource, XDG_WM_BASE_ERROR_DEFUNCT_SURFACES,
                            "xdg_wm_base destroyed before its surfaces");

  wl_resource_destroy (resource);
}

static void
xdg_wm_base_create_positioner (struct wl_client   *client,
                               struct wl_resource *resource,
                               uint32_t            id)
{
  MetaWaylandXdgPositioner *positioner;
  struct wl_resource *positioner_resource;

  positioner = g_new0 (MetaWaylandXdgPositioner, 1);
  positioner_resource = wl_resource_create (client,
                                            &xdg_positioner_interface,
                                            wl_resource_get_version (resource),
                                            id);
  wl_resource_set_implementation (positioner_resource,
                                  &meta_wayland_xdg_positioner_interface,
                                  positioner,
                                  xdg_positioner_destructor);
}

static void
xdg_wm_base_get_xdg_surface (struct wl_client   *client,
                             struct wl_resource *resource,
                             uint32_t            id,
                             struct wl_resource *surface_resource)
{
  MetaWaylandXdgShellClient *shell_client = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandXdgSurfaceConstructor *constructor;

  if (surface->role && !META_IS_WAYLAND_XDG_SURFACE (surface->role))
    {
      wl_resource_post_error (resource, XDG_WM_BASE_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  if (surface->role && META_IS_WAYLAND_XDG_SURFACE (surface->role) &&
      meta_wayland_xdg_surface_is_assigned (META_WAYLAND_XDG_SURFACE (surface->role)))
    {
      wl_resource_post_error (surface_resource,
                              XDG_WM_BASE_ERROR_ROLE,
                              "xdg_wm_base::get_xdg_surface already requested");
      return;
    }

  if (surface->buffer_ref.buffer)
    {
      wl_resource_post_error (resource,
                              XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE,
                              "wl_surface@%d already has a buffer committed",
                              wl_resource_get_id (surface->resource));
      return;
    }

  constructor = g_new0 (MetaWaylandXdgSurfaceConstructor, 1);
  constructor->surface = surface;
  constructor->shell_client = shell_client;
  constructor->resource = wl_resource_create (client,
                                              &xdg_surface_interface,
                                              wl_resource_get_version (resource),
                                              id);
  wl_resource_set_implementation (constructor->resource,
                                  &meta_wayland_xdg_surface_constructor_interface,
                                  constructor,
                                  xdg_surface_constructor_destructor);

  shell_client->surface_constructors =
    g_list_append (shell_client->surface_constructors, constructor);
}

static void
xdg_wm_base_pong (struct wl_client   *client,
                  struct wl_resource *resource,
                  uint32_t            serial)
{
  MetaDisplay *display = meta_get_display ();

  meta_display_pong_for_serial (display, serial);
}

static const struct xdg_wm_base_interface meta_wayland_xdg_wm_base_interface = {
  xdg_wm_base_destroy,
  xdg_wm_base_create_positioner,
  xdg_wm_base_get_xdg_surface,
  xdg_wm_base_pong,
};

static void
meta_wayland_xdg_shell_client_destroy (MetaWaylandXdgShellClient *shell_client)
{
  while (shell_client->surface_constructors)
    {
      MetaWaylandXdgSurfaceConstructor *constructor =
        g_list_first (shell_client->surface_constructors)->data;

      wl_resource_destroy (constructor->resource);
    }
  g_list_free (shell_client->surface_constructors);

  while (shell_client->surfaces)
    {
      MetaWaylandXdgSurface *xdg_surface =
        g_list_first (shell_client->surfaces)->data;

      meta_wayland_xdg_surface_shell_client_destroyed  (xdg_surface);
    }
  g_list_free (shell_client->surfaces);

  g_free (shell_client);
}

static void
xdg_wm_base_destructor (struct wl_resource *resource)
{
  MetaWaylandXdgShellClient *shell_client =
    wl_resource_get_user_data (resource);

  meta_wayland_xdg_shell_client_destroy (shell_client);
}

static void
bind_xdg_wm_base (struct wl_client *client,
                  void             *data,
                  uint32_t          version,
                  uint32_t          id)
{
  MetaWaylandXdgShellClient *shell_client;

  shell_client = g_new0 (MetaWaylandXdgShellClient, 1);

  shell_client->resource = wl_resource_create (client,
                                               &xdg_wm_base_interface,
                                               version, id);
  wl_resource_set_implementation (shell_client->resource,
                                  &meta_wayland_xdg_wm_base_interface,
                                  shell_client, xdg_wm_base_destructor);
}

void
meta_wayland_xdg_shell_init (MetaWaylandCompositor *compositor)
{
  if (wl_global_create (compositor->wayland_display,
                        &xdg_wm_base_interface,
                        META_XDG_WM_BASE_VERSION,
                        compositor, bind_xdg_wm_base) == NULL)
    g_error ("Failed to register a global xdg-shell object");
}
