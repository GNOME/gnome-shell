/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * meta-background-actor.h: Actor for painting the root window background
 *
 * Copyright 2010 Red Hat, Inc.
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

#ifndef META_BACKGROUND_ACTOR_H
#define META_BACKGROUND_ACTOR_H

#include <clutter/clutter.h>
#include <cogl/cogl.h>

#include <meta/gradient.h>
#include <meta/screen.h>

#include <gsettings-desktop-schemas/gdesktop-enums.h>

/**
 * MetaBackgroundActor:
 *
 * This class handles tracking and painting the root window background.
 * By integrating with #MetaWindowGroup we can avoid painting parts of
 * the background that are obscured by other windows.
 */

#define META_TYPE_BACKGROUND_ACTOR            (meta_background_actor_get_type ())
#define META_BACKGROUND_ACTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_BACKGROUND_ACTOR, MetaBackgroundActor))
#define META_BACKGROUND_ACTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_BACKGROUND_ACTOR, MetaBackgroundActorClass))
#define META_IS_BACKGROUND_ACTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_BACKGROUND_ACTOR))
#define META_IS_BACKGROUND_ACTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_BACKGROUND_ACTOR))
#define META_BACKGROUND_ACTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), META_TYPE_BACKGROUND_ACTOR, MetaBackgroundActorClass))

typedef struct _MetaBackgroundActor        MetaBackgroundActor;
typedef struct _MetaBackgroundActorClass   MetaBackgroundActorClass;
typedef struct _MetaBackgroundActorPrivate MetaBackgroundActorPrivate;

struct _MetaBackgroundActorClass
{
  /*< private >*/
  ClutterActorClass parent_class;
};

struct _MetaBackgroundActor
{
  ClutterActor parent;

  MetaBackgroundActorPrivate *priv;
};

GType meta_background_actor_get_type (void);

ClutterActor *meta_background_actor_new (void);

#endif /* META_BACKGROUND_ACTOR_H */
