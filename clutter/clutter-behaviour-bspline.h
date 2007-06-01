/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Tomas Frydrych  <tf@openedhand.com>
 *
 * Copyright (C) 2006, 2007 OpenedHand
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

#ifndef __CLUTTER_BEHAVIOUR_BSPLINE_H__
#define __CLUTTER_BEHAVIOUR_BSPLINE_H__

#include <clutter/clutter-alpha.h>
#include <clutter/clutter-actor.h>
#include <clutter/clutter-behaviour.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BEHAVIOUR_BSPLINE (clutter_behaviour_bspline_get_type ())

#define CLUTTER_BEHAVIOUR_BSPLINE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_BSPLINE, ClutterBehaviourBspline))

#define CLUTTER_BEHAVIOUR_BSPLINE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_BEHAVIOUR_BSPLINE, ClutterBehaviourBsplineClass))

#define CLUTTER_IS_BEHAVIOUR_BSPLINE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_BSPLINE))

#define CLUTTER_IS_BEHAVIOUR_BSPLINE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_BEHAVIOUR_BSPLINE))

#define CLUTTER_BEHAVIOUR_BSPLINE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_BSPLINE, ClutterBehaviourBsplineClass))

typedef struct _ClutterBehaviourBspline        ClutterBehaviourBspline;
typedef struct _ClutterBehaviourBsplinePrivate ClutterBehaviourBsplinePrivate;
typedef struct _ClutterBehaviourBsplineClass   ClutterBehaviourBsplineClass;
 
struct _ClutterBehaviourBspline
{
  ClutterBehaviour parent_instance;
  ClutterBehaviourBsplinePrivate *priv;
};

struct _ClutterBehaviourBsplineClass
{
  ClutterBehaviourClass   parent_class;

  void (*knot_reached) (ClutterBehaviourBspline *bsplineb,
                        const ClutterKnot       *knot);

  void (*_clutter_bspline_1) (void);
  void (*_clutter_bspline_2) (void);
  void (*_clutter_bspline_3) (void);
  void (*_clutter_bspline_4) (void);
};

GType clutter_behaviour_bspline_get_type (void) G_GNUC_CONST;

ClutterBehaviour *clutter_behaviour_bspline_new         (ClutterAlpha            *alpha,
                                                         const ClutterKnot       *knots,
                                                         guint                    n_knots);
void              clutter_behaviour_bspline_append_knot (ClutterBehaviourBspline *bs,
                                                         const ClutterKnot       *knot);
void              clutter_behaviour_bspline_append      (ClutterBehaviourBspline *bs,
                                                         const ClutterKnot       *first_knot,
                                                         ...) G_GNUC_NULL_TERMINATED;
void              clutter_behaviour_bspline_truncate    (ClutterBehaviourBspline *bs,
                                                         guint                    offset);
void              clutter_behaviour_bspline_join        (ClutterBehaviourBspline *bs1,
                                                         ClutterBehaviourBspline *bs2);
ClutterBehaviour *clutter_behaviour_bspline_split       (ClutterBehaviourBspline *bs,
                                                         guint                    offset);
void              clutter_behaviour_bspline_clear       (ClutterBehaviourBspline *bs);
void              clutter_behaviour_bspline_adjust      (ClutterBehaviourBspline *bs,
                                                         guint                    offset,
                                                         ClutterKnot             *knot);
void              clutter_behaviour_bspline_set_origin  (ClutterBehaviourBspline *bs,
                                                         ClutterKnot             *knot);
void              clutter_behaviour_bspline_get_origin  (ClutterBehaviourBspline *bs,
                                                         ClutterKnot             *knot);

G_END_DECLS

#endif /* __CLUTTER_BEHAVIOUR_BSPLINE_H__ */
