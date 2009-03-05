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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_BEHAVIOUR_SCALE_H__
#define __CLUTTER_BEHAVIOUR_SCALE_H__

#include <clutter/clutter-behaviour.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BEHAVIOUR_SCALE            (clutter_behaviour_scale_get_type ())
#define CLUTTER_BEHAVIOUR_SCALE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_BEHAVIOUR_SCALE, ClutterBehaviourScale))
#define CLUTTER_BEHAVIOUR_SCALE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BEHAVIOUR_SCALE, ClutterBehaviourScaleClass))
#define CLUTTER_IS_BEHAVIOUR_SCALE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_BEHAVIOUR_SCALE))
#define CLUTTER_IS_BEHAVIOUR_SCALE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BEHAVIOUR_SCALE))
#define CLUTTER_BEHAVIOUR_SCALE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BEHAVIOUR_SCALE, ClutterBehaviourScaleClass))

typedef struct _ClutterBehaviourScale           ClutterBehaviourScale;
typedef struct _ClutterBehaviourScalePrivate    ClutterBehaviourScalePrivate;
typedef struct _ClutterBehaviourScaleClass      ClutterBehaviourScaleClass;

/**
 * ClutterBehaviourScale:
 *
 * The #ClutterBehaviourScale struct contains only private data and
 * should be accessed using the provided API
 *
 * Since: 0.2
 */
struct _ClutterBehaviourScale
{
  /*< private >*/
  ClutterBehaviour parent_instance;

  ClutterBehaviourScalePrivate *priv;
};

/**
 * ClutterBehaviourScaleClass:
 *
 * The #ClutterBehaviourScaleClass struct contains only private data
 *
 * Since: 0.2
 */
struct _ClutterBehaviourScaleClass
{
  /*< private >*/
  ClutterBehaviourClass parent_class;
};

GType clutter_behaviour_scale_get_type (void) G_GNUC_CONST;

ClutterBehaviour *clutter_behaviour_scale_new  (ClutterAlpha   *alpha,
                                                gdouble         x_scale_start,
                                                gdouble         y_scale_start,
                                                gdouble         x_scale_end,
                                                gdouble         y_scale_end);
ClutterBehaviour *clutter_behaviour_scale_newx (ClutterAlpha   *alpha,
                                                CoglFixed       x_scale_start,
                                                CoglFixed       y_scale_start,
                                                CoglFixed       x_scale_end,
                                                CoglFixed       y_scale_end);

void clutter_behaviour_scale_set_bounds  (ClutterBehaviourScale *scale,
                                          gdouble                x_scale_start,
                                          gdouble                y_scale_start,
                                          gdouble                x_scale_end,
                                          gdouble                y_scale_end);
void clutter_behaviour_scale_get_bounds  (ClutterBehaviourScale *scale,
                                          gdouble               *x_scale_start,
                                          gdouble               *y_scale_start,
                                          gdouble               *x_scale_end,
                                          gdouble               *y_scale_end);

void clutter_behaviour_scale_set_boundsx (ClutterBehaviourScale *scale,
                                          CoglFixed              x_scale_start,
                                          CoglFixed              y_scale_start,
                                          CoglFixed              x_scale_end,
                                          CoglFixed              y_scale_end);
void clutter_behaviour_scale_get_boundsx (ClutterBehaviourScale *scale,
                                          CoglFixed             *x_scale_start,
                                          CoglFixed             *y_scale_start,
                                          CoglFixed             *x_scale_end,
                                          CoglFixed             *y_scale_end);

G_END_DECLS

#endif /* __CLUTTER_BEHAVIOUR_SCALE_H__ */
