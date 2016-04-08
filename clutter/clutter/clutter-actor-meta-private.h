/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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
 * Author:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

#ifndef __CLUTTER_ACTOR_META_PRIVATE_H__
#define __CLUTTER_ACTOR_META_PRIVATE_H__

#include <clutter/clutter-actor-meta.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_META_GROUP         (_clutter_meta_group_get_type ())
#define CLUTTER_META_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_META_GROUP, ClutterMetaGroup))
#define CLUTTER_IS_META_GROUP(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_META_GROUP))

typedef struct _ClutterMetaGroup        ClutterMetaGroup;
typedef struct _ClutterMetaGroupClass   ClutterMetaGroupClass;

struct _ClutterMetaGroup
{
  GObject parent_instance;

  ClutterActor *actor;

  GList *meta;
};

struct _ClutterMetaGroupClass
{
  GObjectClass parent_class;
};

/* Each actor meta has a priority with zero as a default. A higher
   number means higher priority. Higher priority metas stay at the
   beginning of the list. The priority can be negative to give lower
   priority than the default. */

#define CLUTTER_ACTOR_META_PRIORITY_DEFAULT 0

/* Any value greater than this is considered an 'internal' priority
   and if we expose the priority property publicly then an application
   would not be able to use these values. */

#define CLUTTER_ACTOR_META_PRIORITY_INTERNAL_HIGH (G_MAXINT / 2)
#define CLUTTER_ACTOR_META_PRIORITY_INTERNAL_LOW (G_MININT / 2)

GType _clutter_meta_group_get_type (void) G_GNUC_CONST;

void                    _clutter_meta_group_add_meta    (ClutterMetaGroup *group,
                                                         ClutterActorMeta *meta);
void                    _clutter_meta_group_remove_meta (ClutterMetaGroup *group,
                                                         ClutterActorMeta *meta);
const GList *           _clutter_meta_group_peek_metas  (ClutterMetaGroup *group);
void                    _clutter_meta_group_clear_metas (ClutterMetaGroup *group);
ClutterActorMeta *      _clutter_meta_group_get_meta    (ClutterMetaGroup *group,
                                                         const gchar      *name);

gboolean                _clutter_meta_group_has_metas_no_internal (ClutterMetaGroup *group);

GList *                 _clutter_meta_group_get_metas_no_internal   (ClutterMetaGroup *group);
void                    _clutter_meta_group_clear_metas_no_internal (ClutterMetaGroup *group);

/* ActorMeta */
void                    _clutter_actor_meta_set_actor           (ClutterActorMeta *meta,
                                                                 ClutterActor     *actor);

const gchar *           _clutter_actor_meta_get_debug_name      (ClutterActorMeta *meta);

void                    _clutter_actor_meta_set_priority        (ClutterActorMeta *meta,
                                                                 gint priority);
int                     _clutter_actor_meta_get_priority        (ClutterActorMeta *meta);

gboolean                _clutter_actor_meta_is_internal         (ClutterActorMeta *meta);

G_END_DECLS

#endif /* __CLUTTER_ACTOR_META_PRIVATE_H__ */
