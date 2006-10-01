/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity RGB color stuff */

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

#ifndef META_COLORS_H
#define META_COLORS_H

/* This stuff will all just be XlibRGB eventually. Right now
 * it has a stub implementation.
 */

#include "screen.h"
#include "util.h"
#include "api.h"
gulong              meta_screen_get_x_pixel      (MetaScreen         *screen,
                                                  const PangoColor   *color);
void                meta_screen_init_visual_info (MetaScreen         *screen);
void                meta_screen_set_ui_colors    (MetaScreen         *screen,
                                                  const MetaUIColors *colors);
void                meta_screen_init_ui_colors   (MetaScreen         *screen);


#endif
