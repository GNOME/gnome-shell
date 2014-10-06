/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * meta-feedback-actor-private.h: Actor for painting user interaction feedback
 *
 * Copyright 2014 Red Hat, Inc.
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef META_FEEDBACK_ACTOR_PRIVATE_H
#define META_FEEDBACK_ACTOR_PRIVATE_H

#include <clutter/clutter.h>

/**
 * MetaFeedbackActor:
 *
 * This class handles the rendering of user interaction feedback
 */

#define META_TYPE_FEEDBACK_ACTOR            (meta_feedback_actor_get_type ())
#define META_FEEDBACK_ACTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_FEEDBACK_ACTOR, MetaFeedbackActor))
#define META_FEEDBACK_ACTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_FEEDBACK_ACTOR, MetaFeedbackActorClass))
#define META_IS_FEEDBACK_ACTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_FEEDBACK_ACTOR))
#define META_IS_FEEDBACK_ACTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_FEEDBACK_ACTOR))
#define META_FEEDBACK_ACTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), META_TYPE_FEEDBACK_ACTOR, MetaFeedbackActorClass))

typedef struct _MetaFeedbackActor        MetaFeedbackActor;
typedef struct _MetaFeedbackActorClass   MetaFeedbackActorClass;

struct _MetaFeedbackActorClass
{
  /*< private >*/
  ClutterActorClass parent_class;
};

struct _MetaFeedbackActor
{
  ClutterActor parent;
};

GType meta_feedback_actor_get_type (void);

ClutterActor *meta_feedback_actor_new (int anchor_x,
                                       int anchor_y);

void meta_feedback_actor_set_anchor (MetaFeedbackActor *actor,
                                     int                anchor_x,
                                     int                anchor_y);
void meta_feedback_actor_get_anchor (MetaFeedbackActor *actor,
                                     int               *anchor_x,
                                     int               *anchor_y);

void meta_feedback_actor_set_position (MetaFeedbackActor  *self,
                                       int                 x,
                                       int                 y);

void meta_feedback_actor_update (MetaFeedbackActor  *self,
                                 const ClutterEvent *event);

#endif /* META_FEEDBACK_ACTOR_PRIVATE_H */
