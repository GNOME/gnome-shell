/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _HAVE_CLUTTER_TIMELINE_H
#define _HAVE_CLUTTER_TIMELINE_H

/* clutter-timeline.h */

#include <glib-object.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_TIMELINE clutter_timeline_get_type()

#define CLUTTER_TIMELINE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_TIMELINE, ClutterTimeline))

#define CLUTTER_TIMELINE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_TIMELINE, ClutterTimelineClass))

#define CLUTTER_IS_TIMELINE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_TIMELINE))

#define CLUTTER_IS_TIMELINE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_TIMELINE))

#define CLUTTER_TIMELINE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_TIMELINE, ClutterTimelineClass))

typedef struct _ClutterTimeline ClutterTimeline;
typedef struct _ClutterTimelineClass ClutterTimelineClass; 
typedef struct ClutterTimelinePrivate ClutterTimelinePrivate;

struct _ClutterTimeline
{
  GObject                 parent;
  ClutterTimelinePrivate *priv;
};

struct _ClutterTimelineClass
{
  GObjectClass parent_class;
  
  void (*new_frame) (ClutterTimeline *timeline, gint frame_num);
  void (*completed) (ClutterTimeline *timeline);
}; 

GType clutter_timeline_get_type (void);

ClutterTimeline*
clutter_timeline_new (guint nframes, guint fps);

void
clutter_timeline_set_speed (ClutterTimeline *timeline, guint fps);

void
clutter_timeline_start (ClutterTimeline *timeline);

void
clutter_timeline_pause (ClutterTimeline *timeline);

void
clutter_timeline_stop (ClutterTimeline *timeline);

void
clutter_timeline_set_loop (ClutterTimeline *timeline, gboolean loop);

void
clutter_timeline_rewind (ClutterTimeline *timeline);

void
clutter_timeline_skip (ClutterTimeline *timeline, guint nframes);

void
clutter_timeline_advance (ClutterTimeline *timeline, guint frame_num);

gint
clutter_timeline_get_current_frame (ClutterTimeline *timeline);

guint
clutter_timeline_get_n_frames (ClutterTimeline *timeline);

gboolean
clutter_timeline_is_playing (ClutterTimeline *timeline);

G_END_DECLS

#endif
