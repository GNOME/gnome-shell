/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2008 Iain Holmes
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

#include <X11/Xlib.h>
#include <glib-object.h>
#include <meta/types.h>
#include <meta/workspace.h>

#define META_TYPE_SCREEN            (meta_screen_get_type ())
#define META_SCREEN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_SCREEN, MetaScreen))
#define META_SCREEN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_SCREEN, MetaScreenClass))
#define META_IS_SCREEN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_SCREEN))
#define META_IS_SCREEN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_SCREEN))
#define META_SCREEN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_SCREEN, MetaScreenClass))

typedef struct _MetaScreenClass   MetaScreenClass;

GType meta_screen_get_type (void);

int meta_screen_get_screen_number (MetaScreen *screen);
MetaDisplay *meta_screen_get_display (MetaScreen *screen);

Window meta_screen_get_xroot (MetaScreen *screen);
void meta_screen_get_size (MetaScreen *screen,
                           int        *width,
                           int        *height);

gpointer meta_screen_get_compositor_data (MetaScreen *screen);
void meta_screen_set_compositor_data (MetaScreen *screen,
                                      gpointer    info);

MetaScreen *meta_screen_for_x_screen (Screen *xscreen);

void meta_screen_set_cm_selection (MetaScreen *screen);
void meta_screen_unset_cm_selection (MetaScreen *screen);

GSList *meta_screen_get_startup_sequences (MetaScreen *screen);

GList *meta_screen_get_workspaces (MetaScreen *screen);

int meta_screen_get_n_workspaces (MetaScreen *screen);

MetaWorkspace* meta_screen_get_workspace_by_index (MetaScreen    *screen,
                                                   int            index);
void meta_screen_remove_workspace (MetaScreen    *screen,
                                   MetaWorkspace *workspace,
                                   guint32        timestamp);

MetaWorkspace *meta_screen_append_new_workspace (MetaScreen    *screen,
                                                 gboolean       activate,
                                                 guint32        timestamp);

int meta_screen_get_active_workspace_index (MetaScreen *screen);

MetaWorkspace * meta_screen_get_active_workspace (MetaScreen *screen);

int  meta_screen_get_n_monitors       (MetaScreen    *screen);
int  meta_screen_get_primary_monitor  (MetaScreen    *screen);
int  meta_screen_get_current_monitor  (MetaScreen    *screen);
int  meta_screen_get_current_monitor_for_pos  (MetaScreen    *screen,
                                               int x,
                                               int y);
void meta_screen_get_monitor_geometry (MetaScreen    *screen,
                                       int            monitor,
                                       MetaRectangle *geometry);

gboolean meta_screen_get_monitor_in_fullscreen (MetaScreen  *screen,
                                                int          monitor);

int meta_screen_get_monitor_index_for_rect (MetaScreen    *screen,
                                            MetaRectangle *rect);

void meta_screen_focus_default_window (MetaScreen *screen,
                                       guint32     timestamp);

/**
 * MetaScreenCorner:
 * @META_SCREEN_TOPLEFT: top-left corner
 * @META_SCREEN_TOPRIGHT: top-right corner
 * @META_SCREEN_BOTTOMLEFT: bottom-left corner
 * @META_SCREEN_BOTTOMRIGHT: bottom-right corner
 */
typedef enum
{
  META_SCREEN_TOPLEFT,
  META_SCREEN_TOPRIGHT,
  META_SCREEN_BOTTOMLEFT,
  META_SCREEN_BOTTOMRIGHT
} MetaScreenCorner;

void meta_screen_override_workspace_layout (MetaScreen      *screen,
                                            MetaScreenCorner starting_corner,
                                            gboolean         vertical_layout,
                                            int              n_rows,
                                            int              n_columns);

void          meta_screen_set_cursor          (MetaScreen                 *screen,
                                               MetaCursor                  cursor);

#endif
