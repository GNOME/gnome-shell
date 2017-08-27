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

#ifndef META_WORKSPACE_MANAGER_PRIVATE_H
#define META_WORKSPACE_MANAGER_PRIVATE_H

#include <glib.h>

#include "core/display-private.h"
#include "meta/common.h"
#include "meta/types.h"
#include "meta/meta-workspace-manager.h"

struct _MetaWorkspaceManager
{
  GObject parent;

  MetaDisplay *display;
  MetaWorkspace *active_workspace;

  GList *workspaces;

  int rows_of_workspaces;
  int columns_of_workspaces;
  MetaDisplayCorner starting_corner;
  guint vertical_workspaces : 1;
  guint workspace_layout_overridden : 1;
};

MetaWorkspaceManager *meta_workspace_manager_new (MetaDisplay *display);

void meta_workspace_manager_init_workspaces         (MetaWorkspaceManager *workspace_manager);
void meta_workspace_manager_update_workspace_layout (MetaWorkspaceManager *workspace_manager,
                                                     MetaDisplayCorner     starting_corner,
                                                     gboolean              vertical_layout,
                                                     int                   n_rows,
                                                     int                   n_columns);

void meta_workspace_manager_reload_work_areas (MetaWorkspaceManager *workspace_manager);

typedef struct MetaWorkspaceLayout MetaWorkspaceLayout;

struct MetaWorkspaceLayout
{
  int rows;
  int cols;
  int *grid;
  int grid_area;
  int current_row;
  int current_col;
};

void meta_workspace_manager_calc_workspace_layout (MetaWorkspaceManager *workspace_manager,
                                                   int                   num_workspaces,
                                                   int                   current_space,
                                                   MetaWorkspaceLayout  *layout);

void meta_workspace_manager_free_workspace_layout (MetaWorkspaceLayout *layout);

void meta_workspace_manager_minimize_all_on_active_workspace_except (MetaWorkspaceManager *workspace_manager,
                                                                     MetaWindow           *keep);

/* Show/hide the desktop (temporarily hide all windows) */
void meta_workspace_manager_show_desktop   (MetaWorkspaceManager *workspace_manager,
                                            guint32               timestamp);
void meta_workspace_manager_unshow_desktop (MetaWorkspaceManager *workspace_manager);

void meta_workspace_manager_workspace_switched (MetaWorkspaceManager *workspace_manager,
                                                int                   from,
                                                int                   to,
                                                MetaMotionDirection   direction);

void meta_workspace_manager_update_num_workspaces (MetaWorkspaceManager *workspace_manager,
                                                   guint32               timestamp,
                                                   int                   new_num);

#endif /* META_WORKSPACE_MANAGER_PRIVATE_H */
