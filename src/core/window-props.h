/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * \file window-props.h    MetaWindow property handling
 *
 * A system which can inspect sets of properties of given windows
 * and take appropriate action given their values.
 *
 * Note that all the meta_window_reload_propert* functions require a
 * round trip to the server.
 */

/* 
 * Copyright (C) 2001, 2002 Red Hat, Inc.
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

#ifndef META_WINDOW_PROPS_H
#define META_WINDOW_PROPS_H

#include "window-private.h"

/**
 * Requests the current values of a single property for a given
 * window from the server, and deals with it appropriately.
 * Does not return it to the caller (it's been dealt with!)
 *
 * \param window     The window.
 * \param property   A single X atom.
 */
void meta_window_reload_property   (MetaWindow *window,
                                    Atom        property,
                                    gboolean    initial);


/**
 * Requests the current values of a set of properties for a given
 * window from the server, and deals with them appropriately.
 * Does not return them to the caller (they've been dealt with!)
 *
 * \param window      The window.
 * \param properties  A pointer to a list of X atoms, "n_properties" long.
 * \param n_properties  The length of the properties list.
 */
void meta_window_reload_properties (MetaWindow *window,
                                    const Atom *properties,
                                    int         n_properties,
                                    gboolean    initial);

/**
 * Requests the current values of a single property for a given
 * window from the server, and deals with it appropriately.
 * Does not return it to the caller (it's been dealt with!)
 *
 * \param window     A window on the same display as the one we're
 *                   investigating (only used to find the display)
 * \param xwindow    The X handle for the window.
 * \param property   A single X atom.
 */
void meta_window_reload_property_from_xwindow
                                   (MetaWindow *window,
                                    Window      xwindow,
                                    Atom        property,
                                    gboolean    initial);

/**
 * Requests the current values of a set of properties for a given
 * window from the server, and deals with them appropriately.
 * Does not return them to the caller (they've been dealt with!)
 *
 * \param window     A window on the same display as the one we're
 *                   investigating (only used to find the display)
 * \param xwindow     The X handle for the window.
 * \param properties  A pointer to a list of X atoms, "n_properties" long.
 * \param n_properties  The length of the properties list.
 */
void meta_window_reload_properties_from_xwindow
                                   (MetaWindow *window,
                                    Window      xwindow,
                                    const Atom *properties,
                                    int         n_properties,
                                    gboolean    initial);

/**
 * Requests the current values for standard properties for a given
 * window from the server, and deals with them appropriately.
 * Does not return them to the caller (they've been dealt with!)
 *
 * \param window      The window.
 */
void meta_window_load_initial_properties (MetaWindow *window);

/**
 * Initialises the hooks used for the reload_propert* functions
 * on a particular display, and stores a pointer to them in the
 * display.
 *
 * \param display  The display.
 */
void meta_display_init_window_prop_hooks (MetaDisplay *display);

/**
 * Frees the hooks used for the reload_propert* functions
 * for a particular display.
 *
 * \param display  The display.
 */
void meta_display_free_window_prop_hooks (MetaDisplay *display);

/**
 * Sets the size hints for a window.  This happens when a
 * WM_NORMAL_HINTS property is set on a window, but it is public
 * because the size hints are set to defaults when a window is
 * created.  See
 * http://tronche.com/gui/x/icccm/sec-4.html#WM_NORMAL_HINTS
 * for the X details.
 *
 * \param window   The window to set the size hints on.
 * \param hints    Either some X size hints, or NULL for default.
 */
void meta_set_normal_hints (MetaWindow *window,
			    XSizeHints *hints);

#endif /* META_WINDOW_PROPS_H */
