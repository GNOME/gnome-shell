/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002, 2003, 2004 Red Hat, Inc.
 * Copyright (C) 2003, 2004 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
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

#include "config.h"

#include "core/meta-workspace-manager-private.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/window-private.h"
#include "core/workspace-private.h"

#include "meta/meta-enum-types.h"
#include "meta/prefs.h"
#include "meta/util.h"

G_DEFINE_TYPE (MetaWorkspaceManager, meta_workspace_manager, G_TYPE_OBJECT)

enum
{
  WORKSPACE_ADDED,
  WORKSPACE_REMOVED,
  WORKSPACE_SWITCHED,
  ACTIVE_WORKSPACE_CHANGED,
  SHOWING_DESKTOP_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,

  PROP_N_WORKSPACES
};

static guint workspace_manager_signals [LAST_SIGNAL] = { 0 };

static void prefs_changed_callback (MetaPreference pref,
                                    gpointer       data);

static void
meta_workspace_manager_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  MetaWorkspaceManager *workspace_manager = META_WORKSPACE_MANAGER (object);

  switch (prop_id)
    {
    case PROP_N_WORKSPACES:
      g_value_set_int (value, meta_workspace_manager_get_n_workspaces (workspace_manager));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_workspace_manager_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
meta_workspace_manager_finalize (GObject *object)
{
  MetaWorkspaceManager *workspace_manager = META_WORKSPACE_MANAGER (object);

  meta_prefs_remove_listener (prefs_changed_callback, workspace_manager);

  G_OBJECT_CLASS (meta_workspace_manager_parent_class)->finalize (object);
}

static void
meta_workspace_manager_class_init (MetaWorkspaceManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = meta_workspace_manager_get_property;
  object_class->set_property = meta_workspace_manager_set_property;

  object_class->finalize = meta_workspace_manager_finalize;

  workspace_manager_signals[WORKSPACE_ADDED] =
    g_signal_new ("workspace-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_INT);

  workspace_manager_signals[WORKSPACE_REMOVED] =
    g_signal_new ("workspace-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_INT);

  workspace_manager_signals[WORKSPACE_SWITCHED] =
    g_signal_new ("workspace-switched",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_INT,
                  G_TYPE_INT,
                  META_TYPE_MOTION_DIRECTION);

  workspace_manager_signals[ACTIVE_WORKSPACE_CHANGED] =
    g_signal_new ("active-workspace-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  workspace_manager_signals[SHOWING_DESKTOP_CHANGED] =
    g_signal_new ("showing-desktop-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
                                   PROP_N_WORKSPACES,
                                   g_param_spec_int ("n-workspaces",
                                                     "N Workspaces",
                                                     "Number of workspaces",
                                                     1, G_MAXINT, 1,
                                                     G_PARAM_READABLE));
}

static void
meta_workspace_manager_init (MetaWorkspaceManager *workspace_manager)
{
}

void
meta_workspace_manager_reload_work_areas (MetaWorkspaceManager *workspace_manager)
{
  GList *l;

  for (l = workspace_manager->workspaces; l; l = l->next)
    {
      MetaWorkspace *workspace = l->data;

      meta_workspace_invalidate_work_area (workspace);
    }
}

MetaWorkspaceManager *
meta_workspace_manager_new (MetaDisplay *display)
{
  MetaWorkspaceManager *workspace_manager;

  workspace_manager = g_object_new (META_TYPE_WORKSPACE_MANAGER, NULL);

  workspace_manager->display = display;
  workspace_manager->active_workspace = NULL;
  workspace_manager->workspaces = NULL;
  workspace_manager->rows_of_workspaces = 1;
  workspace_manager->columns_of_workspaces = -1;
  workspace_manager->vertical_workspaces = FALSE;
  workspace_manager->starting_corner = META_DISPLAY_TOPLEFT;

  /* This is the default layout extracted from default
   * variable values in update_num_workspaces ()
   * This can be overriden using _NET_DESKTOP_LAYOUT in
   * meta_x11_display_new (), if it's specified */
  meta_workspace_manager_update_workspace_layout (workspace_manager,
                                                  META_DISPLAY_TOPLEFT,
                                                  FALSE,
                                                  -1,
                                                  1);

  /* There must be at least one workspace at all times,
   * so create that required workspace.
   */
  meta_workspace_new (workspace_manager);

  meta_workspace_manager_init_workspaces (workspace_manager);

  meta_prefs_add_listener (prefs_changed_callback, workspace_manager);

  return workspace_manager;
}

void
meta_workspace_manager_init_workspaces (MetaWorkspaceManager *workspace_manager)
{
  int num;

  g_return_if_fail (META_IS_WORKSPACE_MANAGER (workspace_manager));

  if (meta_prefs_get_dynamic_workspaces ())
    /* This will be properly updated using _NET_NUMBER_OF_DESKTOPS
     * (if set) in meta_x11_display_new () */
    num = 1;
  else
    num = meta_prefs_get_num_workspaces ();

  meta_workspace_manager_update_num_workspaces (workspace_manager, META_CURRENT_TIME, num);

  meta_workspace_activate (workspace_manager->workspaces->data, META_CURRENT_TIME);

  meta_workspace_manager_reload_work_areas (workspace_manager);
}

int
meta_workspace_manager_get_n_workspaces (MetaWorkspaceManager *workspace_manager)
{
  return g_list_length (workspace_manager->workspaces);
}

/**
 * meta_workspace_manager_get_workspace_by_index:
 * @workspace_manager: a #MetaWorkspaceManager
 * @index: index of one of the display's workspaces
 *
 * Gets the workspace object for one of a workspace manager's workspaces given the workspace
 * index. It's valid to call this function with an out-of-range index and it
 * will robustly return %NULL.
 *
 * Return value: (transfer none): the workspace object with specified index, or %NULL
 *   if the index is out of range.
 */
MetaWorkspace *
meta_workspace_manager_get_workspace_by_index (MetaWorkspaceManager *workspace_manager,
                                               int                   idx)
{
  return g_list_nth_data (workspace_manager->workspaces, idx);
}

void
meta_workspace_manager_remove_workspace (MetaWorkspaceManager *workspace_manager,
                                         MetaWorkspace        *workspace,
                                         guint32               timestamp)
{
  GList *l;
  GList *next;
  MetaWorkspace *neighbour = NULL;
  int index;
  int active_index;
  gboolean active_index_changed;
  int new_num;

  l = g_list_find (workspace_manager->workspaces, workspace);
  if (!l)
    return;

  next = l->next;

  if (l->prev)
    neighbour = l->prev->data;
  else if (l->next)
    neighbour = l->next->data;
  else
    {
      /* Cannot remove the only workspace! */
      return;
    }

  meta_workspace_relocate_windows (workspace, neighbour);

  if (workspace == workspace_manager->active_workspace)
    meta_workspace_activate (neighbour, timestamp);

  /* To emit the signal after removing the workspace */
  index = meta_workspace_index (workspace);
  active_index = meta_workspace_manager_get_active_workspace_index (workspace_manager);
  active_index_changed = index < active_index;

  /* This also removes the workspace from the displays list */
  meta_workspace_remove (workspace);

  new_num = g_list_length (workspace_manager->workspaces);

  if (!meta_prefs_get_dynamic_workspaces ())
    meta_prefs_set_num_workspaces (new_num);

  /* If deleting a workspace before the current workspace, the active
   * workspace index changes, so we need to update that hint */
  if (active_index_changed)
    g_signal_emit (workspace_manager,
                   workspace_manager_signals[ACTIVE_WORKSPACE_CHANGED],
                   0, NULL);

  for (l = next; l; l = l->next)
    {
      MetaWorkspace *w = l->data;
      meta_workspace_index_changed (w);
    }

  meta_display_queue_workarea_recalc (workspace_manager->display);

  g_signal_emit (workspace_manager,
                 workspace_manager_signals[WORKSPACE_REMOVED],
                 0, index);
  g_object_notify (G_OBJECT (workspace_manager), "n-workspaces");
}

/**
 * meta_workspace_manager_append_new_workspace:
 * @workspace_manager: a #MetaWorkspaceManager
 * @activate: %TRUE if the workspace should be switched to after creation
 * @timestamp: if switching to a new workspace, timestamp to be used when
 *   focusing a window on the new workspace. (Doesn't hurt to pass a valid
 *   timestamp when available even if not switching workspaces.)
 *
 * Append a new workspace to the workspace manager and (optionally) switch to that
 * display.
 *
 * Return value: (transfer none): the newly appended workspace.
 */
MetaWorkspace *
meta_workspace_manager_append_new_workspace (MetaWorkspaceManager *workspace_manager,
                                             gboolean              activate,
                                             guint32               timestamp)
{
  MetaWorkspace *w;
  int new_num;

  /* This also adds the workspace to the workspace manager list */
  w = meta_workspace_new (workspace_manager);

  if (!w)
    return NULL;

  if (activate)
    meta_workspace_activate (w, timestamp);

  new_num = g_list_length (workspace_manager->workspaces);

  if (!meta_prefs_get_dynamic_workspaces ())
    meta_prefs_set_num_workspaces (new_num);

  meta_display_queue_workarea_recalc (workspace_manager->display);

  g_signal_emit (workspace_manager, workspace_manager_signals[WORKSPACE_ADDED],
                 0, meta_workspace_index (w));
  g_object_notify (G_OBJECT (workspace_manager), "n-workspaces");

  return w;
}

void
meta_workspace_manager_update_num_workspaces (MetaWorkspaceManager *workspace_manager,
                                              guint32               timestamp,
                                              int                   new_num)
{
  int old_num;
  GList *l;
  int i = 0;
  GList *extras = NULL;
  MetaWorkspace *last_remaining = NULL;
  gboolean need_change_space = FALSE;

  g_assert (new_num > 0);

  if (g_list_length (workspace_manager->workspaces) == (guint) new_num)
    return;

  for (l = workspace_manager->workspaces; l; l = l->next)
    {
      MetaWorkspace *w = l->data;

      if (i >= new_num)
        extras = g_list_prepend (extras, w);
      else
        last_remaining = w;

      ++i;
    }
  old_num = i;

  g_assert (last_remaining);

  /* Get rid of the extra workspaces by moving all their windows
   * to last_remaining, then activating last_remaining if
   * one of the removed workspaces was active. This will be a bit
   * wacky if the config tool for changing number of workspaces
   * is on a removed workspace ;-)
   */
  for (l = extras; l; l = l->next)
    {
      MetaWorkspace *w = l->data;

      meta_workspace_relocate_windows (w, last_remaining);

      if (w == workspace_manager->active_workspace)
        need_change_space = TRUE;
    }

  if (need_change_space)
    meta_workspace_activate (last_remaining, timestamp);

  /* Should now be safe to free the workspaces */
  for (l = extras; l; l = l->next)
    {
      MetaWorkspace *w = l->data;

      meta_workspace_remove (w);
    }

  g_list_free (extras);

  for (i = old_num; i < new_num; i++)
    meta_workspace_new (workspace_manager);

  meta_display_queue_workarea_recalc (workspace_manager->display);

  for (i = old_num; i < new_num; i++)
    g_signal_emit (workspace_manager,
                   workspace_manager_signals[WORKSPACE_ADDED],
                   0, i);

  g_object_notify (G_OBJECT (workspace_manager), "n-workspaces");
}

void
meta_workspace_manager_update_workspace_layout (MetaWorkspaceManager *workspace_manager,
                                                MetaDisplayCorner     starting_corner,
                                                gboolean              vertical_layout,
                                                int                   n_rows,
                                                int                   n_columns)
{
  g_return_if_fail (META_IS_WORKSPACE_MANAGER (workspace_manager));
  g_return_if_fail (n_rows > 0 || n_columns > 0);
  g_return_if_fail (n_rows != 0 && n_columns != 0);

  if (workspace_manager->workspace_layout_overridden)
    return;

  workspace_manager->vertical_workspaces = vertical_layout != FALSE;
  workspace_manager->starting_corner = starting_corner;
  workspace_manager->rows_of_workspaces = n_rows;
  workspace_manager->columns_of_workspaces = n_columns;

  meta_verbose ("Workspace layout rows = %d cols = %d orientation = %d starting corner = %u\n",
                workspace_manager->rows_of_workspaces,
                workspace_manager->columns_of_workspaces,
                workspace_manager->vertical_workspaces,
                workspace_manager->starting_corner);
}

/**
 * meta_workspace_manager_override_workspace_layout:
 * @workspace_manager: a #MetaWorkspaceManager
 * @starting_corner: the corner at which the first workspace is found
 * @vertical_layout: if %TRUE the workspaces are laid out in columns rather than rows
 * @n_rows: number of rows of workspaces, or -1 to determine the number of rows from
 *   @n_columns and the total number of workspaces
 * @n_columns: number of columns of workspaces, or -1 to determine the number of columns from
 *   @n_rows and the total number of workspaces
 *
 * Explicitly set the layout of workspaces. Once this has been called, the contents of the
 * _NET_DESKTOP_LAYOUT property on the root window are completely ignored.
 */
void
meta_workspace_manager_override_workspace_layout (MetaWorkspaceManager *workspace_manager,
                                                  MetaDisplayCorner     starting_corner,
                                                  gboolean              vertical_layout,
                                                  int                   n_rows,
                                                  int                   n_columns)
{
  meta_workspace_manager_update_workspace_layout (workspace_manager,
                                                  starting_corner,
                                                  vertical_layout,
                                                  n_rows,
                                                  n_columns);

  workspace_manager->workspace_layout_overridden = TRUE;
}

#ifdef WITH_VERBOSE_MODE
static const char *
meta_workspace_manager_corner_to_string (MetaDisplayCorner corner)
{
  switch (corner)
    {
    case META_DISPLAY_TOPLEFT:
      return "TopLeft";
    case META_DISPLAY_TOPRIGHT:
      return "TopRight";
    case META_DISPLAY_BOTTOMLEFT:
      return "BottomLeft";
    case META_DISPLAY_BOTTOMRIGHT:
      return "BottomRight";
    }

  return "Unknown";
}
#endif /* WITH_VERBOSE_MODE */

void
meta_workspace_manager_calc_workspace_layout (MetaWorkspaceManager *workspace_manager,
                                              int                   num_workspaces,
                                              int                   current_space,
                                              MetaWorkspaceLayout  *layout)
{
  int rows, cols;
  int grid_area;
  int *grid;
  int i, r, c;
  int current_row, current_col;

  rows = workspace_manager->rows_of_workspaces;
  cols = workspace_manager->columns_of_workspaces;
  if (rows <= 0 && cols <= 0)
    cols = num_workspaces;

  if (rows <= 0)
    rows = num_workspaces / cols + ((num_workspaces % cols) > 0 ? 1 : 0);
  if (cols <= 0)
    cols = num_workspaces / rows + ((num_workspaces % rows) > 0 ? 1 : 0);

  /* paranoia */
  if (rows < 1)
    rows = 1;
  if (cols < 1)
    cols = 1;

  g_assert (rows != 0 && cols != 0);

  grid_area = rows * cols;

  meta_verbose ("Getting layout rows = %d cols = %d current = %d "
                "num_spaces = %d vertical = %s corner = %s\n",
                rows, cols, current_space, num_workspaces,
                workspace_manager->vertical_workspaces ? "(true)" : "(false)",
                meta_workspace_manager_corner_to_string (workspace_manager->starting_corner));

  /* ok, we want to setup the distances in the workspace array to go
   * in each direction. Remember, there are many ways that a workspace
   * array can be setup.
   * see http://www.freedesktop.org/standards/wm-spec/1.2/html/x109.html
   * and look at the _NET_DESKTOP_LAYOUT section for details.
   * For instance:
   */
  /* starting_corner = META_DISPLAY_TOPLEFT
   *  vertical_workspaces = 0                 vertical_workspaces=1
   *       1234                                    1357
   *       5678                                    2468
   *
   * starting_corner = META_DISPLAY_TOPRIGHT
   *  vertical_workspaces = 0                 vertical_workspaces=1
   *       4321                                    7531
   *       8765                                    8642
   *
   * starting_corner = META_DISPLAY_BOTTOMLEFT
   *  vertical_workspaces = 0                 vertical_workspaces=1
   *       5678                                    2468
   *       1234                                    1357
   *
   * starting_corner = META_DISPLAY_BOTTOMRIGHT
   *  vertical_workspaces = 0                 vertical_workspaces=1
   *       8765                                    8642
   *       4321                                    7531
   *
   */
  /* keep in mind that we could have a ragged layout, e.g. the "8"
   * in the above grids could be missing
   */


  grid = g_new (int, grid_area);

  current_row = -1;
  current_col = -1;
  i = 0;

  switch (workspace_manager->starting_corner)
    {
    case META_DISPLAY_TOPLEFT:
      if (workspace_manager->vertical_workspaces)
        {
          c = 0;
          while (c < cols)
            {
              r = 0;
              while (r < rows)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++r;
                }
              ++c;
            }
        }
      else
        {
          r = 0;
          while (r < rows)
            {
              c = 0;
              while (c < cols)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++c;
                }
              ++r;
            }
        }
      break;
    case META_DISPLAY_TOPRIGHT:
      if (workspace_manager->vertical_workspaces)
        {
          c = cols - 1;
          while (c >= 0)
            {
              r = 0;
              while (r < rows)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++r;
                }
              --c;
            }
        }
      else
        {
          r = 0;
          while (r < rows)
            {
              c = cols - 1;
              while (c >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --c;
                }
              ++r;
            }
        }
      break;
    case META_DISPLAY_BOTTOMLEFT:
      if (workspace_manager->vertical_workspaces)
        {
          c = 0;
          while (c < cols)
            {
              r = rows - 1;
              while (r >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --r;
                }
              ++c;
            }
        }
      else
        {
          r = rows - 1;
          while (r >= 0)
            {
              c = 0;
              while (c < cols)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  ++c;
                }
              --r;
            }
        }
      break;
    case META_DISPLAY_BOTTOMRIGHT:
      if (workspace_manager->vertical_workspaces)
        {
          c = cols - 1;
          while (c >= 0)
            {
              r = rows - 1;
              while (r >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --r;
                }
              --c;
            }
        }
      else
        {
          r = rows - 1;
          while (r >= 0)
            {
              c = cols - 1;
              while (c >= 0)
                {
                  grid[r*cols+c] = i;
                  ++i;
                  --c;
                }
              --r;
            }
        }
      break;
    }

  if (i != grid_area)
    meta_bug ("did not fill in the whole workspace grid in %s (%d filled)\n",
              G_STRFUNC, i);

  current_row = 0;
  current_col = 0;
  r = 0;
  while (r < rows)
    {
      c = 0;
      while (c < cols)
        {
          if (grid[r*cols+c] == current_space)
            {
              current_row = r;
              current_col = c;
            }
          else if (grid[r*cols+c] >= num_workspaces)
            {
              /* flag nonexistent spaces with -1 */
              grid[r*cols+c] = -1;
            }
          ++c;
        }
      ++r;
    }

  layout->rows = rows;
  layout->cols = cols;
  layout->grid = grid;
  layout->grid_area = grid_area;
  layout->current_row = current_row;
  layout->current_col = current_col;

#ifdef WITH_VERBOSE_MODE
  if (meta_is_verbose ())
    {
      r = 0;
      while (r < layout->rows)
        {
          meta_verbose (" ");
          meta_push_no_msg_prefix ();
          c = 0;
          while (c < layout->cols)
            {
              if (r == layout->current_row &&
                  c == layout->current_col)
                meta_verbose ("*%2d ", layout->grid[r*layout->cols+c]);
              else
                meta_verbose ("%3d ", layout->grid[r*layout->cols+c]);
              ++c;
            }
          meta_verbose ("\n");
          meta_pop_no_msg_prefix ();
          ++r;
        }
    }
#endif /* WITH_VERBOSE_MODE */
}

void
meta_workspace_manager_free_workspace_layout (MetaWorkspaceLayout *layout)
{
  g_free (layout->grid);
}

static void
queue_windows_showing (MetaWorkspaceManager *workspace_manager)
{
  GSList *windows, *l;

  /* Must operate on all windows on display instead of just on the
   * active_workspace's window list, because the active_workspace's
   * window list may not contain the on_all_workspace windows.
   */
  windows = meta_display_list_windows (workspace_manager->display, META_LIST_DEFAULT);

  for (l = windows; l; l = l->next)
    {
      MetaWindow *w = l->data;

      meta_window_queue (w, META_QUEUE_CALC_SHOWING);
    }

  g_slist_free (windows);
}

void
meta_workspace_manager_minimize_all_on_active_workspace_except (MetaWorkspaceManager *workspace_manager,
                                                                MetaWindow           *keep)
{
  GList *l;

  for (l = workspace_manager->active_workspace->windows; l; l = l->next)
    {
      MetaWindow *w = l->data;

      if (w->has_minimize_func && w != keep)
        meta_window_minimize (w);
    }
}

void
meta_workspace_manager_show_desktop (MetaWorkspaceManager *workspace_manager,
                                     guint32               timestamp)
{
  GList *l;

  if (workspace_manager->active_workspace->showing_desktop)
    return;

  workspace_manager->active_workspace->showing_desktop = TRUE;

  queue_windows_showing (workspace_manager);

  /* Focus the most recently used META_WINDOW_DESKTOP window, if there is one;
   * see bug 159257.
   */
  for (l = workspace_manager->active_workspace->mru_list; l; l = l->next)
    {
      MetaWindow *w = l->data;

      if (w->type == META_WINDOW_DESKTOP)
        {
          meta_window_focus (w, timestamp);
          break;
        }
    }

  g_signal_emit (workspace_manager,
                 workspace_manager_signals[SHOWING_DESKTOP_CHANGED],
                 0, NULL);
}

void
meta_workspace_manager_unshow_desktop (MetaWorkspaceManager *workspace_manager)
{
  if (!workspace_manager->active_workspace->showing_desktop)
    return;

  workspace_manager->active_workspace->showing_desktop = FALSE;

  queue_windows_showing (workspace_manager);

  g_signal_emit (workspace_manager,
                 workspace_manager_signals[SHOWING_DESKTOP_CHANGED],
                 0, NULL);
}

/**
 * meta_workspace_manager_get_workspaces: (skip)
 * @workspace_manager: a #MetaWorkspaceManager
 *
 * Returns: (transfer none) (element-type Meta.Workspace): The workspaces for @display
 */
GList *
meta_workspace_manager_get_workspaces (MetaWorkspaceManager *workspace_manager)
{
  return workspace_manager->workspaces;
}

int
meta_workspace_manager_get_active_workspace_index (MetaWorkspaceManager *workspace_manager)
{
  MetaWorkspace *active = workspace_manager->active_workspace;

  if (!active)
    return -1;

  return meta_workspace_index (active);
}

/**
 * meta_workspace_manager_get_active_workspace:
 * @workspace_manager: A #MetaWorkspaceManager
 *
 * Returns: (transfer none): The current workspace
 */
MetaWorkspace *
meta_workspace_manager_get_active_workspace (MetaWorkspaceManager *workspace_manager)
{
  return workspace_manager->active_workspace;
}

void 
meta_workspace_manager_workspace_switched (MetaWorkspaceManager *workspace_manager,
                                           int                   from,
                                           int                   to,
                                           MetaMotionDirection   direction)
{
  g_signal_emit (workspace_manager,
                 workspace_manager_signals[WORKSPACE_SWITCHED], 0,
                 from, to, direction);
}

static void
prefs_changed_callback (MetaPreference pref,
                        gpointer       data)
{
  MetaWorkspaceManager *workspace_manager = data;

  if ((pref == META_PREF_NUM_WORKSPACES ||
       pref == META_PREF_DYNAMIC_WORKSPACES) &&
      !meta_prefs_get_dynamic_workspaces ())
    {
      guint32 timestamp;
      int new_num;

      timestamp =
        meta_display_get_current_time_roundtrip (workspace_manager->display);
      new_num = meta_prefs_get_num_workspaces ();
      meta_workspace_manager_update_num_workspaces (workspace_manager,
                                                    timestamp, new_num);
    }
}
