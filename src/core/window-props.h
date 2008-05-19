/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* MetaWindow property handling */

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

void meta_window_reload_property   (MetaWindow *window,
                                    Atom        property);
void meta_window_reload_properties (MetaWindow *window,
                                    const Atom *properties,
                                    int         n_properties);
void meta_window_reload_property_from_xwindow
                                   (MetaWindow *window,
                                    Window      xwindow,
                                    Atom        property);
void meta_window_reload_properties_from_xwindow
                                   (MetaWindow *window,
                                    Window      xwindow,
                                    const Atom *properties,
                                    int         n_properties);

void meta_display_init_window_prop_hooks (MetaDisplay *display);
void meta_display_free_window_prop_hooks (MetaDisplay *display);

void meta_set_normal_hints (MetaWindow *window,
			    XSizeHints *hints);

#endif /* META_WINDOW_PROPS_H */
