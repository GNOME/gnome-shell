/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * \file screen-private.h  Screens which Mutter manages
 *
 * Managing X screens.
 * This file contains methods on this class which are available to
 * routines in core but not outside it.  (See screen.h for the routines
 * which the rest of the world is allowed to use.)
 */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
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

#ifndef META_SCREEN_PRIVATE_H
#define META_SCREEN_PRIVATE_H

#include "display-private.h"
#include <meta/screen.h>
#include <X11/Xutil.h>
#include "stack-tracker.h"
#include "ui.h"
#include "meta-monitor-manager-private.h"

typedef void (* MetaScreenWindowFunc) (MetaWindow *window,
                                       gpointer    user_data);

#define META_WIREFRAME_XOR_LINE_WIDTH 2

struct _MetaScreen
{
  GObject parent_instance;

  MetaDisplay *display;
  MetaRectangle rect;  /* Size of screen; rect.x & rect.y are always 0 */
  MetaUI *ui;

  guint tile_preview_timeout_id;
  guint preview_tile_mode : 2;

  MetaWorkspace *active_workspace;

  /* This window holds the focus when we don't want to focus
   * any actual clients
   */
  Window no_focus_window;

  GList *workspaces;

  MetaStack *stack;
  MetaStackTracker *stack_tracker;

  MetaCursor current_cursor;

  Window wm_sn_selection_window;
  Atom wm_sn_atom;
  guint32 wm_sn_timestamp;

  gboolean has_xinerama_indices;

  GSList *startup_sequences;

  Window wm_cm_selection_window;
  guint work_area_later;
  guint check_fullscreen_later;

  int rows_of_workspaces;
  int columns_of_workspaces;
  MetaScreenCorner starting_corner;
  guint vertical_workspaces : 1;
  guint workspace_layout_overridden : 1;

  guint keys_grabbed : 1;

  int closing;

  /* Instead of unmapping withdrawn windows we can leave them mapped
   * and restack them below a guard window. When using a compositor
   * this allows us to provide live previews of unmapped windows */
  Window guard_window;

  Window composite_overlay_window;
};

struct _MetaScreenClass
{
  GObjectClass parent_class;

  void (*restacked)         (MetaScreen *);
  void (*workareas_changed) (MetaScreen *);
  void (*monitors_changed)  (MetaScreen *);
};

MetaScreen*   meta_screen_new                 (MetaDisplay                *display,
                                               guint32                     timestamp);
void          meta_screen_free                (MetaScreen                 *screen,
                                               guint32                     timestamp);
void          meta_screen_init_workspaces     (MetaScreen                 *screen);
void          meta_screen_manage_all_windows  (MetaScreen                 *screen);
void          meta_screen_foreach_window      (MetaScreen                 *screen,
                                               MetaListWindowsFlags        flags,
                                               MetaScreenWindowFunc        func,
                                               gpointer                    data);

void          meta_screen_update_cursor       (MetaScreen                 *screen);

void          meta_screen_update_tile_preview          (MetaScreen    *screen,
                                                        gboolean       delay);
void          meta_screen_hide_tile_preview            (MetaScreen    *screen);

MetaWindow*   meta_screen_get_mouse_window     (MetaScreen                 *screen,
                                                MetaWindow                 *not_this_one);

void          meta_screen_update_workspace_layout (MetaScreen             *screen);
void          meta_screen_update_workspace_names  (MetaScreen             *screen);
void          meta_screen_queue_workarea_recalc   (MetaScreen             *screen);
void          meta_screen_queue_check_fullscreen  (MetaScreen             *screen);

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

void meta_screen_calc_workspace_layout (MetaScreen          *screen,
                                        int                  num_workspaces,
                                        int                  current_space,
                                        MetaWorkspaceLayout *layout);
void meta_screen_free_workspace_layout (MetaWorkspaceLayout *layout);

void     meta_screen_minimize_all_on_active_workspace_except (MetaScreen *screen,
                                                              MetaWindow *keep);

/* Show/hide the desktop (temporarily hide all windows) */
void     meta_screen_show_desktop        (MetaScreen *screen,
                                          guint32     timestamp);
void     meta_screen_unshow_desktop      (MetaScreen *screen);

/* Update whether the destkop is being shown for the current active_workspace */
void     meta_screen_update_showing_desktop_hint          (MetaScreen *screen);

gboolean meta_screen_apply_startup_properties (MetaScreen *screen,
                                               MetaWindow *window);
void     meta_screen_restacked (MetaScreen *screen);

void     meta_screen_workspace_switched (MetaScreen         *screen,
                                         int                 from,
                                         int                 to,
                                         MetaMotionDirection direction);

void meta_screen_set_active_workspace_hint (MetaScreen *screen);

void meta_screen_create_guard_window (MetaScreen *screen);

gboolean meta_screen_handle_xevent (MetaScreen *screen,
                                    XEvent     *xevent);

MetaLogicalMonitor * meta_screen_xinerama_index_to_logical_monitor (MetaScreen *screen,
                                                                    int         index);

int meta_screen_logical_monitor_to_xinerama_index (MetaScreen         *screen,
                                                   MetaLogicalMonitor *logical_monitor);

#endif
