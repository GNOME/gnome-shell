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

#include "constraints.h"
#include "place.h"

/* The way this code works was suggested by Owen Taylor.
 *
 * For any move_resize, we determine which variables are "free
 * variables" and apply constraints in terms of those. During the move
 * resize, we only want to modify those variables; otherwise the
 * constraint process can have peculiar side effects when the size and
 * position constraints interact. For example, resizing a window from
 * the top might go wrong when position constraints apply to the top
 * edge, and result in the bottom edge moving downward while the top
 * stays fixed.
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
 * separately. This is a rather complicated fix for an obscure bug
 * that happened when resizing a window and encountering a constraint
 * such as the top edge of the screen.
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
 * We use the same method to e.g. maximize a window; if the window is
 * maximized, we want to MOVE_VERTICAL/MOVE_HORIZONTAL to the top
 * center of the screen, then RESIZE_BOTTOM and
 * RESIZE_HORIZONTAL_CENTER. i.e. essentially NorthGravity.
 * 
 */

typedef struct
{
  MetaWindow *window;
  MetaFrameGeometry fgeom;
  const MetaXineramaScreenInfo *xinerama;
  MetaRectangle work_area_xinerama;
  MetaRectangle work_area_screen;
  int nw_x, nw_y, se_x, se_y;
} ConstraintInfo;

/* (FIXME instead of TITLEBAR_LENGTH_ONSCREEN, get the actual
 * size of the menu control?).
 */

#define TITLEBAR_LENGTH_ONSCREEN 36

typedef gboolean (* MetaConstraintAppliesFunc) (MetaWindow *window);

/* There's a function for each case with a different "free variable" */
typedef void (* MetaConstrainTopFunc)     (MetaWindow           *window,
                                           const ConstraintInfo *info,
                                           const MetaRectangle  *orig,
                                           int                  *y_delta);
typedef void (* MetaConstrainBottomFunc)  (MetaWindow           *window,
                                           const ConstraintInfo *info,
                                           const MetaRectangle  *orig,
                                           int                  *y_delta);
typedef void (* MetaConstrainVCenterFunc) (MetaWindow           *window,
                                           const ConstraintInfo *info,
                                           const MetaRectangle  *orig,
                                           int                  *y_delta);
typedef void (* MetaConstrainLeftFunc)    (MetaWindow           *window,
                                           const ConstraintInfo *info,
                                           const MetaRectangle  *orig,
                                           int                  *x_delta);
typedef void (* MetaConstrainRightFunc)   (MetaWindow           *window,
                                           const ConstraintInfo *info,
                                           const MetaRectangle  *orig,
                                           int                  *x_delta);
typedef void (* MetaConstrainHCenterFunc) (MetaWindow           *window,
                                           const ConstraintInfo *info,
                                           const MetaRectangle  *orig,
                                           int                  *x_delta);
typedef void (* MetaConstrainMoveFunc)    (MetaWindow           *window,
                                           const ConstraintInfo *info,
                                           const MetaRectangle  *orig,
                                           int                  *x_delta,
                                           int                  *y_delta);

typedef struct
{
  MetaConstraintAppliesFunc applies_func;
  MetaConstrainTopFunc      top_func;
  MetaConstrainBottomFunc   bottom_func;
  MetaConstrainVCenterFunc  vcenter_func;
  MetaConstrainLeftFunc     left_func;
  MetaConstrainRightFunc    right_func;
  MetaConstrainHCenterFunc  hcenter_func;
  MetaConstrainMoveFunc     move_func;
} Constraint;

/* "Is the desktop window" constraint:
 *
 *  new_x = 0;
 *  new_y = 0;
 *  new_w = orig_width;
 *  new_h = orig_height;
 *
 * Note that if we are applying a resize constraint,
 * e.g. constraint_desktop_top_func, this is kind of broken since we
 * end up resizing the window in order to get its position right. But
 * that case shouldn't happen in practice.
 */
static gboolean
constraint_desktop_applies_func (MetaWindow *window)
{
  return window->type == META_WINDOW_DESKTOP;
}

static void
constraint_desktop_top_func     (MetaWindow           *window,
                                 const ConstraintInfo *info,
                                 const MetaRectangle  *orig,
                                 int                  *y_delta)
{
  *y_delta = 0 - orig->y;
}

static void
constraint_desktop_bottom_func  (MetaWindow           *window,
                                 const ConstraintInfo *info,
                                 const MetaRectangle  *orig,
                                 int                  *y_delta)
{
  /* nothing */
}

static void
constraint_desktop_vcenter_func (MetaWindow           *window,
                                 const ConstraintInfo *info,
                                 const MetaRectangle  *orig,
                                 int                  *y_delta)
{
  *y_delta = 0 - orig->y;
}

static void
constraint_desktop_left_func    (MetaWindow           *window,
                                 const ConstraintInfo *info,
                                 const MetaRectangle  *orig,
                                 int                  *x_delta)
{
  *x_delta = 0 - orig->x;
}

static void
constraint_desktop_right_func   (MetaWindow           *window,
                                 const ConstraintInfo *info,
                                 const MetaRectangle  *orig,
                                 int                  *x_delta)
{
  /* nothing */
}

static void
constraint_desktop_hcenter_func (MetaWindow           *window,
                                 const ConstraintInfo *info,
                                 const MetaRectangle  *orig,
                                 int                  *x_delta)
{
  *x_delta = 0 - orig->x;
}

static void
constraint_desktop_move_func    (MetaWindow           *window,
                                 const ConstraintInfo *info,
                                 const MetaRectangle  *orig,
                                 int                  *x_delta,
                                 int                  *y_delta)
{
  *x_delta = 0 - orig->x;  
  *y_delta = 0 - orig->y;
}

static const Constraint constraint_desktop = {
  constraint_desktop_applies_func,
  constraint_desktop_top_func,
  constraint_desktop_bottom_func,
  constraint_desktop_vcenter_func,
  constraint_desktop_left_func,
  constraint_desktop_right_func,
  constraint_desktop_hcenter_func,
  constraint_desktop_move_func
};

/* Titlebar is onscreen constraint:
 *
 * Constants:
 *  titlebar_width_onscreen = amount of titlebar width that has to be onscreen
 *  nw_x, nw_y = left/top edges that titlebar can't go outside
 *  se_x, se_y = right/bottom edges
 *
 * NW limit has priority over SE, since titlebar is on NW
 *
 * Left resize
 * ===
 * 
 *   new_width = orig_width - dx
 *   new_x = orig_x + dx
 *
 * Amount of window+frame that doesn't fit in the work area:
 *
 *   offscreen_width = left_width + new_width + right_width - (se_x - nw_x)
 *
 * If we keep the old metacity rule where a window can be offscreen by
 * offscreen_width, then the math works out that left/top resizes are not
 * constrained. If we instead have a rule where the window can never be offscreen,
 * you get the following:
 *
 *  new_x >= nw_x + left_width + titlebar_width_offscreen
 *  orig_x + dx >= nw_x + left_width + titlebar_width_onscreen
 *  dx >= nw_x + left_width + titlebar_width_onscreen - orig_x
 *
 * i.e. the minimum dx is: nw_x + left_width + titlebar_width_onscreen - orig_x
 *
 * We could have a more complicated rule that constrains only if the current
 * offscreen width is positive, thus allowing something more like the old
 * behavior, but not doing that for now.
 *
 * Top resize works the same as left resize. Right/bottom resize don't have a limit
 * because the constraint is designed to keep the top right corner of the
 * window or its titlebar on the screen, and right/bottom resize will never move that
 * area. Center resize is almost like left/top but dx has the opposite sign
 * and new_width = orig_width + 2dx.
 *
 * For right/bottom we can try to handle windows that aren't in a valid
 * location to begin with:
 *
 *  new_x <= se_x - titlebar_width_onscreen
 *  dx <= se_x - titlebar_width_onscreen - orig_x
 *
 * but in principle this constraint is never triggered.
 *
 * Vertical move
 * ===
 * 
 *  new_height = orig_height
 *  new_y = orig_y + dy
 *
 *  new_y >= nw_y + top_height
 *
 *  Min negative dy (nw_y + top_height - orig_y) just as with top resize.
 *  Max positive dy has to be computed from se_y and given less priority than the
 *  min negative:
 *
 *   new_y < se_y
 *   orig_y + dy = se_y
 *   so max dy is (se_y - orig_y)
 * 
 * Horizontal move is equivalent to vertical.
 *   
 */

static gboolean
constraint_onscreen_applies_func (MetaWindow *window)
{
  return
    window->type != META_WINDOW_DESKTOP &&
    window->type != META_WINDOW_DOCK;
}

static void
constraint_onscreen_top_func     (MetaWindow           *window,
                                  const ConstraintInfo *info,
                                  const MetaRectangle  *orig,
                                  int                  *y_delta)
{
  int min_dy;
  
  min_dy = info->nw_y + info->fgeom.top_height - orig->y;

  if (*y_delta < min_dy)
    *y_delta = min_dy;
}

static void
constraint_onscreen_bottom_func  (MetaWindow           *window,
                                  const ConstraintInfo *info,
                                  const MetaRectangle  *orig,
                                  int                  *y_delta)
{
  int max_dy;

  max_dy = info->se_y - info->fgeom.top_height - orig->y;

  if (*y_delta > max_dy)
    *y_delta = max_dy;
}

static void
constraint_onscreen_vcenter_func (MetaWindow           *window,
                                  const ConstraintInfo *info,
                                  const MetaRectangle  *orig,
                                  int                  *y_delta)
{
  int max_dy;
  
  max_dy = info->nw_y + info->fgeom.top_height - orig->y;
  max_dy = ABS (max_dy);
  
  if (*y_delta > max_dy)
    *y_delta = max_dy;
}

static void
constraint_onscreen_left_func    (MetaWindow           *window,
                                  const ConstraintInfo *info,
                                  const MetaRectangle  *orig,
                                  int                  *x_delta)
{
  int min_dx;
  
  min_dx = info->nw_x + info->fgeom.left_width + TITLEBAR_WIDTH_ONSCREEN - orig->x;

  if (*x_delta < min_dx)
    *x_delta = min_dx;
}

static void
constraint_onscreen_right_func   (MetaWindow           *window,
                                  const ConstraintInfo *info,
                                  const MetaRectangle  *orig,
                                  int                  *x_delta)
{
  int max_dx;

  max_dx = info->se_x - TITLEBAR_WIDTH_ONSCREEN - orig->x;

  if (*x_delta > max_dx)
    *x_delta = max_dx;
}

static void
constraint_onscreen_hcenter_func (MetaWindow           *window,
                                  const ConstraintInfo *info,
                                  const MetaRectangle  *orig,
                                  int                  *x_delta)
{
  int max_dx;
  
  max_dx = info->nw_x + info->fgeom.left_width + TITLEBAR_WIDTH_ONSCREEN - orig->x;
  max_dx = ABS (max_dx);
  
  if (*x_delta > max_dx)
    *x_delta = max_dx;
}

static void
constraint_onscreen_move_func    (MetaWindow           *window,
                                  const ConstraintInfo *info,
                                  const MetaRectangle  *orig,
                                  int                  *x_delta,
                                  int                  *y_delta)
{
  int min_delta;
  int max_delta;
  
  min_delta = info->nw_y + info->fgeom.top_height - orig->y;

  if (window->frame) /* if frame, the titlebar is always above the y pos */
    max_delta = info->se_y - orig->y;
  else               /* else keep some client area pixels on the screen */
    max_delta = info->se_y - orig->y - TITLEBAR_WIDTH_ONSCREEN;

  
  /* Note that min delta (top left) has priority over
   * max delta (bottom right) to facilitate keeping
   * titlebar on the screen
   */
  if (*y_delta > max_delta)
    *y_delta = max_delta;
  if (*y_delta < min_delta)
    *y_delta = min_delta;

  min_delta = info->nw_x + TITLEBAR_WIDTH_ONSCREEN - orig->x;
  max_delta = info->se_x - orig->x - TITLEBAR_WIDTH_ONSCREEN;

  if (*x_delta > max_delta)
    *x_delta = max_delta;
  if (*x_delta < min_delta)
    *x_delta = min_delta;
}

static const Constraint constraint_onscreen = {
  constraint_onscreen_applies_func,
  constraint_onscreen_top_func,
  constraint_onscreen_bottom_func,
  constraint_onscreen_vcenter_func,
  constraint_onscreen_left_func,
  constraint_onscreen_right_func,
  constraint_onscreen_hcenter_func,
  constraint_onscreen_move_func
};


/* Size hints constraints:
 * 
 * For min/max size we just clamp to those, and for resize increment
 * we clamp to the one at or below the requested place.
 *
 * For aspect ratio, we special-case it at the end of
 * meta_window_constrain, because it involves both dimensions, and
 * thus messes up our generic framework.
 *
 * Left resize:
 *   new_width = orig_width - dx
 *   new_x = orig_x + dx
 *
 *   new_width >= min_width
 *   orig_width - dx >= min_width
 *   - dx >= min_width - orig_width
 *   dx <= orig_width - min_width
 *
 *   new_width <= max_width
 *   orig_width - dx <= max_width
 *   - dx <= max_width - orig_width
 *   dx >= orig_width - max_width
 * 
 */

static gboolean
constraint_hints_applies_func (MetaWindow *window)
{
  return TRUE;
}

static void
constraint_hints_top_func     (MetaWindow           *window,
                               const ConstraintInfo *info,
                               const MetaRectangle  *orig,
                               int                  *y_delta)
{
  
}

static void
constraint_hints_bottom_func  (MetaWindow           *window,
                               const ConstraintInfo *info,
                               const MetaRectangle  *orig,
                               int                  *y_delta)
{

}

static void
constraint_hints_vcenter_func (MetaWindow           *window,
                               const ConstraintInfo *info,
                               const MetaRectangle  *orig,
                               int                  *y_delta)
{

}

static void
constraint_hints_left_func    (MetaWindow           *window,
                               const ConstraintInfo *info,
                               const MetaRectangle  *orig,
                               int                  *x_delta)
{
  int min_dx;
  int max_dx;
  int width;
  
  max_dx = orig->width - window->size_hints.min_width;
  min_dx = orig->width - window->size_hints.max_width;

  if (*x_delta > max_dx)
    *x_delta = max_dx;
  if (*x_delta < min_dx)
    *x_delta = min_dx;

  /* shrink to base + N * inc
   */
  width = orig->width - *x_delta;
  width = window->size_hints.base_width +
    FLOOR (width - window->size_hints.base_width, window->size_hints.width_inc);
  
  *x_delta = orig->width - width;
}

static void
constraint_hints_right_func   (MetaWindow           *window,
                               const ConstraintInfo *info,
                               const MetaRectangle  *orig,
                               int                  *x_delta)
{

}

static void
constraint_hints_hcenter_func (MetaWindow           *window,
                               const ConstraintInfo *info,
                               const MetaRectangle  *orig,
                               int                  *x_delta)
{

}

static void
constraint_hints_move_func    (MetaWindow           *window,
                               const ConstraintInfo *info,
                               const MetaRectangle  *orig,
                               int                  *x_delta,
                               int                  *y_delta)
{
  /* nothing */
}

static const Constraint constraint_hints = {
  constraint_hints_applies_func,
  constraint_hints_top_func,
  constraint_hints_bottom_func,
  constraint_hints_vcenter_func,
  constraint_hints_left_func,
  constraint_hints_right_func,
  constraint_hints_hcenter_func,
  constraint_hints_move_func
};

/* Array of all constraints at once */
static const Constraint *all_constraints[] = {
  &constraint_desktop,
  &constraint_onscreen,
  &constraint_hints,
  NULL
};

/* Move with no accompanying change to window size */
static void
constrain_move (MetaWindow           *window,
                const ConstraintInfo *info,
                const MetaRectangle  *orig,
                int                   x_delta,
                int                   y_delta,
                MetaRectangle        *new)
{
  const Constraint **cp;

  cp = &all_constraints[0];

  while (*cp)
    {
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->move_func) (window, info, orig,
                              &x_delta, &y_delta);

      ++cp;
    }

  new->x = orig->x + x_delta;
  new->y = orig->y + y_delta;
}

static void
constrain_resize_left (MetaWindow           *window,
                       const ConstraintInfo *info,
                       const MetaRectangle  *orig,
                       int                   x_delta,
                       MetaRectangle        *new)
{
  const Constraint **cp;

  cp = &all_constraints[0];

  while (*cp)
    {
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->left_func) (window, info, orig,
                              &x_delta);

      ++cp;
    }

  /* Moving mouse from 10 to 5 means current - orig means 5 - 10 means
   * a delta of -5
   */
  new->x = orig->x - x_delta;
  new->width = orig->width - x_delta;
}

static void
constrain_resize_hcenter (MetaWindow           *window,
                          const ConstraintInfo *info,
                          const MetaRectangle  *orig,
                          int                   x_delta,
                          MetaRectangle        *new)
{
  const Constraint **cp;

  cp = &all_constraints[0];

  while (*cp)
    {
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->hcenter_func) (window, info, orig,
                                 &x_delta);

      ++cp;
    }

  /* center deltas are positive to grow the window and negative to
   * shrink it.
   */
  new->x = orig->x - x_delta;
  new->width = orig->width + x_delta * 2;
}

static void
constrain_resize_right (MetaWindow           *window,
                        const ConstraintInfo *info,
                        const MetaRectangle  *orig,
                        int                   x_delta,
                        MetaRectangle        *new)
{
  const Constraint **cp;

  cp = &all_constraints[0];

  while (*cp)
    {
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->right_func) (window, info, orig,
                               &x_delta);

      ++cp;
    }

  new->width = orig->width + x_delta;
}

static void
constrain_resize_top (MetaWindow           *window,
                      const ConstraintInfo *info,
                      const MetaRectangle  *orig,
                      int                   y_delta,
                      MetaRectangle        *new)
{
  const Constraint **cp;

  cp = &all_constraints[0];

  while (*cp)
    {
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->top_func) (window, info, orig,
                             &y_delta);

      ++cp;
    }

  new->y = orig->y - y_delta;
  new->height = orig->height - y_delta;
}

static void
constrain_resize_vcenter (MetaWindow           *window,
                          const ConstraintInfo *info,
                          const MetaRectangle  *orig,
                          int                   y_delta,
                          MetaRectangle        *new)
{
  const Constraint **cp;

  cp = &all_constraints[0];

  while (*cp)
    {
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->vcenter_func) (window, info, orig,
                                 &y_delta);

      ++cp;
    }

  /* center deltas are positive to grow the window and negative to
   * shrink it.
   */
  new->y = orig->y - y_delta;
  new->height = orig->height + y_delta * 2;
}

static void
constrain_resize_bottom (MetaWindow           *window,
                         const ConstraintInfo *info,
                         const MetaRectangle  *orig,
                         int                   y_delta,
                         MetaRectangle        *new)
{
  const Constraint **cp;

  cp = &all_constraints[0];

  while (*cp)
    {
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->bottom_func) (window, info, orig,
                                &y_delta);

      ++cp;
    }

  new->height = orig->y + y_delta;
}

static void
update_position_limits (MetaWindow          *window,
                        ConstraintInfo      *info)
{
  int nw_x, nw_y;
  int se_x, se_y;
  int offscreen_w, offscreen_h;
  
  nw_x = info->work_area_screen.x;
  nw_y = info->work_area_screen.y;
  
  /* find bottom-right corner of workarea */
  se_x = info->work_area_screen.x + info->work_area_screen.width;
  se_y = info->work_area_screen.y + info->work_area_screen.height;
  
  /* If we have a micro-screen or huge frames maybe nw/se got
   * swapped
   */
  if (nw_x > se_x)
    {
      int tmp = nw_x;
      nw_x = se_x;
      se_x = tmp;
    }
  
  if (nw_y > se_y)
    {
      int tmp = nw_y;
      nw_y = se_y;
      se_y = tmp;
    }

  info->nw_x = nw_x;
  info->nw_y = nw_y;
  info->se_x = se_x;
  info->se_y = se_y;
}

/* The delta values are the mouse motion distance deltas,
 * i.e. mouse_current_pos - mouse_orig_pos, for resizing on
 * the sides, or moving. For center resize, the delta
 * value is positive to grow the window and negative to
 * shrink it (while the sign of the mouse delta
 * depends on which side of the window you are center resizing
 * from)
 */
void
meta_window_constrain (MetaWindow          *window,
                       MetaFrameGeometry   *orig_fgeom,
                       const MetaRectangle *orig,
                       int                  x_move_delta,
                       int                  y_move_delta,
                       MetaResizeDirection  x_direction,
                       int                  x_delta,
                       MetaResizeDirection  y_direction,
                       int                  y_delta,
                       MetaRectangle       *new)
{
  ConstraintInfo info;
  MetaRectangle current;
  
  /* Create a fake frame geometry if none really exists */
  if (orig_fgeom && !window->fullscreen)
    info.fgeom = *orig_fgeom;
  else
    {
      info.fgeom.top_height = 0;
      info.fgeom.bottom_height = 0;
      info.fgeom.left_width = 0;
      info.fgeom.right_width = 0;
    }

  meta_window_get_work_area (window, TRUE, &info.work_area_xinerama);
  meta_window_get_work_area (window, FALSE, &info.work_area_screen);
  
  info.window = window;
  info.xinerama = meta_screen_get_xinerama_for_window (window->screen,
                                                       window);

  /* Init info->nw_x etc. */
  update_position_limits (window, &info);

  current = *orig;
  *new = current;
  
  /* Do placement if any, so we go ahead and apply position
   * constraints in a move-only context. Don't place
   * maximized/fullscreen windows until they are unmaximized
   * and unfullscreened
   */
  if (!window->placed &&
      window->calc_placement &&
      !window->maximized &&
      !window->fullscreen)
    {
      int x, y;
      
      meta_window_place (window, orig_fgeom, x, y, &x, &y);

      constrain_move (window, &info, &current,
                      x - current.x,
                      y - current.y,
                      new);
    }

  /* Maximization, fullscreen, etc. are defined as a move followed by
   * a resize, as explained in one of the big comments at the top of
   * this file.
   */
  if (window->fullscreen)
    {      
      constrain_move (window, &info, &current,
                      info.xinerama->x_origin - current.x,
                      info.xinerama->y_origin - current.y,
                      new);

      current = *new;
      
      constrain_resize_bottom (window, &info, &current,
                               info.xinerama->height - current.height,
                               new);

      current = *new;
      
      constrain_resize_hcenter (window, &info, &current,
                                (info.xinerama->width - current.width) / 2,
                                new);
    }
  else if (window->maximized)
    {
      constrain_move (window, &info, &current,
                      info.work_area_xinerama.x - current.x,
                      info.work_area_xinerama.y - current.y,
                      new);

      current = *new;
      
      constrain_resize_bottom (window, &info, &current,
                               info.work_area_xinerama.height - current.height,
                               new);

      current = *new;
      
      constrain_resize_hcenter (window, &info, &current,
                                (info.work_area_xinerama.width - current.width) / 2,
                                new);
    }
  else
    {      
      constrain_move (window, &info, &current,
                      x_move_delta, y_move_delta,
                      new);

      current = *new;
      
      switch (x_direction)
        {
        case META_RESIZE_LEFT_OR_TOP:
          constrain_resize_left (window, &info, &current,
                                 x_delta, new);
          break;
        case META_RESIZE_CENTER:
          constrain_resize_hcenter (window, &info, &current,
                                    x_delta, new);
          break;
        case META_RESIZE_RIGHT_OR_BOTTOM:
          constrain_resize_right (window, &info, &current,
                                  x_delta, new);
          break;
        }

      switch (y_direction)
        {
        case META_RESIZE_LEFT_OR_TOP:
          constrain_resize_top (window, &info, &current,
                                y_delta, new);
          break;
        case META_RESIZE_CENTER:
          constrain_resize_vcenter (window, &info, &current,
                                    y_delta, new);
          break;
        case META_RESIZE_RIGHT_OR_BOTTOM:
          constrain_resize_bottom (window, &info, &current,
                                   y_delta, new);
          break;
        }
    }

  current = *new;

  /* Now we have to sort out the aspect ratio */
#define FLOOR(value, base)	( ((int) ((value) / (base))) * (base) )
 {
   /*
    *                width     
    * min_aspect <= -------- <= max_aspect
    *                height    
    */  
   double min_aspect, max_aspect;
   int width, height;
   
   min_aspect = window->size_hints.min_aspect.x / (double) window->size_hints.min_aspect.y;
   max_aspect = window->size_hints.max_aspect.x / (double) window->size_hints.max_aspect.y;

   width = current.width;
   height = current.height;

   /* Use the standard cut-and-pasted-between-every-WM code: */
   if (min_aspect * height > width)
     {
       delta = FLOOR (height - width * min_aspect, window->size_hints.height_inc);
       if (height - delta >= window->size_hints.min_height)
         height -= delta;
       else
         { 
           delta = FLOOR (height * min_aspect - width, window->size_hints.width_inc);
           if (width + delta <= window->size_hints.max_width) 
             width += delta;
         }
     }
      
   if (max_aspect * height < width)
     {
       delta = FLOOR (width - height * max_aspect, window->size_hints.width_inc);
       if (width - delta >= window->size_hints.min_width) 
         width -= delta;
       else
         {
           delta = FLOOR (width / max_aspect - height, window->size_hints.height_inc);
           if (height + delta <= window->size_hints.max_height)
             height += delta;
         }
     }

   /* Convert into terms of the direction of resize and reapply the
    * earlier constraints; this means aspect ratio becomes the
    * least-important of the constraints. If we wanted aspect to be
    * the most important, we could just not do this next bit.
    */

   if (current.width != width)
     {
       x_delta = width - current.width; /* positive delta to increase width */
       switch (x_direction)
         {
         case META_RESIZE_LEFT_OR_TOP:
           constrain_resize_left (window, &info, &current,
                                  - x_delta, new);
           break;
         case META_RESIZE_CENTER:
           constrain_resize_hcenter (window, &info, &current,
                                     x_delta, new);
           break;
         case META_RESIZE_RIGHT_OR_BOTTOM:
           constrain_resize_right (window, &info, &current,
                                   x_delta, new);
           break;
         }
     }

   if (current.height != height)
     {
       y_delta = height - current.height; /* positive to increase height */
       
       switch (y_direction)
         {
         case META_RESIZE_LEFT_OR_TOP:
           constrain_resize_top (window, &info, &current,
                                 - y_delta, new);
           break;
         case META_RESIZE_CENTER:
           constrain_resize_vcenter (window, &info, &current,
                                     y_delta, new);
           break;
         case META_RESIZE_RIGHT_OR_BOTTOM:
           constrain_resize_bottom (window, &info, &current,
                                    y_delta, new);
           break;
         }
     }
 }
#undef FLOOR

}

MetaResizeDirection
meta_x_direction_from_gravity (int gravity)
{
  switch (gravity)
    {
    case EastGravity:
    case NorthEastGravity:
    case SouthEastGravity:
      return META_RESIZE_LEFT_OR_TOP;
      break;

    case WestGravity:
    case NorthWestGravity:
    case SouthWestGravity:
    case StaticGravity:
      return META_RESIZE_RIGHT_OR_BOTTOM;
      break;

    default:
      return META_RESIZE_CENTER;
      break;
    }      
}

MetaResizeDirection
meta_y_direction_from_gravity (int gravity)
{
  switch (gravity)
    {
    case SouthGravity:
    case SouthWestGravity:
    case SouthEastGravity:
      return META_RESIZE_LEFT_OR_TOP;
      break;

    case NorthGravity:
    case NorthWestGravity:
    case NorthEastGravity:
    case StaticGravity:
      return META_RESIZE_RIGHT_OR_BOTTOM;
      break;
      
    default:
      return META_RESIZE_CENTER;
    }
}
