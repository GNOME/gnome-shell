/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_FRAMES_H
#define META_FRAMES_H

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <meta/common.h>
#include <meta/types.h>
#include "theme-private.h"
#include "ui.h"

typedef enum
{
  META_FRAME_CONTROL_NONE,
  META_FRAME_CONTROL_TITLE,
  META_FRAME_CONTROL_DELETE,
  META_FRAME_CONTROL_MENU,
  META_FRAME_CONTROL_APPMENU,
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
#define META_FRAMES(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_FRAMES, MetaFrames))
#define META_FRAMES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_FRAMES, MetaFramesClass))
#define META_IS_FRAMES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_FRAMES))
#define META_IS_FRAMES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_FRAMES))
#define META_FRAMES_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), META_TYPE_FRAMES, MetaFramesClass))

typedef struct _MetaFrames        MetaFrames;
typedef struct _MetaFramesClass   MetaFramesClass;

struct _MetaUIFrame
{
  MetaFrames *frames;
  MetaWindow *meta_window;
  Window xwindow;
  GdkWindow *window;
  MetaStyleInfo *style_info;
  MetaFrameLayout *cache_layout;
  PangoLayout *text_layout;
  int text_height;
  char *title; /* NULL once we have a layout */
  guint maybe_ignore_leave_notify : 1;

  /* FIXME get rid of this, it can just be in the MetaFrames struct */
  MetaFrameControl prelit_control;
  MetaButtonState button_state;
  int grab_button;
};

struct _MetaFrames
{
  GtkWindow parent_instance;

  GHashTable *text_heights;

  GHashTable *frames;

  MetaStyleInfo *normal_style;
  GHashTable *style_variants;

  MetaGrabOp current_grab_op;
  MetaUIFrame *grab_frame;
  guint grab_button;
  gdouble grab_x;
  gdouble grab_y;
};

struct _MetaFramesClass
{
  GtkWindowClass parent_class;

};

GType        meta_frames_get_type               (void) G_GNUC_CONST;

MetaFrames *meta_frames_new (void);

MetaUIFrame * meta_frames_manage_window (MetaFrames *frames,
                                         MetaWindow *meta_window,
                                         Window      xwindow,
                                         GdkWindow  *window);

void meta_ui_frame_unmanage (MetaUIFrame *frame);

void meta_ui_frame_set_title (MetaUIFrame *frame,
                              const char *title);

void meta_ui_frame_update_style (MetaUIFrame *frame);

void meta_ui_frame_get_borders (MetaUIFrame      *frame,
                                MetaFrameBorders *borders);

cairo_region_t * meta_ui_frame_get_bounds (MetaUIFrame *frame);

void meta_ui_frame_get_mask (MetaUIFrame *frame,
                             cairo_t     *cr);

void meta_ui_frame_move_resize (MetaUIFrame *frame,
                                int x, int y, int width, int height);

void meta_ui_frame_queue_draw (MetaUIFrame *frame);

gboolean meta_ui_frame_handle_event (MetaUIFrame *frame, const ClutterEvent *event);

#endif
