/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

#ifndef __CLUTTER_GROUP_H__
#define __CLUTTER_GROUP_H__

#include <glib-object.h>
#include <clutter/clutter-types.h>
#include <clutter/clutter-actor.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_GROUP clutter_group_get_type()

#define CLUTTER_GROUP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_GROUP, ClutterGroup))

#define CLUTTER_GROUP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_GROUP, ClutterGroupClass))

#define CLUTTER_IS_GROUP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_GROUP))

#define CLUTTER_IS_GROUP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_GROUP))

#define CLUTTER_GROUP_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_GROUP, ClutterGroupClass))

typedef struct _ClutterGroup        ClutterGroup;
typedef struct _ClutterGroupClass   ClutterGroupClass;
typedef struct _ClutterGroupPrivate ClutterGroupPrivate;
 
struct _ClutterGroup
{
  ClutterActor parent_instance;

  /*< private >*/
  ClutterGroupPrivate *priv;
};

struct _ClutterGroupClass
{
  /*< private >*/
  ClutterActorClass parent_class;

  void (*add)    (ClutterGroup *group,
		  ClutterActor *child);
  void (*remove) (ClutterGroup *group,
		  ClutterActor *child);

  /* padding for future expansion */
  void (*_clutter_reserved1) (void);
  void (*_clutter_reserved2) (void);
  void (*_clutter_reserved3) (void);
  void (*_clutter_reserved4) (void);
  void (*_clutter_reserved5) (void);
  void (*_clutter_reserved6) (void);
};

GType         clutter_group_get_type         (void) G_GNUC_CONST;
ClutterActor *clutter_group_new              (void);
ClutterActor *clutter_group_get_nth_child    (ClutterGroup    *self,
                                              gint             index_);
gint          clutter_group_get_n_children   (ClutterGroup    *self);
void          clutter_group_remove_all       (ClutterGroup    *group);

/* for Mr. Mallum */
#define clutter_group_add(group,actor)                  G_STMT_START {  \
  if (CLUTTER_IS_GROUP ((group)) && CLUTTER_IS_ACTOR ((actor)))         \
    {                                                                   \
      ClutterContainer *_container = (ClutterContainer *) (group);      \
      ClutterActor *_actor = (ClutterActor *) (actor);                  \
      clutter_container_add_actor (_container, _actor);                 \
    }                                                   } G_STMT_END

#ifndef CLUTTER_DISABLE_DEPRECATED
void          clutter_group_add_many         (ClutterGroup    *group,
                                              ClutterActor    *first_actor,
                                              ...) G_GNUC_NULL_TERMINATED;
void          clutter_group_add_many_valist  (ClutterGroup    *group,
                                              ClutterActor    *first_actor,
                                              va_list          var_args);
void          clutter_group_remove           (ClutterGroup    *group,
                                              ClutterActor    *actor);
void          clutter_group_raise            (ClutterGroup    *self,
					      ClutterActor    *actor, 
					      ClutterActor    *sibling);
void          clutter_group_lower            (ClutterGroup    *self,
					      ClutterActor    *actor, 
					      ClutterActor    *sibling);
void          clutter_group_sort_depth_order (ClutterGroup    *self);
#endif /* CLUTTER_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __CLUTTER_GROUP_H__ */
