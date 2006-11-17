/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen  <jorn@openedhand.com>
 *             Emmanuele Bassi  <ebassi@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

#ifndef __CLUTTER_BEHAVIOUR_SCALE_H__
#define __CLUTTER_BEHAVIOUR_SCALE_H__

#include <clutter/clutter-alpha.h>
#include <clutter/clutter-behaviour.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BEHAVIOUR_SCALE (clutter_behaviour_scale_get_type ())

#define CLUTTER_BEHAVIOUR_SCALE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_SCALE, ClutterBehaviourScale))

#define CLUTTER_BEHAVIOUR_SCALE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_BEHAVIOUR_SCALE, ClutterBehaviourScaleClass))

#define CLUTTER_IS_BEHAVIOUR_SCALE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_SCALE))

#define CLUTTER_IS_BEHAVIOUR_SCALE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_BEHAVIOUR_SCALE))

#define CLUTTER_BEHAVIOUR_SCALE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_SCALE, ClutterBehaviourScaleClass))

typedef struct _ClutterBehaviourScale        ClutterBehaviourScale;
typedef struct _ClutterBehaviourScalePrivate ClutterBehaviourScalePrivate;
typedef struct _ClutterBehaviourScaleClass   ClutterBehaviourScaleClass;
 
struct _ClutterBehaviourScale
{
  ClutterBehaviour             parent;
  ClutterBehaviourScalePrivate *priv;
};

struct _ClutterBehaviourScaleClass
{
  ClutterBehaviourClass   parent_class;
};

typedef enum {
  CLUTTER_GRAVITY_NONE = 0,
  CLUTTER_GRAVITY_NORTH,
  CLUTTER_GRAVITY_NORTH_EAST,
  CLUTTER_GRAVITY_EAST,
  CLUTTER_GRAVITY_SOUTH_EAST,
  CLUTTER_GRAVITY_SOUTH,
  CLUTTER_GRAVITY_SOUTH_WEST,
  CLUTTER_GRAVITY_WEST,
  CLUTTER_GRAVITY_NORTH_WEST,
  CLUTTER_GRAVITY_CENTER
} ClutterGravity;

GType clutter_behaviour_scale_get_type (void) G_GNUC_CONST;

ClutterBehaviour*
clutter_behaviour_scale_new (ClutterAlpha  *alpha,
			     gdouble        scale_begin,
			     gdouble        scale_end,
			     ClutterGravity gravity);

ClutterBehaviour*
clutter_behaviour_scale_newx (ClutterAlpha   *alpha,
			      ClutterFixed    scale_begin,
			      ClutterFixed    scale_end,
			      ClutterGravity  gravity);

G_END_DECLS

#endif
