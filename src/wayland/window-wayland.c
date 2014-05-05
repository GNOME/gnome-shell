/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
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
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "window-wayland.h"

#include "window-private.h"
#include "boxes-private.h"
#include "stack-tracker.h"
#include "meta-wayland-surface.h"

struct _MetaWindowWayland
{
  MetaWindow parent;

  gboolean has_saved_pos;
  int saved_x;
  int saved_y;
};

struct _MetaWindowWaylandClass
{
  MetaWindowClass parent_class;
};

G_DEFINE_TYPE (MetaWindowWayland, meta_window_wayland, META_TYPE_WINDOW)

static void
meta_window_wayland_manage (MetaWindow *window)
{
  MetaDisplay *display = window->display;

  meta_display_register_wayland_window (display, window);

  {
    MetaStackWindow stack_window;
    stack_window.any.type = META_WINDOW_CLIENT_TYPE_WAYLAND;
    stack_window.wayland.meta_window = window;
    meta_stack_tracker_record_add (window->screen->stack_tracker,
                                   &stack_window,
                                   0);
  }
}

static void
meta_window_wayland_unmanage (MetaWindow *window)
{
  {
    MetaStackWindow stack_window;
    stack_window.any.type = META_WINDOW_CLIENT_TYPE_WAYLAND;
    stack_window.wayland.meta_window = window;
    meta_stack_tracker_record_remove (window->screen->stack_tracker,
                                      &stack_window,
                                      0);
  }

  meta_display_unregister_wayland_window (window->display, window);
}

static void
meta_window_wayland_ping (MetaWindow *window,
                          guint32     serial)
{
  meta_wayland_surface_ping (window->surface, serial);
}

static void
meta_window_wayland_delete (MetaWindow *window,
                            guint32     timestamp)
{
  meta_wayland_surface_delete (window->surface);
}

static void
meta_window_wayland_kill (MetaWindow *window)
{
  MetaWaylandSurface *surface = window->surface;
  struct wl_resource *resource = surface->resource;

  /* Send the client an unrecoverable error to kill the client. */
  wl_resource_post_error (resource,
                          WL_DISPLAY_ERROR_NO_MEMORY,
                          "User requested that we kill you. Sorry. Don't take it too personally.");
}

static void
meta_window_wayland_focus (MetaWindow *window,
                           guint32     timestamp)
{
  meta_display_set_input_focus_window (window->display,
                                       window,
                                       FALSE,
                                       timestamp);
}

static void
meta_window_wayland_move_resize_internal (MetaWindow                *window,
                                          int                        gravity,
                                          MetaRectangle              requested_rect,
                                          MetaRectangle              constrained_rect,
                                          MetaMoveResizeFlags        flags,
                                          MetaMoveResizeResultFlags *result)
{
  MetaWindowWayland *wl_window = META_WINDOW_WAYLAND (window);
  gboolean should_move = FALSE;

  g_assert (window->frame == NULL);

  /* For wayland clients, the size is completely determined by the client,
   * and while this allows to avoid some trickery with frames and the resulting
   * lagging, we also need to insist a bit when the constraints would apply
   * a different size than the client decides.
   *
   * Note that this is not generally a problem for normal toplevel windows (the
   * constraints don't see the size hints, or just change the position), but
   * it can be for maximized or fullscreen.
   */

  if (flags & META_IS_WAYLAND_RESIZE)
    {
      /* This is a call to wl_surface_commit(), ignore the constrained_rect and
       * update the real client size to match the buffer size.
       */

      *result |= META_MOVE_RESIZE_RESULT_RESIZED;
      window->rect.width = requested_rect.width;
      window->rect.height = requested_rect.height;

      /* This is a commit of an attach. We should move the window to match the
       * new position the client wants. */
      should_move = TRUE;
    }

  if (constrained_rect.width != window->rect.width ||
      constrained_rect.height != window->rect.height)
    {
      wl_window->has_saved_pos = TRUE;
      wl_window->saved_x = constrained_rect.x;
      wl_window->saved_y = constrained_rect.y;

      meta_wayland_surface_configure_notify (window->surface,
                                             constrained_rect.width,
                                             constrained_rect.height);
    }
  else
    {
      /* We're just moving the window, so we don't need to wait for a configure
       * and then ack to simply move the window. */
      should_move = TRUE;
    }

  if (should_move)
    {
      int new_x = constrained_rect.x;
      int new_y = constrained_rect.y;

      if (new_x != window->rect.x || new_y != window->rect.y)
        {
          *result |= META_MOVE_RESIZE_RESULT_MOVED;
          window->rect.x = new_x;
          window->rect.y = new_y;
        }
    }
}

static void
surface_state_changed (MetaWindow *window)
{
  meta_wayland_surface_configure_notify (window->surface,
                                         window->rect.width,
                                         window->rect.height);
}

static void
appears_focused_changed (GObject    *object,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  MetaWindow *window = META_WINDOW (object);

  /* When we're unmanaging, we remove focus from the window,
   * causing this to fire. Don't do anything in that case. */
  if (window->unmanaging)
    return;

  surface_state_changed (window);
}

static void
meta_window_wayland_init (MetaWindowWayland *wl_window)
{
  MetaWindow *window = META_WINDOW (wl_window);

  g_signal_connect (window, "notify::appears-focused",
                    G_CALLBACK (appears_focused_changed), NULL);
}

static void
meta_window_wayland_class_init (MetaWindowWaylandClass *klass)
{
  MetaWindowClass *window_class = META_WINDOW_CLASS (klass);

  window_class->manage = meta_window_wayland_manage;
  window_class->unmanage = meta_window_wayland_unmanage;
  window_class->ping = meta_window_wayland_ping;
  window_class->delete = meta_window_wayland_delete;
  window_class->kill = meta_window_wayland_kill;
  window_class->focus = meta_window_wayland_focus;
  window_class->move_resize_internal = meta_window_wayland_move_resize_internal;
}

/**
 * meta_window_move_resize_wayland:
 *
 * Complete a resize operation from a wayland client.
 */
void
meta_window_wayland_move_resize (MetaWindow *window,
                                 int         width,
                                 int         height,
                                 int         dx,
                                 int         dy)
{
  MetaWindowWayland *wl_window = META_WINDOW_WAYLAND (window);
  int gravity;
  MetaRectangle rect;
  MetaMoveResizeFlags flags;

  flags = META_IS_WAYLAND_RESIZE;

  /* x/y are ignored when we're doing interactive resizing */
  if (!meta_grab_op_is_resizing (window->display->grab_op))
    {
      if (wl_window->has_saved_pos)
        {
          rect.x = wl_window->saved_x;
          rect.y = wl_window->saved_y;
          wl_window->has_saved_pos = FALSE;
          flags |= META_IS_MOVE_ACTION;
        }
      else
        {
          meta_window_get_position (window, &rect.x, &rect.y);
        }

      if (dx != 0 || dy != 0)
        {
          rect.x += dx;
          rect.y += dy;
          flags |= META_IS_MOVE_ACTION;
        }
    }

  rect.width = width;
  rect.height = height;

  if (rect.width != window->rect.width || rect.height != window->rect.height)
    flags |= META_IS_RESIZE_ACTION;

  gravity = meta_resize_gravity_from_grab_op (window->display->grab_op);
  meta_window_move_resize_internal (window, flags, gravity, rect);
  meta_window_save_user_window_placement (window);
}
