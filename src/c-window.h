/* 
 * Copyright (C) 2006 Red Hat, Inc.
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

#include <cm/node.h>
#include "display.h"
#include "effects.h"

#ifndef C_WINDOW_H
#define C_WINDOW_H

typedef struct _MetaCompWindow MetaCompWindow;

MetaCompWindow *meta_comp_window_new (MetaDisplay *display,
				      WsDrawable *drawable);
CmNode	       *meta_comp_window_get_node (MetaCompWindow *window);
void		meta_comp_window_free (MetaCompWindow *window);
void		meta_comp_window_set_size (MetaCompWindow *window,
					   WsRectangle *size);

void		meta_comp_window_refresh_attrs (MetaCompWindow *comp_window);
void		meta_comp_window_set_updates (MetaCompWindow *comp_window,
					      gboolean updates);

void		meta_comp_window_explode (MetaCompWindow *comp_window,
					  MetaEffect *effect);

#if 0
void		meta_comp_window_set_explode (MetaCompWindow *comp_window,
					      double	      level);
#endif

#endif

