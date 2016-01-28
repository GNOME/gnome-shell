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

typedef enum
{
  META_WL_SHELL_SURFACE_STATE_NONE,
  META_WL_SHELL_SURFACE_STATE_TOPLEVEL,
  META_WL_SHELL_SURFACE_STATE_POPUP,
  META_WL_SHELL_SURFACE_STATE_TRANSIENT,
  META_WL_SHELL_SURFACE_STATE_FULLSCREEN,
  META_WL_SHELL_SURFACE_STATE_MAXIMIZED,
} MetaWlShellSurfaceState;

struct _MetaWaylandWlShellSurface
{
  MetaWaylandSurfaceRoleShellSurface parent;

  struct wl_resource *resource;

  MetaWlShellSurfaceState state;

  char *title;
  char *wm_class;

  MetaWaylandSurface *parent_surface;
  GList *children;

  MetaWaylandSeat *popup_seat;
  MetaWaylandPopup *popup;
  gboolean pending_popup;

  int x;
  int y;
};

static void
popup_surface_iface_init (MetaWaylandPopupSurfaceInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MetaWaylandWlShellSurface,
                         meta_wayland_wl_shell_surface,
                         META_TYPE_WAYLAND_SURFACE_ROLE_SHELL_SURFACE,
                         G_IMPLEMENT_INTERFACE (META_TYPE_WAYLAND_POPUP_SURFACE,
                                                popup_surface_iface_init));

static MetaWaylandSurface *
surface_from_wl_shell_surface_resource (struct wl_resource *resource)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    wl_resource_get_user_data (resource);
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (wl_shell_surface);

  return meta_wayland_surface_role_get_surface (surface_role);
}

static void
sync_wl_shell_parent_relationship (MetaWaylandSurface *surface,
                                   MetaWaylandSurface *parent);

static void
wl_shell_surface_destructor (struct wl_resource *resource)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (wl_resource_get_user_data (resource));
  MetaWaylandSurface *surface =
    surface_from_wl_shell_surface_resource (resource);
  GList *l;

  meta_wayland_compositor_destroy_frame_callbacks (surface->compositor,
                                                   surface);

  if (wl_shell_surface->popup)
    meta_wayland_popup_dismiss (wl_shell_surface->popup);

  for (l = wl_shell_surface->children; l; l = l->next)
    {
      MetaWaylandSurface *child_surface = l->data;
      MetaWaylandWlShellSurface *child_wl_shell_surface =
        META_WAYLAND_WL_SHELL_SURFACE (child_surface->role);

      child_wl_shell_surface->parent_surface = NULL;

      if (child_wl_shell_surface->parent_surface == surface)
        {
          meta_wayland_popup_dismiss (child_wl_shell_surface->popup);
          child_wl_shell_surface->parent_surface = NULL;
        }
    }

  if (wl_shell_surface->parent_surface)
    {
      MetaWaylandSurface *parent_surface = wl_shell_surface->parent_surface;
      MetaWaylandWlShellSurface *parent_wl_shell_surface =
        META_WAYLAND_WL_SHELL_SURFACE (parent_surface->role);

      parent_wl_shell_surface->children =
        g_list_remove (parent_wl_shell_surface->children, surface);
    }

  g_free (wl_shell_surface->title);
  g_free (wl_shell_surface->wm_class);

  if (wl_shell_surface->popup)
    {
      wl_shell_surface->parent_surface = NULL;

      meta_wayland_popup_dismiss (wl_shell_surface->popup);
    }

  wl_shell_surface->resource = NULL;
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
  MetaWaylandSurface *surface =
    surface_from_wl_shell_surface_resource (resource);
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
  MetaWaylandSurface *surface =
    surface_from_wl_shell_surface_resource (resource);
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
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (surface->role);
  MetaWlShellSurfaceState old_state = wl_shell_surface->state;

  wl_shell_surface->state = state;

  if (surface->window && old_state != state)
    {
      if (old_state == META_WL_SHELL_SURFACE_STATE_POPUP &&
          wl_shell_surface->popup)
        {
          meta_wayland_popup_dismiss (wl_shell_surface->popup);
          wl_shell_surface->popup = NULL;
        }

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
  MetaWaylandSurface *surface =
    surface_from_wl_shell_surface_resource (resource);

  wl_shell_surface_set_state (surface,
                              META_WL_SHELL_SURFACE_STATE_TOPLEVEL);
}

static void
set_wl_shell_surface_parent (MetaWaylandSurface *surface,
                             MetaWaylandSurface *parent)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (surface->role);
  MetaWaylandWlShellSurface *parent_wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (parent->role);

  if (wl_shell_surface->parent_surface)
    {
      MetaWaylandWlShellSurface *old_parent =
        META_WAYLAND_WL_SHELL_SURFACE (wl_shell_surface->parent_surface->role);

      old_parent->children = g_list_remove (old_parent->children, surface);
    }

  parent_wl_shell_surface->children =
    g_list_append (parent_wl_shell_surface->children, surface);
  wl_shell_surface->parent_surface = parent;
}

static void
wl_shell_surface_set_transient (struct wl_client   *client,
                                struct wl_resource *resource,
                                struct wl_resource *parent_resource,
                                int32_t             x,
                                int32_t             y,
                                uint32_t            flags)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (wl_resource_get_user_data (resource));
  MetaWaylandSurface *surface =
    surface_from_wl_shell_surface_resource (resource);
  MetaWaylandSurface *parent_surf = wl_resource_get_user_data (parent_resource);

  wl_shell_surface_set_state (surface,
                              META_WL_SHELL_SURFACE_STATE_TRANSIENT);

  set_wl_shell_surface_parent (surface, parent_surf);
  wl_shell_surface->x = x;
  wl_shell_surface->y = y;

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
  MetaWaylandSurface *surface =
    surface_from_wl_shell_surface_resource (resource);

  wl_shell_surface_set_state (surface,
                              META_WL_SHELL_SURFACE_STATE_FULLSCREEN);
}

static void
meta_wayland_wl_shell_surface_create_popup (MetaWaylandWlShellSurface *wl_shell_surface)
{
  MetaWaylandPopupSurface *popup_surface =
    META_WAYLAND_POPUP_SURFACE (wl_shell_surface);
  MetaWaylandSeat *seat = wl_shell_surface->popup_seat;
  MetaWaylandPopup *popup;

  popup = meta_wayland_pointer_start_popup_grab (&seat->pointer, popup_surface);
  if (!popup)
    {
      wl_shell_surface_send_popup_done (wl_shell_surface->resource);
      return;
    }

  wl_shell_surface->popup = popup;
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
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (wl_resource_get_user_data (resource));
  MetaWaylandSurface *surface =
    surface_from_wl_shell_surface_resource (resource);
  MetaWaylandSurface *parent_surf = wl_resource_get_user_data (parent_resource);
  MetaWaylandSeat *seat = wl_resource_get_user_data (seat_resource);

  if (wl_shell_surface->popup)
    {
      wl_shell_surface->parent_surface = NULL;

      meta_wayland_popup_dismiss (wl_shell_surface->popup);
    }

  wl_shell_surface_set_state (surface,
                              META_WL_SHELL_SURFACE_STATE_POPUP);

  if (!meta_wayland_seat_can_popup (seat, serial))
    {
      wl_shell_surface_send_popup_done (resource);
      return;
    }

  set_wl_shell_surface_parent (surface, parent_surf);
  wl_shell_surface->popup_seat = seat;
  wl_shell_surface->x = x;
  wl_shell_surface->y = y;
  wl_shell_surface->pending_popup = TRUE;

  if (surface->window && parent_surf->window)
    sync_wl_shell_parent_relationship (surface, parent_surf);
}

static void
wl_shell_surface_set_maximized (struct wl_client   *client,
                                struct wl_resource *resource,
                                struct wl_resource *output)
{
  MetaWaylandSurface *surface =
    surface_from_wl_shell_surface_resource (resource);

  wl_shell_surface_set_state (surface,
                              META_WL_SHELL_SURFACE_STATE_MAXIMIZED);
}

static void
wl_shell_surface_set_title (struct wl_client   *client,
                            struct wl_resource *resource,
                            const char         *title)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (wl_resource_get_user_data (resource));
  MetaWaylandSurface *surface =
    surface_from_wl_shell_surface_resource (resource);

  g_clear_pointer (&wl_shell_surface->title, g_free);

  if (!g_utf8_validate (title, -1, NULL))
    title = "";

  wl_shell_surface->title = g_strdup (title);

  if (surface->window)
    meta_window_set_title (surface->window, title);
}

static void
wl_shell_surface_set_class (struct wl_client *client,
                            struct wl_resource *resource,
                            const char *class_)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (wl_resource_get_user_data (resource));
  MetaWaylandSurface *surface =
    surface_from_wl_shell_surface_resource (resource);

  g_clear_pointer (&wl_shell_surface->wm_class, g_free);

  if (!g_utf8_validate (class_, -1, NULL))
    class_ = "";

  wl_shell_surface->wm_class = g_strdup (class_);

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
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (surface->role);

  meta_window_set_transient_for (surface->window, parent->window);

  if (wl_shell_surface->state == META_WL_SHELL_SURFACE_STATE_POPUP ||
      wl_shell_surface->state == META_WL_SHELL_SURFACE_STATE_TRANSIENT)
    meta_window_wayland_place_relative_to (surface->window,
                                           parent->window,
                                           wl_shell_surface->x,
                                           wl_shell_surface->y);

  if (wl_shell_surface->state == META_WL_SHELL_SURFACE_STATE_POPUP &&
      wl_shell_surface->pending_popup)
    {
      meta_wayland_wl_shell_surface_create_popup (wl_shell_surface);
      wl_shell_surface->pending_popup = FALSE;
    }
}

static void
create_wl_shell_surface_window (MetaWaylandSurface *surface)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (surface->role);
  MetaWaylandSurface *parent;
  GList *l;

  surface->window = meta_window_wayland_new (meta_get_display (), surface);
  meta_wayland_surface_set_window (surface, surface->window);

  if (wl_shell_surface->title)
    meta_window_set_title (surface->window, wl_shell_surface->title);
  if (wl_shell_surface->wm_class)
    meta_window_set_wm_class (surface->window,
                              wl_shell_surface->wm_class,
                              wl_shell_surface->wm_class);

  parent = wl_shell_surface->parent_surface;
  if (parent && parent->window)
    sync_wl_shell_parent_relationship (surface, parent);

  for (l = wl_shell_surface->children; l; l = l->next)
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
  MetaWaylandWlShellSurface *wl_shell_surface;

  if (META_IS_WAYLAND_WL_SHELL_SURFACE (surface->role) &&
      META_WAYLAND_WL_SHELL_SURFACE (surface->role)->resource)
    {
      wl_resource_post_error (surface_resource,
                              WL_DISPLAY_ERROR_INVALID_OBJECT,
                              "wl_shell::get_shell_surface already requested");
      return;
    }

  if (!meta_wayland_surface_assign_role (surface,
                                         META_TYPE_WAYLAND_WL_SHELL_SURFACE,
                                         NULL))
    {
      wl_resource_post_error (resource, WL_SHELL_ERROR_ROLE,
                              "wl_surface@%d already has a different role",
                              wl_resource_get_id (surface->resource));
      return;
    }

  wl_shell_surface = META_WAYLAND_WL_SHELL_SURFACE (surface->role);
  wl_shell_surface->resource =
    wl_resource_create (client,
                        &wl_shell_surface_interface,
                        wl_resource_get_version (resource),
                        id);
  wl_resource_set_implementation (wl_shell_surface->resource,
                                  &meta_wayland_wl_shell_surface_interface,
                                  wl_shell_surface,
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
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (surface_role);
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);
  MetaWindow *window = surface->window;
  MetaRectangle geom = { 0 };

  surface_role_class =
    META_WAYLAND_SURFACE_ROLE_CLASS (meta_wayland_wl_shell_surface_parent_class);
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
      if (wl_shell_surface->popup)
        meta_wayland_popup_dismiss (wl_shell_surface->popup);
      else
        meta_wayland_surface_destroy_window (surface);
      return;
    }

  if (!window)
    return;

  if (!pending->newly_attached)
    return;

  meta_wayland_surface_apply_window_state (surface, pending);
  meta_wayland_surface_calculate_window_geometry (surface, &geom, 0, 0);
  meta_window_wayland_move_resize (window,
                                   NULL,
                                   geom, pending->dx, pending->dy);
}

static MetaWaylandSurface *
wl_shell_surface_role_get_toplevel (MetaWaylandSurfaceRole *surface_role)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (surface_role);

  if (wl_shell_surface->state == META_WL_SHELL_SURFACE_STATE_POPUP &&
      wl_shell_surface->parent_surface)
    return meta_wayland_surface_get_toplevel (wl_shell_surface->parent_surface);
  else
    return meta_wayland_surface_role_get_surface (surface_role);
}

static void
wl_shell_surface_role_configure (MetaWaylandSurfaceRoleShellSurface *shell_surface_role,
                                 int                                 new_width,
                                 int                                 new_height,
                                 MetaWaylandSerial                  *sent_serial)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (shell_surface_role);

  if (!wl_shell_surface->resource)
    return;

  wl_shell_surface_send_configure (wl_shell_surface->resource,
                                   0,
                                   new_width, new_height);
}

static void
wl_shell_surface_role_managed (MetaWaylandSurfaceRoleShellSurface *shell_surface_role,
                               MetaWindow                         *window)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (shell_surface_role);

  if (wl_shell_surface->state == META_WL_SHELL_SURFACE_STATE_POPUP)
    meta_window_set_type (window, META_WINDOW_DROPDOWN_MENU);
}

static void
wl_shell_surface_role_ping (MetaWaylandSurfaceRoleShellSurface *shell_surface_role,
                            guint32                             serial)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (shell_surface_role);

  wl_shell_surface_send_ping (wl_shell_surface->resource, serial);
}

static void
wl_shell_surface_role_close (MetaWaylandSurfaceRoleShellSurface *shell_surface_role)
{
  /* Not supported by wl_shell_surface. */
}

static void
meta_wayland_wl_shell_surface_popup_done (MetaWaylandPopupSurface *popup_surface)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (popup_surface);

  wl_shell_surface_send_popup_done (wl_shell_surface->resource);
}

static void
meta_wayland_wl_shell_surface_popup_dismiss (MetaWaylandPopupSurface *popup_surface)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (popup_surface);
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (popup_surface);
  MetaWaylandSurface *surface =
    meta_wayland_surface_role_get_surface (surface_role);

  wl_shell_surface->popup = NULL;

  meta_wayland_surface_destroy_window (surface);
}

static MetaWaylandSurface *
meta_wayland_wl_shell_surface_popup_get_surface (MetaWaylandPopupSurface *popup_surface)
{
  MetaWaylandSurfaceRole *surface_role =
    META_WAYLAND_SURFACE_ROLE (popup_surface);

  return meta_wayland_surface_role_get_surface (surface_role);
}

static void
popup_surface_iface_init (MetaWaylandPopupSurfaceInterface *iface)
{
  iface->done = meta_wayland_wl_shell_surface_popup_done;
  iface->dismiss = meta_wayland_wl_shell_surface_popup_dismiss;
  iface->get_surface = meta_wayland_wl_shell_surface_popup_get_surface;
}

static void
wl_shell_surface_role_finalize (GObject *object)
{
  MetaWaylandWlShellSurface *wl_shell_surface =
    META_WAYLAND_WL_SHELL_SURFACE (object);
  GObjectClass *object_class;

  g_clear_pointer (&wl_shell_surface->resource, wl_resource_destroy);

  object_class =
    G_OBJECT_CLASS (meta_wayland_wl_shell_surface_parent_class);
  object_class->finalize (object);
}

static void
meta_wayland_wl_shell_surface_init (MetaWaylandWlShellSurface *wl_shell_surface)
{
}

static void
meta_wayland_wl_shell_surface_class_init (MetaWaylandWlShellSurfaceClass *klass)
{
  GObjectClass *object_class;
  MetaWaylandSurfaceRoleClass *surface_role_class;
  MetaWaylandSurfaceRoleShellSurfaceClass *shell_surface_role_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = wl_shell_surface_role_finalize;

  surface_role_class = META_WAYLAND_SURFACE_ROLE_CLASS (klass);
  surface_role_class->commit = wl_shell_surface_role_commit;
  surface_role_class->get_toplevel = wl_shell_surface_role_get_toplevel;

  shell_surface_role_class =
    META_WAYLAND_SURFACE_ROLE_SHELL_SURFACE_CLASS (klass);
  shell_surface_role_class->configure = wl_shell_surface_role_configure;
  shell_surface_role_class->managed = wl_shell_surface_role_managed;
  shell_surface_role_class->ping = wl_shell_surface_role_ping;
  shell_surface_role_class->close = wl_shell_surface_role_close;
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
