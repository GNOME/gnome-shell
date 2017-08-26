/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2008 Iain Holmes
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

#ifndef META_X11_DISPLAY_H
#define META_X11_DISPLAY_H

#include <glib-object.h>
#include <X11/Xlib.h>

#include <meta/common.h>
#include <meta/prefs.h>
#include <meta/types.h>

#define META_TYPE_X11_DISPLAY (meta_x11_display_get_type ())
G_DECLARE_FINAL_TYPE (MetaX11Display, meta_x11_display, META, X11_DISPLAY, GObject)

int      meta_x11_display_get_screen_number (MetaX11Display *x11_display);
Display *meta_x11_display_get_xdisplay      (MetaX11Display *x11_display);
Window   meta_x11_display_get_xroot         (MetaX11Display *x11_display);

int      meta_x11_display_get_xinput_opcode     (MetaX11Display *x11_display);
int      meta_x11_display_get_damage_event_base (MetaX11Display *x11_display);
int      meta_x11_display_get_shape_event_base  (MetaX11Display *x11_display);
gboolean meta_x11_display_has_shape             (MetaX11Display *x11_display);

#endif /* META_X11_DISPLAY_H */
