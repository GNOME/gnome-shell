/* Metacity X window decorations */

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

#ifndef META_FRAME_H
#define META_FRAME_H

#include "window.h"

typedef enum
{
  META_FRAME_ACTION_NONE,
  META_FRAME_ACTION_MOVING,
  META_FRAME_ACTION_RESIZING_SE
} MetaFrameAction;

struct _MetaFrame
{
  /* window we frame */
  MetaWindow *window;

  /* reparent window */
  Window xwindow;

  /* This rect is trusted info from where we put the
   * frame, not the result of ConfigureNotify
   */
  MetaRectangle rect;
  int child_x;
  int child_y;
  
  gpointer theme_data;
  gulong bg_pixel;
  
  MetaFrameAction action;
  /* reference point for drags */
  int last_x, last_y;
  int start_button;

  guint theme_acquired : 1;
};

void     meta_window_ensure_frame           (MetaWindow *window);
void     meta_window_destroy_frame          (MetaWindow *window);
void     meta_frame_move                    (MetaFrame  *frame,
                                             int         root_x,
                                             int         root_y);
void     meta_frame_child_configure_request (MetaFrame  *frame);
void     meta_frame_recalc_now              (MetaFrame  *frame);
void     meta_frame_queue_recalc            (MetaFrame  *frame);
void     meta_frame_queue_draw              (MetaFrame  *frame);
gboolean meta_frame_event                   (MetaFrame  *frame,
                                             XEvent     *event);




#endif




