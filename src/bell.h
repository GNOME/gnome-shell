/* Metacity visual bell */

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

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif
#include "display.h"
#include "frame.h"

#ifdef HAVE_XKB
void meta_bell_notify (MetaDisplay *display, XkbAnyEvent *xkb_ev);
#endif
void meta_bell_set_audible (MetaDisplay *display, gboolean audible);
gboolean meta_bell_init (MetaDisplay *display);
void meta_bell_shutdown (MetaDisplay *display);
void meta_bell_notify_frame_destroy (MetaFrame *frame);
