/* Metacity Workspaces */

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

#include <config.h>
#include "workspace.h"
#include "errors.h"
#include "prefs.h"
#include <X11/Xatom.h>
#include <string.h>

void meta_workspace_queue_calc_showing  (MetaWorkspace *workspace);

static void set_active_space_hint      (MetaScreen *screen);

MetaWorkspace*
meta_workspace_new (MetaScreen *screen)
{
  MetaWorkspace *workspace;

  workspace = g_new (MetaWorkspace, 1);

  workspace->screen = screen;
  workspace->screen->workspaces =
    g_list_append (workspace->screen->workspaces, workspace);
  workspace->windows = NULL;

  workspace->work_area.x = 0;
  workspace->work_area.y = 0;
  workspace->work_area.width = screen->width;
  workspace->work_area.height = screen->height;
  workspace->work_area_invalid = TRUE;
  
  return workspace;
}

void
meta_workspace_free (MetaWorkspace *workspace)
{
  GList *tmp;
  MetaScreen *screen;

  g_return_if_fail (workspace != workspace->screen->active_workspace);

  /* Here we assume all the windows are already on another workspace
   * as well, so they won't be "orphaned"
   */
  
  tmp = workspace->windows;
  while (tmp != NULL)
    {
      GList *next;
      MetaWindow *window = tmp->data;
      next = tmp->next;

      /* pop front of list we're iterating over */
      meta_workspace_remove_window (workspace, window);
      g_assert (window->workspaces != NULL);

      tmp = next;
    }

  g_assert (workspace->windows == NULL);

  screen = workspace->screen;
  
  workspace->screen->workspaces =
    g_list_remove (workspace->screen->workspaces, workspace);
  
  g_free (workspace);

  /* don't bother to reset names, pagers can just ignore
   * extra ones
   */
}

void
meta_workspace_add_window (MetaWorkspace *workspace,
                           MetaWindow    *window)
{
  g_return_if_fail (!meta_workspace_contains_window (workspace, window));
  
  workspace->windows = g_list_prepend (workspace->windows, window);
  window->workspaces = g_list_prepend (window->workspaces, workspace);

  meta_window_set_current_workspace_hint (window);
  
  meta_window_queue_calc_showing (window);
  if (window->has_struts)
    {
      meta_topic (META_DEBUG_WORKAREA,
                  "Invalidating work area of workspace %d since we're adding window %s to it\n",
                  meta_workspace_index (workspace), window->desc);
      meta_workspace_invalidate_work_area (workspace);
    }

  /* queue a move_resize since changing workspaces may change
   * the relevant struts
   */
  meta_window_queue_move_resize (window);
}

void
meta_workspace_remove_window (MetaWorkspace *workspace,
                              MetaWindow    *window)
{
  g_return_if_fail (meta_workspace_contains_window (workspace, window));

  workspace->windows = g_list_remove (workspace->windows, window);
  window->workspaces = g_list_remove (window->workspaces, workspace);

  meta_window_set_current_workspace_hint (window);
  
  meta_window_queue_calc_showing (window);

  if (window->has_struts)
    {
      meta_topic (META_DEBUG_WORKAREA,
                  "Invalidating work area of workspace %d since we're removing window %s from it\n",
                  meta_workspace_index (workspace), window->desc);
      meta_workspace_invalidate_work_area (workspace);
    }

  /* queue a move_resize since changing workspaces may change
   * the relevant struts
   */
  meta_window_queue_move_resize (window);
}

void
meta_workspace_relocate_windows (MetaWorkspace *workspace,
                                 MetaWorkspace *new_home)
{
  GList *tmp;
  GList *copy;
  
  g_return_if_fail (workspace != new_home);

  /* can't modify list we're iterating over */
  copy = g_list_copy (workspace->windows);
  
  tmp = copy;
  while (tmp != NULL)
    {
      MetaWindow *window = tmp->data;

      meta_workspace_add_window (new_home, window);
      meta_workspace_remove_window (workspace, window);
      
      tmp = tmp->next;
    }

  g_list_free (copy);
  
  g_assert (workspace->windows == NULL);
}

gboolean
meta_workspace_contains_window (MetaWorkspace *workspace,
                                MetaWindow    *window)
{
  return g_list_find (workspace->windows, window) != NULL;
}

void
meta_workspace_queue_calc_showing  (MetaWorkspace *workspace)
{
  GList *tmp;

  tmp = workspace->windows;
  while (tmp != NULL)
    {
      meta_window_queue_calc_showing (tmp->data);

      tmp = tmp->next;
    }
}

void
meta_workspace_activate (MetaWorkspace *workspace)
{
  MetaWorkspace *old;
  
  meta_verbose ("Activating workspace %d\n",
                meta_workspace_index (workspace));
  
  if (workspace->screen->active_workspace == workspace)
    return;

  old = workspace->screen->active_workspace;
  
  workspace->screen->active_workspace = workspace;

  set_active_space_hint (workspace->screen);

  if (old == NULL)
    return;

  meta_workspace_queue_calc_showing (old);
  meta_workspace_queue_calc_showing (workspace);

  /* in mouse focus modes, this will probably get undone by an EnterNotify,
   * but that's OK
   */
  meta_topic (META_DEBUG_FOCUS, "Focusing top window on new workspace\n");
  meta_screen_focus_top_window (workspace->screen, NULL);
}

int
meta_workspace_index (MetaWorkspace *workspace)
{
  GList *tmp;
  int i;

  i = 0;
  tmp = workspace->screen->workspaces;
  while (tmp != NULL)
    {
      if (tmp->data == workspace)
        return i;

      ++i;
                    
      tmp = tmp->next;
    }

  meta_bug ("Workspace does not exist to index!\n");
  return -1; /* compiler warnings */
}

/* get windows contained on workspace, including workspace->windows
 * and also sticky windows.
 */
GList*
meta_workspace_list_windows (MetaWorkspace *workspace)
{
  GSList *display_windows;
  GSList *tmp;
  GList *workspace_windows;
  
  display_windows = meta_display_list_windows (workspace->screen->display);

  workspace_windows = NULL;
  tmp = display_windows;
  while (tmp != NULL)
    {
      MetaWindow *window = tmp->data;

      if (meta_window_visible_on_workspace (window, workspace))
        workspace_windows = g_list_prepend (workspace_windows,
                                            window);

      tmp = tmp->next;
    }

  g_slist_free (display_windows);

  return workspace_windows;
}

static void
set_active_space_hint (MetaScreen *screen)
{
  unsigned long data[1];

  /* this is because we destroy the spaces in order,
   * so we always end up setting a current desktop of
   * 0 when closing a screen, so lose the current desktop
   * on restart. By doing this we keep the current
   * desktop on restart.
   */
  if (screen->closing > 0)
    return;
  
  data[0] = meta_workspace_index (screen->active_workspace);

  meta_verbose ("Setting _NET_CURRENT_DESKTOP to %ld\n", data[0]);
  
  meta_error_trap_push (screen->display);
  XChangeProperty (screen->display->xdisplay, screen->xroot,
                   screen->display->atom_net_current_desktop,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);
  meta_error_trap_pop (screen->display, FALSE);
}

void
meta_workspace_invalidate_work_area (MetaWorkspace *workspace)
{
  GList *tmp;
  GList *windows;
  
  if (workspace->work_area_invalid)
    {
      meta_topic (META_DEBUG_WORKAREA,
                  "Work area for workspace %d is already invalid\n",
                  meta_workspace_index (workspace));
      return;
    }

  meta_topic (META_DEBUG_WORKAREA,
              "Invalidating work area for workspace %d\n",
              meta_workspace_index (workspace));
  
  workspace->work_area_invalid = TRUE;

  /* redo the size/position constraints on all windows */
  windows = meta_workspace_list_windows (workspace);
  tmp = windows;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;

      meta_window_queue_move_resize (w);
      
      tmp = tmp->next;
    }

  g_list_free (windows);

  meta_screen_queue_workarea_recalc (workspace->screen);
}

void
meta_workspace_get_work_area (MetaWorkspace *workspace,
                              MetaRectangle *area)
{  
  if (workspace->work_area_invalid)
    {
      int left_strut = 0;
      int right_strut = 0;
      int top_strut = 0;
      int bottom_strut = 0;
      GList *tmp;
      GList *windows;

      windows = meta_workspace_list_windows (workspace);
      tmp = windows;
      while (tmp != NULL)
        {
          MetaWindow *w = tmp->data;

          if (w->has_struts)
            {
              meta_topic (META_DEBUG_WORKAREA,
                          "Merging win %s with %d %d %d %d with %d %d %d %d\n",
                          w->desc,
                          w->left_strut, w->right_strut, w->top_strut, w->bottom_strut,
                          left_strut, right_strut, top_strut, bottom_strut);
              left_strut = MAX (left_strut, w->left_strut);
              right_strut = MAX (right_strut, w->right_strut);
              top_strut = MAX (top_strut, w->top_strut);
              bottom_strut = MAX (bottom_strut, w->bottom_strut);
            }
          
          tmp = tmp->next;
        }

      g_list_free (windows);

      /* Some paranoid robustness */
#define MIN_SANE_AREA 100
      
      if ((left_strut + right_strut) > (workspace->screen->width - MIN_SANE_AREA))
        {
          meta_topic (META_DEBUG_WORKAREA,
                      "Making left/right struts %d %d sane\n",
                      left_strut, right_strut);
          left_strut = (workspace->screen->width - MIN_SANE_AREA) / 2;
          right_strut = left_strut;
        }

      if ((top_strut + bottom_strut) > (workspace->screen->height - MIN_SANE_AREA))
        {
          meta_topic (META_DEBUG_WORKAREA,
                      "Making top/bottom struts %d %d sane\n",
                      top_strut, bottom_strut);
          top_strut = (workspace->screen->height - MIN_SANE_AREA) / 2;
          bottom_strut = top_strut;
        }
      
      workspace->work_area.x = left_strut;
      workspace->work_area.y = top_strut;
      workspace->work_area.width = workspace->screen->width - left_strut - right_strut;
      workspace->work_area.height = workspace->screen->height - top_strut - bottom_strut;

      workspace->work_area_invalid = FALSE;

      meta_topic (META_DEBUG_WORKAREA,
                  "Computed work area for workspace %d: %d,%d %d x %d\n",
                  meta_workspace_index (workspace),
                  workspace->work_area.x,
                  workspace->work_area.y,
                  workspace->work_area.width,
                  workspace->work_area.height);
    }

  *area = workspace->work_area;
}

#ifdef WITH_VERBOSE_MODE
static char *
meta_motion_direction_to_string (MetaMotionDirection direction)
{
  switch (direction)
    {
    case META_MOTION_UP:
      return "Up";
    case META_MOTION_DOWN:
      return "Down";
    case META_MOTION_LEFT:
      return "Left";
    case META_MOTION_RIGHT:
      return "Right";
    }

  return "Unknown";
}
#endif /* WITH_VERBOSE_MODE */

MetaWorkspace*
meta_workspace_get_neighbor (MetaWorkspace      *workspace,
                             MetaMotionDirection direction)
{
  MetaWorkspaceLayout layout;  
  int i, current_space, num_workspaces;

  current_space = meta_workspace_index (workspace);
  num_workspaces = meta_screen_get_n_workspaces (workspace->screen);
  meta_screen_calc_workspace_layout (workspace->screen, num_workspaces,
                                     current_space, &layout);

  meta_verbose ("Getting neighbor of %d in direction %s\n",
                current_space, meta_motion_direction_to_string (direction));
  
  switch (direction) 
    {
    case META_MOTION_LEFT:
      layout.current_col -= 1;
      break;
    case META_MOTION_RIGHT:
      layout.current_col += 1;
      break;
    case META_MOTION_UP:
      layout.current_row -= 1;
      break;
    case META_MOTION_DOWN:
      layout.current_row += 1;
      break;
    }

  if (layout.current_col < 0)
    layout.current_col = 0;
  if (layout.current_col >= layout.cols)
    layout.current_col = layout.cols - 1;
  if (layout.current_row < 0)
    layout.current_row = 0;
  if (layout.current_row >= layout.rows)
    layout.current_row = layout.rows - 1;

  i = layout.grid[layout.current_row * layout.cols + layout.current_col];

  if (i < 0)
    i = current_space;

  if (i >= num_workspaces)
    meta_bug ("calc_workspace_layout left an invalid (too-high) workspace number %d in the grid\n",
              i);
    
  meta_verbose ("Neighbor workspace is %d at row %d col %d\n",
                i, layout.current_row, layout.current_col);

  meta_screen_free_workspace_layout (&layout);
  
  return meta_screen_get_workspace_by_index (workspace->screen, i);
}

const char*
meta_workspace_get_name (MetaWorkspace *workspace)
{
  return meta_prefs_get_workspace_name (meta_workspace_index (workspace));
}
