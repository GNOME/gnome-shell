/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter window placement */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2005 Elijah Newren
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "boxes-private.h"
#include "place.h"
#include <meta/workspace.h>
#include <meta/prefs.h>
#include <gdk/gdk.h>
#include <math.h>
#include <stdlib.h>

typedef enum
{
  META_LEFT,
  META_RIGHT,
  META_TOP,
  META_BOTTOM
} MetaWindowDirection;

static gint
northwestcmp (gconstpointer a, gconstpointer b)
{
  MetaWindow *aw = (gpointer) a;
  MetaWindow *bw = (gpointer) b;
  MetaRectangle a_frame;
  MetaRectangle b_frame;
  int from_origin_a;
  int from_origin_b;
  int ax, ay, bx, by;

  meta_window_get_frame_rect (aw, &a_frame);
  meta_window_get_frame_rect (bw, &b_frame);
  ax = a_frame.x;
  ay = a_frame.y;
  bx = b_frame.x;
  by = b_frame.y;
  
  /* probably there's a fast good-enough-guess we could use here. */
  from_origin_a = sqrt (ax * ax + ay * ay);
  from_origin_b = sqrt (bx * bx + by * by);
    
  if (from_origin_a < from_origin_b)
    return -1;
  else if (from_origin_a > from_origin_b)
    return 1;
  else
    return 0;
}

static void
find_next_cascade (MetaWindow *window,
                   /* visible windows on relevant workspaces */
                   GList      *windows,
                   int         x,
                   int         y,
                   int        *new_x,
                   int        *new_y)
{
  GList *tmp;
  GList *sorted;
  int cascade_x, cascade_y;
  int x_threshold, y_threshold;
  MetaRectangle frame_rect;
  int window_width, window_height;
  int cascade_stage;
  MetaRectangle work_area;
  int current;
  
  sorted = g_list_copy (windows);
  sorted = g_list_sort (sorted, northwestcmp);

  /* This is a "fuzzy" cascade algorithm. 
   * For each window in the list, we find where we'd cascade a
   * new window after it. If a window is already nearly at that
   * position, we move on.
   */
  
  /* arbitrary-ish threshold, honors user attempts to
   * manually cascade.
   */
#define CASCADE_FUZZ 15
  if (window->frame)
    {
      MetaFrameBorders borders;

      meta_frame_calc_borders (window->frame, &borders);
      x_threshold = MAX (borders.visible.left, CASCADE_FUZZ);
      y_threshold = MAX (borders.visible.top, CASCADE_FUZZ);
    }
  else
    {
      x_threshold = CASCADE_FUZZ;
      y_threshold = CASCADE_FUZZ;
    }
  
  /* Find furthest-SE origin of all workspaces.
   * cascade_x, cascade_y are the target position
   * of NW corner of window frame.
   */

  current = meta_screen_get_current_monitor (window->screen);
  meta_window_get_work_area_for_monitor (window, current, &work_area);

  cascade_x = MAX (0, work_area.x);
  cascade_y = MAX (0, work_area.y);
  
  /* Find first cascade position that's not used. */

  meta_window_get_frame_rect (window, &frame_rect);
  window_width = frame_rect.width;
  window_height = frame_rect.height;
  
  cascade_stage = 0;
  tmp = sorted;
  while (tmp != NULL)
    {
      MetaWindow *w;
      MetaRectangle w_frame_rect;
      int wx, wy;
      
      w = tmp->data;

      /* we want frame position, not window position */
      meta_window_get_frame_rect (w, &w_frame_rect);
      wx = w_frame_rect.x;
      wy = w_frame_rect.y;
      
      if (ABS (wx - cascade_x) < x_threshold &&
          ABS (wy - cascade_y) < y_threshold)
        {
          /* This window is "in the way", move to next cascade
           * point. The new window frame should go at the origin
           * of the client window we're stacking above.
           */
          meta_window_get_position (w, &wx, &wy);
          cascade_x = wx;
          cascade_y = wy;
          
          /* If we go off the screen, start over with a new cascade */
	  if (((cascade_x + window_width) >
               (work_area.x + work_area.width)) ||
              ((cascade_y + window_height) >
	       (work_area.y + work_area.height)))
	    {
	      cascade_x = MAX (0, work_area.x);
	      cascade_y = MAX (0, work_area.y);
              
#define CASCADE_INTERVAL 50 /* space between top-left corners of cascades */
              cascade_stage += 1;
	      cascade_x += CASCADE_INTERVAL * cascade_stage;
              
	      /* start over with a new cascade translated to the right, unless
               * we are out of space
               */
              if ((cascade_x + window_width) <
                  (work_area.x + work_area.width))
                {
                  tmp = sorted;
                  continue;
                }
              else
                {
                  /* All out of space, this cascade_x won't work */
                  cascade_x = MAX (0, work_area.x);
                  break;
                }
	    }
        }
      else
        {
          /* Keep searching for a further-down-the-diagonal window. */
        }
        
      tmp = tmp->next;
    }

  /* cascade_x and cascade_y will match the last window in the list
   * that was "in the way" (in the approximate cascade diagonal)
   */
  
  g_list_free (sorted);

  *new_x = cascade_x;
  *new_y = cascade_y;
}

static void
find_most_freespace (MetaWindow *window,
                     /* visible windows on relevant workspaces */
                     MetaWindow *focus_window,
                     int         x,
                     int         y,
                     int        *new_x,
                     int        *new_y)
{
  MetaWindowDirection side;
  int max_area;
  int max_width, max_height, left, right, top, bottom;
  int left_space, right_space, top_space, bottom_space;
  MetaRectangle work_area;
  MetaRectangle avoid;
  MetaRectangle frame_rect;

  meta_window_get_work_area_current_monitor (focus_window, &work_area);
  meta_window_get_frame_rect (focus_window, &avoid);
  meta_window_get_frame_rect (window, &frame_rect);

  /* Find the areas of choosing the various sides of the focus window */
  max_width  = MIN (avoid.width, frame_rect.width);
  max_height = MIN (avoid.height, frame_rect.height);
  left_space   = avoid.x - work_area.x;
  right_space  = work_area.width - (avoid.x + avoid.width - work_area.x);
  top_space    = avoid.y - work_area.y;
  bottom_space = work_area.height - (avoid.y + avoid.height - work_area.y);
  left   = MIN (left_space,   frame_rect.width);
  right  = MIN (right_space,  frame_rect.width);
  top    = MIN (top_space,    frame_rect.height);
  bottom = MIN (bottom_space, frame_rect.height);

  /* Find out which side of the focus_window can show the most of the window */
  side = META_LEFT;
  max_area = left*max_height;
  if (right*max_height > max_area)
    {
      side = META_RIGHT;
      max_area = right*max_height;
    }
  if (top*max_width > max_area)
    {
      side = META_TOP;
      max_area = top*max_width;
    }
  if (bottom*max_width > max_area)
    {
      side = META_BOTTOM;
      max_area = bottom*max_width;
    }

  /* Give up if there's no where to put it (i.e. focus window is maximized) */
  if (max_area == 0)
    return;

  /* Place the window on the relevant side; if the whole window fits,
   * make it adjacent to the focus window; if not, make sure the
   * window doesn't go off the edge of the screen.
   */
  switch (side)
    {
    case META_LEFT:
      *new_y = avoid.y;
      if (left_space > frame_rect.width)
        *new_x = avoid.x - frame_rect.width;
      else
        *new_x = work_area.x;
      break;
    case META_RIGHT:
      *new_y = avoid.y;
      if (right_space > frame_rect.width)
        *new_x = avoid.x + avoid.width;
      else
        *new_x = work_area.x + work_area.width - frame_rect.width;
      break;
    case META_TOP:
      *new_x = avoid.x;
      if (top_space > frame_rect.height)
        *new_y = avoid.y - frame_rect.height;
      else
        *new_y = work_area.y;
      break;
    case META_BOTTOM:
      *new_x = avoid.x;
      if (bottom_space > frame_rect.height)
        *new_y = avoid.y + avoid.height;
      else
        *new_y = work_area.y + work_area.height - frame_rect.height;
      break;
    }
}

static gboolean
window_overlaps_focus_window (MetaWindow *window)
{
  MetaWindow *focus_window;
  MetaRectangle window_frame, focus_frame, overlap;

  focus_window = window->display->focus_window;
  if (focus_window == NULL)
    return FALSE;

  meta_window_get_frame_rect (window, &window_frame);
  meta_window_get_frame_rect (focus_window, &focus_frame);

  return meta_rectangle_intersect (&window_frame,
                                   &focus_frame,
                                   &overlap);
}

static gboolean
window_place_centered (MetaWindow *window)
{
  MetaWindowType type;

  type = window->type;

  return (type == META_WINDOW_DIALOG ||
    type == META_WINDOW_MODAL_DIALOG ||
    type == META_WINDOW_SPLASHSCREEN ||
    (type == META_WINDOW_NORMAL && meta_prefs_get_center_new_windows ()));
}

static void
avoid_being_obscured_as_second_modal_dialog (MetaWindow *window,
                                             int        *x,
                                             int        *y)
{
  /* We can't center this dialog if it was denied focus and it
   * overlaps with the focus window and this dialog is modal and this
   * dialog is in the same app as the focus window (*phew*...please
   * don't make me say that ten times fast). See bug 307875 comment 11
   * and 12 for details, but basically it means this is probably a
   * second modal dialog for some app while the focus window is the
   * first modal dialog.  We should probably make them simultaneously
   * visible in general, but it becomes mandatory to do so due to
   * buggy apps (e.g. those using gtk+ *sigh*) because in those cases
   * this second modal dialog also happens to be modal to the first
   * dialog in addition to the main window, while it has only let us
   * know about the modal-to-the-main-window part.
   */

  MetaWindow *focus_window;

  focus_window = window->display->focus_window;

  /* denied_focus_and_not_transient is only set when focus_window != NULL */

  if (window->denied_focus_and_not_transient &&
      window->type == META_WINDOW_MODAL_DIALOG &&
      meta_window_same_application (window, focus_window) &&
      window_overlaps_focus_window (window))
    {
      find_most_freespace (window, focus_window, *x, *y, x, y);
      meta_topic (META_DEBUG_PLACEMENT,
                  "Dialog window %s was denied focus but may be modal "
                  "to the focus window; had to move it to avoid the "
                  "focus window\n",
                  window->desc);
    }
}

static gboolean
rectangle_overlaps_some_window (MetaRectangle *rect,
                                GList         *windows)
{
  GList *tmp;
  MetaRectangle dest;
  
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *other = tmp->data;
      MetaRectangle other_rect;      

      switch (other->type)
        {
        case META_WINDOW_DOCK:
        case META_WINDOW_SPLASHSCREEN:
        case META_WINDOW_DESKTOP:
        case META_WINDOW_DIALOG:
        case META_WINDOW_MODAL_DIALOG:
	/* override redirect window types: */
	case META_WINDOW_DROPDOWN_MENU:
	case META_WINDOW_POPUP_MENU:
	case META_WINDOW_TOOLTIP:
	case META_WINDOW_NOTIFICATION:
	case META_WINDOW_COMBO:
	case META_WINDOW_DND:
	case META_WINDOW_OVERRIDE_OTHER:
          break;

        case META_WINDOW_NORMAL:
        case META_WINDOW_UTILITY:
        case META_WINDOW_TOOLBAR:
        case META_WINDOW_MENU:
          meta_window_get_frame_rect (other, &other_rect);
          
          if (meta_rectangle_intersect (rect, &other_rect, &dest))
            return TRUE;
          break;
        }
      
      tmp = tmp->next;
    }

  return FALSE;
}

static gint
leftmost_cmp (gconstpointer a, gconstpointer b)
{
  MetaWindow *aw = (gpointer) a;
  MetaWindow *bw = (gpointer) b;
  MetaRectangle a_frame;
  MetaRectangle b_frame;
  int ax, bx;

  meta_window_get_frame_rect (aw, &a_frame);
  meta_window_get_frame_rect (bw, &b_frame);
  ax = a_frame.x;
  bx = b_frame.x;

  if (ax < bx)
    return -1;
  else if (ax > bx)
    return 1;
  else
    return 0;
}

static gint
topmost_cmp (gconstpointer a, gconstpointer b)
{
  MetaWindow *aw = (gpointer) a;
  MetaWindow *bw = (gpointer) b;
  MetaRectangle a_frame;
  MetaRectangle b_frame;
  int ay, by;

  meta_window_get_frame_rect (aw, &a_frame);
  meta_window_get_frame_rect (bw, &b_frame);
  ay = a_frame.y;
  by = b_frame.y;

  if (ay < by)
    return -1;
  else if (ay > by)
    return 1;
  else
    return 0;
}

static void
center_tile_rect_in_area (MetaRectangle *rect,
                          MetaRectangle *work_area)
{
  int fluff;

  /* The point here is to tile a window such that "extra"
   * space is equal on either side (i.e. so a full screen
   * of windows tiled this way would center the windows
   * as a group)
   */

  fluff = (work_area->width % (rect->width+1)) / 2;
  rect->x = work_area->x + fluff;
  fluff = (work_area->height % (rect->height+1)) / 3;
  rect->y = work_area->y + fluff;
}

/* Find the leftmost, then topmost, empty area on the workspace
 * that can contain the new window.
 *
 * Cool feature to have: if we can't fit the current window size,
 * try shrinking the window (within geometry constraints). But
 * beware windows such as Emacs with no sane minimum size, we
 * don't want to create a 1x1 Emacs.
 */
static gboolean
find_first_fit (MetaWindow *window,
                /* visible windows on relevant workspaces */
                GList      *windows,
		int         monitor,
                int         x,
                int         y,
                int        *new_x,
                int        *new_y)
{
  /* This algorithm is limited - it just brute-force tries
   * to fit the window in a small number of locations that are aligned
   * with existing windows. It tries to place the window on
   * the bottom of each existing window, and then to the right
   * of each existing window, aligned with the left/top of the
   * existing window in each of those cases.
   */  
  int retval;
  GList *below_sorted;
  GList *right_sorted;
  GList *tmp;
  MetaRectangle rect;
  MetaRectangle work_area;
  
  retval = FALSE;

  /* Below each window */
  below_sorted = g_list_copy (windows);
  below_sorted = g_list_sort (below_sorted, leftmost_cmp);
  below_sorted = g_list_sort (below_sorted, topmost_cmp);  

  /* To the right of each window */
  right_sorted = g_list_copy (windows);
  right_sorted = g_list_sort (right_sorted, topmost_cmp);
  right_sorted = g_list_sort (right_sorted, leftmost_cmp);

  meta_window_get_frame_rect (window, &rect);

#ifdef WITH_VERBOSE_MODE
    {
      char monitor_location_string[RECT_LENGTH];
      meta_rectangle_to_string (&window->screen->monitor_infos[monitor].rect,
                                monitor_location_string);
      meta_topic (META_DEBUG_XINERAMA,
		  "Natural monitor is %s\n",
		  monitor_location_string);
    }
#endif

    meta_window_get_work_area_for_monitor (window, monitor, &work_area);

    center_tile_rect_in_area (&rect, &work_area);

    if (meta_rectangle_contains_rect (&work_area, &rect) &&
        !rectangle_overlaps_some_window (&rect, windows))
      {
        *new_x = rect.x;
        *new_y = rect.y;
    
        retval = TRUE;
       
        goto out;
      }

    /* try below each window */
    tmp = below_sorted;
    while (tmp != NULL)
      {
        MetaWindow *w = tmp->data;
        MetaRectangle frame_rect;

        meta_window_get_frame_rect (w, &frame_rect);
      
        rect.x = frame_rect.x;
        rect.y = frame_rect.y + frame_rect.height;
      
        if (meta_rectangle_contains_rect (&work_area, &rect) &&
            !rectangle_overlaps_some_window (&rect, below_sorted))
          {
            *new_x = rect.x;
            *new_y = rect.y;
          
            retval = TRUE;
          
            goto out;
          }

        tmp = tmp->next;
      }

    /* try to the right of each window */
    tmp = right_sorted;
    while (tmp != NULL)
      {
        MetaWindow *w = tmp->data;
        MetaRectangle frame_rect;
   
        meta_window_get_frame_rect (w, &frame_rect);
     
        rect.x = frame_rect.x + frame_rect.width;
        rect.y = frame_rect.y;
   
        if (meta_rectangle_contains_rect (&work_area, &rect) &&
            !rectangle_overlaps_some_window (&rect, right_sorted))
          {
            *new_x = rect.x;
            *new_y = rect.y;
        
            retval = TRUE;
       
            goto out;
          }

        tmp = tmp->next;
      }
      
 out:

  g_list_free (below_sorted);
  g_list_free (right_sorted);
  return retval;
}

void
meta_window_place (MetaWindow        *window,
                   int                x,
                   int                y,
                   int               *new_x,
                   int               *new_y)
{
  GList *windows;
  const MetaMonitorInfo *xi;

  meta_topic (META_DEBUG_PLACEMENT, "Placing window %s\n", window->desc);

  windows = NULL;

  switch (window->type)
    {
      /* Run placement algorithm on these. */
    case META_WINDOW_NORMAL:
    case META_WINDOW_DIALOG:
    case META_WINDOW_MODAL_DIALOG:
    case META_WINDOW_SPLASHSCREEN:
      break;
          
      /* Assume the app knows best how to place these, no placement
       * algorithm ever (other than "leave them as-is")
       */
    case META_WINDOW_DESKTOP:
    case META_WINDOW_DOCK:
    case META_WINDOW_TOOLBAR:
    case META_WINDOW_MENU:
    case META_WINDOW_UTILITY:
    /* override redirect window types: */
    case META_WINDOW_DROPDOWN_MENU:
    case META_WINDOW_POPUP_MENU:
    case META_WINDOW_TOOLTIP:
    case META_WINDOW_NOTIFICATION:
    case META_WINDOW_COMBO:
    case META_WINDOW_DND:
    case META_WINDOW_OVERRIDE_OTHER:
      goto done_no_constraints;
    }

  if (meta_prefs_get_disable_workarounds ())
    {
      switch (window->type)
        {
          /* Only accept USPosition on normal windows because the app is full
           * of shit claiming the user set -geometry for a dialog or dock
           */
        case META_WINDOW_NORMAL:
          if (window->size_hints.flags & USPosition)
            {
              /* don't constrain with placement algorithm */
              meta_topic (META_DEBUG_PLACEMENT,
                          "Honoring USPosition for %s instead of using placement algorithm\n", window->desc);

              goto done;
            }
          break;

          /* Ignore even USPosition on dialogs, splashscreen */
        case META_WINDOW_DIALOG:
        case META_WINDOW_MODAL_DIALOG:
        case META_WINDOW_SPLASHSCREEN:
          break;
          
          /* Assume the app knows best how to place these. */
        case META_WINDOW_DESKTOP:
        case META_WINDOW_DOCK:
        case META_WINDOW_TOOLBAR:
        case META_WINDOW_MENU:
        case META_WINDOW_UTILITY:
	/* override redirect window types: */
	case META_WINDOW_DROPDOWN_MENU:
	case META_WINDOW_POPUP_MENU:
	case META_WINDOW_TOOLTIP:
	case META_WINDOW_NOTIFICATION:
	case META_WINDOW_COMBO:
	case META_WINDOW_DND:
	case META_WINDOW_OVERRIDE_OTHER:
          if (window->size_hints.flags & PPosition)
            {
              meta_topic (META_DEBUG_PLACEMENT,
                          "Not placing non-normal non-dialog window with PPosition set\n");
              goto done_no_constraints;
            }
          break;
        }
    }
  else
    {
      /* workarounds enabled */
      
      if ((window->size_hints.flags & PPosition) ||
          (window->size_hints.flags & USPosition))
        {
          meta_topic (META_DEBUG_PLACEMENT,
                      "Not placing window with PPosition or USPosition set\n");
          avoid_being_obscured_as_second_modal_dialog (window, &x, &y);
          goto done_no_constraints;
        }
    }

  if (window->type == META_WINDOW_DIALOG ||
      window->type == META_WINDOW_MODAL_DIALOG)
    {
      MetaWindow *parent = meta_window_get_transient_for (window);

      if (parent)
        {
          MetaRectangle frame_rect, parent_frame_rect;

          meta_window_get_frame_rect (window, &frame_rect);
          meta_window_get_frame_rect (parent, &parent_frame_rect);

          y = parent_frame_rect.y;

          /* center of parent */
          x = parent_frame_rect.x + parent_frame_rect.width / 2;
          /* center of child over center of parent */
          x -= frame_rect.width / 2;

          /* "visually" center window over parent, leaving twice as
           * much space below as on top.
           */
          y += (parent_frame_rect.height - frame_rect.height)/3;

          meta_topic (META_DEBUG_PLACEMENT, "Centered window %s over transient parent\n",
                      window->desc);
          
          avoid_being_obscured_as_second_modal_dialog (window, &x, &y);

          goto done;
        }
    }
  
  /* FIXME UTILITY with transient set should be stacked up
   * on the sides of the parent window or something.
   */
  
  if (window_place_centered (window))
    {
      /* Center on current monitor */
      int w, h;
      MetaRectangle frame_rect;

      meta_window_get_frame_rect (window, &frame_rect);

      /* Warning, this function is a round trip! */
      xi = meta_screen_get_current_monitor_info (window->screen);

      w = xi->rect.width;
      h = xi->rect.height;

      x = (w - frame_rect.width) / 2;
      y = (h - frame_rect.height) / 2;

      x += xi->rect.x;
      y += xi->rect.y;
      
      meta_topic (META_DEBUG_PLACEMENT, "Centered window %s on screen %d monitor %d\n",
                  window->desc, window->screen->number, xi->number);

      goto done_check_denied_focus;
    }
  
  /* Find windows that matter (not minimized, on same workspace
   * as placed window, may be shaded - if shaded we pretend it isn't
   * for placement purposes)
   */
  {
    GSList *all_windows;
    GSList *tmp;
    
    all_windows = meta_display_list_windows (window->display, META_LIST_DEFAULT);

    tmp = all_windows;
    while (tmp != NULL)
      {
        MetaWindow *w = tmp->data;

        if (w != window &&
            meta_window_showing_on_its_workspace (w) &&
            meta_window_located_on_workspace (w, window->workspace))
          windows = g_list_prepend (windows, w);

        tmp = tmp->next;
      }

    g_slist_free (all_windows);
  }

  /* Warning, this is a round trip! */
  xi = meta_screen_get_current_monitor_info (window->screen);
  
  /* "Origin" placement algorithm */
  x = xi->rect.x;
  y = xi->rect.y;

  if (find_first_fit (window, windows,
                      xi->number,
                      x, y, &x, &y))
    goto done_check_denied_focus;

  /* Maximize windows if they are too big for their work area (bit of
   * a hack here). Assume undecorated windows probably don't intend to
   * be maximized.  
   */
  if (window->has_maximize_func && window->decorated &&
      !window->fullscreen)
    {
      MetaRectangle workarea;
      MetaRectangle frame_rect;

      meta_window_get_work_area_for_monitor (window,
                                             xi->number,
                                             &workarea);      
      meta_window_get_frame_rect (window, &frame_rect);
      
      /* If the window is bigger than the screen, then automaximize.  Do NOT
       * auto-maximize the directions independently.  See #419810.
       */
      if (frame_rect.width >= workarea.width && frame_rect.height >= workarea.height)
        {
          window->maximize_horizontally_after_placement = TRUE;
          window->maximize_vertically_after_placement = TRUE;
        }
    }

  /* If no placement has been done, revert to cascade to avoid 
   * fully overlapping window (e.g. starting multiple terminals)
   * */
  if (x == xi->rect.x && y == xi->rect.y)  
    find_next_cascade (window, windows, x, y, &x, &y);

 done_check_denied_focus:
  /* If the window is being denied focus and isn't a transient of the
   * focus window, we do NOT want it to overlap with the focus window
   * if at all possible.  This is guaranteed to only be called if the
   * focus_window is non-NULL, and we try to avoid that window.
   */
  if (window->denied_focus_and_not_transient)
    {
      MetaWindow    *focus_window;
      gboolean       found_fit;

      focus_window = window->display->focus_window;
      g_assert (focus_window != NULL);

      /* No need to do anything if the window doesn't overlap at all */
      found_fit = !window_overlaps_focus_window (window);

      /* Try to do a first fit again, this time only taking into account the
       * focus window.
       */
      if (!found_fit)
        {
          GList *focus_window_list;
          focus_window_list = g_list_prepend (NULL, focus_window);

          /* Reset x and y ("origin" placement algorithm) */
          x = xi->rect.x;
          y = xi->rect.y;

          found_fit = find_first_fit (window, focus_window_list,
                                      xi->number,
                                      x, y, &x, &y);
          g_list_free (focus_window_list);
	}

      /* If that still didn't work, just place it where we can see as much
       * as possible.
       */
      if (!found_fit)
        find_most_freespace (window, focus_window, x, y, &x, &y);
    }
  
 done:
  g_list_free (windows);
  
 done_no_constraints:

  *new_x = x;
  *new_y = y;
}
