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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_ERRORS_H
#define META_ERRORS_H

#include <X11/Xlib.h>

#include <meta/util.h>
#include <meta/display.h>

void      meta_error_trap_push (MetaDisplay *display);
void      meta_error_trap_pop  (MetaDisplay *display);

/* returns X error code, or 0 for no error */
int       meta_error_trap_pop_with_return  (MetaDisplay *display);


#endif
