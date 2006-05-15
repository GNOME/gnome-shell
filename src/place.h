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

typedef enum _MetaWindowEdgePosition MetaWindowEdgePosition;

enum _MetaWindowEdgePosition
{
  META_WINDOW_EDGE_TOP = 0,
  META_WINDOW_EDGE_LEFT,
  META_WINDOW_EDGE_RIGHT,
  META_WINDOW_EDGE_BOTTOM
};

void meta_window_place (MetaWindow *window,
                        MetaFrameGeometry *fgeom,
                        int         x,
                        int         y,
                        int        *new_x,
                        int        *new_y);

#endif
