/* Metacity Theme Engine Header */

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

#ifndef META_THEME_H
#define META_THEME_H

#include <Xlib.h>

typedef struct _MetaFrameInfo       MetaFrameInfo;
typedef struct _MetaThemeEngine     MetaThemeEngine;

typedef enum
{
  META_FRAME_ALLOWS_DELETE    = 1 << 0,
  META_FRAME_ALLOWS_MENU      = 1 << 1,
  META_FRAME_ALLOWS_ICONIFY   = 1 << 2,
  META_FRAME_ALLOWS_MAXIMIZE  = 1 << 3, 
  META_FRAME_ALLOWS_RESIZE    = 1 << 4,
  META_FRAME_TRANSIENT        = 1 << 5
} MetaFrameFlags;

typedef enum
{
  META_FRAME_CONTROL_DELETE,
  META_FRAME_CONTROL_MENU,
  META_FRAME_CONTROL_ICONIFY,
  META_FRAME_CONTROL_MAXIMIZE,
  META_FRAME_CONTROL_RESIZE_SE,
  META_FRAME_CONTROL_RESIZE_S,
  META_FRAME_CONTROL_RESIZE_SW,
  META_FRAME_CONTROL_RESIZE_N,
  META_FRAME_CONTROL_RESIZE_NE,
  META_FRAME_CONTROL_RESIZE_NW,
  META_FRAME_CONTROL_RESIZE_W,
  META_FRAME_CONTROL_RESIZE_E
} MetaFrameControl;

struct _MetaFrameInfo
{
  /* These are read-only to engines */
  MetaFrameFlags flags;
  Window xwindow; /* == None in fill_frame_geometry */
  Display *display;
  Visual *visual;
  int depth;
  Colormap cmap;

  char *title;
  
  /* total frame size normally, but equal to child size in
   * fill_frame_geometry
   */
  int width;
  int height;
  
  /* Below here the engine can change things
   * in fill_frame_geometry, though defaults are already
   * filled in.
   */
  
  /* border sizes (space between frame and child) */
  int left_width;
  int right_width;
  int top_height;
  int bottom_height;

  /* background color */
  unsigned long background_pixel;

  Pixmap shape_mask;
  /* FIXME shape region */
};

struct _MetaThemeEngine
{
  void             (* unload_engine)       (void);
  
  /* returns frame_data to use */
  gpointer         (* acquire_frame)       (MetaFrameInfo *info);
  /* should free frame_data */
  void             (* release_frame)       (MetaFrameInfo *info,
                                            gpointer       frame_data);
  
  void             (* fill_frame_geometry) (MetaFrameInfo *info,
                                            gpointer       frame_data);

  void             (* expose_frame)        (MetaFrameInfo *info,
                                            int x, int y,
                                            int width, int height,
                                            gpointer frame_data);

  MetaFrameControl (* get_control)         (MetaFrameInfo *info,
                                            int x, int y,
                                            gpointer frame_data);
};

extern MetaThemeEngine meta_default_engine;

#endif
