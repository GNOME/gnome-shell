/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
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

#ifndef __CLUTTER_BEHAVIOUR_ROTATE_H__
#define __CLUTTER_BEHAVIOUR_ROTATE_H__

#include <clutter/clutter-behaviour.h>
#include <clutter/clutter-alpha.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BEHAVIOUR_ROTATE            (clutter_behaviour_rotate_get_type ())
#define CLUTTER_BEHAVIOUR_ROTATE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BEHAVIOUR_ROTATE, ClutterBehaviourRotate))
#define CLUTTER_IS_BEHAVIOUR_ROTATE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BEHAVIOUR_ROTATE))
#define CLUTTER_BEHAVIOUR_ROTATE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BEHAVIOUR_ROTATE, ClutterBehaviourRotateClass))
#define CLUTTER_IS_BEHAVIOUR_ROTATE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BEHAVIOUR_ROTATE))
#define CLUTTER_BEHAVIOUR_ROTATE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((klass), CLUTTER_TYPE_BEHAVIOUR_ROTATE, ClutterBehaviourRotateClass))

typedef enum { /*< prefix=CLUTTER >*/
  CLUTTER_X_AXIS,
  CLUTTER_Y_AXIS,
  CLUTTER_Z_AXIS
} ClutterRotateAxis;

typedef enum { /*< prefix=CLUTTER_ROTATE >*/
  CLUTTER_ROTATE_CW,
  CLUTTER_ROTATE_CCW
} ClutterRotateDirection;

typedef struct _ClutterBehaviourRotate          ClutterBehaviourRotate;
typedef struct _ClutterBehaviourRotatePrivate   ClutterBehaviourRotatePrivate;
typedef struct _ClutterBehaviourRotateClass     ClutterBehaviourRotateClass;

struct _ClutterBehaviourRotate
{
  ClutterBehaviour parent_instance;

  /*< private >*/
  ClutterBehaviourRotatePrivate *priv;
};

struct _ClutterBehaviourRotateClass
{
  ClutterBehaviourClass parent_class;
};

GType clutter_behaviour_rotate_get_type (void) G_GNUC_CONST;

ClutterBehaviour *clutter_behaviour_rotate_new  (ClutterAlpha           *alpha,
                                                 ClutterRotateAxis       axis,
                                                 ClutterRotateDirection  direction,
                                                 gdouble                 angle_begin,
                                                 gdouble                 angle_end);
ClutterBehaviour *clutter_behaviour_rotate_newx (ClutterAlpha           *alpha,
                                                 ClutterRotateAxis       axis,
                                                 ClutterRotateDirection  direction,
                                                 ClutterFixed            angle_begin,
                                                 ClutterFixed            angle_end);

void                   clutter_behaviour_rotate_get_center    (ClutterBehaviourRotate *rotate,
							       gint                   *x,
							       gint                   *y,
							       gint                   *z);
void                   clutter_behaviour_rotate_set_center    (ClutterBehaviourRotate *rotate,
							       gint                   x,
							       gint                   y,
							       gint                   z);

ClutterRotateAxis      clutter_behaviour_rotate_get_axis      (ClutterBehaviourRotate *rotate);
void                   clutter_behaviour_rotate_set_axis      (ClutterBehaviourRotate *rotate,
                                                               ClutterRotateAxis       axis);
ClutterRotateDirection clutter_behaviour_rotate_get_direction (ClutterBehaviourRotate *rotate);
void                   clutter_behaviour_rotate_set_direction (ClutterBehaviourRotate *rotate,
                                                               ClutterRotateDirection  direction);
void                   clutter_behaviour_rotate_get_bounds    (ClutterBehaviourRotate *rotate,
                                                               gdouble                *angle_begin,
                                                               gdouble                *angle_end);
void                   clutter_behaviour_rotate_set_bounds    (ClutterBehaviourRotate *rotate,
                                                               gdouble                 angle_begin,
                                                               gdouble                 angle_end);
void                   clutter_behaviour_rotate_get_boundsx   (ClutterBehaviourRotate *rotate,
                                                               ClutterFixed           *angle_begin,
                                                               ClutterFixed           *angle_end);
void                   clutter_behaviour_rotate_set_boundsx   (ClutterBehaviourRotate *rotate,
                                                               ClutterFixed            angle_begin,
                                                               ClutterFixed            angle_end);

G_END_DECLS

#endif /* __CLUTTER_BEHAVIOUR_ROTATE_H__ */
