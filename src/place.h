/* Metacity window placement */

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

#ifndef META_PLACE_H
#define META_PLACE_H

#include "window.h"
#include "frame.h"

void meta_window_place (MetaWindow *window,
                        MetaFrameGeometry *fgeom,
                        int         x,
                        int         y,
                        int        *new_x,
                        int        *new_y);

/* Returns the position to move the window to in order
 * to snap it to the next edge in the given direction,
 * while moving.
 */
int meta_window_find_next_vertical_edge   (MetaWindow *window,
                                           gboolean    right);
int meta_window_find_next_horizontal_edge (MetaWindow *window,
                                           gboolean    down);

/* Returns the position to move the window to in order
 * to snap it to the nearest edge, while moving.
 */
int meta_window_find_nearest_vertical_edge (MetaWindow *window,
                                            int         x_pos);

int meta_window_find_nearest_horizontal_edge (MetaWindow *window,
                                              int         y_pos);

/* FIXME need edge-snap functions for resizing as well, those
 * behave somewhat differently.
 */

#endif




