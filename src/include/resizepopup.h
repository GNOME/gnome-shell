/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter resizing-terminal-window feedback */

/* 
 * Copyright (C) 2001 Havoc Pennington
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
 */

#ifndef META_RESIZEPOPUP_H
#define META_RESIZEPOPUP_H

/* Don't include gtk.h or gdk.h here */
#include "boxes.h"
#include "common.h"
#include <X11/Xlib.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

MetaResizePopup* meta_ui_resize_popup_new          (Display *display,
                                                    int      screen_number);
void             meta_ui_resize_popup_free         (MetaResizePopup *popup);
void             meta_ui_resize_popup_set (MetaResizePopup *popup,
                                           MetaRectangle    rect,
                                           int              base_width,
                                           int              base_height,
                                           int              width_inc,
                                           int              height_inc);
void             meta_ui_resize_popup_set_showing  (MetaResizePopup *popup,
                                                    gboolean         showing);

#endif

