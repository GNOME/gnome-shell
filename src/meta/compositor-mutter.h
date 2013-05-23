/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2008 Matthew Allum
 * Copyright (C) 2007 Iain Holmes
 * Based on xcompmgr - (c) 2003 Keith Packard
 *          xfwm4    - (c) 2005-2007 Olivier Fourdan
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

#ifndef MUTTER_H_
#define MUTTER_H_

#include <clutter/clutter.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include <meta/types.h>
#include <meta/compositor.h>
#include <meta/meta-window-actor.h>

/* Public compositor API */
ClutterActor *meta_get_stage_for_screen         (MetaScreen *screen);
Window        meta_get_overlay_window           (MetaScreen *screen);
GList        *meta_get_window_actors            (MetaScreen *screen);
ClutterActor *meta_get_window_group_for_screen  (MetaScreen *screen);
ClutterActor *meta_get_top_window_group_for_screen (MetaScreen *screen);

void        meta_disable_unredirect_for_screen  (MetaScreen *screen);
void        meta_enable_unredirect_for_screen   (MetaScreen *screen);

void meta_set_stage_input_region     (MetaScreen    *screen,
                                      XserverRegion  region);
void meta_empty_stage_input_region   (MetaScreen    *screen);
void meta_focus_stage_window         (MetaScreen    *screen,
                                      guint32        timestamp);

#endif
