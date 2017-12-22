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

#include "wayland/meta-wayland-legacy-xdg-shell.h"

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

#include "xdg-shell-unstable-v6-server-protocol.h"

enum
{
  ZXDG_SURFACE_V6_PROP_0,

  ZXDG_SURFACE_V6_PROP_SHELL_CLIENT,
  ZXDG_SURFACE_V6_PROP_RESOURCE,
};

typedef struct _MetaWaylandZxdgShellV6Client
{
  struct wl_resource *resource;
  GList *surfaces;
  GList *surface_constructors;
} MetaWaylandZxdgShellV6Client;

typedef struct _MetaWaylandZxdgPositionerV6
{
  MetaRectangle anchor_rect;
  int32_t width;
  int32_t height;
  uint32_t gravity;
  uint32_t anchor;
  uint32_t constraint_adjustment;
  int32_t offset_x;
  int32_t offset_y;
} MetaWaylandZxdgPositionerV6;

typedef struct _MetaWaylandZxdgSurfaceV6Constructor
{
  MetaWaylandSurface *surface;
  struct wl_resource *resource;
  MetaWaylandZxdgShellV6Client *shell_client;
} MetaWaylandZxdgSurfaceV6Constructor;

typedef struct _MetaWaylandZxdgSurfaceV6Private
{
  struct wl_resource *resource;
  MetaWaylandZxdgShellV6Client *shell_client;
  MetaWaylandSerial acked_configure_serial;
  MetaRectangle geometry;

  guint configure_sent : 1;
  guint first_buffer_attached : 1;
  guint has_set_geometry : 1;
} MetaWaylandZxdgSurfaceV6Private;

G_DEFINE_TYPE_WITH_PRIVATE (MetaWaylandZxdgSurfaceV6,
                            meta_wayland_zxdg_surface_v6,
                            META_TYPE_WAYLAND_SHELL_SURFACE);

struct _MetaWaylandZxdgToplevelV6
{
  MetaWaylandZxdgSurfaceV6 parent;

  struct wl_resource *resource;
};

G_DEFINE_TYPE (MetaWaylandZxdgToplevelV6,
               meta_wayland_zxdg_toplevel_v6,
               META_TYPE_WAYLAND_ZXDG_SURFACE_V6);

struct _MetaWaylandZxdgPopupV6
{
  MetaWaylandZxdgSurfaceV6 parent;

  struct wl_resource *resource;

  MetaWaylandSurface *parent_surface;
  struct wl_listener parent_destroy_listener;

  MetaWaylandPopup *popup;

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

G_DEFINE_TYPE_WITH_CODE (MetaWaylandZxdgPopupV6,
                         meta_wayland_zxdg_popup_v6,
                         META_TYPE_WAYLAND_ZXDG_SURFACE_V6,
                         G_IMPLEMENT_INTERFACE (META_TYPE_WAYLAND_POPUP_SURFACE,
                                                popup_surface_iface_init));

static MetaPlacementRule
meta_wayland_zxdg_positioner_v6_to_placement (MetaWaylandZxdgPositionerV6 *xdg_positioner);

static struct wl_resource *
meta_wayland_zxdg_surface_v6_get_shell_resource (MetaWaylandZxdgSurfaceV6 *xdg_surface);

static MetaRectangle
meta_wayland_zxdg_surface_v6_get_window_geometry (MetaWaylandZxdgSurfaceV6 *xdg_surface);

static uint32_t
meta_wayland_zxdg_surface_v6_send_configure (MetaWaylandZxdgSurfaceV6 *xdg_surface);

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
zxdg_toplevel_v6_destructor (struct wl_resource *resource)
{
  MetaWaylandZxdgToplevelV6 *xdg_toplevel =
    wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  meta_wayland_surface_destroy_window (surface);
  xdg_toplevel->resource = NULL;
}

static void
zxdg_toplevel_v6_destroy (struct wl_client   *client,
                          struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
zxdg_toplevel_v6_set_parent (struct wl_client   *client,
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
zxdg_toplevel_v6_set_title (struct wl_client   *client,
                            struct wl_resource *resource,
                            const char         *title)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  if (!g_utf8_validate (title, -1, NULL))
    title = "";

  meta_window_set_title (surface->window, title);
}

static void
zxdg_toplevel_v6_set_app_id (struct wl_client   *client,
                             struct wl_resource *resource,
                             const char         *app_id)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  if (!g_utf8_validate (app_id, -1, NULL))
    app_id = "";

  meta_window_set_wm_class (surface->window, app_id, app_id);
}

static void
zxdg_toplevel_v6_show_window_menu (struct wl_client   *client,
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
zxdg_toplevel_v6_move (struct wl_client   *client,
                       struct wl_resource *resource,
                       struct wl_resource *seat_resource,
                       uint32_t            serial)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);
  gfloat x, y;

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, TRUE, &x, &y))
    return;

  meta_wayland_surface_begin_grab_op (surface, seat, META_GRAB_OP_MOVING, x, y);
}

static MetaGrabOp
grab_op_for_xdg_toplevel_resize_edge (int edge)
{
  MetaGrabOp op = META_GRAB_OP_WINDOW_BASE;

  if (edge & ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP)
    op |= META_GRAB_OP_WINDOW_DIR_NORTH;
  if (edge & ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM)
    op |= META_GRAB_OP_WINDOW_DIR_SOUTH;
  if (edge & ZXDG_TOPLEVEL_V6_RESIZE_EDGE_LEFT)
    op |= META_GRAB_OP_WINDOW_DIR_WEST;
  if (edge & ZXDG_TOPLEVEL_V6_RESIZE_EDGE_RIGHT)
    op |= META_GRAB_OP_WINDOW_DIR_EAST;

  if (op == META_GRAB_OP_WINDOW_BASE)
    {
      g_warning ("invalid edge: %d", edge);
      return META_GRAB_OP_NONE;
    }

  return op;
}

static void
zxdg_toplevel_v6_resize (struct wl_client   *client,
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
zxdg_toplevel_v6_set_max_size (struct wl_client   *client,
                               struct wl_resource *resource,
                               int32_t             width,
                               int32_t             height)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  if (width < 0 || height < 0)
    {
      wl_resource_post_error (resource,
                              ZXDG_SHELL_V6_ERROR_INVALID_SURFACE_STATE,
                              "invalid negative max size requested %i x %i",
                              width, height);
      return;
    }

  surface->pending->has_new_max_size = TRUE;
  surface->pending->new_max_width = width;
  surface->pending->new_max_height = height;
}

static void
zxdg_toplevel_v6_set_min_size (struct wl_client   *client,
                               struct wl_resource *resource,
                               int32_t             width,
                               int32_t             height)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  if (width < 0 || height < 0)
    {
      wl_resource_post_error (resource,
                              ZXDG_SHELL_V6_ERROR_INVALID_SURFACE_STATE,
                              "invalid negative min size requested %i x %i",
                              width, height);
      return;
    }

  surface->pending->has_new_min_size = TRUE;
  surface->pending->new_min_width = width;
  surface->pending->new_min_height = height;
}

static void
zxdg_toplevel_v6_set_maximized (struct wl_client   *client,
                                struct wl_resource *resource)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  meta_window_force_placement (surface->window, TRUE);
  meta_window_maximize (surface->window, META_MAXIMIZE_BOTH);
}

static void
zxdg_toplevel_v6_unset_maximized (struct wl_client   *client,
                                  struct wl_resource *resource)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  meta_window_unmaximize (surface->window, META_MAXIMIZE_BOTH);
}

static void
zxdg_toplevel_v6_set_fullscreen (struct wl_client   *client,
                                 struct wl_resource *resource,
                                 struct wl_resource *output_resource)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  if (output_resource)
    {
      MetaWaylandOutput *output = wl_resource_get_user_data (output_resource);
      if (output)
        meta_window_move_to_monitor (surface->window, output->logical_monitor->number);
    }

  meta_window_make_fullscreen (surface->window);
}

static void
zxdg_toplevel_v6_unset_fullscreen (struct wl_client   *client,
                                   struct wl_resource *resource)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  meta_window_unmake_fullscreen (surface->window);
}

static void
zxdg_toplevel_v6_set_minimized (struct wl_client   *client,
                                struct wl_resource *resource)
{
  MetaWaylandSurface *surface = surface_from_xdg_toplevel_resource (resource);

  meta_window_minimize (surface->window);
}

static const struct zxdg_toplevel_v6_interface meta_wayland_zxdg_toplevel_v6_interface = {
  zxdg_toplevel_v6_destroy,
  zxdg_toplevel_v6_set_parent,
  zxdg_toplevel_v6_set_title,
  zxdg_toplevel_v6_set_app_id,
  zxdg_toplevel_v6_show_window_menu,
  zxdg_toplevel_v6_move,
  zxdg_toplevel_v6_resize,
  zxdg_toplevel_v6_set_max_size,
  zxdg_toplevel_v6_set_min_size,
  zxdg_toplevel_v6_set_maximized,
  zxdg_toplevel_v6_unset_maximized,
  zxdg_toplevel_v6_set_fullscreen,
  zxdg_toplevel_v6_unset_fullscreen,
  zxdg_toplevel_v6_set_minimized,
};

static void
zxdg_popup_v6_destructor (struct wl_resource *resource)
{
  MetaWaylandZxdgPopupV6 *xdg_popup =
    META_WAYLAND_ZXDG_POPUP_V6 (wl_resource_get_user_data (resource));

  if (xdg_popup->parent_surface)
    {
      wl_list_remove (&xdg_popup->parent_destroy_listener.link);
      xdg_popup->parent_surface = NULL;
    }

  if (xdg_popup->popup)
    meta_wayland_popup_dismiss (xdg_popup->popup);

  xdg_popup->resource = NULL;
}

static void
zxdg_popup_v6_destroy (struct wl_client   *client,
                       struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
zxdg_popup_v6_grab (struct wl_client   *client,
                    struct wl_resource *resource,
                    struct wl_resource *seat_resource,
                    uint32_t            serial)
{
  MetaWaylandZxdgPopupV6 *xdg_popup =
    META_WAYLAND_ZXDG_POPUP_V6 (wl_resource_get_user_data (resource));
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *parent_surface;

  parent_surface = xdg_popup->setup.parent_surface;
  if (!parent_surface)
    {
      wl_resource_post_error (resource,
                              ZXDG_POPUP_V6_ERROR_INVALID_GRAB,
                              "tried to grab after popup was mapped");
      return;
    }

  xdg_popup->setup.grab_seat = seat;
  xdg_popup->setup.grab_serial = serial;
}

static const struct zxdg_popup_v6_interface meta_wayland_zxdg_popup_v6_interface = {
  zxdg_popup_v6_destroy,
  zxdg_popup_v6_grab,
};

static void
handle_popup_parent_destroyed (struct wl_listener *listener,
                               void               *data)
{
  MetaWaylandZxdgPopupV6 *xdg_popup =
    wl_container_of (listener, xdg_popup, parent_destroy_listener);
  MetaWaylandZxdgSurfaceV6 *xdg_surface =
    META_WAYLAND_ZXDG_SURFACE_V6 (xdg_popup);
  struct wl_resource *xdg_shell_resource =
    meta_wayland_zxdg_surface_v6_get_shell_resource (xdg_surface);
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (xdg_popup);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  wl_resource_post_error (xdg_shell_resource,
                          ZXDG_SHELL_V6_ERROR_NOT_THE_TOPMOST_POPUP,
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
      *s = ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED;
    }
  if (meta_window_is_fullscreen (window))
    {
      s = wl_array_add (states, sizeof *s);
      *s = ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN;
    }
  if (meta_grab_op_is_resizing (window->display->grab_op))
    {
      s = wl_array_add (states, sizeof *s);
      *s = ZXDG_TOPLEVEL_V6_STATE_RESIZING;
    }
  if (meta_window_appears_focused (window))
    {
      s = wl_array_add (states, sizeof *s);
      *s = ZXDG_TOPLEVEL_V6_STATE_ACTIVATED;
    }
}

static void
meta_wayland_zxdg_toplevel_v6_send_configure (MetaWaylandZxdgToplevelV6 *xdg_toplevel,
                                              int                        new_width,
                                              int                        new_height,
                                              MetaWaylandSerial         *sent_serial)
{
  MetaWaylandZxdgSurfaceV6 *xdg_surface =
    META_WAYLAND_ZXDG_SURFACE_V6 (xdg_toplevel);
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (xdg_toplevel);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  struct wl_array states;
  uint32_t serial;

  wl_array_init (&states);
  fill_states (&states, surface->window);

  zxdg_toplevel_v6_send_configure (xdg_toplevel->resource,
                                   new_width, new_height,
                                   &states);
  wl_array_release (&states);

  serial = meta_wayland_zxdg_surface_v6_send_configure (xdg_surface);

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
meta_wayland_zxdg_toplevel_v6_commit (MetaWaylandSurfaceRole  *surface_role,
                                      MetaWaylandPendingState *pending)
{
  MetaWaylandZxdgToplevelV6 *xdg_toplevel =
    META_WAYLAND_ZXDG_TOPLEVEL_V6 (surface_role);
  MetaWaylandZxdgSurfaceV6 *xdg_surface =
    META_WAYLAND_ZXDG_SURFACE_V6 (xdg_toplevel);
  MetaWaylandZxdgSurfaceV6Private *xdg_surface_priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWindow *window = surface->window;
  MetaRectangle window_geometry;

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_zxdg_toplevel_v6_parent_class);
  surface_role_class->commit (surface_role, pending);

  if (!xdg_surface_priv->configure_sent)
    {
      meta_wayland_zxdg_toplevel_v6_send_configure (xdg_toplevel, 0, 0, NULL);
      return;
    }

  if (!pending->newly_attached)
    return;

  /* If the window disappeared the surface is not coming back. */
  if (!window)
    return;

  if (pending->has_new_geometry)
    {
      window_geometry =
        meta_wayland_zxdg_surface_v6_get_window_geometry (xdg_surface);
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
                                  ZXDG_SHELL_V6_ERROR_INVALID_SURFACE_STATE,
                                  "Invalid min/max size");

        }
    }

  xdg_surface_priv->acked_configure_serial.set = FALSE;
}

static MetaWaylandSurface *
meta_wayland_zxdg_toplevel_v6_get_toplevel (MetaWaylandSurfaceRole *surface_role)
{
  return meta_wayland_surface_role_get_surface (surface_role);
}

static void
meta_wayland_zxdg_toplevel_v6_configure (MetaWaylandShellSurface *shell_surface,
                                         int                      new_x,
                                         int                      new_y,
                                         int                      new_width,
                                         int                      new_height,
                                         MetaWaylandSerial       *sent_serial)
{
  MetaWaylandZxdgToplevelV6 *xdg_toplevel =
    META_WAYLAND_ZXDG_TOPLEVEL_V6 (shell_surface);
  MetaWaylandZxdgSurfaceV6 *xdg_surface =
    META_WAYLAND_ZXDG_SURFACE_V6 (xdg_toplevel);
  MetaWaylandZxdgSurfaceV6Private *xdg_surface_priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);

  if (!xdg_surface_priv->resource)
    return;

  if (!xdg_toplevel->resource)
    return;

  meta_wayland_zxdg_toplevel_v6_send_configure (xdg_toplevel,
                                                new_width, new_height,
                                                sent_serial);
}

static void
meta_wayland_zxdg_toplevel_v6_managed (MetaWaylandShellSurface *shell_surface,
                                       MetaWindow              *window)
{
}

static void
meta_wayland_zxdg_toplevel_v6_close (MetaWaylandShellSurface *shell_surface)
{
  MetaWaylandZxdgToplevelV6 *xdg_toplevel =
    META_WAYLAND_ZXDG_TOPLEVEL_V6 (shell_surface);

  zxdg_toplevel_v6_send_close (xdg_toplevel->resource);
}

static void
meta_wayland_zxdg_toplevel_v6_shell_client_destroyed (MetaWaylandZxdgSurfaceV6 *xdg_surface)
{
  MetaWaylandZxdgToplevelV6 *xdg_toplevel =
    META_WAYLAND_ZXDG_TOPLEVEL_V6 (xdg_surface);
  struct wl_resource *xdg_shell_resource =
    meta_wayland_zxdg_surface_v6_get_shell_resource (xdg_surface);
  MetaWaylandZxdgSurfaceV6Class *xdg_surface_class =
    META_WAYLAND_ZXDG_SURFACE_V6_CLASS (meta_wayland_zxdg_toplevel_v6_parent_class);

  xdg_surface_class->shell_client_destroyed (xdg_surface);

  if (xdg_toplevel->resource)
    {
      wl_resource_post_error (xdg_shell_resource,
                              ZXDG_SHELL_V6_ERROR_DEFUNCT_SURFACES,
                              "xdg_shell of xdg_toplevel@%d was destroyed",
                              wl_resource_get_id (xdg_toplevel->resource));

      wl_resource_destroy (xdg_toplevel->resource);
    }
}

static void
meta_wayland_zxdg_toplevel_v6_finalize (GObject *object)
{
  MetaWaylandZxdgToplevelV6 *xdg_toplevel =
    META_WAYLAND_ZXDG_TOPLEVEL_V6 (object);

  g_clear_pointer (&xdg_toplevel->resource, wl_resource_destroy);

  G_OBJECT_CLASS (meta_wayland_zxdg_toplevel_v6_parent_class)->finalize (object);
}

static void
meta_wayland_zxdg_toplevel_v6_init (MetaWaylandZxdgToplevelV6 *role)
{
}

static void
meta_wayland_zxdg_toplevel_v6_class_init (MetaWaylandZxdgToplevelV6Class *klass)
{
  GObjectClass *object_class;
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandShellSurfaceClass *shell_surface_class;
  MetaWaylandZxdgSurfaceV6Class *xdg_surface_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = meta_wayland_zxdg_toplevel_v6_finalize;

  surface_role_class = META_WAYLAND_SURFACE_ROLE_CLASS (klass);
  surface_role_class->commit = meta_wayland_zxdg_toplevel_v6_commit;
  surface_role_class->get_toplevel = meta_wayland_zxdg_toplevel_v6_get_toplevel;

  shell_surface_class = META_WAYLAND_SHELL_SURFACE_CLASS (klass);
  shell_surface_class->configure = meta_wayland_zxdg_toplevel_v6_configure;
  shell_surface_class->managed = meta_wayland_zxdg_toplevel_v6_managed;
  shell_surface_class->close = meta_wayland_zxdg_toplevel_v6_close;

  xdg_surface_class = META_WAYLAND_ZXDG_SURFACE_V6_CLASS (klass);
  xdg_surface_class->shell_client_destroyed =
    meta_wayland_zxdg_toplevel_v6_shell_client_destroyed;
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
finish_popup_setup (MetaWaylandZxdgPopupV6 *xdg_popup)
{
  MetaWaylandZxdgSurfaceV6 *xdg_surface =
    META_WAYLAND_ZXDG_SURFACE_V6 (xdg_popup);
  MetaWaylandShellSurface *shell_surface =
    META_WAYLAND_SHELL_SURFACE (xdg_popup);
  MetaWaylandSurfaceRole *surface_role = META_WAYLAND_SURFACE_ROLE (xdg_popup);
  struct wl_resource *xdg_shell_resource =
    meta_wayland_zxdg_surface_v6_get_shell_resource (xdg_surface);
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
      zxdg_popup_v6_send_popup_done (xdg_popup->resource);
      return;
    }

  if (seat)
    {
      MetaWaylandSurface *top_popup;

      if (!meta_wayland_seat_can_popup (seat, serial))
        {
          zxdg_popup_v6_send_popup_done (xdg_popup->resource);
          return;
        }

      top_popup = meta_wayland_pointer_get_top_popup (seat->pointer);
      if (top_popup && parent_surface != top_popup)
        {
          wl_resource_post_error (xdg_shell_resource,
                                  ZXDG_SHELL_V6_ERROR_NOT_THE_TOPMOST_POPUP,
                                  "parent not top most surface");
          return;
        }
    }

  xdg_popup->parent_surface = parent_surface;
  xdg_popup->parent_destroy_listener.notify = handle_popup_parent_destroyed;
  wl_resource_add_destroy_listener (parent_surface->resource,
                                    &xdg_popup->parent_destroy_listener);

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
          zxdg_popup_v6_send_popup_done (xdg_popup->resource);
          meta_wayland_surface_destroy_window (surface);
          return;
        }

      xdg_popup->popup = popup;
    }
  else
    {
      /* The keyboard focus semantics for non-grabbing zxdg_shell_v6 popups
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
meta_wayland_zxdg_popup_v6_commit (MetaWaylandSurfaceRole  *surface_role,
                                   MetaWaylandPendingState *pending)
{
  MetaWaylandZxdgPopupV6 *xdg_popup = META_WAYLAND_ZXDG_POPUP_V6 (surface_role);
  MetaWaylandZxdgSurfaceV6 *xdg_surface =
    META_WAYLAND_ZXDG_SURFACE_V6 (surface_role);
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaRectangle window_geometry;

  if (xdg_popup->setup.parent_surface)
    finish_popup_setup (xdg_popup);

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_zxdg_popup_v6_parent_class);
  surface_role_class->commit (surface_role, pending);

  /* If the window disappeared the surface is not coming back. */
  if (!surface->window)
    return;

  if (!pending->newly_attached)
    return;

  if (!surface->buffer_ref.buffer)
    return;

  window_geometry =
    meta_wayland_zxdg_surface_v6_get_window_geometry (xdg_surface);
  meta_window_wayland_move_resize (surface->window,
                                   NULL,
                                   window_geometry,
                                   pending->dx, pending->dy);
}

static MetaWaylandSurface *
meta_wayland_zxdg_popup_v6_get_toplevel (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandZxdgPopupV6 *xdg_popup = META_WAYLAND_ZXDG_POPUP_V6 (surface_role);

  if (xdg_popup->parent_surface)
    return meta_wayland_surface_get_toplevel (xdg_popup->parent_surface);
  else
    return NULL;
}

static void
meta_wayland_zxdg_popup_v6_configure (MetaWaylandShellSurface *shell_surface,
                                      int                      new_x,
                                      int                      new_y,
                                      int                      new_width,
                                      int                      new_height,
                                      MetaWaylandSerial       *sent_serial)
{
  MetaWaylandZxdgPopupV6 *xdg_popup =
    META_WAYLAND_ZXDG_POPUP_V6 (shell_surface);
  MetaWaylandZxdgSurfaceV6 *xdg_surface =
    META_WAYLAND_ZXDG_SURFACE_V6 (xdg_popup);
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
  zxdg_popup_v6_send_configure (xdg_popup->resource,
                                x, y, new_width, new_height);
  meta_wayland_zxdg_surface_v6_send_configure (xdg_surface);
}

static void
meta_wayland_zxdg_popup_v6_managed (MetaWaylandShellSurface *shell_surface,
                                    MetaWindow              *window)
{
  MetaWaylandZxdgPopupV6 *xdg_popup =
    META_WAYLAND_ZXDG_POPUP_V6 (shell_surface);
  MetaWaylandSurface *parent = xdg_popup->parent_surface;

  g_assert (parent);

  meta_window_set_transient_for (window, parent->window);
  meta_window_set_type (window, META_WINDOW_DROPDOWN_MENU);
}

static void
meta_wayland_zxdg_popup_v6_shell_client_destroyed (MetaWaylandZxdgSurfaceV6 *xdg_surface)
{
  MetaWaylandZxdgPopupV6 *xdg_popup = META_WAYLAND_ZXDG_POPUP_V6 (xdg_surface);
  struct wl_resource *xdg_shell_resource =
    meta_wayland_zxdg_surface_v6_get_shell_resource (xdg_surface);
  MetaWaylandZxdgSurfaceV6Class *xdg_surface_class =
    META_WAYLAND_ZXDG_SURFACE_V6_CLASS (meta_wayland_zxdg_popup_v6_parent_class);

  xdg_surface_class->shell_client_destroyed (xdg_surface);

  if (xdg_popup->resource)
    {
      wl_resource_post_error (xdg_shell_resource,
                              ZXDG_SHELL_V6_ERROR_DEFUNCT_SURFACES,
                              "xdg_shell of xdg_popup@%d was destroyed",
                              wl_resource_get_id (xdg_popup->resource));

      wl_resource_destroy (xdg_popup->resource);
    }
}

static void
meta_wayland_zxdg_popup_v6_done (MetaWaylandPopupSurface *popup_surface)
{
  MetaWaylandZxdgPopupV6 *xdg_popup = META_WAYLAND_ZXDG_POPUP_V6 (popup_surface);

  zxdg_popup_v6_send_popup_done (xdg_popup->resource);
}

static void
meta_wayland_zxdg_popup_v6_dismiss (MetaWaylandPopupSurface *popup_surface)
{
  MetaWaylandZxdgPopupV6 *xdg_popup =
    META_WAYLAND_ZXDG_POPUP_V6 (popup_surface);
  MetaWaylandZxdgSurfaceV6 *xdg_surface =
    META_WAYLAND_ZXDG_SURFACE_V6 (xdg_popup);
  struct wl_resource *xdg_shell_resource =
    meta_wayland_zxdg_surface_v6_get_shell_resource (xdg_surface);
  MetaWaylandSurfaceRole *surface_role = META_WAYLAND_SURFACE_ROLE (xdg_popup);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandSurface *top_popup;

  top_popup = meta_wayland_popup_get_top_popup (xdg_popup->popup);
  if (surface != top_popup)
    {
      wl_resource_post_error (xdg_shell_resource,
                              ZXDG_SHELL_V6_ERROR_NOT_THE_TOPMOST_POPUP,
                              "destroyed popup not top most popup");
    }

  xdg_popup->popup = NULL;

  meta_wayland_surface_destroy_window (surface);
}

static MetaWaylandSurface *
meta_wayland_zxdg_popup_v6_get_surface (MetaWaylandPopupSurface *popup_surface)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (popup_surface);

  return meta_wayland_surface_role_get_surface (surface_role);
}

static void
popup_surface_iface_init (MetaWaylandPopupSurfaceInterface *iface)
{
  iface->done = meta_wayland_zxdg_popup_v6_done;
  iface->dismiss = meta_wayland_zxdg_popup_v6_dismiss;
  iface->get_surface = meta_wayland_zxdg_popup_v6_get_surface;
}

static void
meta_wayland_zxdg_popup_v6_role_finalize (GObject *object)
{
  MetaWaylandZxdgPopupV6 *xdg_popup = META_WAYLAND_ZXDG_POPUP_V6 (object);

  g_clear_pointer (&xdg_popup->resource, wl_resource_destroy);

  G_OBJECT_CLASS (meta_wayland_zxdg_popup_v6_parent_class)->finalize (object);
}

static void
meta_wayland_zxdg_popup_v6_init (MetaWaylandZxdgPopupV6 *role)
{
}

static void
meta_wayland_zxdg_popup_v6_class_init (MetaWaylandZxdgPopupV6Class *klass)
{
  GObjectClass *object_class;
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandShellSurfaceClass *shell_surface_class;
  MetaWaylandZxdgSurfaceV6Class *xdg_surface_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = meta_wayland_zxdg_popup_v6_role_finalize;

  surface_role_class = META_WAYLAND_SURFACE_ROLE_CLASS (klass);
  surface_role_class->commit = meta_wayland_zxdg_popup_v6_commit;
  surface_role_class->get_toplevel = meta_wayland_zxdg_popup_v6_get_toplevel;

  shell_surface_class = META_WAYLAND_SHELL_SURFACE_CLASS (klass);
  shell_surface_class->configure = meta_wayland_zxdg_popup_v6_configure;
  shell_surface_class->managed = meta_wayland_zxdg_popup_v6_managed;

  xdg_surface_class = META_WAYLAND_ZXDG_SURFACE_V6_CLASS (klass);
  xdg_surface_class->shell_client_destroyed =
    meta_wayland_zxdg_popup_v6_shell_client_destroyed;
}

static struct wl_resource *
meta_wayland_zxdg_surface_v6_get_shell_resource (MetaWaylandZxdgSurfaceV6 *xdg_surface)
{
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);

  return priv->shell_client->resource;
}

static MetaRectangle
meta_wayland_zxdg_surface_v6_get_window_geometry (MetaWaylandZxdgSurfaceV6 *xdg_surface)
{
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);

  return priv->geometry;
}

static gboolean
meta_wayland_zxdg_surface_v6_is_assigned (MetaWaylandZxdgSurfaceV6 *xdg_surface)
{
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);

  return priv->resource != NULL;
}

static uint32_t
meta_wayland_zxdg_surface_v6_send_configure (MetaWaylandZxdgSurfaceV6 *xdg_surface)
{
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);
  struct wl_display *display;
  uint32_t serial;

  display = wl_client_get_display (wl_resource_get_client (priv->resource));
  serial = wl_display_next_serial (display);
  zxdg_surface_v6_send_configure (priv->resource, serial);

  priv->configure_sent = TRUE;

  return serial;
}

static void
zxdg_surface_v6_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = surface_from_xdg_surface_resource (resource);
  MetaWaylandZxdgSurfaceV6 *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);

  meta_wayland_compositor_destroy_frame_callbacks (surface->compositor,
                                                   surface);

  priv->shell_client->surfaces = g_list_remove (priv->shell_client->surfaces,
                                                xdg_surface);

  priv->resource = NULL;
  priv->first_buffer_attached = FALSE;
}

static void
zxdg_surface_v6_destroy (struct wl_client   *client,
                         struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
zxdg_surface_v6_get_toplevel (struct wl_client   *client,
                              struct wl_resource *resource,
                              uint32_t            id)
{
  MetaWaylandZxdgSurfaceV6 *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = surface_from_xdg_surface_resource (resource);
  struct wl_resource *xdg_shell_resource =
    meta_wayland_zxdg_surface_v6_get_shell_resource (xdg_surface);

  wl_resource_post_error (xdg_shell_resource, ZXDG_SHELL_V6_ERROR_ROLE,
                          "wl_surface@%d already has a role assigned",
                          wl_resource_get_id (surface->resource));
}

static void
zxdg_surface_v6_get_popup (struct wl_client   *client,
                           struct wl_resource *resource,
                           uint32_t            id,
                           struct wl_resource *parent_resource,
                           struct wl_resource *positioner_resource)
{
  MetaWaylandZxdgSurfaceV6 *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);
  MetaWaylandSurface *surface = surface_from_xdg_surface_resource (resource);

  wl_resource_post_error (priv->shell_client->resource,
                          ZXDG_SHELL_V6_ERROR_ROLE,
                          "wl_surface@%d already has a role assigned",
                          wl_resource_get_id (surface->resource));
}

static void
zxdg_surface_v6_set_window_geometry (struct wl_client   *client,
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
zxdg_surface_v6_ack_configure (struct wl_client   *client,
                               struct wl_resource *resource,
                               uint32_t            serial)
{
  MetaWaylandZxdgSurfaceV6 *xdg_surface = wl_resource_get_user_data (resource);
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);

  priv->acked_configure_serial.set = TRUE;
  priv->acked_configure_serial.value = serial;
}

static const struct zxdg_surface_v6_interface meta_wayland_zxdg_surface_v6_interface = {
  zxdg_surface_v6_destroy,
  zxdg_surface_v6_get_toplevel,
  zxdg_surface_v6_get_popup,
  zxdg_surface_v6_set_window_geometry,
  zxdg_surface_v6_ack_configure,
};

static void
meta_wayland_zxdg_surface_v6_finalize (GObject *object)
{
  MetaWaylandZxdgSurfaceV6 *xdg_surface = META_WAYLAND_ZXDG_SURFACE_V6 (object);
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);

  g_clear_pointer (&priv->resource, wl_resource_destroy);

  G_OBJECT_CLASS (meta_wayland_zxdg_surface_v6_parent_class)->finalize (object);
}

static void
meta_wayland_zxdg_surface_v6_commit (MetaWaylandSurfaceRole  *surface_role,
                                     MetaWaylandPendingState *pending)
{
  MetaWaylandZxdgSurfaceV6 *xdg_surface =
    META_WAYLAND_ZXDG_SURFACE_V6 (surface_role);
  MetaWaylandShellSurface *shell_surface =
    META_WAYLAND_SHELL_SURFACE (xdg_surface);
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWindow *window = surface->window;
  MetaWaylandSurfaceRoleClass *surface_role_class;

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_zxdg_surface_v6_parent_class);
  surface_role_class->commit (surface_role, pending);

  /* Ignore commits when unassigned. */
  if (!priv->resource)
    return;

  if (surface->buffer_ref.buffer == NULL && priv->first_buffer_attached)
    {
      /* XDG surfaces can't commit NULL buffers */
      wl_resource_post_error (surface->resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "Cannot commit a NULL buffer to an xdg_surface");
      return;
    }

  if (surface->buffer_ref.buffer && !priv->configure_sent)
    {
      wl_resource_post_error (surface->resource,
                              ZXDG_SURFACE_V6_ERROR_UNCONFIGURED_BUFFER,
                              "buffer committed to unconfigured xdg_surface");
      return;
    }

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
meta_wayland_zxdg_surface_v6_assigned (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandZxdgSurfaceV6 *xdg_surface =
    META_WAYLAND_ZXDG_SURFACE_V6 (surface_role);
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  struct wl_resource *xdg_shell_resource =
    meta_wayland_zxdg_surface_v6_get_shell_resource (xdg_surface);
  MetaWaylandSurfaceRoleClass *surface_role_class;

  priv->configure_sent = FALSE;
  priv->first_buffer_attached = FALSE;

  if (surface->buffer_ref.buffer)
    {
      wl_resource_post_error (xdg_shell_resource,
                              ZXDG_SHELL_V6_ERROR_INVALID_SURFACE_STATE,
                              "wl_surface@%d already has a buffer committed",
                              wl_resource_get_id (surface->resource));
      return;
    }

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_zxdg_surface_v6_parent_class);
  surface_role_class->assigned (surface_role);
}

static void
meta_wayland_zxdg_surface_v6_ping (MetaWaylandShellSurface *shell_surface,
                                   uint32_t                 serial)
{
  MetaWaylandZxdgSurfaceV6 *xdg_surface =
    META_WAYLAND_ZXDG_SURFACE_V6 (shell_surface);
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);

  zxdg_shell_v6_send_ping (priv->shell_client->resource, serial);
}

static void
meta_wayland_zxdg_surface_v6_real_shell_client_destroyed (MetaWaylandZxdgSurfaceV6 *xdg_surface)
{
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);

  if (priv->resource)
    {
      wl_resource_post_error (priv->shell_client->resource,
                              ZXDG_SHELL_V6_ERROR_DEFUNCT_SURFACES,
                              "xdg_shell of xdg_surface@%d was destroyed",
                              wl_resource_get_id (priv->resource));

      wl_resource_destroy (priv->resource);
    }
}

static void
meta_wayland_zxdg_surface_v6_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  MetaWaylandZxdgSurfaceV6 *xdg_surface = META_WAYLAND_ZXDG_SURFACE_V6 (object);
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);

  switch (prop_id)
    {
    case ZXDG_SURFACE_V6_PROP_SHELL_CLIENT:
      priv->shell_client  = g_value_get_pointer (value);
      break;

    case ZXDG_SURFACE_V6_PROP_RESOURCE:
      priv->resource  = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_wayland_zxdg_surface_v6_get_property (GObject      *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  MetaWaylandZxdgSurfaceV6 *xdg_surface = META_WAYLAND_ZXDG_SURFACE_V6 (object);
  MetaWaylandZxdgSurfaceV6Private *priv =
    meta_wayland_zxdg_surface_v6_get_instance_private (xdg_surface);

  switch (prop_id)
    {
    case ZXDG_SURFACE_V6_PROP_SHELL_CLIENT:
      g_value_set_pointer (value, priv->shell_client);
      break;

    case ZXDG_SURFACE_V6_PROP_RESOURCE:
      g_value_set_pointer (value, priv->resource);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_wayland_zxdg_surface_v6_init (MetaWaylandZxdgSurfaceV6 *xdg_surface)
{
}

static void
meta_wayland_zxdg_surface_v6_class_init (MetaWaylandZxdgSurfaceV6Class *klass)
{
  GObjectClass *object_class;
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandShellSurfaceClass *shell_surface_class;
  GParamSpec *pspec;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = meta_wayland_zxdg_surface_v6_finalize;
  object_class->set_property = meta_wayland_zxdg_surface_v6_set_property;
  object_class->get_property = meta_wayland_zxdg_surface_v6_get_property;

  surface_role_class = META_WAYLAND_SURFACE_ROLE_CLASS (klass);
  surface_role_class->commit = meta_wayland_zxdg_surface_v6_commit;
  surface_role_class->assigned = meta_wayland_zxdg_surface_v6_assigned;

  shell_surface_class = META_WAYLAND_SHELL_SURFACE_CLASS (klass);
  shell_surface_class->ping = meta_wayland_zxdg_surface_v6_ping;

  klass->shell_client_destroyed =
    meta_wayland_zxdg_surface_v6_real_shell_client_destroyed;

  pspec = g_param_spec_pointer ("shell-client",
                                "MetaWaylandZxdgShellV6Client",
                                "The shell client instance",
                                G_PARAM_READWRITE |
                                G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
                                   ZXDG_SURFACE_V6_PROP_SHELL_CLIENT,
                                   pspec);
  pspec = g_param_spec_pointer ("xdg-surface-resource",
                                "xdg_surface wl_resource",
                                "The xdg_surface wl_resource instance",
                                G_PARAM_READWRITE |
                                G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
                                   ZXDG_SURFACE_V6_PROP_RESOURCE,
                                   pspec);
}

static void
meta_wayland_zxdg_surface_v6_shell_client_destroyed (MetaWaylandZxdgSurfaceV6 *xdg_surface)
{
  MetaWaylandZxdgSurfaceV6Class *xdg_surface_class =
    META_WAYLAND_ZXDG_SURFACE_V6_GET_CLASS (xdg_surface);

  xdg_surface_class->shell_client_destroyed (xdg_surface);
}

static void
meta_wayland_zxdg_surface_v6_constructor_finalize (MetaWaylandZxdgSurfaceV6Constructor *constructor,
                                                   MetaWaylandZxdgSurfaceV6            *xdg_surface)
{
  MetaWaylandZxdgShellV6Client *shell_client = constructor->shell_client;

  shell_client->surface_constructors =
    g_list_remove (shell_client->surface_constructors, constructor);
  shell_client->surfaces = g_list_append (shell_client->surfaces, xdg_surface);

  wl_resource_set_implementation (constructor->resource,
                                  &meta_wayland_zxdg_surface_v6_interface,
                                  xdg_surface,
                                  zxdg_surface_v6_destructor);

  g_free (constructor);
}

static void
zxdg_surface_v6_constructor_destroy (struct wl_client   *client,
                                     struct wl_resource *resource)
{
  wl_resource_post_error (resource,
                          ZXDG_SURFACE_V6_ERROR_NOT_CONSTRUCTED,
                          "xdg_surface destroyed before constructed");
  wl_resource_destroy (resource);
}

static void
zxdg_surface_v6_constructor_get_toplevel (struct wl_client   *client,
                                          struct wl_resource *resource,
                                          uint32_t            id)
{
  MetaWaylandZxdgSurfaceV6Constructor *constructor =
    wl_resource_get_user_data (resource);
  MetaWaylandZxdgShellV6Client *shell_client = constructor->shell_client;
  struct wl_resource *xdg_surface_resource = constructor->resource;
  MetaWaylandSurface *surface = constructor->surface;
  MetaWaylandZxdgToplevelV6 *xdg_toplevel;
  MetaWaylandZxdgSurfaceV6 *xdg_surface;
  MetaWaylandShellSurface *shell_surface;
  MetaWindow *window;

  if (!meta_wayland_surface_assign_role (surface,
                                         META_TYPE_WAYLAND_ZXDG_TOPLEVEL_V6,
                                         "shell-client", shell_client,
                                         "xdg-surface-resource", xdg_surface_resource,
                                         NULL))
    {
      wl_resource_post_error (resource, ZXDG_SHELL_V6_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  xdg_toplevel = META_WAYLAND_ZXDG_TOPLEVEL_V6 (surface->role);
  xdg_toplevel->resource = wl_resource_create (client,
                                               &zxdg_toplevel_v6_interface,
                                               wl_resource_get_version (resource),
                                               id);
  wl_resource_set_implementation (xdg_toplevel->resource,
                                  &meta_wayland_zxdg_toplevel_v6_interface,
                                  xdg_toplevel,
                                  zxdg_toplevel_v6_destructor);

  xdg_surface = META_WAYLAND_ZXDG_SURFACE_V6 (xdg_toplevel);
  meta_wayland_zxdg_surface_v6_constructor_finalize (constructor, xdg_surface);

  window = meta_window_wayland_new (meta_get_display (), surface);
  shell_surface = META_WAYLAND_SHELL_SURFACE (xdg_surface);
  meta_wayland_shell_surface_set_window (shell_surface, window);
}

static void
zxdg_surface_v6_constructor_get_popup (struct wl_client   *client,
                                       struct wl_resource *resource,
                                       uint32_t            id,
                                       struct wl_resource *parent_resource,
                                       struct wl_resource *positioner_resource)
{
  MetaWaylandZxdgSurfaceV6Constructor *constructor =
    wl_resource_get_user_data (resource);
  MetaWaylandZxdgShellV6Client *shell_client = constructor->shell_client;
  MetaWaylandSurface *surface = constructor->surface;
  struct wl_resource *xdg_shell_resource = constructor->shell_client->resource;
  struct wl_resource *xdg_surface_resource = constructor->resource;
  MetaWaylandSurface *parent_surface =
    surface_from_xdg_surface_resource (parent_resource);
  MetaWaylandZxdgPositionerV6 *xdg_positioner;
  MetaWaylandZxdgPopupV6 *xdg_popup;
  MetaWaylandZxdgSurfaceV6 *xdg_surface;

  if (!meta_wayland_surface_assign_role (surface,
                                         META_TYPE_WAYLAND_ZXDG_POPUP_V6,
                                         "shell-client", shell_client,
                                         "xdg-surface-resource", xdg_surface_resource,
                                         NULL))
    {
      wl_resource_post_error (xdg_shell_resource, ZXDG_SHELL_V6_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  if (!META_IS_WAYLAND_ZXDG_SURFACE_V6 (parent_surface->role))
    {
      wl_resource_post_error (xdg_shell_resource,
                              ZXDG_SHELL_V6_ERROR_INVALID_POPUP_PARENT,
                              "Invalid popup parent role");
      return;
    }

  xdg_popup = META_WAYLAND_ZXDG_POPUP_V6 (surface->role);

  xdg_popup->resource = wl_resource_create (client,
                                            &zxdg_popup_v6_interface,
                                            wl_resource_get_version (resource),
                                            id);
  wl_resource_set_implementation (xdg_popup->resource,
                                  &meta_wayland_zxdg_popup_v6_interface,
                                  xdg_popup,
                                  zxdg_popup_v6_destructor);

  xdg_surface = META_WAYLAND_ZXDG_SURFACE_V6 (xdg_popup);
  meta_wayland_zxdg_surface_v6_constructor_finalize (constructor, xdg_surface);

  xdg_positioner = wl_resource_get_user_data (positioner_resource);
  xdg_popup->setup.placement_rule =
    meta_wayland_zxdg_positioner_v6_to_placement (xdg_positioner);
  xdg_popup->setup.parent_surface = parent_surface;
}

static void
zxdg_surface_v6_constructor_set_window_geometry (struct wl_client   *client,
                                                 struct wl_resource *resource,
                                                 int32_t             x,
                                                 int32_t             y,
                                                 int32_t             width,
                                                 int32_t             height)
{
  wl_resource_post_error (resource,
                          ZXDG_SURFACE_V6_ERROR_NOT_CONSTRUCTED,
                          "xdg_surface::set_window_geometry called before constructed");
}

static void
zxdg_surface_v6_constructor_ack_configure (struct wl_client   *client,
                                           struct wl_resource *resource,
                                           uint32_t            serial)
{
  wl_resource_post_error (resource,
                          ZXDG_SURFACE_V6_ERROR_NOT_CONSTRUCTED,
                          "xdg_surface::ack_configure called before constructed");
}

static const struct zxdg_surface_v6_interface meta_wayland_zxdg_surface_v6_constructor_interface = {
  zxdg_surface_v6_constructor_destroy,
  zxdg_surface_v6_constructor_get_toplevel,
  zxdg_surface_v6_constructor_get_popup,
  zxdg_surface_v6_constructor_set_window_geometry,
  zxdg_surface_v6_constructor_ack_configure,
};

static void
zxdg_surface_v6_constructor_destructor (struct wl_resource *resource)
{
  MetaWaylandZxdgSurfaceV6Constructor *constructor =
    wl_resource_get_user_data (resource);

  constructor->shell_client->surface_constructors =
    g_list_remove (constructor->shell_client->surface_constructors,
                   constructor);

  g_free (constructor);
}

static MetaPlacementRule
meta_wayland_zxdg_positioner_v6_to_placement (MetaWaylandZxdgPositionerV6 *xdg_positioner)
{
  return (MetaPlacementRule) {
    .anchor_rect = xdg_positioner->anchor_rect,
    .gravity = xdg_positioner->gravity,
    .anchor = xdg_positioner->anchor,
    .constraint_adjustment = xdg_positioner->constraint_adjustment,
    .offset_x = xdg_positioner->offset_x,
    .offset_y = xdg_positioner->offset_y,
    .width = xdg_positioner->width,
    .height = xdg_positioner->height,
  };
}

static void
zxdg_positioner_v6_destroy (struct wl_client   *client,
                            struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
zxdg_positioner_v6_set_size (struct wl_client   *client,
                             struct wl_resource *resource,
                             int32_t             width,
                             int32_t             height)
{
  MetaWaylandZxdgPositionerV6 *positioner = wl_resource_get_user_data (resource);

  if (width <= 0 || height <= 0)
    {
      wl_resource_post_error (resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                              "Invalid size");
      return;
    }

  positioner->width = width;
  positioner->height = height;
}

static void
zxdg_positioner_v6_set_anchor_rect (struct wl_client   *client,
                                    struct wl_resource *resource,
                                    int32_t             x,
                                    int32_t             y,
                                    int32_t             width,
                                    int32_t             height)
{
  MetaWaylandZxdgPositionerV6 *positioner = wl_resource_get_user_data (resource);

  if (width <= 0 || height <= 0)
    {
      wl_resource_post_error (resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
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
zxdg_positioner_v6_set_anchor (struct wl_client   *client,
                               struct wl_resource *resource,
                               uint32_t            anchor)
{
  MetaWaylandZxdgPositionerV6 *positioner = wl_resource_get_user_data (resource);

  if ((anchor & ZXDG_POSITIONER_V6_ANCHOR_LEFT &&
       anchor & ZXDG_POSITIONER_V6_ANCHOR_RIGHT) ||
      (anchor & ZXDG_POSITIONER_V6_ANCHOR_TOP &&
       anchor & ZXDG_POSITIONER_V6_ANCHOR_BOTTOM))
    {
      wl_resource_post_error (resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                              "Invalid anchor");
      return;
    }

  positioner->anchor = anchor;
}

static void
zxdg_positioner_v6_set_gravity (struct wl_client   *client,
                                struct wl_resource *resource,
                                uint32_t            gravity)
{
  MetaWaylandZxdgPositionerV6 *positioner = wl_resource_get_user_data (resource);

  if ((gravity & ZXDG_POSITIONER_V6_GRAVITY_LEFT &&
       gravity & ZXDG_POSITIONER_V6_GRAVITY_RIGHT) ||
      (gravity & ZXDG_POSITIONER_V6_GRAVITY_TOP &&
       gravity & ZXDG_POSITIONER_V6_GRAVITY_BOTTOM))
    {
      wl_resource_post_error (resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                              "Invalid gravity");
      return;
    }

  positioner->gravity = gravity;
}

static void
zxdg_positioner_v6_set_constraint_adjustment (struct wl_client   *client,
                                              struct wl_resource *resource,
                                              uint32_t            constraint_adjustment)
{
  MetaWaylandZxdgPositionerV6 *positioner = wl_resource_get_user_data (resource);
  uint32_t all_adjustments = (ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_X |
                              ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X |
                              ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_Y |
                              ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y |
                              ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_X |
                              ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_RESIZE_Y);

  if ((constraint_adjustment & ~all_adjustments) != 0)
    {
      wl_resource_post_error (resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                              "Invalid constraint action");
      return;
    }

  positioner->constraint_adjustment = constraint_adjustment;
}

static void
zxdg_positioner_v6_set_offset (struct wl_client   *client,
                               struct wl_resource *resource,
                               int32_t             x,
                               int32_t             y)
{
  MetaWaylandZxdgPositionerV6 *positioner = wl_resource_get_user_data (resource);

  positioner->offset_x = x;
  positioner->offset_y = y;
}

static const struct zxdg_positioner_v6_interface meta_wayland_zxdg_positioner_v6_interface = {
  zxdg_positioner_v6_destroy,
  zxdg_positioner_v6_set_size,
  zxdg_positioner_v6_set_anchor_rect,
  zxdg_positioner_v6_set_anchor,
  zxdg_positioner_v6_set_gravity,
  zxdg_positioner_v6_set_constraint_adjustment,
  zxdg_positioner_v6_set_offset,
};

static void
zxdg_positioner_v6_destructor (struct wl_resource *resource)
{
  MetaWaylandZxdgPositionerV6 *positioner = wl_resource_get_user_data (resource);

  g_free (positioner);
}

static void
zxdg_shell_v6_destroy (struct wl_client   *client,
                       struct wl_resource *resource)
{
  MetaWaylandZxdgShellV6Client *shell_client = wl_resource_get_user_data (resource);

  if (shell_client->surfaces || shell_client->surface_constructors)
    wl_resource_post_error (resource, ZXDG_SHELL_V6_ERROR_DEFUNCT_SURFACES,
                            "xdg_shell destroyed before its surfaces");

  wl_resource_destroy (resource);
}

static void
zxdg_shell_v6_create_positioner (struct wl_client   *client,
                                 struct wl_resource *resource,
                                 uint32_t            id)
{
  MetaWaylandZxdgPositionerV6 *positioner;
  struct wl_resource *positioner_resource;

  positioner = g_new0 (MetaWaylandZxdgPositionerV6, 1);
  positioner_resource = wl_resource_create (client,
                                            &zxdg_positioner_v6_interface,
                                            wl_resource_get_version (resource),
                                            id);
  wl_resource_set_implementation (positioner_resource,
                                  &meta_wayland_zxdg_positioner_v6_interface,
                                  positioner,
                                  zxdg_positioner_v6_destructor);
}

static void
zxdg_shell_v6_get_xdg_surface (struct wl_client   *client,
                               struct wl_resource *resource,
                               uint32_t            id,
                               struct wl_resource *surface_resource)
{
  MetaWaylandZxdgShellV6Client *shell_client = wl_resource_get_user_data (resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandZxdgSurfaceV6 *xdg_surface = NULL;
  MetaWaylandZxdgSurfaceV6Constructor *constructor;

  if (surface->role && !META_IS_WAYLAND_ZXDG_SURFACE_V6 (surface->role))
    {
      wl_resource_post_error (resource, ZXDG_SHELL_V6_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  if (surface->role)
    xdg_surface = META_WAYLAND_ZXDG_SURFACE_V6 (surface->role);
  if (xdg_surface && meta_wayland_zxdg_surface_v6_is_assigned (xdg_surface))
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "zxdg_shell_v6::get_xdg_surface already requested");
      return;
    }

  if (surface->buffer_ref.buffer)
    {
      wl_resource_post_error (resource,
                              ZXDG_SHELL_V6_ERROR_INVALID_SURFACE_STATE,
                              "wl_surface@%d already has a buffer committed",
                              wl_resource_get_id (surface->resource));
      return;
    }

  constructor = g_new0 (MetaWaylandZxdgSurfaceV6Constructor, 1);
  constructor->surface = surface;
  constructor->shell_client = shell_client;
  constructor->resource = wl_resource_create (client,
                                              &zxdg_surface_v6_interface,
                                              wl_resource_get_version (resource),
                                              id);
  wl_resource_set_implementation (constructor->resource,
                                  &meta_wayland_zxdg_surface_v6_constructor_interface,
                                  constructor,
                                  zxdg_surface_v6_constructor_destructor);

  shell_client->surface_constructors =
    g_list_append (shell_client->surface_constructors, constructor);
}

static void
zxdg_shell_v6_pong (struct wl_client   *client,
                    struct wl_resource *resource,
                    uint32_t            serial)
{
  MetaDisplay *display = meta_get_display ();

  meta_display_pong_for_serial (display, serial);
}

static const struct zxdg_shell_v6_interface meta_wayland_zxdg_shell_v6_interface = {
  zxdg_shell_v6_destroy,
  zxdg_shell_v6_create_positioner,
  zxdg_shell_v6_get_xdg_surface,
  zxdg_shell_v6_pong,
};

static void
meta_wayland_zxdg_shell_v6_client_destroy (MetaWaylandZxdgShellV6Client *shell_client)
{
  while (shell_client->surface_constructors)
    {
      MetaWaylandZxdgSurfaceV6Constructor *constructor =
        g_list_first (shell_client->surface_constructors)->data;

      wl_resource_destroy (constructor->resource);
    }
  g_list_free (shell_client->surface_constructors);

  while (shell_client->surfaces)
    {
      MetaWaylandZxdgSurfaceV6 *xdg_surface =
        g_list_first (shell_client->surfaces)->data;

      meta_wayland_zxdg_surface_v6_shell_client_destroyed (xdg_surface);
    }
  g_list_free (shell_client->surfaces);

  g_free (shell_client);
}

static void
zxdg_shell_v6_destructor (struct wl_resource *resource)
{
  MetaWaylandZxdgShellV6Client *shell_client = wl_resource_get_user_data (resource);

  meta_wayland_zxdg_shell_v6_client_destroy (shell_client);
}

static void
bind_zxdg_shell_v6 (struct wl_client *client,
                    void             *data,
                    guint32           version,
                    guint32           id)
{
  MetaWaylandZxdgShellV6Client *shell_client;

  shell_client = g_new0 (MetaWaylandZxdgShellV6Client, 1);

  shell_client->resource = wl_resource_create (client,
                                               &zxdg_shell_v6_interface,
                                               version, id);
  wl_resource_set_implementation (shell_client->resource,
                                  &meta_wayland_zxdg_shell_v6_interface,
                                  shell_client, zxdg_shell_v6_destructor);
}

void
meta_wayland_legacy_xdg_shell_init (MetaWaylandCompositor *compositor)
{
  if (wl_global_create (compositor->wayland_display,
                        &zxdg_shell_v6_interface,
                        META_ZXDG_SHELL_V6_VERSION,
                        compositor, bind_zxdg_shell_v6) == NULL)
    g_error ("Failed to register a global xdg-shell object");
}
