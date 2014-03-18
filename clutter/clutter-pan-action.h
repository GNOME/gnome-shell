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
 * Based on ClutterDragAction, ClutterSwipeAction, and MxKineticScrollView,
 * written by:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 *   Tomeu Vizoso <tomeu.vizoso@collabora.co.uk>
 *   Chris Lord <chris@linux.intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_PAN_ACTION_H__
#define __CLUTTER_PAN_ACTION_H__

#include <clutter/clutter-gesture-action.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_PAN_ACTION               (clutter_pan_action_get_type ())
#define CLUTTER_PAN_ACTION(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_PAN_ACTION, ClutterPanAction))
#define CLUTTER_IS_PAN_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_PAN_ACTION))
#define CLUTTER_PAN_ACTION_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_PAN_ACTION, ClutterPanActionClass))
#define CLUTTER_IS_PAN_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_PAN_ACTION))
#define CLUTTER_PAN_ACTION_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_PAN_ACTION, ClutterPanActionClass))

typedef struct _ClutterPanAction              ClutterPanAction;
typedef struct _ClutterPanActionPrivate       ClutterPanActionPrivate;
typedef struct _ClutterPanActionClass         ClutterPanActionClass;

/**
 * ClutterPanAction:
 *
 * The #ClutterPanAction structure contains
 * only private data and should be accessed using the provided API
 *
 * Since: 1.12
 */
struct _ClutterPanAction
{
  /*< private >*/
  ClutterGestureAction parent_instance;

  ClutterPanActionPrivate *priv;
};

/**
 * ClutterPanActionClass:
 * @pan: class handler for the #ClutterPanAction::pan signal
 * @pan_stopped: class handler for the #ClutterPanAction::pan-stopped signal
 *
 * The #ClutterPanActionClass structure contains
 * only private data.
 *
 * Since: 1.12
 */
struct _ClutterPanActionClass
{
  /*< private >*/
  ClutterGestureActionClass parent_class;

  /*< public >*/
  gboolean (* pan)               (ClutterPanAction    *action,
                                  ClutterActor        *actor,
                                  gboolean             is_interpolated);
  void     (* pan_stopped)       (ClutterPanAction    *action,
                                  ClutterActor        *actor);

  /*< private >*/
  void (* _clutter_pan_action1) (void);
  void (* _clutter_pan_action2) (void);
  void (* _clutter_pan_action3) (void);
  void (* _clutter_pan_action4) (void);
  void (* _clutter_pan_action5) (void);
  void (* _clutter_pan_action6) (void);
};

CLUTTER_AVAILABLE_IN_1_12
GType clutter_pan_action_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_12
ClutterAction * clutter_pan_action_new                      (void);
CLUTTER_AVAILABLE_IN_1_12
void            clutter_pan_action_set_pan_axis             (ClutterPanAction *self,
                                                             ClutterPanAxis    axis);
CLUTTER_AVAILABLE_IN_1_12
ClutterPanAxis clutter_pan_action_get_pan_axis              (ClutterPanAction *self);
CLUTTER_AVAILABLE_IN_1_12
void            clutter_pan_action_set_interpolate          (ClutterPanAction *self,
                                                             gboolean          should_interpolate);
CLUTTER_AVAILABLE_IN_1_12
gboolean        clutter_pan_action_get_interpolate          (ClutterPanAction *self);
CLUTTER_AVAILABLE_IN_1_12
void            clutter_pan_action_set_deceleration         (ClutterPanAction *self,
                                                             gdouble           rate);
CLUTTER_AVAILABLE_IN_1_12
gdouble         clutter_pan_action_get_deceleration         (ClutterPanAction *self);
CLUTTER_AVAILABLE_IN_1_12
void            clutter_pan_action_set_acceleration_factor  (ClutterPanAction *self,
                                                             gdouble           factor);
CLUTTER_AVAILABLE_IN_1_12
gdouble         clutter_pan_action_get_acceleration_factor  (ClutterPanAction *self);
CLUTTER_AVAILABLE_IN_1_12
void            clutter_pan_action_get_interpolated_coords  (ClutterPanAction *self,
                                                             gfloat           *interpolated_x,
                                                             gfloat           *interpolated_y);
CLUTTER_AVAILABLE_IN_1_12
gfloat          clutter_pan_action_get_interpolated_delta   (ClutterPanAction *self,
                                                             gfloat           *delta_x,
                                                             gfloat           *delta_y);
CLUTTER_AVAILABLE_IN_1_14
gfloat          clutter_pan_action_get_motion_delta         (ClutterPanAction *self,
                                                             guint             point,
                                                             gfloat           *delta_x,
                                                             gfloat           *delta_y);
CLUTTER_AVAILABLE_IN_1_14
void            clutter_pan_action_get_motion_coords        (ClutterPanAction *self,
                                                             guint             point,
                                                             gfloat           *motion_x,
                                                             gfloat           *motion_y);
G_END_DECLS

#endif /* __CLUTTER_PAN_ACTION_H__ */
