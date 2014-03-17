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

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_CONSTRAINT_H__
#define __CLUTTER_CONSTRAINT_H__

#include <clutter/clutter-actor-meta.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_CONSTRAINT                 (clutter_constraint_get_type ())
#define CLUTTER_CONSTRAINT(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_CONSTRAINT, ClutterConstraint))
#define CLUTTER_IS_CONSTRAINT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_CONSTRAINT))
#define CLUTTER_CONSTRAINT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_CONSTRAINT, ClutterConstraintClass))
#define CLUTTER_IS_CONSTRAINT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_CONSTRAINT))
#define CLUTTER_CONSTRAINT_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_CONSTRAINT, ClutterConstraintClass))

typedef struct _ClutterConstraintClass          ClutterConstraintClass;

/**
 * ClutterConstraint:
 *
 * The <structname>ClutterConstraint</structname> structure contains only
 * private data and should be accessed using the provided API
 *
 * Since: 1.4
 */
struct _ClutterConstraint
{
  /*< private >*/
  ClutterActorMeta parent_instance;
};

/**
 * ClutterConstraintClass:
 *
 * The <structname>ClutterConstraintClass</structname> structure contains
 * only private data
 *
 * Since: 1.4
 */
struct _ClutterConstraintClass
{
  /*< private >*/
  ClutterActorMetaClass parent_class;

  void (* update_allocation) (ClutterConstraint *constraint,
                              ClutterActor      *actor,
                              ClutterActorBox   *allocation);

  /*< private >*/
  void (* _clutter_constraint1) (void);
  void (* _clutter_constraint2) (void);
  void (* _clutter_constraint3) (void);
  void (* _clutter_constraint4) (void);
  void (* _clutter_constraint5) (void);
  void (* _clutter_constraint6) (void);
  void (* _clutter_constraint7) (void);
  void (* _clutter_constraint8) (void);
};

CLUTTER_AVAILABLE_IN_1_4
GType clutter_constraint_get_type (void) G_GNUC_CONST;

/* ClutterActor API */
CLUTTER_AVAILABLE_IN_1_4
void               clutter_actor_add_constraint            (ClutterActor      *self,
                                                            ClutterConstraint *constraint);
CLUTTER_AVAILABLE_IN_1_4
void               clutter_actor_add_constraint_with_name  (ClutterActor      *self,
                                                            const gchar       *name,
                                                            ClutterConstraint *constraint);
CLUTTER_AVAILABLE_IN_1_4
void               clutter_actor_remove_constraint         (ClutterActor      *self,
                                                            ClutterConstraint *constraint);
CLUTTER_AVAILABLE_IN_1_4
void               clutter_actor_remove_constraint_by_name (ClutterActor      *self,
                                                            const gchar       *name);
CLUTTER_AVAILABLE_IN_1_4
GList *            clutter_actor_get_constraints           (ClutterActor      *self);
CLUTTER_AVAILABLE_IN_1_4
ClutterConstraint *clutter_actor_get_constraint            (ClutterActor      *self,
                                                            const gchar       *name);
CLUTTER_AVAILABLE_IN_1_4
void               clutter_actor_clear_constraints         (ClutterActor      *self);

CLUTTER_AVAILABLE_IN_1_10
gboolean           clutter_actor_has_constraints           (ClutterActor *self);

G_END_DECLS

#endif /* __CLUTTER_CONSTRAINT_H__ */
