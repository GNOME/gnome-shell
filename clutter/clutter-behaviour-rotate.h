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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_BEHAVIOUR_ROTATE_H__
#define __CLUTTER_BEHAVIOUR_ROTATE_H__

#include <clutter/clutter-alpha.h>
#include <clutter/clutter-behaviour.h>
#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BEHAVIOUR_ROTATE            (clutter_behaviour_rotate_get_type ())
#define CLUTTER_BEHAVIOUR_ROTATE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BEHAVIOUR_ROTATE, ClutterBehaviourRotate))
#define CLUTTER_IS_BEHAVIOUR_ROTATE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BEHAVIOUR_ROTATE))
#define CLUTTER_BEHAVIOUR_ROTATE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BEHAVIOUR_ROTATE, ClutterBehaviourRotateClass))
#define CLUTTER_IS_BEHAVIOUR_ROTATE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BEHAVIOUR_ROTATE))
#define CLUTTER_BEHAVIOUR_ROTATE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((klass), CLUTTER_TYPE_BEHAVIOUR_ROTATE, ClutterBehaviourRotateClass))

typedef struct _ClutterBehaviourRotate          ClutterBehaviourRotate;
typedef struct _ClutterBehaviourRotatePrivate   ClutterBehaviourRotatePrivate;
typedef struct _ClutterBehaviourRotateClass     ClutterBehaviourRotateClass;

/**
 * ClutterBehaviourRotate:
 *
 * The #ClutterBehaviourRotate struct contains only private data and
 * should be accessed using the provided API
 *
 * Since: 0.4
 */
struct _ClutterBehaviourRotate
{
  /*< private >*/
  ClutterBehaviour parent_instance;

  ClutterBehaviourRotatePrivate *priv;
};

/**
 * ClutterBehaviourRotateClass:
 *
 * The #ClutterBehaviourRotateClass struct contains only private data
 *
 * Since: 0.4
 */
struct _ClutterBehaviourRotateClass
{
  /*< private >*/
  ClutterBehaviourClass parent_class;
};

GType clutter_behaviour_rotate_get_type (void) G_GNUC_CONST;

ClutterBehaviour *     clutter_behaviour_rotate_new           (ClutterAlpha           *alpha,
                                                               ClutterRotateAxis       axis,
                                                               ClutterRotateDirection  direction,
                                                               gdouble                 angle_start,
                                                               gdouble                 angle_end);
void                   clutter_behaviour_rotate_get_center    (ClutterBehaviourRotate *rotate,
							       gint                   *x,
							       gint                   *y,
							       gint                   *z);
void                   clutter_behaviour_rotate_set_center    (ClutterBehaviourRotate *rotate,
							       gint                    x,
							       gint                    y,
							       gint                    z);
ClutterRotateAxis      clutter_behaviour_rotate_get_axis      (ClutterBehaviourRotate *rotate);
void                   clutter_behaviour_rotate_set_axis      (ClutterBehaviourRotate *rotate,
                                                               ClutterRotateAxis       axis);
ClutterRotateDirection clutter_behaviour_rotate_get_direction (ClutterBehaviourRotate *rotate);
void                   clutter_behaviour_rotate_set_direction (ClutterBehaviourRotate *rotate,
                                                               ClutterRotateDirection  direction);
void                   clutter_behaviour_rotate_get_bounds    (ClutterBehaviourRotate *rotate,
                                                               gdouble                *angle_start,
                                                               gdouble                *angle_end);
void                   clutter_behaviour_rotate_set_bounds    (ClutterBehaviourRotate *rotate,
                                                               gdouble                 angle_start,
                                                               gdouble                 angle_end);

G_END_DECLS

#endif /* __CLUTTER_BEHAVIOUR_ROTATE_H__ */
