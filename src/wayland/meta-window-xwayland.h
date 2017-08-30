/*
 * Copyright (C) 2017 Red Hat
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
 *
 */

#ifndef META_WINDOW_XWAYLAND_H
#define META_WINDOW_XWAYLAND_H

#include "meta/window.h"
#include "x11/window-x11.h"
#include "x11/window-x11-private.h"

G_BEGIN_DECLS

#define META_TYPE_WINDOW_XWAYLAND (meta_window_xwayland_get_type())
G_DECLARE_FINAL_TYPE (MetaWindowXwayland, meta_window_xwayland,
                      META, WINDOW_XWAYLAND, MetaWindowX11)

G_END_DECLS

#endif
