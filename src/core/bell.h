/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * \file bell.h Ring the bell or flash the screen
 *
 * Sometimes, X programs "ring the bell", whatever that means. Mutter lets
 * the user configure the bell to be audible or visible (aka visual), and
 * if it's visual it can be configured to be frame-flash or fullscreen-flash.
 * We never get told about audible bells; X handles them just fine by itself.
 *
 * The visual bell was the result of a discussion in Bugzilla here:
 * <http://bugzilla.gnome.org/show_bug.cgi?id=99886>.
 */

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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <X11/Xlib.h>
#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif
#include "display-private.h"
#include "frame-private.h"

#ifdef HAVE_XKB
/**
 * Gives the user some kind of visual bell; in fact, this is our response
 * to any kind of bell request, but we set it up so that we only get
 * notified about visual bells, and X deals with audible ones.
 *
 * If the configure script found we had no XKB, this does not exist.
 *
 * \param display  The display the bell event came in on
 * \param xkb_ev   The bell event we just received
 */
void meta_bell_notify (MetaDisplay *display, XkbAnyEvent *xkb_ev);
#endif

/**
 * Turns the bell to audible or visual. This tells X what to do, but
 * not Mutter; you will need to set the "visual bell" pref for that.
 *
 * If the configure script found we had no XKB, this is a no-op.
 *
 * \param display  The display we're configuring
 * \param audible  True for an audible bell, false for a visual bell
 */
void meta_bell_set_audible (MetaDisplay *display, gboolean audible);

/**
 * Initialises the bell subsystem. This involves intialising
 * XKB (which, despite being a keyboard extension, is the
 * place to look for bell notifications), then asking it
 * to send us bell notifications, and then also switching
 * off the audible bell if we're using a visual one ourselves.
 *
 * Unlike most X extensions we use, we only initialise XKB here
 * (rather than in main()). It's possible that XKB is not
 * installed at all, but if that was known at build time
 * we will have HAVE_XKB undefined, which will cause this
 * function to be a no-op.
 *
 * \param display  The display which is opening
 *
 * \bug There is a line of code that's never run that tells
 * XKB to reset the bell status after we quit. Bill H said
 * (<http://bugzilla.gnome.org/show_bug.cgi?id=99886#c12>)
 * that XFree86's implementation is broken so we shouldn't
 * call it, but that was in 2002. Is it working now?
 */
gboolean meta_bell_init (MetaDisplay *display);

/**
 * Shuts down the bell subsystem.
 *
 * \param display  The display which is closing
 *
 * \bug This is never called! If we had XkbSetAutoResetControls
 * enabled in meta_bell_init(), this wouldn't be a problem, but
 * we don't.
 */
void meta_bell_shutdown (MetaDisplay *display);

/**
 * Deals with a frame being destroyed. This is important because if we're
 * using a visual bell, we might be flashing the edges of the frame, and
 * so we'd have a timeout function waiting ready to un-flash them. If the
 * frame's going away, we can tell the timeout not to bother.
 *
 * \param frame  The frame which is being destroyed
 */
void meta_bell_notify_frame_destroy (MetaFrame *frame);
