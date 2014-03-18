/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012  Intel Corporation.
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
 *   Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_ZOOM_ACTION_H__
#define __CLUTTER_ZOOM_ACTION_H__

#include <clutter/clutter-event.h>
#include <clutter/clutter-gesture-action.h>
#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ZOOM_ACTION                (clutter_zoom_action_get_type ())
#define CLUTTER_ZOOM_ACTION(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ZOOM_ACTION, ClutterZoomAction))
#define CLUTTER_IS_ZOOM_ACTION(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ZOOM_ACTION))
#define CLUTTER_ZOOM_ACTION_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_ZOOM_ACTION, ClutterZoomActionClass))
#define CLUTTER_IS_ZOOM_ACTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_ZOOM_ACTION))
#define CLUTTER_ZOOM_ACTION_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_ZOOM_ACTION, ClutterZoomActionClass))

typedef struct _ClutterZoomAction               ClutterZoomAction;
typedef struct _ClutterZoomActionPrivate        ClutterZoomActionPrivate;
typedef struct _ClutterZoomActionClass          ClutterZoomActionClass;

/**
 * ClutterZoomAction:
 *
 * The #ClutterZoomAction structure contains only
 * private data and should be accessed using the provided API
 *
 * Since: 1.12
 */
struct _ClutterZoomAction
{
  /*< private >*/
  ClutterGestureAction parent_instance;

  ClutterZoomActionPrivate *priv;
};

/**
 * ClutterZoomActionClass:
 * @zoom: class handler of the #ClutterZoomAction::zoom signal
 *
 * The #ClutterZoomActionClass structure contains
 * only private data
 *
 * Since: 1.12
 */
struct _ClutterZoomActionClass
{
  /*< private >*/
  ClutterGestureActionClass parent_class;

  /*< public >*/
  gboolean (* zoom)  (ClutterZoomAction *action,
                      ClutterActor      *actor,
                      ClutterPoint      *focal_point,
                      gdouble            factor);

  /*< private >*/
  void (* _clutter_zoom_action1) (void);
  void (* _clutter_zoom_action2) (void);
  void (* _clutter_zoom_action3) (void);
  void (* _clutter_zoom_action4) (void);
  void (* _clutter_zoom_action5) (void);
};

CLUTTER_AVAILABLE_IN_1_12
GType clutter_zoom_action_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_12
ClutterAction * clutter_zoom_action_new                         (void);

CLUTTER_AVAILABLE_IN_1_12
void            clutter_zoom_action_set_zoom_axis               (ClutterZoomAction *action,
                                                                 ClutterZoomAxis    axis);
CLUTTER_AVAILABLE_IN_1_12
ClutterZoomAxis clutter_zoom_action_get_zoom_axis               (ClutterZoomAction *action);

CLUTTER_AVAILABLE_IN_1_12
void            clutter_zoom_action_get_focal_point             (ClutterZoomAction *action,
                                                                 ClutterPoint      *point);
CLUTTER_AVAILABLE_IN_1_12
void            clutter_zoom_action_get_transformed_focal_point (ClutterZoomAction *action,
                                                                 ClutterPoint      *point);

G_END_DECLS

#endif /* __CLUTTER_ZOOM_ACTION_H__ */
