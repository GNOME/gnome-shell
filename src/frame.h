/* Metacity X window decorations */

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

#ifndef META_FRAME_H
#define META_FRAME_H

#include "window.h"

struct _MetaFrame
{
  /* window we frame */
  MetaWindow *window;

  /* reparent window */
  Window xwindow;

  MetaRectangle rect;
};

void meta_window_ensure_frame  (MetaWindow *window);
void meta_window_destroy_frame (MetaWindow *window);

void     meta_frame_show  (MetaFrame *frame);
void     meta_frame_hide  (MetaFrame *frame);

gboolean meta_frame_event (MetaFrame *frame,
                           XEvent    *event);


#endif
