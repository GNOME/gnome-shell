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

#include "wayland/meta-wayland-wl-shell.h"

#include "core/window-private.h"
#include "wayland/meta-wayland.h"
#include "wayland/meta-wayland-popup.h"
#include "wayland/meta-wayland-private.h"
#include "wayland/meta-wayland-seat.h"
#include "wayland/meta-wayland-surface.h"
#include "wayland/meta-wayland-versions.h"
#include "wayland/meta-window-wayland.h"

struct _MetaWaylandSurfaceRoleWlShellSurface
{
  MetaWaylandSurfaceRoleShellSurface parent;
};

G_DEFINE_TYPE (MetaWaylandSurfaceRoleWlShellSurface,
               meta_wayland_surface_role_wl_shell_surface,
               META_TYPE_WAYLAND_SURFACE_ROLE_SHELL_SURFACE);

static void
sync_wl_shell_parent_relationship (MetaWaylandSurface *surface,
                                   MetaWaylandSurface *parent);

static void
wl_shell_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  GList *l;

  meta_wayland_compositor_destroy_frame_callbacks (surface->compositor,
                                                   surface);

  for (l = surface->wl_shell.children; l; l = l->next)
    {
      MetaWaylandSurface *child_surface = l->data;

      child_surface->wl_shell.parent_surface = NULL;
    }

  if (surface->wl_shell.parent_surface)
    {
      MetaWaylandSurface *parent_surface = surface->wl_shell.parent_surface;

      parent_surface->wl_shell.children =
        g_list_remove (parent_surface->wl_shell.children, surface);
    }

  g_free (surface->wl_shell.title);
  g_free (surface->wl_shell.wm_class);

  if (surface->popup.popup)
    {
      wl_list_remove (&surface->popup.parent_destroy_listener.link);
      surface->popup.parent = NULL;

      meta_wayland_popup_dismiss (surface->popup.popup);
    }

  surface->wl_shell_surface = NULL;
}

static void
wl_shell_surface_pong (struct wl_client   *client,
                       struct wl_resource *resource,
                       uint32_t serial)
{
  MetaDisplay *display = meta_get_display ();

  meta_display_pong_for_serial (display, serial);
}

static void
wl_shell_surface_move (struct wl_client   *client,
                       struct wl_resource *resource,
                       struct wl_resource *seat_resource,
                       uint32_t serial)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  gfloat x, y;

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, TRUE, &x, &y))
    return;

  meta_wayland_surface_begin_grab_op (surface, seat, META_GRAB_OP_MOVING, x, y);
}

static MetaGrabOp
grab_op_for_wl_shell_surface_resize_edge (int edge)
{
  MetaGrabOp op = META_GRAB_OP_WINDOW_BASE;

  if (edge & WL_SHELL_SURFACE_RESIZE_TOP)
    op |= META_GRAB_OP_WINDOW_DIR_NORTH;
  if (edge & WL_SHELL_SURFACE_RESIZE_BOTTOM)
    op |= META_GRAB_OP_WINDOW_DIR_SOUTH;
  if (edge & WL_SHELL_SURFACE_RESIZE_LEFT)
    op |= META_GRAB_OP_WINDOW_DIR_WEST;
  if (edge & WL_SHELL_SURFACE_RESIZE_RIGHT)
    op |= META_GRAB_OP_WINDOW_DIR_EAST;

  if (op == META_GRAB_OP_WINDOW_BASE)
    {
      g_warning ("invalid edge: %d", edge);
      return META_GRAB_OP_NONE;
    }

  return op;
}

static void
wl_shell_surface_resize (struct wl_client   *client,
                         struct wl_resource *resource,
                         struct wl_resource *seat_resource,
                         uint32_t            serial,
                         uint32_t            edges)
{
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  gfloat x, y;
  MetaGrabOp grab_op;

  if (!meta_wayland_seat_get_grab_info (seat, surface, serial, TRUE, &x, &y))
    return;

  grab_op = grab_op_for_wl_shell_surface_resize_edge (edges);
  meta_wayland_surface_begin_grab_op (surface, seat, grab_op, x, y);
}

static void
wl_shell_surface_set_state (MetaWaylandSurface     *surface,
                            MetaWlShellSurfaceState state)
{
  MetaWlShellSurfaceState old_state = surface->wl_shell.state;

  surface->wl_shell.state = state;

  if (surface->window && old_state != state)
    {
      if (state == META_WL_SHELL_SURFACE_STATE_FULLSCREEN)
        meta_window_make_fullscreen (surface->window);
      else
        meta_window_unmake_fullscreen (surface->window);

      if (state == META_WL_SHELL_SURFACE_STATE_MAXIMIZED)
        meta_window_maximize (surface->window, META_MAXIMIZE_BOTH);
      else
        meta_window_unmaximize (surface->window, META_MAXIMIZE_BOTH);
    }
}

static void
wl_shell_surface_set_toplevel (struct wl_client *client,
                               struct wl_resource *resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  wl_shell_surface_set_state (surface,
                              META_WL_SHELL_SURFACE_STATE_TOPLEVEL);
}

static void
set_wl_shell_surface_parent (MetaWaylandSurface *surface,
                             MetaWaylandSurface *parent)
{
  MetaWaylandSurface *old_parent = surface->wl_shell.parent_surface;

  if (old_parent)
    {
      old_parent->wl_shell.children =
        g_list_remove (old_parent->wl_shell.children, surface);
    }

  parent->wl_shell.children = g_list_append (parent->wl_shell.children,
                                             surface);
  surface->wl_shell.parent_surface = parent;
}

static void
wl_shell_surface_set_transient (struct wl_client   *client,
                                struct wl_resource *resource,
                                struct wl_resource *parent_resource,
                                int32_t             x,
                                int32_t             y,
                                uint32_t            flags)
{
  MetaWaylandSurface *parent_surf = wl_resource_get_user_data (parent_resource);
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  wl_shell_surface_set_state (surface,
                              META_WL_SHELL_SURFACE_STATE_TRANSIENT);

  set_wl_shell_surface_parent (surface, parent_surf);
  surface->wl_shell.x = x;
  surface->wl_shell.y = y;

  if (surface->window && parent_surf->window)
    sync_wl_shell_parent_relationship (surface, parent_surf);
}

static void
wl_shell_surface_set_fullscreen (struct wl_client   *client,
                                 struct wl_resource *resource,
                                 uint32_t            method,
                                 uint32_t            framerate,
                                 struct wl_resource *output)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  wl_shell_surface_set_state (surface,
                              META_WL_SHELL_SURFACE_STATE_FULLSCREEN);
}

static void
handle_wl_shell_popup_parent_destroyed (struct wl_listener *listener,
                                        void *data)
{
  MetaWaylandSurface *surface =
    wl_container_of (listener, surface, popup.parent_destroy_listener);

  wl_list_remove (&surface->popup.parent_destroy_listener.link);
  surface->popup.parent = NULL;
}

static void
handle_wl_shell_popup_destroyed (struct wl_listener *listener,
                                 void               *data)
{
  MetaWaylandSurface *surface =
    wl_container_of (listener, surface, popup.destroy_listener);

  surface->popup.popup = NULL;
}

static void
create_popup (MetaWaylandSurface *surface)
{
  MetaWaylandSeat *seat = surface->wl_shell.popup_seat;
  MetaWaylandPopup *popup;

  popup = meta_wayland_pointer_start_popup_grab (&seat->pointer, surface);
  if (!popup)
    {
      wl_shell_surface_send_popup_done (surface->wl_shell_surface);
      return;
    }

  surface->popup.popup = popup;
  surface->popup.destroy_listener.notify = handle_wl_shell_popup_destroyed;
  wl_signal_add (meta_wayland_popup_get_destroy_signal (popup),
                 &surface->popup.destroy_listener);
}

static void
wl_shell_surface_set_popup (struct wl_client   *client,
                            struct wl_resource *resource,
                            struct wl_resource *seat_resource,
                            uint32_t            serial,
                            struct wl_resource *parent_resource,
                            int32_t             x,
                            int32_t             y,
                            uint32_t            flags)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);
  MetaWaylandSurface *parent_surf = wl_resource_get_user_data (parent_resource);
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);

  if (surface->popup.popup)
    {
      surface->popup.parent = NULL;
      wl_list_remove (&surface->popup.parent_destroy_listener.link);

      meta_wayland_popup_dismiss (surface->popup.popup);
    }

  wl_shell_surface_set_state (surface,
                              META_WL_SHELL_SURFACE_STATE_POPUP);

  if (!meta_wayland_seat_can_popup (seat, serial))
    {
      wl_shell_surface_send_popup_done (resource);
      return;
    }

  surface->popup.parent = parent_surf;
  surface->popup.parent_destroy_listener.notify =
    handle_wl_shell_popup_parent_destroyed;
  wl_resource_add_destroy_listener (parent_surf->resource,
                                    &surface->popup.parent_destroy_listener);

  set_wl_shell_surface_parent (surface, parent_surf);
  surface->wl_shell.popup_seat = seat;
  surface->wl_shell.x = x;
  surface->wl_shell.y = y;
  surface->wl_shell.pending_popup = TRUE;

  if (surface->window && parent_surf->window)
    sync_wl_shell_parent_relationship (surface, parent_surf);
}

static void
wl_shell_surface_set_maximized (struct wl_client   *client,
                                struct wl_resource *resource,
                                struct wl_resource *output)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  wl_shell_surface_set_state (surface,
                              META_WL_SHELL_SURFACE_STATE_MAXIMIZED);
}

static void
wl_shell_surface_set_title (struct wl_client   *client,
                            struct wl_resource *resource,
                            const char         *title)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  g_clear_pointer (&surface->wl_shell.title, g_free);
  surface->wl_shell.title = g_strdup (title);

  if (surface->window)
    meta_window_set_title (surface->window, title);
}

static void
wl_shell_surface_set_class (struct wl_client *client,
                            struct wl_resource *resource,
                            const char *class_)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (resource);

  g_clear_pointer (&surface->wl_shell.wm_class, g_free);
  surface->wl_shell.wm_class = g_strdup (class_);

  if (surface->window)
    meta_window_set_wm_class (surface->window, class_, class_);
}

static const struct wl_shell_surface_interface meta_wayland_wl_shell_surface_interface = {
  wl_shell_surface_pong,
  wl_shell_surface_move,
  wl_shell_surface_resize,
  wl_shell_surface_set_toplevel,
  wl_shell_surface_set_transient,
  wl_shell_surface_set_fullscreen,
  wl_shell_surface_set_popup,
  wl_shell_surface_set_maximized,
  wl_shell_surface_set_title,
  wl_shell_surface_set_class,
};

static void
sync_wl_shell_parent_relationship (MetaWaylandSurface *surface,
                                   MetaWaylandSurface *parent)
{
  meta_window_set_transient_for (surface->window, parent->window);

  if (surface->wl_shell.state == META_WL_SHELL_SURFACE_STATE_POPUP ||
      surface->wl_shell.state == META_WL_SHELL_SURFACE_STATE_TRANSIENT)
    meta_window_wayland_place_relative_to (surface->window,
                                           parent->window,
                                           surface->wl_shell.x,
                                           surface->wl_shell.y);

  if (surface->wl_shell.state == META_WL_SHELL_SURFACE_STATE_POPUP &&
      surface->wl_shell.pending_popup)
    {
      create_popup (surface);
      surface->wl_shell.pending_popup = FALSE;
    }
}

static void
create_wl_shell_surface_window (MetaWaylandSurface *surface)
{
  MetaWaylandSurface *parent;
  GList *l;

  surface->window = meta_window_wayland_new (meta_get_display (), surface);
  meta_wayland_surface_set_window (surface, surface->window);

  if (surface->wl_shell.title)
    meta_window_set_title (surface->window, surface->wl_shell.title);
  if (surface->wl_shell.wm_class)
    meta_window_set_wm_class (surface->window,
                              surface->wl_shell.wm_class,
                              surface->wl_shell.wm_class);

  parent = surface->wl_shell.parent_surface;
  if (parent && parent->window)
    sync_wl_shell_parent_relationship (surface, parent);

  for (l = surface->wl_shell.children; l; l = l->next)
    {
      MetaWaylandSurface *child = l->data;

      if (child->window)
        sync_wl_shell_parent_relationship (child, surface);
    }
}

static void
wl_shell_get_shell_surface (struct wl_client   *client,
                            struct wl_resource *resource,
                            uint32_t            id,
                            struct wl_resource *surface_resource)
{
  MetaWaylandSurface *surface = wl_resource_get_user_data (surface_resource);

  if (surface->wl_shell_surface != NULL)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "wl_shell::get_shell_surface already requested");
      return;
    }

  if (!meta_wayland_surface_assign_role (surface,
                                         META_TYPE_WAYLAND_SURFACE_ROLE_WL_SHELL_SURFACE))
    {
      wl_resource_post_error (resource, WL_SHELL_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  surface->wl_shell_surface = wl_resource_create (client,
                                                  &wl_shell_surface_interface,
                                                  wl_resource_get_version (resource),
                                                  id);
  wl_resource_set_implementation (surface->wl_shell_surface,
                                  &meta_wayland_wl_shell_surface_interface,
                                  surface,
                                  wl_shell_surface_destructor);

  create_wl_shell_surface_window (surface);
}

static const struct wl_shell_interface meta_wayland_wl_shell_interface = {
  wl_shell_get_shell_surface,
};

static void
bind_wl_shell (struct wl_client *client,
               void             *data,
               uint32_t          version,
               uint32_t          id)
{
  struct wl_resource *resource;

  resource = wl_resource_create (client, &wl_shell_interface, version, id);
  wl_resource_set_implementation (resource, &meta_wayland_wl_shell_interface, data, NULL);
}

static void
wl_shell_surface_role_commit (MetaWaylandSurfaceRole  *surface_role,
                              MetaWaylandPendingState *pending)
{
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWindow *window = surface->window;

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_surface_role_wl_shell_surface_parent_class);
  surface_role_class->commit (surface_role, pending);

  /* For wl_shell, it's equivalent to an unmap. Semantics
   * are poorly defined, so we can choose some that are
   * convenient for us. */
  if (surface->buffer_ref.buffer && !window)
    {
      create_wl_shell_surface_window (surface);
    }
  else if (!surface->buffer_ref.buffer && window)
    {
      meta_wayland_surface_destroy_window (surface);
      return;
    }

  if (pending->newly_attached)
    meta_wayland_surface_apply_window_state (surface, pending);
}

static void
wl_shell_surface_role_configure (MetaWaylandSurfaceRoleShellSurface *shell_surface_role,
                                 int                                 new_width,
                                 int                                 new_height,
                                 MetaWaylandSerial                  *sent_serial)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (shell_surface_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  if (!surface->wl_shell_surface)
    return;

  wl_shell_surface_send_configure (surface->wl_shell_surface,
                                   0,
                                   new_width, new_height);
}

static void
wl_shell_surface_role_ping (MetaWaylandSurfaceRoleShellSurface *shell_surface_role,
                            guint32                             serial)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (shell_surface_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  wl_shell_surface_send_ping (surface->wl_shell_surface, serial);
}

static void
wl_shell_surface_role_close (MetaWaylandSurfaceRoleShellSurface *shell_surface_role)
{
  /* Not supported by wl_shell_surface. */
}

static void
wl_shell_surface_role_popup_done (MetaWaylandSurfaceRoleShellSurface *shell_surface_role)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (shell_surface_role);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  wl_shell_surface_send_popup_done (surface->wl_shell_surface);
}

static void
meta_wayland_surface_role_wl_shell_surface_init (MetaWaylandSurfaceRoleWlShellSurface *wl_shell_surface)
{
}

static void
meta_wayland_surface_role_wl_shell_surface_class_init (MetaWaylandSurfaceRoleWlShellSurfaceClass *klass)
{
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandSurfaceRoleShellSurfaceClass *shell_surface_role_class;

  surface_role_class = META_WAYLAND_SURFACE_ROLE_CLASS (klass);
  surface_role_class->commit = wl_shell_surface_role_commit;

  shell_surface_role_class =
    META_WAYLAND_SURFACE_ROLE_SHELL_SURFACE_CLASS (klass);
  shell_surface_role_class->configure = wl_shell_surface_role_configure;
  shell_surface_role_class->ping = wl_shell_surface_role_ping;
  shell_surface_role_class->close = wl_shell_surface_role_close;
  shell_surface_role_class->popup_done = wl_shell_surface_role_popup_done;
}

void
meta_wayland_wl_shell_init (MetaWaylandCompositor *compositor)
{
  if (wl_global_create (compositor->wayland_display,
                        &wl_shell_interface,
                        META_WL_SHELL_VERSION,
                        compositor, bind_wl_shell) == NULL)
    g_error ("Failed to register a global wl-shell object");
}
