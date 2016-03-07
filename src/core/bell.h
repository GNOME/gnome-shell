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

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include "display-private.h"
#include "frame.h"

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
 * meta_bell_set_audible:
 * @display: The display we're configuring
 * @audible: True for an audible bell, false for a visual bell
 *
 * Turns the bell to audible or visual. This tells X what to do, but
 * not Mutter; you will need to set the "visual bell" pref for that.
 *
 * If the configure script found we had no XKB, this is a no-op.
 */
void meta_bell_set_audible (MetaDisplay *display, gboolean audible);

/**
 * meta_bell_init:
 * @display: The display which is opening
 *
 * Initialises the bell subsystem. This involves intialising
 * XKB (which, despite being a keyboard extension, is the
 * place to look for bell notifications), then asking it
 * to send us bell notifications, and then also switching
 * off the audible bell if we're using a visual one ourselves.
 *
 * \bug There is a line of code that's never run that tells
 * XKB to reset the bell status after we quit. Bill H said
 * (<http://bugzilla.gnome.org/show_bug.cgi?id=99886#c12>)
 * that XFree86's implementation is broken so we shouldn't
 * call it, but that was in 2002. Is it working now?
 */
gboolean meta_bell_init (MetaDisplay *display);

/**
 * meta_bell_shutdown:
 * @display: The display which is closing
 *
 * Shuts down the bell subsystem.
 *
 * \bug This is never called! If we had XkbSetAutoResetControls
 * enabled in meta_bell_init(), this wouldn't be a problem, but
 * we don't.
 */
void meta_bell_shutdown (MetaDisplay *display);

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
