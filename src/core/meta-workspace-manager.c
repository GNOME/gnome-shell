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

#include "meta/meta-enum-types.h"

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
meta_workspace_manager_class_init (MetaWorkspaceManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = meta_workspace_manager_get_property;
  object_class->set_property = meta_workspace_manager_set_property;

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
}

static void
meta_workspace_manager_init (MetaWorkspaceManager *workspace_manager)
{
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

  return workspace_manager;
}

int
meta_workspace_manager_get_n_workspaces (MetaWorkspaceManager *workspace_manager)
{
  return g_list_length (workspace_manager->workspaces);
}
