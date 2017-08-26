/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter X display handler */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2003 Rob Adams
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

#ifndef META_X11_DISPLAY_PRIVATE_H
#define META_X11_DISPLAY_PRIVATE_H

#include <glib.h>
#include <X11/Xlib.h>

#include "core/display-private.h"
#include "meta/common.h"
#include "meta/types.h"
#include "meta/meta-x11-display.h"

struct _MetaX11Display
{
  GObject parent;

  MetaDisplay *display;

  char *name;
  char *screen_name;

  Display *xdisplay;
  Window xroot;
  int default_depth;
  Visual *default_xvisual;

  /* Pull in all the names of atoms as fields; we will intern them when the
   * class is constructed.
   */
#define item(x) Atom atom_##x;
#include "x11/atomnames.h"
#undef item

  int composite_event_base;
  int composite_error_base;
  int composite_major_version;
  int composite_minor_version;
  int damage_event_base;
  int damage_error_base;
  int xfixes_event_base;
  int xfixes_error_base;
  int xinput_error_base;
  int xinput_event_base;
  int xinput_opcode;
  int xsync_event_base;
  int xsync_error_base;
  int shape_event_base;
  int shape_error_base;
  unsigned int have_xsync : 1;
#define META_X11_DISPLAY_HAS_XSYNC(x11_display) ((x11_display)->have_xsync)
  unsigned int have_shape : 1;
#define META_X11_DISPLAY_HAS_SHAPE(x11_display) ((x11_display)->have_shape)
  unsigned int have_composite : 1;
  unsigned int have_damage : 1;
#define META_X11_DISPLAY_HAS_COMPOSITE(x11_display) ((x11_display)->have_composite)
#define META_X11_DISPLAY_HAS_DAMAGE(x11_display) ((x11_display)->have_damage)
#ifdef HAVE_XI23
  gboolean have_xinput_23 : 1;
#define META_X11_DISPLAY_HAS_XINPUT_23(x11_display) ((x11_display)->have_xinput_23)
#else
#define META_X11_DISPLAY_HAS_XINPUT_23(x11_display) FALSE
#endif /* HAVE_XI23 */
};

MetaX11Display *meta_x11_display_new (MetaDisplay *display, GError **error);

Window meta_x11_display_create_offscreen_window (MetaX11Display *x11_display,
                                                 Window          parent,
                                                 long            valuemask);

Cursor meta_x11_display_create_x_cursor (MetaX11Display *x11_display,
                                         MetaCursor      cursor);

void meta_x11_display_reload_cursor (MetaX11Display *x11_display);

#endif /* META_X11_DISPLAY_PRIVATE_H */
