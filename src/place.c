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

#include "place.h"
#include "workspace.h"
#include <gdk/gdkregion.h>
#include <math.h>
#include <stdlib.h>

static gint
northwestcmp (gconstpointer a, gconstpointer b)
{
  MetaWindow *aw = (gpointer) a;
  MetaWindow *bw = (gpointer) b;
  int from_origin_a;
  int from_origin_b;
  int ax, ay, bx, by;

  /* we're interested in the frame position for cascading,
   * not meta_window_get_position()
   */
  if (aw->frame)
    {
      ax = aw->frame->rect.x;
      ay = aw->frame->rect.y;
    }
  else
    {
      ax = aw->rect.x;
      ay = aw->rect.y;
    }

  if (bw->frame)
    {
      bx = bw->frame->rect.x;
      by = bw->frame->rect.y;
    }
  else
    {
      bx = bw->rect.x;
      by = bw->rect.y;
    }
  
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
                   MetaFrameGeometry *fgeom,
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
  MetaRectangle work_area;  
  
  sorted = g_list_copy (windows);
  sorted = g_list_sort (sorted, northwestcmp);

  /* This is a "fuzzy" cascade algorithm. 
   * For each window in the list, we find where we'd cascade a
   * new window after it. If a window is already nearly at that
   * position, we move on.
   */

  /* Find furthest-SE origin of all workspaces.
   * cascade_x, cascade_y are the target position
   * of NW corner of window frame.
   */
  meta_window_get_work_area (window, &work_area);
  
  cascade_x = MAX (0, work_area.x);
  cascade_y = MAX (0, work_area.y);

  /* Find first cascade position that's not used. */

  /* arbitrary-ish threshold, honors user attempts to
   * manually cascade.
   */
  if (fgeom)
    {
      x_threshold = MAX (fgeom->left_width, 10);
      y_threshold = MAX (fgeom->top_height, 10);
    }
  else
    {
      x_threshold = 10;
      y_threshold = 10;
    }
  
  tmp = sorted;
  while (tmp != NULL)
    {
      MetaWindow *w;
      int wx, wy;
      
      w = tmp->data;

      /* we want frame position, not window position */
      if (w->frame)
        {
          wx = w->frame->rect.x;
          wy = w->frame->rect.y;
        }
      else
        {
          wx = w->rect.x;
          wy = w->rect.y;
        }

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
        }
      else
        goto found; /* no window at this cascade point. */
        
      tmp = tmp->next;
    }

  /* cascade_x and cascade_y will match the last window in the list. */
  
 found:
  g_list_free (sorted);

  /* Convert coords to position of window, not position of frame. */
  if (fgeom == NULL)
    {
      *new_x = cascade_x;
      *new_y = cascade_y;
    }
  else
    {
      *new_x = cascade_x + fgeom->left_width;
      *new_y = cascade_y + fgeom->top_height;
    }
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
                MetaFrameGeometry *fgeom,
                /* visible windows on relevant workspaces */
                GList      *windows,
                int         x,
                int         y,
                int        *new_x,
                int        *new_y)
{
  /* FIXME */
}

static void
constrain_placement (MetaWindow        *window,
                     MetaFrameGeometry *fgeom,
                     int                x,
                     int                y,
                     int               *new_x,
                     int               *new_y)
{
  /* The purpose of this function is to apply constraints that are not
   * covered by window.c:constrain_position(), but should apply
   * whenever we are _placing_ a window regardless of placement algorithm.
   */
  MetaRectangle work_area;  
  int nw_x, nw_y;
  
  meta_window_get_work_area (window, &work_area);  

  nw_x = work_area.x;
  nw_y = work_area.y;
  if (window->frame)
    {
      nw_x += fgeom->left_width;
      nw_y += fgeom->top_height;
    }

  /* Keep window from going off left edge, though we don't have
   * this constraint once the window has been placed.
   */
  if (x < nw_x)
    x = nw_x;
  if (y < nw_y)
    y = nw_y;

  *new_x = x;
  *new_y = y;
}                    

void
meta_window_place (MetaWindow        *window,
                   MetaFrameGeometry *fgeom,
                   int                x,
                   int                y,
                   int               *new_x,
                   int               *new_y)
{
  GList *windows;
  
  /* frame member variables should NEVER be used in here, only
   * MetaFrameGeometry. But remember fgeom == NULL
   * for undecorated windows. Also, this function should
   * NEVER have side effects other than computing the
   * placement coordinates.
   */
  
  meta_topic (META_DEBUG_PLACEMENT, "Placing window %s\n", window->desc);      
  
  if ((window->type == META_WINDOW_DIALOG ||
       window->type == META_WINDOW_MODAL_DIALOG) &&
      window->xtransient_for != None)
    {
      /* Center horizontally, at top of parent vertically */

      MetaWindow *parent;
          
      parent =
        meta_display_lookup_x_window (window->display,
                                      window->xtransient_for);

      if (parent)
        {
          int w;

          meta_window_get_position (parent, &x, &y);
          w = parent->rect.width;

          /* center of parent */
          x = x + w / 2;
          /* center of child over center of parent */
          x -= window->rect.width / 2;

          /* put child down 1/5 or so from the top of parent, unless
           * it makes us have more of parent showing above child than
           * below
           */
          if (window->rect.height <= (parent->rect.height - (parent->rect.height / 5) * 2))
            y += parent->rect.height / 5;

          /* put top of child's frame, not top of child's client */
          if (fgeom)
            y += fgeom->top_height;

          meta_topic (META_DEBUG_PLACEMENT, "Centered window %s over transient parent\n",
                      window->desc);
          
          goto done;
        }
    }

  /* FIXME UTILITY with transient set should be stacked up
   * on the sides of the parent window or something.
   */
  
  if (window->type == META_WINDOW_DIALOG ||
      window->type == META_WINDOW_MODAL_DIALOG ||
      window->type == META_WINDOW_SPLASHSCREEN)
    {
      /* Center on screen */
      int w, h;
      const MetaXineramaScreenInfo *xi;

      /* I think whole screen will look nicer than workarea */
      xi = meta_screen_get_current_xinerama (window->screen);      

      w = xi->width;
      h = xi->height;

      x = (w - window->rect.width) / 2;
      y = (h - window->rect.height) / 2;

      x += xi->x_origin;
      y += xi->y_origin;
      
      meta_topic (META_DEBUG_PLACEMENT, "Centered window %s on screen %d xinerama %d\n",
                  window->desc, window->screen->number, xi->number);

      goto done;
    }
  
  /* Find windows that matter (not minimized, on same workspace
   * as placed window, may be shaded - if shaded we pretend it isn't
   * for placement purposes)
   */
  windows = NULL;
  {
    GSList *all_windows;
    GSList *tmp;
    
    all_windows = meta_display_list_windows (window->display);

    tmp = all_windows;
    while (tmp != NULL)
      {
        MetaWindow *w = tmp->data;

        if (!w->minimized &&
            w != window && 
            meta_window_shares_some_workspace (window, w))
          windows = g_list_prepend (windows, w);

        tmp = tmp->next;
      }
  }
  
  /* "Origin" placement algorithm */
  x = 0;
  y = 0;

  /* Cascade */
  find_next_cascade (window, fgeom, windows, x, y, &x, &y);

  g_list_free (windows);
  
 done:
  constrain_placement (window, fgeom, x, y, &x, &y);
  
  *new_x = x;
  *new_y = y;
}


/* These are used while moving or resizing to "snap" to useful
 * places; the return value is the x/y position of the window to
 * be snapped to the given edge.
 *
 * They only use edges on the current workspace, since things
 * would be weird otherwise.
 */
static GSList*
get_windows_on_same_workspace (MetaWindow *window,
                               int        *n_windows)
{
  GSList *windows;
  GSList *all_windows;
  GSList *tmp;
  int i;
  
  windows = NULL;

  i = 0;
  all_windows = meta_display_list_windows (window->display);
  
  tmp = all_windows;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;
      
      if (!w->minimized &&
          w != window && 
          meta_window_visible_on_workspace (w,
                                            window->screen->active_workspace))
        {            
          windows = g_slist_prepend (windows, w);
          ++i;
        }
      
      tmp = tmp->next;
    }

  if (n_windows)
    *n_windows = i;
  
  return windows;
}

static void
window_get_edges (MetaWindow *w,
                  int        *left,
                  int        *right,
                  int        *top,
                  int        *bottom)
{
  int left_edge;
  int right_edge;
  int top_edge;
  int bottom_edge;
  MetaRectangle rect;

  meta_window_get_outer_rect (w, &rect);
  
  left_edge = rect.x;
  right_edge = rect.x + rect.width;
  top_edge = rect.y;
  bottom_edge = rect.y + rect.height;
  
  if (left)
    *left = left_edge;
  if (right)
    *right = right_edge;
  if (top)
    *top = top_edge;
  if (bottom)
    *bottom = bottom_edge;
}

static int
intcmp (const void* a, const void* b)
{
  const int *ai = a;
  const int *bi = b;

  if (*ai < *bi)
    return -1;
  else if (*ai > *bi)
    return 1;
  else
    return 0;
}

static gboolean
rects_overlap_vertically (const MetaRectangle *a,
                          const MetaRectangle *b)
{
  /* if they don't overlap, then either a is above b
   * or b is above a
   */
  if ((a->y + a->height) < b->y)
    return FALSE;
  else if ((b->y + b->height) < a->y)
    return FALSE;
  else
    return TRUE;
}

static gboolean
rects_overlap_horizontally (const MetaRectangle *a,
                            const MetaRectangle *b)
{
  if ((a->x + a->width) < b->x)
    return FALSE;
  else if ((b->x + b->width) < a->x)
    return FALSE;
  else
    return TRUE;
}

static void
get_vertical_edges (MetaWindow *window,
                    int       **edges_p,
                    int        *n_edges_p)
{
  GSList *windows;
  GSList *tmp;
  int n_windows;
  int *edges;
  int i;
  int n_edges;
  MetaRectangle rect;
  MetaRectangle work_area;
  
  windows = get_windows_on_same_workspace (window, &n_windows);

  i = 0;
  n_edges = n_windows * 2 + 4; /* 4 = workspace/screen edges */
  edges = g_new (int, n_edges);

  /* workspace/screen edges */
  meta_window_get_work_area (window, &work_area);

  edges[i] = work_area.x;
  ++i;
  edges[i] =
    work_area.x +
    work_area.width;
  ++i;
  edges[i] = 0;
  ++i;
  edges[i] = window->screen->width;
  ++i;

  g_assert (i == 4);

  meta_window_get_outer_rect (window, &rect);
  
  /* get window edges */
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;
      MetaRectangle w_rect;

      meta_window_get_outer_rect (w, &w_rect);
      
      if (rects_overlap_vertically (&rect, &w_rect))
        {
          window_get_edges (w, &edges[i], &edges[i+1], NULL, NULL);
          i += 2;
        }

      tmp = tmp->next;
    }
  n_edges = i;
  
  g_slist_free (windows);

  /* Sort */
  qsort (edges, n_edges, sizeof (int), intcmp);

  *edges_p = edges;
  *n_edges_p = n_edges;
}  

static void
get_horizontal_edges (MetaWindow *window,
                      int       **edges_p,
                      int        *n_edges_p)
{
  GSList *windows;
  GSList *tmp;
  int n_windows;
  int *edges;
  int i;
  int n_edges;
  MetaRectangle rect;
  MetaRectangle work_area;
  
  windows = get_windows_on_same_workspace (window, &n_windows);

  i = 0;
  n_edges = n_windows * 2 + 4; /* 4 = workspace/screen edges */
  edges = g_new (int, n_edges);

  /* workspace/screen edges */
  meta_window_get_work_area (window, &work_area);
  
  edges[i] = work_area.y;
  ++i;
  edges[i] =
    work_area.y +
    work_area.height;
  ++i;
  edges[i] = 0;
  ++i;
  edges[i] = window->screen->height;
  ++i;

  g_assert (i == 4);

  meta_window_get_outer_rect (window, &rect);
  
  /* get window edges */
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;
      MetaRectangle w_rect;

      meta_window_get_outer_rect (w, &w_rect);
      
      if (rects_overlap_horizontally (&rect, &w_rect))
        {
          window_get_edges (w, NULL, NULL, &edges[i], &edges[i+1]);
          i += 2;
        }

      tmp = tmp->next;
    }
  n_edges = i;
  
  g_slist_free (windows);

  /* Sort */
  qsort (edges, n_edges, sizeof (int), intcmp);

  *edges_p = edges;
  *n_edges_p = n_edges;  
}

int
meta_window_find_next_vertical_edge (MetaWindow *window,
                                     gboolean    right)
{
  int left_edge, right_edge;
  int *edges;
  int i;
  int n_edges;
  int retval;

  get_vertical_edges (window, &edges, &n_edges);
  
  /* Find next */
  meta_window_get_position (window, &retval, NULL);

  window_get_edges (window, &left_edge, &right_edge, NULL, NULL);
  
  if (right)
    {
      i = 0;
      while (i < n_edges)
        {
          if (edges[i] > right_edge)
            {
              /* This is the one we want, snap right
               * edge of window to edges[i]
               */
              retval = edges[i];
              if (window->frame)
                {
                  retval -= window->frame->rect.width;
                  retval += window->frame->child_x;
                }
              else
                {
                  retval -= window->rect.width;
                }
              break;
            }
          
          ++i;
        }
    }
  else
    {
      i = n_edges;
      do
        {
          --i;
          
          if (edges[i] < left_edge)
            {
              /* This is the one we want */
              retval = edges[i];
              if (window->frame)
                retval += window->frame->child_x;

              break;
            }
        }
      while (i > 0);
    }

  g_free (edges);
  
  return retval;
}

int
meta_window_find_next_horizontal_edge (MetaWindow *window,
                                       gboolean    down)
{
  int top_edge, bottom_edge;
  int *edges;
  int i;
  int n_edges;
  int retval;

  get_horizontal_edges (window, &edges, &n_edges);
  
  /* Find next */
  meta_window_get_position (window, NULL, &retval);

  window_get_edges (window, NULL, NULL, &top_edge, &bottom_edge);
  
  if (down)
    {
      i = 0;
      while (i < n_edges)
        {
          if (edges[i] > bottom_edge)
            {
              /* This is the one we want, snap right
               * edge of window to edges[i]
               */
              retval = edges[i];
              if (window->frame)
                {
                  retval -= window->frame->rect.height;
                  retval += window->frame->child_y;
                }
              else
                {
                  retval -= window->rect.height;
                }
              break;
            }
          
          ++i;
        }
    }
  else
    {
      i = n_edges;
      do
        {
          --i;
          
          if (edges[i] < top_edge)
            {
              /* This is the one we want */
              retval = edges[i];
              if (window->frame)
                retval += window->frame->child_y;

              break;
            }
        }
      while (i > 0);
    }

  g_free (edges);
  
  return retval;
}


int
meta_window_find_nearest_vertical_edge (MetaWindow *window,
                                        int         x_pos)
{
  int *edges;
  int i;
  int n_edges;
  int *positions;
  int n_positions;
  int retval;
  
  get_vertical_edges (window, &edges, &n_edges);

  /* Create an array of all snapped positions our window could have */
  n_positions = n_edges * 2;
  positions = g_new (int, n_positions);
  
  i = 0;
  while (i < n_edges)
    {
      int left_pos, right_pos;

      left_pos = edges[i];
      if (window->frame)
        left_pos += window->frame->child_x;

      if (window->frame)
        {
          right_pos = edges[i] - window->frame->rect.width;
          right_pos += window->frame->child_x;
        }
      else
        {
          right_pos = edges[i] - window->rect.width;
        }

      positions[i * 2] = left_pos;
      positions[i * 2 + 1] = right_pos;
      
      ++i;
    }

  g_free (edges);

  /* Sort */
  qsort (positions, n_positions, sizeof (int), intcmp);
  
  /* Find nearest */

  retval = positions[0];
    
  i = 1;
  while (i < n_positions)
    {
      int delta;
      int best_delta;

      delta = ABS (x_pos - positions[i]);
      best_delta = ABS (x_pos - retval);

      if (delta < best_delta)
        retval = positions[i];
      
      ++i;
    }
  
  g_free (positions);
  
  return retval;
}

int
meta_window_find_nearest_horizontal_edge (MetaWindow *window,
                                          int         y_pos)
{
  int *edges;
  int i;
  int n_edges;
  int *positions;
  int n_positions;
  int retval;
  
  get_horizontal_edges (window, &edges, &n_edges);

  /* Create an array of all snapped positions our window could have */
  n_positions = n_edges * 2;
  positions = g_new (int, n_positions);
  
  i = 0;
  while (i < n_edges)
    {
      int top_pos, bottom_pos;

      top_pos = edges[i];
      if (window->frame)
        top_pos += window->frame->child_y;

      if (window->frame)
        {
          bottom_pos = edges[i] - window->frame->rect.height;
          bottom_pos += window->frame->child_y;
        }
      else
        {
          bottom_pos = edges[i] - window->rect.height;
        }

      positions[i * 2] = top_pos;
      positions[i * 2 + 1] = bottom_pos;
      
      ++i;
    }

  g_free (edges);

  /* Sort */
  qsort (positions, n_positions, sizeof (int), intcmp);
  
  /* Find nearest */

  retval = positions[0];
    
  i = 1;
  while (i < n_positions)
    {
      int delta;
      int best_delta;

      delta = ABS (y_pos - positions[i]);
      best_delta = ABS (y_pos - retval);

      if (delta < best_delta)
        retval = positions[i];
      
      ++i;
    }
  
  g_free (positions);
  
  return retval;
}
