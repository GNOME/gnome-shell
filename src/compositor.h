/* Metacity compositing manager */

/* 
 * Copyright (C) 2003 Red Hat, Inc.
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

#ifndef META_COMPOSITOR_H
#define META_COMPOSITOR_H

#include "util.h"
#include "display.h"

typedef void (* MetaAnimationFinishedFunc) (gpointer data);

MetaCompositor* meta_compositor_new           (MetaDisplay       *display);
void            meta_compositor_unref         (MetaCompositor    *compositor);
void            meta_compositor_process_event (MetaCompositor    *compositor,
                                               XEvent            *xevent,
                                               MetaWindow        *window);
void            meta_compositor_add_window    (MetaCompositor    *compositor,
                                               Window             xwindow,
                                               XWindowAttributes *attrs);
void            meta_compositor_remove_window (MetaCompositor    *compositor,
                                               Window             xwindow);
void		meta_compositor_set_debug_updates (MetaCompositor *compositor,
						   gboolean	   debug_updates);

void meta_compositor_manage_screen   (MetaCompositor *compositor,
                                      MetaScreen     *screen);
void meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                      MetaScreen     *screen);

void meta_compositor_minimize (MetaCompositor            *compositor,
			       MetaWindow                *window,
			       int                        x,
			       int                        y,
			       int                        width,
			       int                        height,
			       MetaAnimationFinishedFunc  finished_cb,
			       gpointer                   finished_data);

void
meta_compositor_unminimize (MetaCompositor            *compositor,
			    MetaWindow                *window,
			    int                        x,
			    int                        y,
			    int                        width,
			    int                        height,
			    MetaAnimationFinishedFunc  finished,
			    gpointer                   data);
void
meta_compositor_set_updates (MetaCompositor *compositor,
			     MetaWindow *window,
			     gboolean updates);
void
meta_compositor_destroy (MetaCompositor *compositor);

void meta_compositor_begin_move (MetaCompositor *compositor,
				 MetaWindow *window,
				 MetaRectangle *initial,
				 int grab_x, int grab_y);
void meta_compositor_update_move (MetaCompositor *compositor,
				  MetaWindow *window,
				  int x, int y);
void meta_compositor_end_move (MetaCompositor *compositor,
			       MetaWindow *window);

#endif /* META_COMPOSITOR_H */
