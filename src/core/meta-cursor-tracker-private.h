/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright 2013 Red Hat, Inc.
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
 *
 * Author: Giovanni Campagna <gcampagn@redhat.com>
 */

#ifndef META_CURSOR_TRACKER_PRIVATE_H
#define META_CURSOR_TRACKER_PRIVATE_H

#include <meta/meta-cursor-tracker.h>
#include <clutter/clutter.h>

gboolean meta_cursor_tracker_handle_xevent (MetaCursorTracker *tracker,
					    XEvent            *xevent);

void     meta_cursor_tracker_set_root_cursor (MetaCursorTracker *tracker,
                                              MetaCursor         cursor);
void     meta_cursor_tracker_revert_root     (MetaCursorTracker *tracker);
void     meta_cursor_tracker_set_sprite      (MetaCursorTracker *tracker,
                                              CoglTexture2D     *texture,
                                              int                hot_x,
                                              int                hot_y);

void     meta_cursor_tracker_update_position (MetaCursorTracker *tracker,
					      int                new_x,
					      int                new_y);
void     meta_cursor_tracker_paint           (MetaCursorTracker *tracker);
void     meta_cursor_tracker_queue_redraw    (MetaCursorTracker *tracker,
					      ClutterActor      *stage);
#endif
