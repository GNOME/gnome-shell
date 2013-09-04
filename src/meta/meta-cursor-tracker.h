/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2013 Red Hat, Inc.
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
 *
 * Author: Giovanni Campagna <gcampagn@redhat.com>
 */

#ifndef META_CURSOR_TRACKER_H
#define META_CURSOR_TRACKER_H

#include <glib-object.h>
#include <meta/types.h>
#include <meta/workspace.h>
#include <cogl/cogl.h>
#include <clutter/clutter.h>

#define META_TYPE_CURSOR_TRACKER            (meta_cursor_tracker_get_type ())
#define META_CURSOR_TRACKER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_CURSOR_TRACKER, MetaCursorTracker))
#define META_CURSOR_TRACKER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_CURSOR_TRACKER, MetaCursorTrackerClass))
#define META_IS_CURSOR_TRACKER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_CURSOR_TRACKER))
#define META_IS_CURSOR_TRACKER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_CURSOR_TRACKER))
#define META_CURSOR_TRACKER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_CURSOR_TRACKER, MetaCursorTrackerClass))

typedef struct _MetaCursorTrackerClass   MetaCursorTrackerClass;

GType meta_cursor_tracker_get_type (void);

MetaCursorTracker *meta_cursor_tracker_get_for_screen (MetaScreen *screen);

void           meta_cursor_tracker_get_hot    (MetaCursorTracker *tracker,
                                               int               *x,
                                               int               *y);
CoglTexture   *meta_cursor_tracker_get_sprite (MetaCursorTracker *tracker);

void           meta_cursor_tracker_get_pointer (MetaCursorTracker   *tracker,
                                                int                 *x,
                                                int                 *y,
                                                ClutterModifierType *mods);
void           meta_cursor_tracker_set_pointer_visible (MetaCursorTracker *tracker,
                                                        gboolean           visible);

#endif
