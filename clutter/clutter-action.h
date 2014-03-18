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

#ifndef __CLUTTER_ACTION_H__
#define __CLUTTER_ACTION_H__

#include <clutter/clutter-actor-meta.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ACTION             (clutter_action_get_type ())
#define CLUTTER_ACTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ACTION, ClutterAction))
#define CLUTTER_IS_ACTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ACTION))
#define CLUTTER_ACTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_ACTION, ClutterActionClass))
#define CLUTTER_IS_ACTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_ACTION))
#define CLUTTER_ACTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_ACTION, ClutterActionClass))

typedef struct _ClutterActionClass      ClutterActionClass;

/**
 * ClutterAction:
 *
 * The #ClutterAction structure contains only private data and
 * should be accessed using the provided API.
 *
 * Since: 1.4
 */
struct _ClutterAction
{
  /*< private >*/
  ClutterActorMeta parent_instance;
};

/**
 * ClutterActionClass:
 *
 * The ClutterActionClass structure contains only private data
 *
 * Since: 1.4
 */
struct _ClutterActionClass
{
  /*< private >*/
  ClutterActorMetaClass parent_class;

  void (* _clutter_action1) (void);
  void (* _clutter_action2) (void);
  void (* _clutter_action3) (void);
  void (* _clutter_action4) (void);
  void (* _clutter_action5) (void);
  void (* _clutter_action6) (void);
  void (* _clutter_action7) (void);
  void (* _clutter_action8) (void);
};

CLUTTER_AVAILABLE_IN_1_4
GType clutter_action_get_type (void) G_GNUC_CONST;

/* ClutterActor API */
CLUTTER_AVAILABLE_IN_1_4
void           clutter_actor_add_action            (ClutterActor  *self,
                                                    ClutterAction *action);
CLUTTER_AVAILABLE_IN_1_4
void           clutter_actor_add_action_with_name  (ClutterActor  *self,
                                                    const gchar   *name,
                                                    ClutterAction *action);
CLUTTER_AVAILABLE_IN_1_4
void           clutter_actor_remove_action         (ClutterActor  *self,
                                                    ClutterAction *action);
CLUTTER_AVAILABLE_IN_1_4
void           clutter_actor_remove_action_by_name (ClutterActor  *self,
                                                    const gchar   *name);
CLUTTER_AVAILABLE_IN_1_4
ClutterAction *clutter_actor_get_action            (ClutterActor  *self,
                                                    const gchar   *name);
CLUTTER_AVAILABLE_IN_1_4
GList *        clutter_actor_get_actions           (ClutterActor  *self);
CLUTTER_AVAILABLE_IN_1_4
void           clutter_actor_clear_actions         (ClutterActor  *self);

CLUTTER_AVAILABLE_IN_1_10
gboolean       clutter_actor_has_actions           (ClutterActor  *self);

G_END_DECLS

#endif /* __CLUTTER_ACTION_H__ */
