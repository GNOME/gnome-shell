/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2008 Matthew Allum
 * Copyright (C) 2007 Iain Holmes
 * Based on xcompmgr - (c) 2003 Keith Packard
 *          xfwm4    - (c) 2005-2007 Olivier Fourdan
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

#ifndef META_WINDOW_ACTOR_H_
#define META_WINDOW_ACTOR_H_

#include <clutter/clutter.h>
#include <X11/Xlib.h>

#include <meta/compositor.h>

/*
 * MetaWindowActor object (ClutterGroup sub-class)
 */
#define META_TYPE_WINDOW_ACTOR            (meta_window_actor_get_type ())
#define META_WINDOW_ACTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_WINDOW_ACTOR, MetaWindowActor))
#define META_WINDOW_ACTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_WINDOW_ACTOR, MetaWindowActorClass))
#define META_IS_WINDOW_ACTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_WINDOW_ACTOR))
#define META_IS_WINDOW_ACTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_WINDOW_ACTOR))
#define META_WINDOW_ACTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), META_TYPE_WINDOW_ACTOR, MetaWindowActorClass))

typedef struct _MetaWindowActor        MetaWindowActor;
typedef struct _MetaWindowActorClass   MetaWindowActorClass;
typedef struct _MetaWindowActorPrivate MetaWindowActorPrivate;

struct _MetaWindowActorClass
{
  /*< private >*/
  ClutterActorClass parent_class;
};

struct _MetaWindowActor
{
  ClutterActor           parent;

  MetaWindowActorPrivate *priv;
};

GType meta_window_actor_get_type (void);

Window             meta_window_actor_get_x_window         (MetaWindowActor *self);
gint               meta_window_actor_get_workspace        (MetaWindowActor *self);
MetaWindow *       meta_window_actor_get_meta_window      (MetaWindowActor *self);
ClutterActor *     meta_window_actor_get_texture          (MetaWindowActor *self);
gboolean           meta_window_actor_is_override_redirect (MetaWindowActor *self);
gboolean       meta_window_actor_showing_on_its_workspace (MetaWindowActor *self);
gboolean       meta_window_actor_is_destroyed (MetaWindowActor *self);

#endif /* META_WINDOW_ACTOR_H */
