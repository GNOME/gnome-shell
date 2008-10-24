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

#ifndef META_SCREEN_H
#define META_SCREEN_H

#include <X11/Xlib.h>
#include <glib.h>
#include "types.h"

int meta_screen_get_screen_number (MetaScreen *screen);
MetaDisplay *meta_screen_get_display (MetaScreen *screen);

Window meta_screen_get_xroot (MetaScreen *screen);
void meta_screen_get_size (MetaScreen *screen,
                           int        *width,
                           int        *height);

gpointer meta_screen_get_compositor_data (MetaScreen *screen);
void meta_screen_set_compositor_data (MetaScreen *screen,
                                      gpointer    info);

MetaScreen *meta_screen_for_x_screen (Screen *xscreen);

#ifdef HAVE_COMPOSITE_EXTENSIONS
void meta_screen_set_cm_selection (MetaScreen *screen);
void meta_screen_unset_cm_selection (MetaScreen *screen);
#endif

GList *meta_screen_get_workspaces (MetaScreen *screen);

MetaWorkspace* meta_screen_get_workspace_by_index (MetaScreen    *screen,
                                                   int            index);
void meta_screen_remove_workspace (MetaScreen    *screen,
                                   MetaWorkspace *workspace,
                                   guint32        timestamp);

#endif
