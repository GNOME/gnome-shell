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
 *
 * Inspired by the StClickable class in GNOME Shell, written by:
 *   Colin Walters <walters@verbum.org>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_CLICK_ACTION_H__
#define __CLUTTER_CLICK_ACTION_H__

#include <clutter/clutter-action.h>
#include <clutter/clutter-event.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_CLICK_ACTION               (clutter_click_action_get_type ())
#define CLUTTER_CLICK_ACTION(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_CLICK_ACTION, ClutterClickAction))
#define CLUTTER_IS_CLICK_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_CLICK_ACTION))
#define CLUTTER_CLICK_ACTION_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_CLICK_ACTION, ClutterClickActionClass))
#define CLUTTER_IS_CLICK_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_CLICK_ACTION))
#define CLUTTER_CLICK_ACTION_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_CLICK_ACTION, ClutterClickActionClass))

typedef struct _ClutterClickAction              ClutterClickAction;
typedef struct _ClutterClickActionPrivate       ClutterClickActionPrivate;
typedef struct _ClutterClickActionClass         ClutterClickActionClass;

/**
 * ClutterClickAction:
 *
 * The #ClutterClickAction structure contains
 * only private data and should be accessed using the provided API
 *
 * Since: 1.4
 */
struct _ClutterClickAction
{
  /*< private >*/
  ClutterAction parent_instance;

  ClutterClickActionPrivate *priv;
};

/**
 * ClutterClickActionClass:
 * @clicked: class handler for the #ClutterClickAction::clicked signal
 * @long_press: class handler for the #ClutterClickAction::long-press signal
 *
 * The #ClutterClickActionClass structure
 * contains only private data
 *
 * Since: 1.4
 */
struct _ClutterClickActionClass
{
  /*< private >*/
  ClutterActionClass parent_class;

  /*< public >*/
  void     (* clicked)    (ClutterClickAction    *action,
                           ClutterActor          *actor);

  gboolean (* long_press) (ClutterClickAction    *action,
                           ClutterActor          *actor,
                           ClutterLongPressState  state);

  /*< private >*/
  void (* _clutter_click_action1) (void);
  void (* _clutter_click_action2) (void);
  void (* _clutter_click_action3) (void);
  void (* _clutter_click_action4) (void);
  void (* _clutter_click_action5) (void);
  void (* _clutter_click_action6) (void);
  void (* _clutter_click_action7) (void);
};

CLUTTER_AVAILABLE_IN_1_4
GType clutter_click_action_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_4
ClutterAction *        clutter_click_action_new        (void);

CLUTTER_AVAILABLE_IN_1_4
guint                  clutter_click_action_get_button (ClutterClickAction *action);
CLUTTER_AVAILABLE_IN_1_4
ClutterModifierType    clutter_click_action_get_state  (ClutterClickAction *action);
CLUTTER_AVAILABLE_IN_1_8
void                   clutter_click_action_get_coords (ClutterClickAction *action,
                                                        gfloat             *press_x,
                                                        gfloat             *press_y);

CLUTTER_AVAILABLE_IN_1_4
void                   clutter_click_action_release    (ClutterClickAction *action);

G_END_DECLS

#endif /* __CLUTTER_CLICK_ACTION_H__ */
