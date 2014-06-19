/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * \file gesture-tracker-private.h  Manages gestures on windows/desktop
 *
 * Forwards touch events to clutter actors, and accepts/rejects touch sequences
 * based on the outcome of those.
 */

/*
 * Copyright (C) 2014 Red Hat
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
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef META_GESTURE_TRACKER_PRIVATE_H
#define META_GESTURE_TRACKER_PRIVATE_H

#include <glib-object.h>
#include <clutter/clutter.h>
#include <meta/window.h>

#define META_TYPE_GESTURE_TRACKER            (meta_gesture_tracker_get_type ())
#define META_GESTURE_TRACKER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_GESTURE_TRACKER, MetaGestureTracker))
#define META_GESTURE_TRACKER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_GESTURE_TRACKER, MetaGestureTrackerClass))
#define META_IS_GESTURE_TRACKER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_GESTURE_TRACKER))
#define META_IS_GESTURE_TRACKER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_GESTURE_TRACKER))
#define META_GESTURE_TRACKER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_GESTURE_TRACKER, MetaGestureTrackerClass))

typedef struct _MetaGestureTracker MetaGestureTracker;
typedef struct _MetaGestureTrackerClass MetaGestureTrackerClass;

typedef enum {
  META_SEQUENCE_NONE,
  META_SEQUENCE_ACCEPTED,
  META_SEQUENCE_REJECTED,
  META_SEQUENCE_PENDING_END
} MetaSequenceState;

struct _MetaGestureTracker
{
  GObject parent_instance;
};

struct _MetaGestureTrackerClass
{
  GObjectClass parent_class;

  void (* state_changed) (MetaGestureTracker   *tracker,
                          ClutterEventSequence *sequence,
                          MetaSequenceState     state);
};

GType                meta_gesture_tracker_get_type           (void) G_GNUC_CONST;

MetaGestureTracker * meta_gesture_tracker_new                (guint                 autodeny_timeout);

gboolean             meta_gesture_tracker_handle_event       (MetaGestureTracker   *tracker,
                                                              const ClutterEvent   *event);
gboolean             meta_gesture_tracker_set_sequence_state (MetaGestureTracker   *tracker,
                                                              ClutterEventSequence *sequence,
                                                              MetaSequenceState     state);
MetaSequenceState    meta_gesture_tracker_get_sequence_state (MetaGestureTracker   *tracker,
                                                              ClutterEventSequence *sequence);
gboolean             meta_gesture_tracker_consumes_event     (MetaGestureTracker   *tracker,
                                                              const ClutterEvent   *event);

#endif /* META_GESTURE_TRACKER_PRIVATE_H */
