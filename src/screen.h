/* Metacity X screen handler */

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

#ifndef META_SCREEN_H
#define META_SCREEN_H

#include "display.h"
#include <X11/Xutil.h>
#include "ui.h"

typedef struct _MetaXineramaScreenInfo MetaXineramaScreenInfo;

struct _MetaXineramaScreenInfo
{
  int number;
  int x_origin;
  int y_origin;
  int width;
  int height;
};

typedef void (* MetaScreenWindowFunc) (MetaScreen *screen, MetaWindow *window,
                                       gpointer user_data);

struct _MetaScreen
{
  MetaDisplay *display;
  int number;
  char *screen_name;
  Screen *xscreen;
  Window xroot;
  int default_depth;
  Visual *default_xvisual;
  int width;
  int height;
  MetaUI *ui;
  MetaTabPopup *tab_popup;
  
  MetaWorkspace *active_workspace;

  MetaStack *stack;

  MetaCursor current_cursor;

  MetaXineramaScreenInfo *xinerama_infos;
  int n_xinerama_infos;

  guint work_area_idle;

  int rows_of_workspaces;
  int columns_of_workspaces;
  guint vertical_workspaces : 1;
  
  guint keys_grabbed : 1;
};

MetaScreen*   meta_screen_new                 (MetaDisplay                *display,
                                               int                         number);
void          meta_screen_free                (MetaScreen                 *screen);
void          meta_screen_manage_all_windows  (MetaScreen                 *screen);
MetaScreen*   meta_screen_for_x_screen        (Screen                     *xscreen);
void          meta_screen_foreach_window      (MetaScreen                 *screen,
                                               MetaScreenWindowFunc        func,
                                               gpointer                    data);
void          meta_screen_queue_frame_redraws (MetaScreen                 *screen);
void          meta_screen_queue_window_resizes (MetaScreen                 *screen);

int           meta_screen_get_n_workspaces    (MetaScreen                 *screen);

void          meta_screen_set_cursor          (MetaScreen                 *screen,
                                               MetaCursor                  cursor);

void          meta_screen_ensure_tab_popup    (MetaScreen                 *screen,
                                               MetaTabList                 type);

void          meta_screen_focus_top_window    (MetaScreen                 *screen,
                                               MetaWindow                 *not_this_one);

const MetaXineramaScreenInfo* meta_screen_get_current_xinerama (MetaScreen *screen);

void          meta_screen_update_workspace_layout (MetaScreen             *screen);

#endif




