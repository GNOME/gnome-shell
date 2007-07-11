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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __CLUTTER_BEHAVIOUR_ELLIPSE_H__
#define __CLUTTER_BEHAVIOUR_ELLIPSE_H__

#include <clutter/clutter-alpha.h>
#include <clutter/clutter-actor.h>
#include <clutter/clutter-behaviour.h>

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
 
struct _ClutterBehaviourEllipse
{
  ClutterBehaviour parent_instance;
  ClutterBehaviourEllipsePrivate *priv;
};

struct _ClutterBehaviourEllipseClass
{
  ClutterBehaviourClass   parent_class;

  void (*knot_reached) (ClutterBehaviourEllipse *ellipseb,
                        const ClutterKnot       *knot);

  void (*_clutter_ellipse_1) (void);
  void (*_clutter_ellipse_2) (void);
  void (*_clutter_ellipse_3) (void);
  void (*_clutter_ellipse_4) (void);
};

GType clutter_behaviour_ellipse_get_type (void) G_GNUC_CONST;

ClutterBehaviour *clutter_behaviour_ellipse_new             (ClutterAlpha               * alpha,
							     gint                         x,
							     gint                         y,
							     gint                         width,
							     gint                         height,
							     gdouble                      begin,
							     gdouble                      end,
							     gdouble                      tilt);

ClutterBehaviour *clutter_behaviour_ellipse_newx            (ClutterAlpha               * alpha,
							     gint                         x,
							     gint                         y,
							     gint                         width,
							     gint                         height,
							     ClutterFixed                 begin,
							     ClutterFixed                 end,
							     ClutterFixed                 tilt);

void              clutter_behaviour_ellipse_set_center      (ClutterBehaviourEllipse    * self,
							     gint                         x,
							     gint                         y);

void              clutter_behaviour_ellipse_get_center      (ClutterBehaviourEllipse    * self,
							     gint                       * x,
							     gint                       * y);

void              clutter_behaviour_ellipse_set_width       (ClutterBehaviourEllipse    * self,
							     gint                         width);

gint              clutter_behaviour_ellipse_get_width       (ClutterBehaviourEllipse    * self);

void              clutter_behaviour_ellipse_set_height      (ClutterBehaviourEllipse    * self,
							     gint                         height);

gint              clutter_behaviour_ellipse_get_height      (ClutterBehaviourEllipse    * self);

void              clutter_behaviour_ellipse_set_angle_begin  (ClutterBehaviourEllipse    * self,
							      gdouble                      angle_begin);

void              clutter_behaviour_ellipse_set_angle_beginx (ClutterBehaviourEllipse    * self,
							      ClutterAngle                 angle_begin);

ClutterAngle      clutter_behaviour_ellipse_get_angle_beginx (ClutterBehaviourEllipse    * self);

gdouble           clutter_behaviour_ellipse_get_angle_begin  (ClutterBehaviourEllipse    * self);

void              clutter_behaviour_ellipse_set_angle_endx   (ClutterBehaviourEllipse    * self,
							      ClutterAngle                 angle_end);

void              clutter_behaviour_ellipse_set_angle_end    (ClutterBehaviourEllipse    * self,
							      gdouble                      angle_end);

ClutterAngle      clutter_behaviour_ellipse_get_angle_endx   (ClutterBehaviourEllipse    * self);

gdouble           clutter_behaviour_ellipse_get_angle_end    (ClutterBehaviourEllipse    * self);

void              clutter_behaviour_ellipse_set_angle_tiltx  (ClutterBehaviourEllipse    * self,
							      ClutterAngle                 angle_tilt);

void              clutter_behaviour_ellipse_set_angle_tilt   (ClutterBehaviourEllipse    * self,
							      gdouble                      angle_tilt);

ClutterAngle      clutter_behaviour_ellipse_get_angle_tiltx  (ClutterBehaviourEllipse    * self);

gdouble           clutter_behaviour_ellipse_get_angle_tilt   (ClutterBehaviourEllipse    * self);

G_END_DECLS

#endif /* __CLUTTER_BEHAVIOUR_ELLIPSE_H__ */
