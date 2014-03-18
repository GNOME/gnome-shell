/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012  Intel Corporation
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_SCROLL_ACTOR_H__
#define __CLUTTER_SCROLL_ACTOR_H__

#include <clutter/clutter-types.h>
#include <clutter/clutter-actor.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_SCROLL_ACTOR               (clutter_scroll_actor_get_type ())
#define CLUTTER_SCROLL_ACTOR(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_SCROLL_ACTOR, ClutterScrollActor))
#define CLUTTER_IS_SCROLL_ACTOR(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_SCROLL_ACTOR))
#define CLUTTER_SCROLL_ACTOR_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_SCROLL_ACTOR, ClutterScrollActorClass))
#define CLUTTER_IS_SCROLL_ACTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_SCROLL_ACTOR))
#define CLUTTER_SCROLL_ACTOR_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_SCROLL_ACTOR, ClutterScrollActorClass))

typedef struct _ClutterScrollActorPrivate       ClutterScrollActorPrivate;
typedef struct _ClutterScrollActorClass         ClutterScrollActorClass;

/**
 * ClutterScrollActor:
 *
 * The #ClutterScrollActor structure contains only
 * private data, and should be accessed using the provided API.
 *
 * Since: 1.12
 */
struct _ClutterScrollActor
{
  /*< private >*/
  ClutterActor parent_instance;

  ClutterScrollActorPrivate *priv;
};

/**
 * ClutterScrollActorClass:
 *
 * The #ClutterScrollActor structure contains only
 * private data.
 *
 * Since: 1.12
 */
struct _ClutterScrollActorClass
{
  /*< private >*/
  ClutterActorClass parent_instance;

  gpointer _padding[8];
};

CLUTTER_AVAILABLE_IN_1_12
GType clutter_scroll_actor_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_12
ClutterActor *          clutter_scroll_actor_new                (void);

CLUTTER_AVAILABLE_IN_1_12
void                    clutter_scroll_actor_set_scroll_mode    (ClutterScrollActor *actor,
                                                                 ClutterScrollMode   mode);
CLUTTER_AVAILABLE_IN_1_12
ClutterScrollMode       clutter_scroll_actor_get_scroll_mode    (ClutterScrollActor *actor);

CLUTTER_AVAILABLE_IN_1_12
void                    clutter_scroll_actor_scroll_to_point    (ClutterScrollActor *actor,
                                                                 const ClutterPoint *point);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_scroll_actor_scroll_to_rect     (ClutterScrollActor *actor,
                                                                 const ClutterRect  *rect);

G_END_DECLS

#endif /* __CLUTTER_SCROLL_ACTOR_H__ */
