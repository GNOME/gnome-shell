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

#ifndef __CLUTTER_CLONE_H__
#define __CLUTTER_CLONE_H__

#include <clutter/clutter-actor.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_CLONE              (clutter_clone_get_type())
#define CLUTTER_CLONE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_CLONE, ClutterClone))
#define CLUTTER_CLONE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_CLONE, ClutterCloneClass))
#define CLUTTER_IS_CLONE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_CLONE))
#define CLUTTER_IS_CLONE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_CLONE))
#define CLUTTER_CLONE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_CLONE, ClutterCloneClass))

typedef struct _ClutterClone            ClutterClone;
typedef struct _ClutterCloneClass       ClutterCloneClass;
typedef struct _ClutterClonePrivate     ClutterClonePrivate;

/**
 * ClutterClone:
 *
 * The #ClutterClone structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.0
 */
struct _ClutterClone
{
  /*< private >*/
  ClutterActor parent_instance;

  ClutterClonePrivate *priv;
};

/**
 * ClutterCloneClass:
 *
 * The #ClutterCloneClass structure contains only private data
 *
 * Since: 1.0
 */
struct _ClutterCloneClass
{
  /*< private >*/
  ClutterActorClass parent_class;

  /* padding for future expansion */
  void (*_clutter_actor_clone1) (void);
  void (*_clutter_actor_clone2) (void);
  void (*_clutter_actor_clone3) (void);
  void (*_clutter_actor_clone4) (void);
};

GType clutter_clone_get_type (void) G_GNUC_CONST;

ClutterActor *clutter_clone_new        (ClutterActor *source);
void          clutter_clone_set_source (ClutterClone *clone,
                                        ClutterActor *source);
ClutterActor *clutter_clone_get_source (ClutterClone *clone);

G_END_DECLS

#endif /* __CLUTTER_CLONE_H__ */
