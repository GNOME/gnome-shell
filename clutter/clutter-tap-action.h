/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
 * Copyright (C) 2011  Robert Bosch Car Multimedia GmbH.
 * Copyright (C) 2012  Collabora Ltd.
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
 *   Emanuele Aina <emanuele.aina@collabora.com>
 *
 * Based on ClutterPanAction
 * Based on ClutterDragAction, ClutterSwipeAction, and MxKineticScrollView,
 * written by:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 *   Tomeu Vizoso <tomeu.vizoso@collabora.co.uk>
 *   Chris Lord <chris@linux.intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_TAP_ACTION_H__
#define __CLUTTER_TAP_ACTION_H__

#include <clutter/clutter-gesture-action.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_TAP_ACTION               (clutter_tap_action_get_type ())
#define CLUTTER_TAP_ACTION(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_TAP_ACTION, ClutterTapAction))
#define CLUTTER_IS_TAP_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_TAP_ACTION))
#define CLUTTER_TAP_ACTION_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_TAP_ACTION, ClutterTapActionClass))
#define CLUTTER_IS_TAP_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_TAP_ACTION))
#define CLUTTER_TAP_ACTION_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_TAP_ACTION, ClutterTapActionClass))

typedef struct _ClutterTapAction              ClutterTapAction;
typedef struct _ClutterTapActionPrivate       ClutterTapActionPrivate;
typedef struct _ClutterTapActionClass         ClutterTapActionClass;

/**
 * ClutterTapAction:
 *
 * The #ClutterTapAction structure contains
 * only private data and should be accessed using the provided API
 *
 * Since: 1.14
 */
struct _ClutterTapAction
{
  /*< private >*/
  ClutterGestureAction parent_instance;
};

/**
 * ClutterTapActionClass:
 * @tap: class handler for the #ClutterTapAction::tap signal
 *
 * The #ClutterTapActionClass structure contains
 * only private data.
 */
struct _ClutterTapActionClass
{
  /*< private >*/
  ClutterGestureActionClass parent_class;

  /*< public >*/
  gboolean (* tap)               (ClutterTapAction    *action,
                                  ClutterActor        *actor);

  /*< private >*/
  void (* _clutter_tap_action1) (void);
  void (* _clutter_tap_action2) (void);
  void (* _clutter_tap_action3) (void);
  void (* _clutter_tap_action4) (void);
  void (* _clutter_tap_action5) (void);
  void (* _clutter_tap_action6) (void);
};

CLUTTER_AVAILABLE_IN_1_14
GType clutter_tap_action_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_14
ClutterAction * clutter_tap_action_new   (void);
G_END_DECLS

#endif /* __CLUTTER_TAP_ACTION_H__ */
