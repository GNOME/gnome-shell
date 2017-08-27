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

#ifndef META_WORKSPACE_MANAGER_H
#define META_WORKSPACE_MANAGER_H

#include <glib-object.h>

#include <meta/common.h>
#include <meta/prefs.h>
#include <meta/types.h>

#define META_TYPE_WORKSPACE_MANAGER (meta_workspace_manager_get_type ())
G_DECLARE_FINAL_TYPE (MetaWorkspaceManager, meta_workspace_manager, META, WORKSPACE_MANAGER, GObject)

GList *meta_workspace_manager_get_workspaces (MetaWorkspaceManager *workspace_manager);

int meta_workspace_manager_get_n_workspaces (MetaWorkspaceManager *workspace_manager);

MetaWorkspace* meta_workspace_manager_get_workspace_by_index (MetaWorkspaceManager *workspace_manager,
                                                              int                   index);

void meta_workspace_manager_remove_workspace (MetaWorkspaceManager *workspace_manager,
                                              MetaWorkspace        *workspace,
                                              guint32               timestamp);

MetaWorkspace *meta_workspace_manager_append_new_workspace (MetaWorkspaceManager *workspace_manager,
                                                            gboolean              activate,
                                                            guint32               timestamp);

int meta_workspace_manager_get_active_workspace_index (MetaWorkspaceManager *workspace_manager);

MetaWorkspace *meta_workspace_manager_get_active_workspace (MetaWorkspaceManager *workspace_manager);

void meta_workspace_manager_override_workspace_layout (MetaWorkspaceManager *workspace_manager,
                                                       MetaDisplayCorner     starting_corner,
                                                       gboolean              vertical_layout,
                                                       int                   n_rows,
                                                       int                   n_columns);
#endif /* META_WORKSPACE_MANAGER_H */
