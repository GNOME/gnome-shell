/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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

/**
 * \file fixedtip.h  Mutter fixed tooltip routine
 * 
 * Sometimes we want to display a small floating rectangle with helpful
 * text near the pointer.  For example, if the user holds the mouse over
 * the maximise button, we can display a tooltip saying "Maximize".
 * The text is localised, of course.
 *
 * This file contains the functions to create and delete these tooltips.
 *
 * \todo  Since we now consider MetaDisplay a singleton, there can be
 *        only one tooltip per display; this might quite simply live in
 *        display.c.  Alternatively, it could move to frames.c, which
 *        is the only place this business is called anyway.
 *
 * \todo  Apparently some UI needs changing (check bugzilla)
 */

#ifndef META_FIXED_TIP_H
#define META_FIXED_TIP_H

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

/**
 * Displays a tooltip.  There can be only one across the entire system.
 * This function behaves identically whether or not a tooltip is already
 * displayed, but if it is the window will be reused rather than destroyed
 * and recreated.
 *
 * \param  xdisplay       An X display.
 * \param  screen_number  The number of the screen.
 * \param  root_x         The X coordinate where the tooltip should appear
 * \param  root_y         The Y coordinate where the tooltip should appear
 * \param  markup_text    Text to display in the tooltip; can contain markup
 */
void meta_fixed_tip_show (Display *xdisplay, int screen_number,
                          int root_x, int root_y,
                          const char *markup_text);

/**
 * Removes the tooltip that was created by meta_fixed_tip_show().  If there
 * is no tooltip currently visible, this is a no-op.
 */
void meta_fixed_tip_hide (void);


#endif
