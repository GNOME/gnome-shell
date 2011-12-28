/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011 Intel Corp
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

#ifndef __CLUTTER_GROUP_DEPRECATED_H__
#define __CLUTTER_GROUP_DEPRECATED_H__

#include <clutter/clutter-types.h>
#include <clutter/clutter-group.h>

G_BEGIN_DECLS

CLUTTER_DEPRECATED_FOR(clutter_actor_new)
ClutterActor *  clutter_group_new               (void);

CLUTTER_DEPRECATED_FOR(clutter_actor_get_child_at_index)
ClutterActor *  clutter_group_get_nth_child     (ClutterGroup *self,
                                                 gint          index_);

CLUTTER_DEPRECATED_FOR(clutter_actor_get_n_children)
gint            clutter_group_get_n_children    (ClutterGroup *self);

CLUTTER_DEPRECATED_FOR(clutter_actor_remove_all_children)
void            clutter_group_remove_all        (ClutterGroup *self);

#ifndef CLUTTER_DISABLE_DEPRECATED

/* for Mr. Mallum only */
#define clutter_group_add(group,actor)                  G_STMT_START {  \
  ClutterActor *_actor = (ClutterActor *) (actor);                      \
  if (CLUTTER_IS_GROUP ((group)) && CLUTTER_IS_ACTOR ((_actor)))        \
    {                                                                   \
      ClutterContainer *_container = (ClutterContainer *) (group);      \
      clutter_container_add_actor (_container, _actor);                 \
    }                                                   } G_STMT_END

#endif /* CLUTTER_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __CLUTTER_GROUP_DEPRECATED_H__ */
