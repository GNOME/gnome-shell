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

#include "workspace.h"
#include "errors.h"
#include <X11/Xatom.h>

void meta_workspace_queue_calc_showing  (MetaWorkspace *workspace);

static int set_number_of_spaces_hint  (MetaScreen *screen);
static int set_active_space_hint      (MetaScreen *screen);

MetaWorkspace*
meta_workspace_new (MetaScreen *screen)
{
  MetaWorkspace *workspace;

  workspace = g_new (MetaWorkspace, 1);

  workspace->screen = screen;
  workspace->screen->display->workspaces =
    g_list_append (workspace->screen->display->workspaces, workspace);
  workspace->windows = NULL;

  workspace->work_area.x = 0;
  workspace->work_area.y = 0;
  workspace->work_area.width = screen->width;
  workspace->work_area.height = screen->height;
  workspace->work_area_invalid = TRUE;
  
  /* Update hint for current number of workspaces */
  set_number_of_spaces_hint (screen);
  
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
  
  workspace->screen->display->workspaces =
    g_list_remove (workspace->screen->display->workspaces, workspace);

  g_free (workspace);

  /* Update hint for current number of workspaces */
  set_number_of_spaces_hint (screen);
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
  tmp = workspace->screen->display->workspaces;
  while (tmp != NULL)
    {
      if (tmp->data == workspace)
        return i;

      ++i;
                    
      tmp = tmp->next;
    }

  meta_bug ("Workspace does not exist to index!\n");
}

int
meta_workspace_screen_index  (MetaWorkspace *workspace)
{
  GList *tmp;
  int i;

  i = 0;
  tmp = workspace->screen->display->workspaces;
  while (tmp != NULL)
    {
      MetaWorkspace *w = tmp->data;

      if (tmp->data == workspace)
        return i;

      if (w->screen == workspace->screen)
        ++i;
                    
      tmp = tmp->next;
    }

  meta_bug ("Workspace does not exist to index!\n");
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

      if (window->on_all_workspaces ||
          meta_workspace_contains_window (workspace, window))
        workspace_windows = g_list_prepend (workspace_windows,
                                            window);

      tmp = tmp->next;
    }

  g_slist_free (display_windows);

  return workspace_windows;
}

static int
set_number_of_spaces_hint (MetaScreen *screen)
{
  unsigned long data[1];
  
  data[0] = meta_screen_get_n_workspaces (screen);

  meta_verbose ("Setting _NET_NUMBER_OF_DESKTOPS to %ld\n", data[0]);
  
  meta_error_trap_push (screen->display);
  XChangeProperty (screen->display->xdisplay, screen->xroot,
                   screen->display->atom_net_number_of_desktops,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);
  return meta_error_trap_pop (screen->display);
}

static int
set_active_space_hint (MetaScreen *screen)
{
  unsigned long data[1];
  
  data[0] = meta_workspace_screen_index (screen->active_workspace);

  meta_verbose ("Setting _NET_CURRENT_DESKTOP to %ld\n", data[0]);
  
  meta_error_trap_push (screen->display);
  XChangeProperty (screen->display->xdisplay, screen->xroot,
                   screen->display->atom_net_current_desktop,
                   XA_CARDINAL,
                   32, PropModeReplace, (guchar*) data, 1);
  return meta_error_trap_pop (screen->display);
}

void
meta_workspace_invalidate_work_area (MetaWorkspace *workspace)
{
  GList *tmp;

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
  tmp = workspace->windows;
  while (tmp != NULL)
    {
      MetaWindow *w = tmp->data;

      meta_window_queue_move_resize (w);
          
      tmp = tmp->next;
    }
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
          left_strut = (workspace->screen->width - MIN_SANE_AREA) / 2;
          right_strut = left_strut;
        }

      if ((top_strut + bottom_strut) > (workspace->screen->height - MIN_SANE_AREA))
        {
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
