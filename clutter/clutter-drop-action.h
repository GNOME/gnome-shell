/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright © 2011  Intel Corporation.
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
#error "Only <clutter/clutter.h> can be directly included."
#endif

#ifndef __CLUTTER_DROP_ACTION_H__
#define __CLUTTER_DROP_ACTION_H__

#include <clutter/clutter-action.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_DROP_ACTION                (clutter_drop_action_get_type ())
#define CLUTTER_DROP_ACTION(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_DROP_ACTION, ClutterDropAction))
#define CLUTTER_IS_DROP_ACTION(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_DROP_ACTION))
#define CLUTTER_DROP_ACTION_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_DROP_ACTION, ClutterDropActionClass))
#define CLUTTER_IS_DROP_ACTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_DROP_ACTION))
#define CLUTTER_DROP_ACTION_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_DROP_ACTION, ClutterDropActionClass))

typedef struct _ClutterDropAction               ClutterDropAction;
typedef struct _ClutterDropActionPrivate        ClutterDropActionPrivate;
typedef struct _ClutterDropActionClass          ClutterDropActionClass;

/**
 * ClutterDropAction:
 *
 * The #ClutterDropAction structure contains only
 * private data and should be accessed using the provided API.
 *
 * Since: 1.8
 */
struct _ClutterDropAction
{
  /*< private >*/
  ClutterAction parent_instance;

  ClutterDropActionPrivate *priv;
};

/**
 * ClutterDropActionClass:
 * @can_drop: class handler for the #ClutterDropAction::can-drop signal
 * @over_in: class handler for the #ClutterDropAction::over-in signal
 * @over_out: class handler for the #ClutterDropAction::over-out signal
 * @drop: class handler for the #ClutterDropAction::drop signal
 *
 * The #ClutterDropActionClass structure contains
 * only private data.
 *
 * Since: 1.8
 */
struct _ClutterDropActionClass
{
  /*< private >*/
  ClutterActionClass parent_class;

  /*< public >*/
  gboolean (* can_drop) (ClutterDropAction *action,
                         ClutterActor      *actor,
                         gfloat             event_x,
                         gfloat             event_y);

  void     (* over_in)  (ClutterDropAction *action,
                         ClutterActor      *actor);
  void     (* over_out) (ClutterDropAction *action,
                         ClutterActor      *actor);

  void     (* drop)     (ClutterDropAction *action,
                         ClutterActor      *actor,
                         gfloat             event_x,
                         gfloat             event_y);

  /*< private >*/
  void (*_clutter_drop_action1) (void);
  void (*_clutter_drop_action2) (void);
  void (*_clutter_drop_action3) (void);
  void (*_clutter_drop_action4) (void);
  void (*_clutter_drop_action5) (void);
  void (*_clutter_drop_action6) (void);
  void (*_clutter_drop_action7) (void);
  void (*_clutter_drop_action8) (void);
};

CLUTTER_AVAILABLE_IN_1_8
GType clutter_drop_action_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_8
ClutterAction *         clutter_drop_action_new         (void);

G_END_DECLS

#endif /* __CLUTTER_DROP_ACTION_H__ */
