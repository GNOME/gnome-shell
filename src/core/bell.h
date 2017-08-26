/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2002 Sun Microsystems Inc.
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

#include "display-private.h"
#include "frame.h"

struct _MetaBell
{
  GObject parent;
};

#define META_TYPE_BELL (meta_bell_get_type ())
G_DECLARE_FINAL_TYPE (MetaBell, meta_bell, META, BELL, GObject)

MetaBell * meta_bell_new (MetaDisplay *display);

/**
 * meta_bell_notify:
 * @display: The display the bell event came in on
 * @window: The window the bell event was received on
 *
 * Gives the user some kind of aural or visual feedback, such as a bell sound
 * or flash. What type of feedback is invoked depends on the configuration.
 * If the aural feedback could not be invoked, FALSE is returned.
 */
gboolean meta_bell_notify (MetaDisplay *display,
                           MetaWindow  *window);

/**
 * meta_bell_notify_frame_destroy:
 * @frame: The frame which is being destroyed
 *
 * Deals with a frame being destroyed. This is important because if we're
 * using a visual bell, we might be flashing the edges of the frame, and
 * so we'd have a timeout function waiting ready to un-flash them. If the
 * frame's going away, we can tell the timeout not to bother.
 */
void meta_bell_notify_frame_destroy (MetaFrame *frame);
