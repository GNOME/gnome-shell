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

#endif /* META_WORKSPACE_MANAGER_PRIVATE_H */
