/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012 Intel Corporation
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
 *
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */

#ifndef __CLUTTER_TRANSITION_GROUP_H__
#define __CLUTTER_TRANSITION_GROUP_H__

#include <clutter/clutter-types.h>
#include <clutter/clutter-transition.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_TRANSITION_GROUP                   (clutter_transition_group_get_type ())
#define CLUTTER_TRANSITION_GROUP(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_TRANSITION_GROUP, ClutterTransitionGroup))
#define CLUTTER_IS_TRANSITION_GROUP(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_TRANSITION_GROUP))
#define CLUTTER_TRANSITION_GROUP_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_TRANSITION_GROUP, ClutterTransitionGroupClass))
#define CLUTTER_IS_TRANSITION_GROUP_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_TRANSITION_GROUP))
#define CLUTTER_TRANSITION_GROUP_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_TRANSITION_GROUP, ClutterTransitionGroup))

typedef struct _ClutterTransitionGroupPrivate           ClutterTransitionGroupPrivate;
typedef struct _ClutterTransitionGroupClass             ClutterTransitionGroupClass;

/**
 * ClutterTransitionGroup:
 *
 * The #ClutterTransitionGroup structure contains
 * private data and should only be accessed using the provided API.
 *
 * Since: 1.12
 */
struct _ClutterTransitionGroup
{
  /*< private >*/
  ClutterTransition parent_instance;

  ClutterTransitionGroupPrivate *priv;
};

/**
 * ClutterTransitionGroupClass:
 *
 * The #ClutterTransitionGroupClass structure
 * contains only private data.
 *
 * Since: 1.12
 */
struct _ClutterTransitionGroupClass
{
  /*< private >*/
  ClutterTransitionClass parent_class;

  gpointer _padding[8];
};

CLUTTER_AVAILABLE_IN_1_12
GType clutter_transition_group_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_12
ClutterTransition *     clutter_transition_group_new            (void);

CLUTTER_AVAILABLE_IN_1_12
void                    clutter_transition_group_add_transition         (ClutterTransitionGroup *group,
                                                                         ClutterTransition      *transition);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_transition_group_remove_transition      (ClutterTransitionGroup *group,
                                                                         ClutterTransition      *transition);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_transition_group_remove_all             (ClutterTransitionGroup *group);

G_END_DECLS

#endif /* __CLUTTER_TRANSITION_GROUP_H__ */
