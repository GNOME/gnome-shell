/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter X error handling */

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

#ifndef META_ERRORS_H
#define META_ERRORS_H

#include <X11/Xlib.h>

#include "util.h"
#include "display.h"

typedef void (* ErrorHandler) (Display *dpy,
                               XErrorEvent *error,
                               gpointer data);

void      meta_errors_init     (void);
void	  meta_errors_register_foreign_display (Display      *foreign_dpy,
						ErrorHandler  handler,
						gpointer      data);
						
void      meta_error_trap_push (MetaDisplay *display);
void      meta_error_trap_pop  (MetaDisplay *display,
                                gboolean     last_request_was_roundtrip);

void      meta_error_trap_push_with_return (MetaDisplay *display);
/* returns X error code, or 0 for no error */
int       meta_error_trap_pop_with_return  (MetaDisplay *display,
                                            gboolean     last_request_was_roundtrip);


#endif
