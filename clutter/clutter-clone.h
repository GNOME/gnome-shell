/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2008  Intel Corporation.
 *
 * Authored By: Robert Bragg <robert@linux.intel.com>
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

#ifndef __CLUTTER_ACTOR_CLONE_H__
#define __CLUTTER_ACTOR_CLONE_H__

#include <glib-object.h>
#include <clutter/clutter-actor.h>
#include <clutter/clutter-color.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ACTOR_CLONE                  (clutter_actor_clone_get_type())
#define CLUTTER_ACTOR_CLONE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ACTOR_CLONE, ClutterActorClone))
#define CLUTTER_ACTOR_CLONE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_ACTOR_CLONE, ClutterActorCloneClass))
#define CLUTTER_IS_ACTOR_CLONE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ACTOR_CLONE))
#define CLUTTER_IS_ACTOR_CLONE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_ACTOR_CLONE))
#define CLUTTER_ACTOR_CLONE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_ACTOR_CLONE, ClutterActorCloneClass))

typedef struct _ClutterActorClone        ClutterActorClone;
typedef struct _ClutterActorCloneClass   ClutterActorCloneClass;
typedef struct _ClutterActorClonePrivate ClutterActorClonePrivate;

struct _ClutterActorClone
{
  /*< private >*/
  ClutterActor              parent;

  ClutterActorClonePrivate *priv;
};

struct _ClutterActorCloneClass
{
  /*< private >*/
  ClutterActorClass parent_class;

  /* padding for future expansion */
  void (*_clutter_actor_clone1) (void);
  void (*_clutter_actor_clone2) (void);
  void (*_clutter_actor_clone3) (void);
  void (*_clutter_actor_clone4) (void);
};

GType clutter_actor_clone_get_type (void) G_GNUC_CONST;

ClutterActor *clutter_actor_clone_new (ClutterActor *clone_source);

ClutterActor *clutter_actor_clone_get_clone_source (ClutterActorClone *clone);

G_END_DECLS

#endif /* __CLUTTER_ACTOR_CLONE_H__ */
