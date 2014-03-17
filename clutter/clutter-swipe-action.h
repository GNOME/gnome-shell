/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
 * Copyright (C) 2011  Robert Bosch Car Multimedia GmbH.
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
 *   Tomeu Vizoso <tomeu.vizoso@collabora.co.uk>
 *
 * Based on ClutterDragAction, written by:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_SWIPE_ACTION_H__
#define __CLUTTER_SWIPE_ACTION_H__

#include <clutter/clutter-gesture-action.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_SWIPE_ACTION               (clutter_swipe_action_get_type ())
#define CLUTTER_SWIPE_ACTION(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_SWIPE_ACTION, ClutterSwipeAction))
#define CLUTTER_IS_SWIPE_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_SWIPE_ACTION))
#define CLUTTER_SWIPE_ACTION_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_SWIPE_ACTION, ClutterSwipeActionClass))
#define CLUTTER_IS_SWIPE_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_SWIPE_ACTION))
#define CLUTTER_SWIPE_ACTION_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_SWIPE_ACTION, ClutterSwipeActionClass))

typedef struct _ClutterSwipeAction              ClutterSwipeAction;
typedef struct _ClutterSwipeActionPrivate       ClutterSwipeActionPrivate;
typedef struct _ClutterSwipeActionClass         ClutterSwipeActionClass;

/**
 * ClutterSwipeAction:
 *
 * The <structname>ClutterSwipeAction</structname> structure contains
 * only private data and should be accessed using the provided API
 *
 * Since: 1.8
 */
struct _ClutterSwipeAction
{
  /*< private >*/
  ClutterGestureAction parent_instance;

  ClutterSwipeActionPrivate *priv;
};

/**
 * ClutterSwipeActionClass:
 * @swept: class handler for the #ClutterSwipeAction::swept signal;
 *   deprecated since 1.14
 * @swipe: class handler for the #ClutterSwipeAction::swipe signal
 *
 * The <structname>ClutterSwipeActionClass</structname> structure contains
 * only private data.
 *
 * Since: 1.8
 */
struct _ClutterSwipeActionClass
{
  /*< private >*/
  ClutterGestureActionClass parent_class;

  /*< public >*/
  void (* swept)  (ClutterSwipeAction    *action,
                   ClutterActor          *actor,
                   ClutterSwipeDirection  direction);

  gboolean (* swipe) (ClutterSwipeAction    *action,
                      ClutterActor          *actor,
                      ClutterSwipeDirection  direction);

  /*< private >*/
  void (* _clutter_swipe_action1) (void);
  void (* _clutter_swipe_action2) (void);
  void (* _clutter_swipe_action3) (void);
  void (* _clutter_swipe_action4) (void);
  void (* _clutter_swipe_action5) (void);
  void (* _clutter_swipe_action6) (void);
};

CLUTTER_AVAILABLE_IN_1_8
GType clutter_swipe_action_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_8
ClutterAction * clutter_swipe_action_new        (void);

G_END_DECLS

#endif /* __CLUTTER_SWIPE_ACTION_H__ */
