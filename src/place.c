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
  cascade_x = 0;
  cascade_y = 0;
  tmp = window->workspaces;
  while (tmp != NULL)
    {
      MetaWorkspace *space = tmp->data;
      
      cascade_x = MAX (cascade_x, space->workarea.x);
      cascade_y = MAX (cascade_y, space->workarea.y);
      
      tmp = tmp->next;
    }

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



}

void
meta_window_place (MetaWindow *window,
                   MetaFrameGeometry *fgeom,
                   int         x,
                   int         y,
                   int        *new_x,
                   int        *new_y)
{
  GList *windows;
  
  /* frame member variables should NEVER be used in here, only
   * MetaFrameGeometry. But remember fgeom == NULL
   * for undecorated windows.
   */
  
  meta_verbose ("Placing window %s\n", window->desc);      
      
  if (window->xtransient_for != None)
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

          if (fgeom)
            y += fgeom->top_height;

          meta_verbose ("Centered window %s over transient parent\n",
                        window->desc);

          goto done;
        }
    }

  if (window->type == META_WINDOW_DIALOG ||
      window->type == META_WINDOW_MODAL_DIALOG)
    {
      /* Center on screen */
      int w, h;

          /* I think whole screen will look nicer than workarea */
      w = WidthOfScreen (window->screen->xscreen);
      h = HeightOfScreen (window->screen->xscreen);

      x = (w - window->rect.width) / 2;
      y = (y - window->rect.height) / 2;

      meta_verbose ("Centered window %s on screen\n",
                    window->desc);

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
  *new_x = x;
  *new_y = y;
}
