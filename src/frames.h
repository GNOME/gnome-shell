/* Metacity window frame manager widget */

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

#ifndef META_FRAMES_H
#define META_FRAMES_H

#include <gtk/gtk.h>

/* This is one widget that manages all the window frames
 * as subwindows.
 */

#define META_TYPE_FRAMES            (meta_frames_get_type ())
#define META_FRAMES(obj)            (META_CHECK_CAST ((obj), META_TYPE_FRAMES, MetaFrames))
#define META_FRAMES_CLASS(klass)    (META_CHECK_CLASS_CAST ((klass), META_TYPE_FRAMES, MetaFramesClass))
#define META_IS_FRAMES(obj)         (META_CHECK_TYPE ((obj), META_TYPE_FRAMES))
#define META_IS_FRAMES_CLASS(klass) (META_CHECK_CLASS_TYPE ((klass), META_TYPE_FRAMES))
#define META_FRAMES_GET_CLASS(obj)  (META_CHECK_GET_CLASS ((obj), META_TYPE_FRAMES, MetaFramesClass))

typedef struct _MetaFrames        MetaFrames;
typedef struct _MetaFramesClass   MetaFramesClass;

struct _MetaFrames
{
  GtkWindow parent_instance;

};

struct _MetaFramesClass
{
  GtkWindowClass parent_class;

};

GType        meta_frames_get_type               (void) G_GNUC_CONST;

MetaFrames *meta_frames_new (void);

void meta_frames_manage_window (MetaFrames *frames,
                                GdkWindow  *window);

#endif
