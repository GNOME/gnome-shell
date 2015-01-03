/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Tomas Frydrych  <tf@openedhand.com>
 *
 * Copyright (C) 2007 OpenedHand
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

#ifndef __CLUTTER_BEHAVIOUR_ELLIPSE_H__
#define __CLUTTER_BEHAVIOUR_ELLIPSE_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BEHAVIOUR_ELLIPSE (clutter_behaviour_ellipse_get_type ())

#define CLUTTER_BEHAVIOUR_ELLIPSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_ELLIPSE, ClutterBehaviourEllipse))

#define CLUTTER_BEHAVIOUR_ELLIPSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_BEHAVIOUR_ELLIPSE, ClutterBehaviourEllipseClass))

#define CLUTTER_IS_BEHAVIOUR_ELLIPSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_ELLIPSE))

#define CLUTTER_IS_BEHAVIOUR_ELLIPSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_BEHAVIOUR_ELLIPSE))

#define CLUTTER_BEHAVIOUR_ELLIPSE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_ELLIPSE, ClutterBehaviourEllipseClass))

typedef struct _ClutterBehaviourEllipse        ClutterBehaviourEllipse;
typedef struct _ClutterBehaviourEllipsePrivate ClutterBehaviourEllipsePrivate;
typedef struct _ClutterBehaviourEllipseClass   ClutterBehaviourEllipseClass;

/**
 * ClutterBehaviourEllipse:
 *
 * The #ClutterBehaviourEllipse struct contains only private data
 * and should be accessed using the provided API
 *
 * Since: 0.4
 *
 * Deprecated: 1.6
 */
struct _ClutterBehaviourEllipse
{
  /*< private >*/
  ClutterBehaviour parent_instance;
  ClutterBehaviourEllipsePrivate *priv;
};

/**
 * ClutterBehaviourEllipseClass:
 *
 * The #ClutterBehaviourEllipseClass struct contains only private data
 *
 * Since: 0.4
 *
 * Deprecated: 1.6
 */
struct _ClutterBehaviourEllipseClass
{
  /*< private >*/
  ClutterBehaviourClass   parent_class;
};

CLUTTER_DEPRECATED_IN_1_8
GType clutter_behaviour_ellipse_get_type (void) G_GNUC_CONST;

CLUTTER_DEPRECATED_IN_1_8_FOR(clutter_actor_animate)
ClutterBehaviour *     clutter_behaviour_ellipse_new             (ClutterAlpha            *alpha,
								  gint                     x,
								  gint                     y,
								  gint                     width,
								  gint                     height,
								  ClutterRotateDirection   direction,
								  gdouble                  start,
								  gdouble                  end);

CLUTTER_DEPRECATED_IN_1_8
void                   clutter_behaviour_ellipse_set_center      (ClutterBehaviourEllipse *self,
								  gint                     x,
								  gint                     y);
CLUTTER_DEPRECATED_IN_1_8
void                   clutter_behaviour_ellipse_get_center      (ClutterBehaviourEllipse *self,
								  gint                    *x,
								  gint                    *y);
CLUTTER_DEPRECATED_IN_1_6
void                   clutter_behaviour_ellipse_set_width       (ClutterBehaviourEllipse *self,
								  gint                     width);
CLUTTER_DEPRECATED_IN_1_6
gint                   clutter_behaviour_ellipse_get_width       (ClutterBehaviourEllipse *self);
CLUTTER_DEPRECATED_IN_1_6
void                   clutter_behaviour_ellipse_set_height      (ClutterBehaviourEllipse *self,
								  gint                     height);
CLUTTER_DEPRECATED_IN_1_6
gint                   clutter_behaviour_ellipse_get_height      (ClutterBehaviourEllipse *self);
CLUTTER_DEPRECATED_IN_1_6
void                   clutter_behaviour_ellipse_set_angle_start (ClutterBehaviourEllipse *self,
								  gdouble                  angle_start);
CLUTTER_DEPRECATED_IN_1_6
gdouble                clutter_behaviour_ellipse_get_angle_start (ClutterBehaviourEllipse *self);
CLUTTER_DEPRECATED_IN_1_6
void                   clutter_behaviour_ellipse_set_angle_end   (ClutterBehaviourEllipse *self,
								  gdouble                  angle_end);
CLUTTER_DEPRECATED_IN_1_6
gdouble                clutter_behaviour_ellipse_get_angle_end   (ClutterBehaviourEllipse *self);
CLUTTER_DEPRECATED_IN_1_6
void                   clutter_behaviour_ellipse_set_angle_tilt  (ClutterBehaviourEllipse *self,
								  ClutterRotateAxis        axis,
								  gdouble                  angle_tilt);
CLUTTER_DEPRECATED_IN_1_6
gdouble                clutter_behaviour_ellipse_get_angle_tilt  (ClutterBehaviourEllipse *self,
								  ClutterRotateAxis        axis);
CLUTTER_DEPRECATED_IN_1_6
void                   clutter_behaviour_ellipse_set_tilt        (ClutterBehaviourEllipse *self,
								  gdouble                  angle_tilt_x,
								  gdouble                  angle_tilt_y,
								  gdouble                  angle_tilt_z);
CLUTTER_DEPRECATED_IN_1_6
void                   clutter_behaviour_ellipse_get_tilt        (ClutterBehaviourEllipse *self,
								  gdouble                 *angle_tilt_x,
								  gdouble                 *angle_tilt_y,
								  gdouble                 *angle_tilt_z);
CLUTTER_DEPRECATED_IN_1_6
ClutterRotateDirection clutter_behaviour_ellipse_get_direction   (ClutterBehaviourEllipse *self);
CLUTTER_DEPRECATED_IN_1_6
void                   clutter_behaviour_ellipse_set_direction   (ClutterBehaviourEllipse *self,
								  ClutterRotateDirection   direction);

G_END_DECLS

#endif /* __CLUTTER_BEHAVIOUR_ELLIPSE_H__ */
