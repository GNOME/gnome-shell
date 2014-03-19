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
meta_window_wayland_init (MetaWindowWayland *window_wayland)
{
}

static void
meta_window_wayland_class_init (MetaWindowWaylandClass *klass)
{
  MetaWindowClass *window_class = META_WINDOW_CLASS (klass);

  window_class->manage = meta_window_wayland_manage;
  window_class->unmanage = meta_window_wayland_unmanage;
}
