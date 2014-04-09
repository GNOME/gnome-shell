/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2013 Red Hat
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
 * Written by:
 *     Owen Taylor <otaylor@redhat.com>
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#ifndef __META_SURFACE_ACTOR_X11_H__
#define __META_SURFACE_ACTOR_X11_H__

#include <glib-object.h>

#include "meta-surface-actor.h"

#include <X11/extensions/Xdamage.h>

#include <meta/display.h>
#include <meta/window.h>

G_BEGIN_DECLS

#define META_TYPE_SURFACE_ACTOR_X11            (meta_surface_actor_x11_get_type ())
#define META_SURFACE_ACTOR_X11(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_SURFACE_ACTOR_X11, MetaSurfaceActorX11))
#define META_SURFACE_ACTOR_X11_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_SURFACE_ACTOR_X11, MetaSurfaceActorX11Class))
#define META_IS_SURFACE_ACTOR_X11(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_SURFACE_ACTOR_X11))
#define META_IS_SURFACE_ACTOR_X11_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_SURFACE_ACTOR_X11))
#define META_SURFACE_ACTOR_X11_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_SURFACE_ACTOR_X11, MetaSurfaceActorX11Class))

typedef struct _MetaSurfaceActorX11      MetaSurfaceActorX11;
typedef struct _MetaSurfaceActorX11Class MetaSurfaceActorX11Class;

struct _MetaSurfaceActorX11
{
  MetaSurfaceActor parent;
};

struct _MetaSurfaceActorX11Class
{
  MetaSurfaceActorClass parent_class;
};

GType meta_surface_actor_x11_get_type (void);

MetaSurfaceActor * meta_surface_actor_x11_new (MetaWindow *window);

void meta_surface_actor_x11_set_size (MetaSurfaceActorX11 *self,
                                      int width, int height);

G_END_DECLS

#endif /* __META_SURFACE_ACTOR_X11_H__ */
