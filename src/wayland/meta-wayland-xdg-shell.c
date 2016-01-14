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

#include "core/window-private.h"
#include "wayland/meta-wayland.h"
#include "wayland/meta-wayland-popup.h"
#include "wayland/meta-wayland-private.h"
#include "wayland/meta-wayland-seat.h"
#include "wayland/meta-wayland-surface.h"
#include "wayland/meta-wayland-versions.h"
#include "wayland/meta-window-wayland.h"
#include "xdg-shell-unstable-v5-server-protocol.h"

struct _MetaWaylandXdgSurface
{
  MetaWaylandSurfaceRoleShellSurface parent;
};

G_DEFINE_TYPE (MetaWaylandXdgSurface,
               meta_wayland_xdg_surface,
               META_TYPE_WAYLAND_SURFACE_ROLE_SHELL_SURFACE);

struct _MetaWaylandXdgPopup
{
  MetaWaylandSurfaceRoleShellSurface parent;
};

G_DEFINE_TYPE (MetaWaylandXdgPopup,
               meta_wayland_xdg_popup,
               META_TYPE_WAYLAND_SURFACE_ROLE_SHELL_SURFACE);

static void
xdg_shell_destroy (struct wl_client   *client,
                   struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
xdg_shell_use_unstable_version (struct wl_client   *client,
                                struct wl_resource *resource,
                                int32_t             version)
{
  if (version != XDG_SHELL_VERSION_CURRENT)
    wl_resource_post_error (resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                            "bad xdg-shell version: %d\n", version);
}

static void
xdg_shell_pong (struct wl_client   *client,
                struct wl_resource *resource,
                uint32_t            serial)
{
  MetaDisplay *display = meta_get_display ();

  meta_display_pong_for_serial (display, serial);
}

static void
xdg_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_wayland_compositor_destroy_frame_callbacks (surface->compositor,
                                                   surface);
  meta_wayland_surface_destroy_window (surface);
  surface->xdg_surface = NULL;
}

static void
xdg_surface_destroy (struct wl_client   *client,
                     struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static void
xdg_surface_set_parent (struct wl_client   *client,
                        struct wl_resource *resource,
                        struct wl_resource *parent_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWindow *transient_for = NULL;

  if (parent_resource)
    {
      MetaWaylandSurface *parent_surface =
        wl_resource_get_user_data (parent_resource);
      transient_for = parent_surface->window;
    }

  meta_window_set_transient_for (surface->window, transient_for);
}

static void
xdg_surface_set_title (struct wl_client   *client,
                       struct wl_resource *resource,
                       const char         *title)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_window_set_title (surface->window, title);
}

static void
xdg_surface_set_app_id (struct wl_client   *client,
                        struct wl_resource *resource,
                        const char         *app_id)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_window_set_wm_class (surface->window, app_id, app_id);
}

static void
xdg_surface_show_window_menu (struct wl_client   *client,
                              struct wl_resource *resource,
                              struct wl_resource *seat_resource,
                              uint32_t            serial,
                              int32_t             x,
                              int32_t             y)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, FALSE, NULL, NULL))
    return;

  meta_window_show_menu (surface->window, META_WINDOW_MENU_WM,
                         surface->window->buffer_rect.x + x,
                         surface->window->buffer_rect.y + y);
}

static void
xdg_surface_move (struct wl_client   *client,
                  struct wl_resource *resource,
                  struct wl_resource *seat_resource,
                  guint32             serial)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  gfloat x, y;

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, TRUE, &x, &y))
    return;

  meta_wayland_surface_begin_grab_op (surface, seat, META_GRAB_OP_MOVING, x, y);
}

static MetaGrabOp
grab_op_for_xdg_surface_resize_edge (int edge)
{
  MetaGrabOp op = META_GRAB_OP_WINDOW_BASE;

  if (edge & XDG_SURFACE_RESIZE_EDGE_TOP)
    op |= META_GRAB_OP_WINDOW_DIR_NORTH;
  if (edge & XDG_SURFACE_RESIZE_EDGE_BOTTOM)
    op |= META_GRAB_OP_WINDOW_DIR_SOUTH;
  if (edge & XDG_SURFACE_RESIZE_EDGE_LEFT)
    op |= META_GRAB_OP_WINDOW_DIR_WEST;
  if (edge & XDG_SURFACE_RESIZE_EDGE_RIGHT)
    op |= META_GRAB_OP_WINDOW_DIR_EAST;

  if (op == META_GRAB_OP_WINDOW_BASE)
    {
      g_warning ("invalid edge: %d", edge);
      return META_GRAB_OP_NONE;
    }

  return op;
}

static void
xdg_surface_resize (struct wl_client   *client,
                    struct wl_resource *resource,
                    struct wl_resource *seat_resource,
                    guint32             serial,
                    guint32             edges)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  gfloat x, y;
  MetaGrabOp grab_op;

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, TRUE, &x, &y))
    return;

  grab_op = grab_op_for_xdg_surface_resize_edge (edges);
  meta_wayland_surface_begin_grab_op (surface, seat, grab_op, x, y);
}

static void
xdg_surface_ack_configure (struct wl_client   *client,
                           struct wl_resource *resource,
                           uint32_t            serial)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  surface->acked_configure_serial.set = TRUE;
  surface->acked_configure_serial.value = serial;
}

static void
xdg_surface_set_window_geometry (struct wl_client   *client,
                                 struct wl_resource *resource,
                                 int32_t             x,
                                 int32_t             y,
                                 int32_t             width,
                                 int32_t             height)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  surface->pending->has_new_geometry = TRUE;
  surface->pending->new_geometry.x = x;
  surface->pending->new_geometry.y = y;
  surface->pending->new_geometry.width = width;
  surface->pending->new_geometry.height = height;
}

static void
xdg_surface_set_maximized (struct wl_client   *client,
                           struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_window_maximize (surface->window, META_MAXIMIZE_BOTH);
}

static void
xdg_surface_unset_maximized (struct wl_client   *client,
                             struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_window_unmaximize (surface->window, META_MAXIMIZE_BOTH);
}

static void
xdg_surface_set_fullscreen (struct wl_client   *client,
                            struct wl_resource *resource,
                            struct wl_resource *output_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_window_make_fullscreen (surface->window);
}

static void
xdg_surface_unset_fullscreen (struct wl_client   *client,
                              struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_window_unmake_fullscreen (surface->window);
}

static void
xdg_surface_set_minimized (struct wl_client   *client,
                           struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_window_minimize (surface->window);
}

static const struct xdg_surface_interface meta_wayland_xdg_surface_interface = {
  xdg_surface_destroy,
  xdg_surface_set_parent,
  xdg_surface_set_title,
  xdg_surface_set_app_id,
  xdg_surface_show_window_menu,
  xdg_surface_move,
  xdg_surface_resize,
  xdg_surface_ack_configure,
  xdg_surface_set_window_geometry,
  xdg_surface_set_maximized,
  xdg_surface_unset_maximized,
  xdg_surface_set_fullscreen,
  xdg_surface_unset_fullscreen,
  xdg_surface_set_minimized,
};

static void
xdg_shell_get_xdg_surface (struct wl_client   *client,
                           struct wl_resource *resource,
                           guint32             id,
                           struct wl_resource *surface_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWindow *window;

  if (surface->xdg_surface != NULL)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "xdg_shell::get_xdg_surface already requested");
      return;
    }

  if (!meta_wayland_surface_assign_role (surface, META_TYPE_WAYLAND_XDG_SURFACE))
    {
      wl_resource_post_error (resource, XDG_SHELL_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  surface->xdg_surface = wl_resource_create (client,
                                             &xdg_surface_interface,
                                             wl_resource_get_version (resource),
                                             id);
  wl_resource_set_implementation (surface->xdg_surface,
                                  &meta_wayland_xdg_surface_interface,
                                  surface,
                                  xdg_surface_destructor);

  surface->xdg_shell_resource = resource;

  window = meta_window_wayland_new (meta_get_display (), surface);
  meta_wayland_surface_set_window (surface, window);
}

static void
xdg_popup_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  meta_wayland_compositor_destroy_frame_callbacks (surface->compositor,
                                                   surface);
  if (surface->popup.parent)
    {
      wl_list_remove (&surface->popup.parent_destroy_listener.link);
      surface->popup.parent = NULL;
    }

  if (surface->popup.popup)
    meta_wayland_popup_dismiss (surface->popup.popup);

  surface->xdg_popup = NULL;
}

static void
xdg_popup_destroy (struct wl_client *client,
                   struct wl_resource *resource)
{
  wl_resource_destroy (resource);
}

static const struct xdg_popup_interface meta_wayland_xdg_popup_interface = {
  xdg_popup_destroy,
};

static void
handle_popup_parent_destroyed (struct wl_listener *listener,
                               void               *data)
{
  MetaWaylandSurface *surface =
    wl_container_of (listener, surface, popup.parent_destroy_listener);

  wl_resource_post_error (surface->xdg_shell_resource,
                          XDG_SHELL_ERROR_NOT_THE_TOPMOST_POPUP,
                          "destroyed popup not top most popup");
  surface->popup.parent = NULL;

  meta_wayland_surface_destroy_window (surface);
}

static void
handle_popup_destroyed (struct wl_listener *listener,
                        void               *data)
{
  MetaWaylandPopup *popup = data;
  MetaWaylandSurface *top_popup;
  MetaWaylandSurface *surface =
    wl_container_of (listener, surface, popup.destroy_listener);

  top_popup = meta_wayland_popup_get_top_popup (popup);
  if (surface != top_popup)
    {
      wl_resource_post_error (surface->xdg_shell_resource,
                              XDG_SHELL_ERROR_NOT_THE_TOPMOST_POPUP,
                              "destroyed popup not top most popup");
    }

  surface->popup.popup = NULL;

  meta_wayland_surface_destroy_window (surface);
}

static void
xdg_shell_get_xdg_popup (struct wl_client   *client,
                         struct wl_resource *resource,
                         uint32_t            id,
                         struct wl_resource *surface_resource,
                         struct wl_resource *parent_resource,
                         struct wl_resource *seat_resource,
                         uint32_t            serial,
                         int32_t             x,
                         int32_t             y)
{
  struct wl_resource *popup_resource;
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);
  MetaWaylandSurface *parent_surf = wl_resource_get_user_data (parent_resource);
  MetaWaylandSurface *top_popup;
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWindow *window;
  MetaDisplay *display = meta_get_display ();
  MetaWaylandPopup *popup;

  if (surface->xdg_popup != NULL)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "xdg_shell::get_xdg_popup already requested");
      return;
    }

  if (!meta_wayland_surface_assign_role (surface, META_TYPE_WAYLAND_XDG_POPUP))
    {
      wl_resource_post_error (resource, XDG_SHELL_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  if (parent_surf == NULL ||
      parent_surf->window == NULL ||
      (parent_surf->xdg_popup == NULL && parent_surf->xdg_surface == NULL))
    {
      wl_resource_post_error (resource,
                              XDG_SHELL_ERROR_INVALID_POPUP_PARENT,
                              "invalid parent surface");
      return;
    }

  top_popup = meta_wayland_pointer_get_top_popup (&seat->pointer);
  if ((top_popup == NULL && parent_surf->xdg_surface == NULL) ||
      (top_popup != NULL && parent_surf != top_popup))
    {
      wl_resource_post_error (resource,
                              XDG_SHELL_ERROR_NOT_THE_TOPMOST_POPUP,
                              "parent not top most surface");
      return;
    }

  popup_resource = wl_resource_create (client, &xdg_popup_interface,
                                       wl_resource_get_version (resource), id);
  wl_resource_set_implementation (popup_resource,
                                  &meta_wayland_xdg_popup_interface,
                                  surface,
                                  xdg_popup_destructor);

  surface->xdg_popup = popup_resource;
  surface->xdg_shell_resource = resource;

  if (!meta_wayland_seat_can_popup (seat, serial))
    {
      xdg_popup_send_popup_done (popup_resource);
      return;
    }

  surface->popup.parent = parent_surf;
  surface->popup.parent_destroy_listener.notify = handle_popup_parent_destroyed;
  wl_resource_add_destroy_listener (parent_surf->resource,
                                    &surface->popup.parent_destroy_listener);

  window = meta_window_wayland_new (display, surface);
  meta_window_wayland_place_relative_to (window, parent_surf->window, x, y);
  window->showing_for_first_time = FALSE;

  meta_wayland_surface_set_window (surface, window);

  meta_window_focus (window, meta_display_get_current_time (display));
  popup = meta_wayland_pointer_start_popup_grab (&seat->pointer, surface);
  if (popup == NULL)
    {
      meta_wayland_surface_destroy_window (surface);
      return;
    }

  surface->popup.destroy_listener.notify = handle_popup_destroyed;
  surface->popup.popup = popup;
  wl_signal_add (meta_wayland_popup_get_destroy_signal (popup),
                 &surface->popup.destroy_listener);
}

static const struct xdg_shell_interface meta_wayland_xdg_shell_interface = {
  xdg_shell_destroy,
  xdg_shell_use_unstable_version,
  xdg_shell_get_xdg_surface,
  xdg_shell_get_xdg_popup,
  xdg_shell_pong,
};

static void
bind_xdg_shell (struct wl_client *client,
                void             *data,
                guint32           version,
                guint32           id)
{
  struct wl_resource *resource;

  if (version != META_XDG_SHELL_VERSION)
    {
      g_warning ("using xdg-shell without stable version %d\n",
                 META_XDG_SHELL_VERSION);
      return;
    }

  resource = wl_resource_create (client, &xdg_shell_interface, version, id);
  wl_resource_set_implementation (resource, &meta_wayland_xdg_shell_interface,
                                  data, NULL);
}

static void
fill_states (struct wl_array *states, MetaWindow *window)
{
  uint32_t *s;

  if (META_WINDOW_MAXIMIZED (window))
    {
      s = wl_array_add (states, sizeof *s);
      *s = XDG_SURFACE_STATE_MAXIMIZED;
    }
  if (meta_window_is_fullscreen (window))
    {
      s = wl_array_add (states, sizeof *s);
      *s = XDG_SURFACE_STATE_FULLSCREEN;
    }
  if (meta_grab_op_is_resizing (window->display->grab_op))
    {
      s = wl_array_add (states, sizeof *s);
      *s = XDG_SURFACE_STATE_RESIZING;
    }
  if (meta_window_appears_focused (window))
    {
      s = wl_array_add (states, sizeof *s);
      *s = XDG_SURFACE_STATE_ACTIVATED;
    }
}

static void
xdg_surface_role_commit (MetaWaylandSurfaceRole  *surface_role,
                         MetaWaylandPendingState *pending)
{
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_xdg_surface_parent_class);
  surface_role_class->commit (surface_role, pending);

  if (surface->buffer_ref.buffer == NULL)
    {
      /* XDG surfaces can't commit NULL buffers */
      wl_resource_post_error (surface->resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "Cannot commit a NULL buffer to an xdg_surface");
      return;
    }

  if (pending->newly_attached)
    meta_wayland_surface_apply_window_state (surface, pending);
}

static MetaWaylandSurface *
xdg_surface_role_get_toplevel (MetaWaylandSurfaceRole *surface_role)
{
  return meta_wayland_surface_role_get_surface (surface_role);
}

static void
xdg_surface_role_configure (MetaWaylandSurfaceRoleShellSurface *shell_surface_role,
                            int                                 new_width,
                            int                                 new_height,
                            MetaWaylandSerial                  *sent_serial)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (shell_surface_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  struct wl_client *client = wl_resource_get_client (surface->xdg_surface);
  struct wl_display *display = wl_client_get_display (client);
  uint32_t serial = wl_display_next_serial (display);
  struct wl_array states;

  if (!surface->xdg_surface)
    return;

  wl_array_init (&states);
  fill_states (&states, surface->window);

  xdg_surface_send_configure (surface->xdg_surface,
                              new_width, new_height,
                              &states,
                              serial);

  wl_array_release (&states);

  if (sent_serial)
    {
      sent_serial->set = TRUE;
      sent_serial->value = serial;
    }
}

static void
xdg_surface_role_managed (MetaWaylandSurfaceRoleShellSurface *shell_surface_role,
                          MetaWindow                         *window)
{
}

static void
xdg_surface_role_ping (MetaWaylandSurfaceRoleShellSurface *shell_surface_role,
                       uint32_t                            serial)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (shell_surface_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  xdg_shell_send_ping (surface->xdg_shell_resource, serial);
}

static void
xdg_surface_role_close (MetaWaylandSurfaceRoleShellSurface *shell_surface_role)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (shell_surface_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  xdg_surface_send_close (surface->xdg_surface);
}

static void
meta_wayland_xdg_surface_init (MetaWaylandXdgSurface *role)
{
}

static void
meta_wayland_xdg_surface_class_init (MetaWaylandXdgSurfaceClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);

  surface_role_class->commit = xdg_surface_role_commit;
  surface_role_class->get_toplevel = xdg_surface_role_get_toplevel;

  MetaWaylandSurfaceRoleShellSurfaceClass *shell_surface_role_class =
    META_WAYLAND_SURFACE_ROLE_SHELL_SURFACE_CLASS (klass);

  shell_surface_role_class->configure = xdg_surface_role_configure;
  shell_surface_role_class->managed = xdg_surface_role_managed;
  shell_surface_role_class->ping = xdg_surface_role_ping;
  shell_surface_role_class->close = xdg_surface_role_close;
}

static void
xdg_popup_role_commit (MetaWaylandSurfaceRole  *surface_role,
                       MetaWaylandPendingState *pending)
{
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_xdg_popup_parent_class);
  surface_role_class->commit (surface_role, pending);

  if (surface->buffer_ref.buffer == NULL)
    {
      /* XDG surfaces can't commit NULL buffers */
      wl_resource_post_error (surface->resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "Cannot commit a NULL buffer to an xdg_popup");
      return;
    }

  if (pending->newly_attached)
    meta_wayland_surface_apply_window_state (surface, pending);
}

static MetaWaylandSurface *
xdg_popup_role_get_toplevel (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandSurface *surface =
     meta_wayland_surface_role_get_surface (surface_role);

  return meta_wayland_surface_get_toplevel (surface->popup.parent);
}

static void
xdg_popup_role_configure (MetaWaylandSurfaceRoleShellSurface *shell_surface_role,
                          int                                 new_width,
                          int                                 new_height,
                          MetaWaylandSerial                  *sent_serial)
{
  /* This can happen if the popup window loses or receives focus.
   * Just ignore it. */
}

static void
xdg_popup_role_managed (MetaWaylandSurfaceRoleShellSurface *shell_surface_role,
                        MetaWindow                         *window)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (shell_surface_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWaylandSurface *parent = surface->popup.parent;

  g_assert (parent);

  meta_window_set_transient_for (window, parent->window);
  meta_window_set_type (window, META_WINDOW_DROPDOWN_MENU);
}

static void
xdg_popup_role_ping (MetaWaylandSurfaceRoleShellSurface *shell_surface_role,
                uint32_t                            serial)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (shell_surface_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  xdg_shell_send_ping (surface->xdg_shell_resource, serial);
}

static void
xdg_popup_role_popup_done (MetaWaylandSurfaceRoleShellSurface *shell_surface_role)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (shell_surface_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  xdg_popup_send_popup_done (surface->xdg_popup);
}

static void
meta_wayland_xdg_popup_init (MetaWaylandXdgPopup *role)
{
}

static void
meta_wayland_xdg_popup_class_init (MetaWaylandXdgPopupClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (klass);

  surface_role_class->commit = xdg_popup_role_commit;
  surface_role_class->get_toplevel = xdg_popup_role_get_toplevel;

  MetaWaylandSurfaceRoleShellSurfaceClass *shell_surface_role_class =
    META_WAYLAND_SURFACE_ROLE_SHELL_SURFACE_CLASS (klass);

  shell_surface_role_class->configure = xdg_popup_role_configure;
  shell_surface_role_class->managed = xdg_popup_role_managed;
  shell_surface_role_class->ping = xdg_popup_role_ping;
  shell_surface_role_class->popup_done = xdg_popup_role_popup_done;
}

void
meta_wayland_xdg_shell_init (MetaWaylandCompositor *compositor)
{
  if (wl_global_create (compositor->wayland_display,
                        &xdg_shell_interface,
                        META_XDG_SHELL_VERSION,
                        compositor, bind_xdg_shell) == NULL)
    g_error ("Failed to register a global xdg-shell object");
}
