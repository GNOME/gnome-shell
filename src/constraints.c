/* Metacity size/position constraints */

/*
 * Copyright (C) 2002, 2003 Red Hat, Inc.
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

#include <config.h>
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

#define FLOOR(value, base)	( ((int) ((value) / (base))) * (base) )

typedef struct
{
  MetaWindow *window;
  MetaFrameGeometry fgeom;
  const MetaXineramaScreenInfo *xinerama;
  MetaRectangle work_area_xinerama;
  MetaRectangle work_area_screen;
  int nw_x, nw_y, se_x, se_y; /* these are whole-screen not xinerama */
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
  const char               *name;
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
  "Desktop",
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
get_outermost_onscreen_positions (MetaWindow           *window,
                                  const ConstraintInfo *info,
                                  const MetaRectangle  *orig,
                                  int                  *leftmost_x,
                                  int                  *rightmost_x,
                                  int                  *topmost_y,
                                  int                  *bottommost_y)
{
  if (leftmost_x)
    *leftmost_x = info->nw_x + TITLEBAR_LENGTH_ONSCREEN - orig->width;

  if (rightmost_x)
    *rightmost_x = info->se_x - TITLEBAR_LENGTH_ONSCREEN;

  if (topmost_y)
    *topmost_y = info->nw_y + info->fgeom.top_height;

  if (bottommost_y)
    {
      /* If no frame, keep random TITLEBAR_LENGTH_ONSCREEN pixels on the
       * screen.
       */
      if (window->frame)
        *bottommost_y = info->se_y;
      else
        *bottommost_y = info->se_y - TITLEBAR_LENGTH_ONSCREEN;
    }
}

static void
constraint_onscreen_top_func     (MetaWindow           *window,
                                  const ConstraintInfo *info,
                                  const MetaRectangle  *orig,
                                  int                  *y_delta)
{
  int min_dy;
  int topmost_y;

  get_outermost_onscreen_positions (window, info, orig,
                                    NULL, NULL, &topmost_y, NULL);

  min_dy = topmost_y - orig->y;

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
  int bottommost_y;

  get_outermost_onscreen_positions (window, info, orig,
                                    NULL, NULL, NULL, &bottommost_y);

  max_dy = bottommost_y - orig->y;

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
  int topmost_y;

  get_outermost_onscreen_positions (window, info, orig,
                                    NULL, NULL, &topmost_y, NULL);

  max_dy = orig->y - topmost_y;

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
  int leftmost_x;

  get_outermost_onscreen_positions (window, info, orig,
                                    &leftmost_x, NULL, NULL, NULL);

  min_dx = leftmost_x - orig->x;

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
  int rightmost_x;

  get_outermost_onscreen_positions (window, info, orig,
                                    NULL, &rightmost_x, NULL, NULL);

  max_dx = rightmost_x - orig->x;

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
  int leftmost_x;
  
  get_outermost_onscreen_positions (window, info, orig,
                                    &leftmost_x, NULL, NULL, NULL);

  max_dx = orig->x - leftmost_x;

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
  int leftmost_x, rightmost_x, topmost_y, bottommost_y;

  get_outermost_onscreen_positions (window, info, orig,
                                    &leftmost_x, &rightmost_x,
                                    &topmost_y, &bottommost_y);

  min_delta = topmost_y - orig->y;
  max_delta = bottommost_y - orig->y;

  /* Note that min delta (top left) has priority over
   * max delta (bottom right) to facilitate keeping
   * titlebar on the screen
   */
  if (*y_delta > max_delta)
    *y_delta = max_delta;
  if (*y_delta < min_delta)
    *y_delta = min_delta;

  min_delta = leftmost_x - orig->x;
  max_delta = rightmost_x - orig->x;

  if (*x_delta > max_delta)
    *x_delta = max_delta;
  if (*x_delta < min_delta)
    *x_delta = min_delta;
}

static const Constraint constraint_onscreen = {
  "Onscreen",
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
 * Left resize can be solved for dx like this:
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
  int min_dy;
  int max_dy;
  int height;

  max_dy = orig->height - window->size_hints.min_height;
  min_dy = orig->height - window->size_hints.max_height;

  g_assert (max_dy >= min_dy);

  if (*y_delta > max_dy)
    *y_delta = max_dy;
  if (*y_delta < min_dy)
    *y_delta = min_dy;

  /* shrink to base + N * inc
   */
  height = orig->height - *y_delta;
  height = window->size_hints.base_height +
    FLOOR (height - window->size_hints.base_height, window->size_hints.height_inc);

  *y_delta = orig->height - height;
}

static void
constraint_hints_bottom_func  (MetaWindow           *window,
                               const ConstraintInfo *info,
                               const MetaRectangle  *orig,
                               int                  *y_delta)
{
  int min_dy;
  int max_dy;
  int height;

  min_dy = window->size_hints.min_height - orig->height;
  max_dy = window->size_hints.max_height - orig->height;

  g_assert (max_dy >= min_dy);

  if (*y_delta > max_dy)
    *y_delta = max_dy;
  if (*y_delta < min_dy)
    *y_delta = min_dy;

  /* shrink to base + N * inc
   */
  height = orig->height + *y_delta;
  height = window->size_hints.base_height +
    FLOOR (height - window->size_hints.base_height, window->size_hints.height_inc);

  *y_delta = height - orig->height;
}

static void
constraint_hints_vcenter_func (MetaWindow           *window,
                               const ConstraintInfo *info,
                               const MetaRectangle  *orig,
                               int                  *y_delta)
{
  int min_dy;
  int max_dy;
  int height;

  /* Remember our delta is negative to shrink window, positive to
   * grow it, and the actual resize is y_delta * 2 (which is broken,
   * but that's how it currently is)
   */

  min_dy = (window->size_hints.min_height - orig->height) / 2;
  max_dy = (window->size_hints.max_height - orig->height) / 2;

  g_assert (max_dy >= min_dy);

  if (*y_delta > max_dy)
    *y_delta = max_dy;
  if (*y_delta < min_dy)
    *y_delta = min_dy;

  /* shrink to base + N * inc
   */
  height = orig->height + *y_delta * 2;
  height = window->size_hints.base_height +
    FLOOR (height - window->size_hints.base_height, window->size_hints.height_inc);

  *y_delta = (height - orig->height) / 2;
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

  g_assert (max_dx >= min_dx);

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
  int min_dx;
  int max_dx;
  int width;

  min_dx = window->size_hints.min_width - orig->width;
  max_dx = window->size_hints.max_width - orig->width;

  g_assert (max_dx >= min_dx);

  if (*x_delta > max_dx)
    *x_delta = max_dx;
  if (*x_delta < min_dx)
    *x_delta = min_dx;

  /* shrink to base + N * inc
   */
  width = orig->width + *x_delta;
  width = window->size_hints.base_width +
    FLOOR (width - window->size_hints.base_width, window->size_hints.width_inc);

  *x_delta = width - orig->width;
}

static void
constraint_hints_hcenter_func (MetaWindow           *window,
                               const ConstraintInfo *info,
                               const MetaRectangle  *orig,
                               int                  *x_delta)
{
  int min_dx;
  int max_dx;
  int width;
  
  /* Remember our delta is negative to shrink window, positive to
   * grow it, and the actual resize is x_delta * 2 (which is broken,
   * but that's how it currently is)
   */

  min_dx = (window->size_hints.min_width - orig->width) / 2;
  max_dx = (window->size_hints.max_width - orig->width) / 2;

  g_assert (max_dx >= min_dx);

  if (*x_delta > max_dx)
    *x_delta = max_dx;
  if (*x_delta < min_dx)
    *x_delta = min_dx;

  /* shrink to base + N * inc
   */
  width = orig->width + *x_delta * 2;
  width = window->size_hints.base_width +
    FLOOR (width - window->size_hints.base_width, window->size_hints.width_inc);

  *x_delta = (width - orig->width) / 2;
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
  "Hints",
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
      meta_topic (META_DEBUG_GEOMETRY,
                  "Before: %d %d (Move constraint '%s')\n",
                  x_delta, y_delta, (*cp)->name);
                  
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->move_func) (window, info, orig,
                              &x_delta, &y_delta);

      meta_topic (META_DEBUG_GEOMETRY,
                  "After:  %d %d (Move constraint '%s')\n",
                  x_delta, y_delta, (*cp)->name);
      
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
      meta_topic (META_DEBUG_GEOMETRY,
                  "Before: %d (Left constraint '%s')\n",
                  x_delta, (*cp)->name);
      
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->left_func) (window, info, orig,
                              &x_delta);

      meta_topic (META_DEBUG_GEOMETRY,
                  "After:  %d (Left constraint '%s')\n",
                  x_delta, (*cp)->name);
      
      ++cp;
    }

  /* Moving mouse from 10 to 5 means current - orig means 5 - 10 means
   * a delta of -5
   */
  new->x = orig->x + x_delta;
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
      meta_topic (META_DEBUG_GEOMETRY,
                  "Before: %d (HCenter constraint '%s')\n",
                  x_delta, (*cp)->name);
      
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->hcenter_func) (window, info, orig,
                                 &x_delta);

      meta_topic (META_DEBUG_GEOMETRY,
                  "After:  %d (HCenter constraint '%s')\n",
                  x_delta, (*cp)->name);
      
      ++cp;
    }
  
  /* center deltas are positive to grow the window and negative to
   * shrink it.
   */
  new->x = orig->x - x_delta;
  new->width = orig->width + x_delta * 2;
  /* FIXME above implies that with center gravity you have to grow
   * in increments of two
   */
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
      meta_topic (META_DEBUG_GEOMETRY,
                  "Before: %d (Right constraint '%s')\n",
                  x_delta, (*cp)->name);
      
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->right_func) (window, info, orig,
                               &x_delta);

      meta_topic (META_DEBUG_GEOMETRY,
                  "After:  %d (Right constraint '%s')\n",
                  x_delta, (*cp)->name);
      
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
      meta_topic (META_DEBUG_GEOMETRY,
                  "Before: %d (Top constraint '%s')\n",
                  y_delta, (*cp)->name);
      
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->top_func) (window, info, orig,
                             &y_delta);

      meta_topic (META_DEBUG_GEOMETRY,
                  "After:  %d (Top constraint '%s')\n",
                  y_delta, (*cp)->name);
      
      ++cp;
    }

  new->y = orig->y + y_delta;
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
      meta_topic (META_DEBUG_GEOMETRY,
                  "Before: %d (VCenter constraint '%s')\n",
                  y_delta, (*cp)->name);
      
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->vcenter_func) (window, info, orig,
                                 &y_delta);

      meta_topic (META_DEBUG_GEOMETRY,
                  "After:  %d (VCenter constraint '%s')\n",
                  y_delta, (*cp)->name);
      
      ++cp;
    }

  /* center deltas are positive to grow the window and negative to
   * shrink it.
   */
  new->y = orig->y - y_delta;
  new->height = orig->height + y_delta * 2;
  /* FIXME above implies that with center gravity you have to grow
   * in increments of two
   */
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
      meta_topic (META_DEBUG_GEOMETRY,
                  "Before: %d (Bottom constraint '%s')\n",
                  y_delta, (*cp)->name);
      
      if ((* (*cp)->applies_func) (window))
        (* (*cp)->bottom_func) (window, info, orig,
                                &y_delta);

      meta_topic (META_DEBUG_GEOMETRY,
                  "After:  %d (Bottom constraint '%s')\n",
                  y_delta, (*cp)->name);
      
      ++cp;
    }

  new->height = orig->height + y_delta;
}

static void
update_position_limits (MetaWindow          *window,
                        ConstraintInfo      *info)
{
  int nw_x, nw_y;
  int se_x, se_y;

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

#define OUTER_WIDTH(rect) ((rect).width + info.fgeom.left_width + info.fgeom.right_width)
#define OUTER_HEIGHT(rect) ((rect).height + info.fgeom.top_height + info.fgeom.bottom_height)
  
  meta_topic (META_DEBUG_GEOMETRY,
              "Constraining %s x_move_delta = %d y_move_delta = %d x_direction = %d y_direction = %d x_delta = %d y_delta = %d orig %d,%d %dx%d\n",
              window->desc, x_move_delta, y_move_delta,
              x_direction, y_direction, x_delta, y_delta,
              orig->x, orig->y, orig->width, orig->height);

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

  meta_window_get_work_area_current_xinerama (window, &info.work_area_xinerama);
  meta_window_get_work_area_all_xineramas (window, &info.work_area_screen);

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

      meta_window_place (window, orig_fgeom, current.x, current.y,
                         &x, &y);

      constrain_move (window, &info, &current,
                      x - current.x,
                      y - current.y,
                      new);
      current = *new;

      /* Ignore any non-placement movement */
      x_move_delta = 0;
      y_move_delta = 0;
    }

  /* Maximization, fullscreen, etc. are defined as a move followed by
   * a resize, as explained in one of the big comments at the top of
   * this file.
   */
  if (window->fullscreen)
    {
      int center_x;

      center_x = info.xinerama->x_origin + (info.xinerama->width / 2);
      center_x -= OUTER_WIDTH (current) / 2;

      constrain_move (window, &info, &current,
                      center_x - current.x + info.fgeom.left_width,
                      info.xinerama->y_origin - current.y + info.fgeom.top_height,
                      new);

      current = *new;

      constrain_resize_bottom (window, &info, &current,
                               (info.xinerama->height - OUTER_HEIGHT (current)),
                               new);

      current = *new;

      constrain_resize_hcenter (window, &info, &current,
                                (info.xinerama->width - OUTER_WIDTH (current)) / 2,
                                new);
      current = *new;
    }
  else if (window->maximized)
    {
      int center_x;

      center_x = info.work_area_xinerama.x + (info.work_area_xinerama.width / 2);
      center_x -= OUTER_WIDTH (current) / 2;

      constrain_move (window, &info, &current,
                      center_x - current.x + info.fgeom.left_width,
                      info.work_area_xinerama.y - current.y + info.fgeom.top_height,
                      new);

      current = *new;

      constrain_resize_bottom (window, &info, &current,
                               (info.work_area_xinerama.height - OUTER_HEIGHT (current)),
                               new);

      current = *new;

      constrain_resize_hcenter (window, &info, &current,
                                (info.work_area_xinerama.width - OUTER_WIDTH (current)) / 2,
                                new);
      current = *new;
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

      current = *new;
    }

#if 0
  /* Now we have to sort out the aspect ratio */
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
        int delta;

        delta = FLOOR (height - width / min_aspect, window->size_hints.height_inc);
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
        int delta;

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

    current = *new;
  }


  g_print ("3 x_delta = %d y_delta = %d pos = %d,%d size = %dx%d\n",
           x_delta, y_delta,
           current.x, current.y, current.width, current.height);
#endif

  meta_topic (META_DEBUG_GEOMETRY,
              "Constrained %s new %d,%d %dx%d old %d,%d %dx%d\n",
              window->desc,
              new->x, new->y, new->width, new->height,
              orig->x, orig->y, orig->width, orig->height);
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
