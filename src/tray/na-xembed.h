/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* na-xembed.h
 * Copyright (C) 2022 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef NA_XEMBED_H
#define NA_XEMBED_H

#include <meta/meta-x11-display.h>
#include <glib-object.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

#define NA_TYPE_XEMBED (na_xembed_get_type ())
G_DECLARE_DERIVABLE_TYPE (NaXembed, na_xembed, NA, XEMBED, GObject)

struct _NaXembedClass
{
  GObjectClass parent_class;

  void (* plug_added) (NaXembed *xembed);
  void (* plug_removed) (NaXembed *xembed);
};

MetaX11Display * na_xembed_get_x11_display (NaXembed *xembed);

void na_xembed_add_id (NaXembed *xembed,
		       Window    window);

Window na_xembed_get_plug_window (NaXembed *xembed);

Window na_xembed_get_socket_window (NaXembed *xembed);

void na_xembed_set_root_position (NaXembed *xembed,
				  int       x,
				  int       y);

void na_xembed_get_size (NaXembed *xembed,
			 int      *width,
			 int      *height);

void na_xembed_set_background_color (NaXembed           *xembed,
				     const ClutterColor *color);

G_END_DECLS

#endif /* NA_XEMBED_H */
