/* Metacity size/position constraints */

/* 
 * Copyright (C) 2002 Red Hat, Inc.
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

#include <constraints.h>

/* The way this code works was suggested by Owen Taylor.
 *
 * For any move_resize, we determine which variables are "free
 * variables" - stated another way, which edges of the window are
 * moving. During the move resize, we only want to modify those
 * variables; otherwise the constraint process can have peculiar side
 * effects when the size and position constraints interact. For
 * example, resizing a window from the top might go wrong when
 * position constraints apply to the top edge, and result in the
 * bottom edge moving downward while the top stays fixed.
 *
 * After selecting the variables we plan to vary, we define
 * each constraint on the window in terms of those variables.
 * 
 * Trivial example, say we are resizing vertically from the top of the
 * window. In that case we are applying the user's mouse motion delta
 * to an original size and position, note that dy is positive to
 * resize downward:
 *
 *   new_height = orig_height - dy;
 *   new_y      = orig_y + dy;
 *
 * A constraint that the position can't go above the top panel would
 * look like this:
 *
 *   new_y > screen_top_bound
 *
 * Substitute:
 *
 *   orig_y + dy > screen_top_bound
 *
 * Find the "boundary point" by changing to an equality:
 *
 *   orig_y + dy = screen_top_bound
 *
 * Solve:
 *
 *   dy = screen_top_bound - orig_y
 *
 * Plug that back into the size/position computations:
 *
 *   new_y = orig_y + screen_top_bound - orig_y
 *   new_y = screen_top_bound
 *   new_height = orig_height - screen_top_bound + orig_y;
 * 
 * This way the constraint is applied simultaneously to size/position,
 * so you aren't running the risk of constraining one but still
 * changing the other. i.e. we've converted an operation that may
 * modify both the Y position and the height of the window into an
 * operation that modifies a single variable, dy.  That variable is
 * then constrained, rather than the constraining the Y pos and height
 * separately.
 *
 */


/* To adjust for window gravity, such as a client moving itself to the
 * southeast corner, we want to compute the gravity reference point
 * - (screen_width,screen_height) in the SE corner case - using the
 * size the client had in its configure request. But then we want
 * to compute the actual position we intend to land on using
 * the real constrained dimensions of the window.
 *
 * So for a window being placed in the SE corner and simultaneously
 * resized, we get the gravity reference point, then compute where the
 * window should go to maintain that ref. point at its current size
 * instead of at the requested size, and conceptually move the window
 * to the requested ref. point but at its current size, without
 * applying any constraints. Then we constrain it with the top and
 * left edges as the edges that vary, with a dx/dy that are the delta
 * from the current size to the requested size. 
 *
 */

typedef void (* MetaConstraintFunc) (MetaWindow          *window,
                                     MetaFrameGeometry   *fgeom,
                                     const MetaRectangle *orig,
                                     MetaRectangle       *new);

enum
{
  VERTICAL_TOP,
  VERTICAL_BOTTOM,
  VERTICAL_CENTER
};

enum
{
  HORIZONTAL_LEFT,
  HORIZONTAL_RIGHT,
  HORIZONTAL_CENTER
};

/* Maximization constraint:
 *
 *  new_x = workarea_x + frame_left
 *  new_y = workarea_y + frame_top
 *  new_w = workarea_w - frame_left - frame_right
 *  new_h = workarea_h - frame_top - frame_bottom
 *
 * No need to do anything hard because it just locks specific
 * size/pos
 */

void
meta_window_constrain (MetaWindow          *window,
                       MetaFrameGeometry   *fgeom,
                       int                  resize_gravity,
                       const MetaRectangle *orig,
                       MetaRectangle       *new)
{



}









