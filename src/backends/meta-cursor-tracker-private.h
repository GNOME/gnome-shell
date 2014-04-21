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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Giovanni Campagna <gcampagn@redhat.com>
 */

#ifndef META_CURSOR_TRACKER_PRIVATE_H
#define META_CURSOR_TRACKER_PRIVATE_H

#include <meta/meta-cursor-tracker.h>
#include <wayland-server.h>
#include <gbm.h>

#include "meta-cursor.h"

struct _MetaCursorTracker {
  GObject parent_instance;

  MetaScreen *screen;

  gboolean is_showing;
  gboolean has_hw_cursor;

  /* The cursor tracker stores the cursor for the current grab
   * operation, the cursor for the window with pointer focus, and
   * the cursor for the root window, which contains either the
   * default arrow cursor or the 'busy' hourglass if we're launching
   * an app.
   *
   * We choose the first one available -- if there's a grab cursor,
   * we choose that cursor, if there's window cursor, we choose that,
   * otherwise we choose the root cursor.
   *
   * The displayed_cursor contains the chosen cursor.
   */
  MetaCursorReference *displayed_cursor;

  MetaCursorReference *grab_cursor;

  /* Wayland clients can set a NULL buffer as their cursor 
   * explicitly, which means that we shouldn't display anything.
   * So, we can't simply store a NULL in window_cursor to
   * determine an unset window cursor; we need an extra boolean.
   */
  gboolean has_window_cursor;
  MetaCursorReference *window_cursor;

  MetaCursorReference *root_cursor;

  int current_x, current_y;
  MetaRectangle current_rect;
  MetaRectangle previous_rect;
  gboolean previous_is_valid;

  CoglPipeline *pipeline;
  int drm_fd;
  struct gbm_device *gbm;
};

struct _MetaCursorTrackerClass {
  GObjectClass parent_class;
};

gboolean meta_cursor_tracker_handle_xevent (MetaCursorTracker *tracker,
					    XEvent            *xevent);

void     meta_cursor_tracker_set_grab_cursor     (MetaCursorTracker   *tracker,
                                                  MetaCursorReference *cursor);
void     meta_cursor_tracker_set_window_cursor   (MetaCursorTracker   *tracker,
                                                  MetaCursorReference *cursor);
void     meta_cursor_tracker_unset_window_cursor (MetaCursorTracker   *tracker);
void     meta_cursor_tracker_set_root_cursor     (MetaCursorTracker   *tracker,
                                                  MetaCursorReference *cursor);

void     meta_cursor_tracker_update_position (MetaCursorTracker *tracker,
					      int                new_x,
					      int                new_y);
void     meta_cursor_tracker_paint           (MetaCursorTracker *tracker);

void     meta_cursor_tracker_force_update (MetaCursorTracker *tracker);

struct gbm_device * meta_cursor_tracker_get_gbm_device (MetaCursorTracker *tracker);

#endif
