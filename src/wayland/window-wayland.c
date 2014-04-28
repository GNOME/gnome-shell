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
      if (constrained_rect.x != window->rect.x ||
          constrained_rect.y != window->rect.y)
        {
          *result |= META_MOVE_RESIZE_RESULT_MOVED;
          window->rect.x = constrained_rect.x;
          window->rect.y = constrained_rect.y;
        }
    }
}

static void
meta_window_wayland_init (MetaWindowWayland *window_wayland)
{
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
