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
 *   new_y >= screen_top_bound
 *
 * Substitute:
 *
 *   orig_y + dy >= screen_top_bound
 *
 * Find the "boundary point" by changing to an equality:
 *
 *   orig_y + dy = screen_top_bound
 *
 * Solve:
 *
 *   dy = screen_top_bound - orig_y
 *
 * This dy is now the _maximum_ dy and you constrain dy with that
 * value, applying it to both the move and the resize:
 *
 *   new_height = orig_height - dy;
 *   new_y      = orig_y + dy;
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
 * This method applies to any ConfigureRequest that does a simultaneous
 * move/resize.
 *
 */

typedef void (* MetaConstraintFunc) (MetaWindow          *window,
                                     MetaFrameGeometry   *fgeom,
                                     const MetaRectangle *orig,
                                     MetaRectangle       *new);


/* Things we can move, constraints apply
 * in the context of these dimensions
 */
enum
{
  RESIZE_TOP,
  RESIZE_BOTTOM,
  RESIZE_LEFT,
  RESIZE_RIGHT,
  RESIZE_VERTICAL_CENTER,
  RESIZE_HORIZONTAL_CENTER,
  MOVE_VERTICAL,
  MOVE_HORIZONTAL
};

/* Maximization constraint:
 *
 *  new_x = workarea_x + frame_left
 *  new_y = workarea_y + frame_top
 *  new_w = workarea_w - frame_left - frame_right
 *  new_h = workarea_h - frame_top - frame_bottom
 *
 * No need to do anything hard because it just locks specific
 * size/pos.
 *
 * The min/max size constraints override maximization.
 */

/* Full screen constraint:
 *
 * new_x = 0;
 * new_y = 0;
 * new_w = xinerama_width;
 * new_h = xinerama_height;
 *
 * The min/max size constraints override fullscreen.
 */

/* Titlebar is onscreen constraint:
 *
 * Constants:
 *  titlebar_width_onscreen = amount of titlebar width that has to be onscreen
 *  nw_x, nw_y = left/top edges that titlebar can't go outside
 *  se_x, se_y = right/bottom edges
 *
 * NW limit has priority over SE, since titlebar is on NW
 *
 * RESIZE_LEFT:
 *   new_width = orig_width + dx
 *   new_x = orig_x - dx
 * 
 *   new_x >= nw_x - (left_width + new_width + right_width - titlebar_width_onscreen)
 *
 *   orig_x - dx >= nw_x - (left_width + orig_width + dx + right_width - titlebar_width_onscreen)
 *   0 >= nw_x - left_width - orig_width - right_width + titlebar_width_onscreen - orig_x
 *
 *   i.e. dx drops out so there is no constraint at all when moving left edge.
 *
 * RESIZE_RIGHT and RESIZE_BOTTOM are the same, cannot break this constraint
 * by moving in those directions.
 *
 * RESIZE_TOP:
 *
 *  new_height = orig_height - dy
 *  new_y      = orig_y + dy
 *
 * Can't move titlebar off the top at all regardless of height:
 *  new_y >= nw_y + top_height
 *
 *  orig_y + dy = nw_y + top_height
 *  dy = nw_y + top_height - orig_y
 *
 * Max dy is thus (nw_y + top_height - orig_y)
 *
 * RESIZE_VERTICAL_CENTER:
 *
 *    
 *       
 *   
 */

void
meta_window_constrain (MetaWindow          *window,
                       MetaFrameGeometry   *orig_fgeom,
                       int                  resize_gravity,
                       const MetaRectangle *orig,
                       MetaRectangle       *new)
{
  MetaFrameGeometry fgeom;

  /* Create a fake frame geometry if none really exists */
  if (orig_fgeom)
    fgeom = *orig_fgeom;
  else
    {
      fgeom.top_height = 0;
      fgeom.bottom_height = 0;
      fgeom.left_width = 0;
      fgeom.right_width = 0;
    }

  
  

}









