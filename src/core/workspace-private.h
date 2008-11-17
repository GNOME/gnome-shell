/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * \file workspace.h    Workspaces
 *
 * A workspace is a set of windows which all live on the same
 * screen.  (You may also see the name "desktop" around the place,
 * which is the EWMH's name for the same thing.)  Only one workspace
 * of a screen may be active at once; all windows on all other workspaces
 * are unmapped.
 */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2004, 2005 Elijah Newren
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

#ifndef META_WORKSPACE_PRIVATE_H
#define META_WORKSPACE_PRIVATE_H

#include "workspace.h"
#include "window-private.h"

struct _MetaWorkspace
{
  GObject parent_instance;
  MetaScreen *screen;
  
  GList *windows;
  GList *mru_list;

  GList  *list_containing_self;

  MetaRectangle work_area_screen;
  MetaRectangle *work_area_xinerama;
  GList  *screen_region;
  GList  **xinerama_region;
  gint n_xinerama_regions;
  GList  *screen_edges;
  GList  *xinerama_edges;
  GSList *all_struts;
  guint work_areas_invalid : 1;

  guint showing_desktop : 1;
};

struct _MetaWorkspaceClass
{
  GObjectClass parent_class;
};

MetaWorkspace* meta_workspace_new           (MetaScreen    *screen);
void           meta_workspace_remove        (MetaWorkspace *workspace);
void           meta_workspace_add_window    (MetaWorkspace *workspace,
                                             MetaWindow    *window);
void           meta_workspace_remove_window (MetaWorkspace *workspace,
                                             MetaWindow    *window);
void           meta_workspace_relocate_windows (MetaWorkspace *workspace,
                                                MetaWorkspace *new_home);
GList*         meta_workspace_list_windows  (MetaWorkspace *workspace);

void meta_workspace_invalidate_work_area (MetaWorkspace *workspace);


void meta_workspace_get_work_area_for_xinerama  (MetaWorkspace *workspace,
                                                 int            which_xinerama,
                                                 MetaRectangle *area);
GList* meta_workspace_get_onscreen_region       (MetaWorkspace *workspace);
GList* meta_workspace_get_onxinerama_region     (MetaWorkspace *workspace,
                                                 int            which_xinerama);
void meta_workspace_get_work_area_all_xineramas (MetaWorkspace *workspace,
                                                 MetaRectangle *area);

void meta_workspace_focus_default_window (MetaWorkspace *workspace,
                                          MetaWindow    *not_this_one,
                                          guint32        timestamp);

MetaWorkspace* meta_workspace_get_neighbor (MetaWorkspace      *workspace,
                                            MetaMotionDirection direction);

const char* meta_workspace_get_name (MetaWorkspace *workspace);

#endif




