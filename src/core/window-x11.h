/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2003, 2004 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_WINDOW_X11_H
#define META_WINDOW_X11_H

#include <meta/window.h>
#include <X11/Xlib.h>

void meta_window_x11_set_net_wm_state  (MetaWindow *window);

void meta_window_x11_update_role (MetaWindow *window);
void meta_window_x11_update_net_wm_type (MetaWindow *window);
void meta_window_x11_update_opaque_region (MetaWindow *window);
void meta_window_x11_update_input_region  (MetaWindow *window);
void meta_window_x11_update_shape_region  (MetaWindow *window);

gboolean meta_window_x11_configure_request       (MetaWindow *window,
                                                  XEvent     *event);
gboolean meta_window_x11_property_notify         (MetaWindow *window,
                                                  XEvent     *event);
gboolean meta_window_x11_client_message          (MetaWindow *window,
                                                  XEvent     *event);

#endif
