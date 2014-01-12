/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter X window decorations */

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

#ifndef META_FRAME_PRIVATE_H
#define META_FRAME_PRIVATE_H

#include "window-private.h"

struct _MetaFrame
{
  /* window we frame */
  MetaWindow *window;

  /* reparent window */
  Window xwindow;

  MetaCursor current_cursor;

  /* This rect is trusted info from where we put the
   * frame, not the result of ConfigureNotify
   */
  MetaRectangle rect;

  MetaFrameBorders cached_borders; /* valid if borders_cached is set */

  /* position of client, size of frame */
  int child_x;
  int child_y;
  int right_width;
  int bottom_height;

  guint mapped : 1;
  guint need_reapply_frame_shape : 1;
  guint is_flashing : 1; /* used by the visual bell flash */
  guint borders_cached : 1;
};

void     meta_window_ensure_frame           (MetaWindow *window);
void     meta_window_destroy_frame          (MetaWindow *window);
void     meta_frame_queue_draw              (MetaFrame  *frame);

MetaFrameFlags meta_frame_get_flags   (MetaFrame *frame);
Window         meta_frame_get_xwindow (MetaFrame *frame);

/* These should ONLY be called from meta_window_move_resize_internal */
void meta_frame_calc_borders      (MetaFrame        *frame,
                                   MetaFrameBorders *borders);

gboolean meta_frame_sync_to_window (MetaFrame         *frame,
                                    int                gravity,
                                    gboolean           need_move,
                                    gboolean           need_resize);

void meta_frame_clear_cached_borders (MetaFrame *frame);

cairo_region_t *meta_frame_get_frame_bounds (MetaFrame *frame);

void meta_frame_get_mask (MetaFrame *frame,
                          cairo_t   *cr);

void meta_frame_set_screen_cursor (MetaFrame	*frame,
				   MetaCursor	cursor);

#endif




