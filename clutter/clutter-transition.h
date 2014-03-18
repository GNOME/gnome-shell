/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012 Intel Corporation
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
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_TRANSITION_H__
#define __CLUTTER_TRANSITION_H__

#include <clutter/clutter-types.h>
#include <clutter/clutter-timeline.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_TRANSITION                 (clutter_transition_get_type ())
#define CLUTTER_TRANSITION(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_TRANSITION, ClutterTransition))
#define CLUTTER_IS_TRANSITION(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_TRANSITION))
#define CLUTTER_TRANSITION_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_TRANSITION, ClutterTransitionClass))
#define CLUTTER_IS_TRANSITION_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_TRANSITION))
#define CLUTTER_TRANSITION_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_TRANSITION, ClutterTransitionClass))

typedef struct _ClutterTransitionPrivate        ClutterTransitionPrivate;
typedef struct _ClutterTransitionClass          ClutterTransitionClass;

/**
 * ClutterTransition:
 *
 * The #ClutterTransition structure contains private
 * data and should only be accessed using the provided API.
 *
 * Since: 1.10
 */
struct _ClutterTransition
{
  /*< private >*/
  ClutterTimeline parent_instance;

  ClutterTransitionPrivate *priv;
};

/**
 * ClutterTransitionClass:
 * @attached: virtual function; called when a transition is attached to
 *   a #ClutterAnimatable instance
 * @detached: virtual function; called when a transition is detached from
 *   a #ClutterAnimatable instance
 * @compute_value: virtual function; called each frame to compute and apply
 *   the interpolation of the interval
 *
 * The #ClutterTransitionClass structure contains
 * private data.
 *
 * Since: 1.10
 */
struct _ClutterTransitionClass
{
  /*< private >*/
  ClutterTimelineClass parent_class;

  /*< public >*/
  void (* attached) (ClutterTransition *transition,
                     ClutterAnimatable *animatable);
  void (* detached) (ClutterTransition *transition,
                     ClutterAnimatable *animatable);

  void (* compute_value) (ClutterTransition *transition,
                          ClutterAnimatable *animatable,
                          ClutterInterval   *interval,
                          gdouble            progress);

  /*< private >*/
  gpointer _padding[8];
};

CLUTTER_AVAILABLE_IN_1_10
GType clutter_transition_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_10
void                    clutter_transition_set_interval                 (ClutterTransition *transition,
                                                                         ClutterInterval   *interval);
CLUTTER_AVAILABLE_IN_1_10
ClutterInterval *       clutter_transition_get_interval                 (ClutterTransition *transition);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_transition_set_from_value               (ClutterTransition *transition,
                                                                         const GValue      *value);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_transition_set_to_value                 (ClutterTransition *transition,
                                                                         const GValue      *value);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_transition_set_from                     (ClutterTransition *transition,
                                                                         GType              value_type,
                                                                         ...);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_transition_set_to                       (ClutterTransition *transition,
                                                                         GType              value_type,
                                                                         ...);

CLUTTER_AVAILABLE_IN_1_10
void                    clutter_transition_set_animatable               (ClutterTransition *transition,
                                                                         ClutterAnimatable *animatable);
CLUTTER_AVAILABLE_IN_1_10
ClutterAnimatable *     clutter_transition_get_animatable               (ClutterTransition *transition);
CLUTTER_AVAILABLE_IN_1_10
void                    clutter_transition_set_remove_on_complete       (ClutterTransition *transition,
                                                                         gboolean           remove_complete);
CLUTTER_AVAILABLE_IN_1_10
gboolean                clutter_transition_get_remove_on_complete       (ClutterTransition *transition);

G_END_DECLS

#endif /* __CLUTTER_TRANSITION_H__ */
