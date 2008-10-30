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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_WINDOW_H
#define META_WINDOW_H

#include <glib.h>
#include <X11/Xlib.h>

#include "boxes.h"
#include "types.h"

typedef enum
{
  META_WINDOW_NORMAL,
  META_WINDOW_DESKTOP,
  META_WINDOW_DOCK,
  META_WINDOW_DIALOG,
  META_WINDOW_MODAL_DIALOG,
  META_WINDOW_TOOLBAR,
  META_WINDOW_MENU,
  META_WINDOW_UTILITY,
  META_WINDOW_SPLASHSCREEN,
} MetaWindowType;

typedef enum
{
  META_MAXIMIZE_HORIZONTAL = 1 << 0,
  META_MAXIMIZE_VERTICAL   = 1 << 1
} MetaMaximizeFlags;

MetaFrame *meta_window_get_frame (MetaWindow *window);
gboolean meta_window_has_focus (MetaWindow *window);
gboolean meta_window_is_shaded (MetaWindow *window);
MetaRectangle *meta_window_get_rect (MetaWindow *window);
MetaScreen *meta_window_get_screen (MetaWindow *window);
MetaDisplay *meta_window_get_display (MetaWindow *window);
Window meta_window_get_xwindow (MetaWindow *window);
MetaWindowType meta_window_get_type (MetaWindow *window);
Atom meta_window_get_type_atom (MetaWindow *window);
MetaWorkspace *meta_window_get_workspace (MetaWindow *window);
gboolean meta_window_is_on_all_workspaces (MetaWindow *window);
gboolean meta_window_is_hidden (MetaWindow *window);
void     meta_window_activate  (MetaWindow *window,guint32 current_time);
void     meta_window_activate_with_workspace (MetaWindow    *window,
                                              guint32        current_time,
                                              MetaWorkspace *workspace);
const char * meta_window_get_description (MetaWindow *window);

#endif
