/* Metacity window frame manager widget */

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

#ifndef META_FRAMES_H
#define META_FRAMES_H

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "common.h"

typedef enum
{
  META_FRAME_CONTROL_NONE,
  META_FRAME_CONTROL_TITLE,
  META_FRAME_CONTROL_DELETE,
  META_FRAME_CONTROL_MENU,
  META_FRAME_CONTROL_MINIMIZE,
  META_FRAME_CONTROL_MAXIMIZE,
  META_FRAME_CONTROL_UNMAXIMIZE,
  META_FRAME_CONTROL_RESIZE_SE,
  META_FRAME_CONTROL_RESIZE_S,
  META_FRAME_CONTROL_RESIZE_SW,
  META_FRAME_CONTROL_RESIZE_N,
  META_FRAME_CONTROL_RESIZE_NE,
  META_FRAME_CONTROL_RESIZE_NW,
  META_FRAME_CONTROL_RESIZE_W,
  META_FRAME_CONTROL_RESIZE_E,
  META_FRAME_CONTROL_CLIENT_AREA
} MetaFrameControl;

/* This is one widget that manages all the window frames
 * as subwindows.
 */

#define META_TYPE_FRAMES            (meta_frames_get_type ())
#define META_FRAMES(obj)            (GTK_CHECK_CAST ((obj), META_TYPE_FRAMES, MetaFrames))
#define META_FRAMES_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), META_TYPE_FRAMES, MetaFramesClass))
#define META_IS_FRAMES(obj)         (GTK_CHECK_TYPE ((obj), META_TYPE_FRAMES))
#define META_IS_FRAMES_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), META_TYPE_FRAMES))
#define META_FRAMES_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), META_TYPE_FRAMES, MetaFramesClass))

typedef struct _MetaFrames        MetaFrames;
typedef struct _MetaFramesClass   MetaFramesClass;

typedef struct _MetaUIFrame         MetaUIFrame;
typedef struct _MetaFrameProperties MetaFrameProperties;

struct _MetaUIFrame
{
  Window xwindow;
  GdkWindow *window;
  PangoLayout *layout;
  guint expose_delayed : 1;
};

struct _MetaFrames
{
  GtkWindow parent_instance;

  /* If we did a widget per frame, we wouldn't want to cache this. */
  MetaFrameProperties *props;

  int text_height;

  GHashTable *frames;

  guint tooltip_timeout;
  MetaUIFrame *last_motion_frame;

  int expose_delay_count;
};

struct _MetaFramesClass
{
  GtkWindowClass parent_class;

};

GType        meta_frames_get_type               (void) G_GNUC_CONST;

MetaFrames *meta_frames_new (void);

void meta_frames_manage_window (MetaFrames *frames,
                                Window      xwindow);
void meta_frames_unmanage_window (MetaFrames *frames,
                                  Window      xwindow);
void meta_frames_set_title (MetaFrames *frames,
                            Window      xwindow,
                            const char *title);

void meta_frames_get_geometry (MetaFrames *frames,
                               Window xwindow,
                               int *top_height, int *bottom_height,
                               int *left_width, int *right_width);

void meta_frames_reset_bg (MetaFrames *frames,
                           Window      xwindow);

void meta_frames_queue_draw (MetaFrames *frames,
                             Window      xwindow);

void meta_frames_get_pixmap_for_control (MetaFrames *frames,
                                         MetaFrameControl control,
                                         GdkPixmap   **pixmap,
                                         GdkBitmap   **mask);

void meta_frames_notify_menu_hide (MetaFrames *frames);

Window meta_frames_get_moving_frame (MetaFrames *frames);

void meta_frames_push_delay_exposes (MetaFrames *frames);
void meta_frames_pop_delay_exposes  (MetaFrames *frames);

#endif
