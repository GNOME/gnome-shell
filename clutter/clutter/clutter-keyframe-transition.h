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

#ifndef __CLUTTER_KEYFRAME_TRANSITION_H__
#define __CLUTTER_KEYFRAME_TRANSITION_H__

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#include <clutter/clutter-types.h>
#include <clutter/clutter-property-transition.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_KEYFRAME_TRANSITION                (clutter_keyframe_transition_get_type ())
#define CLUTTER_KEYFRAME_TRANSITION(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_KEYFRAME_TRANSITION, ClutterKeyframeTransition))
#define CLUTTER_IS_KEYFRAME_TRANSITION(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_KEYFRAME_TRANSITION))
#define CLUTTER_KEYFRAME_TRANSITION_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_KEYFRAME_TRANSITION, ClutterKeyframeTransitionClass))
#define CLUTTER_IS_KEYFRAME_TRANSITION_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_KEYFRAME_TRANSITION))
#define CLUTTER_KEYFRAME_TRANSITION_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_KEYFRAME_TRANSITION, ClutterKeyframeTransitionClass))

typedef struct _ClutterKeyframeTransitionPrivate        ClutterKeyframeTransitionPrivate;
typedef struct _ClutterKeyframeTransitionClass          ClutterKeyframeTransitionClass;

/**
 * ClutterKeyframeTransition:
 *
 * The `ClutterKeyframeTransition` structure contains only private
 * data and should be accessed using the provided API.
 *
 * Since: 1.12
 */
struct _ClutterKeyframeTransition
{
  /*< private >*/
  ClutterPropertyTransition parent_instance;

  ClutterKeyframeTransitionPrivate *priv;
};

/**
 * ClutterKeyframeTransitionClass:
 *
 * The `ClutterKeyframeTransitionClass` structure contains only
 * private data.
 *
 * Since: 1.12
 */
struct _ClutterKeyframeTransitionClass
{
  /*< private >*/
  ClutterPropertyTransitionClass parent_class;

  gpointer _padding[8];
};

CLUTTER_AVAILABLE_IN_1_12
GType clutter_keyframe_transition_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_12
ClutterTransition *     clutter_keyframe_transition_new                 (const char *property_name);

CLUTTER_AVAILABLE_IN_1_12
void                    clutter_keyframe_transition_set_key_frames      (ClutterKeyframeTransition  *transition,
                                                                         guint                       n_key_frames,
                                                                         const double               *key_frames);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_keyframe_transition_set_values          (ClutterKeyframeTransition  *transition,
                                                                         guint                       n_values,
                                                                         const GValue               *values);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_keyframe_transition_set_modes           (ClutterKeyframeTransition  *transition,
                                                                         guint                       n_modes,
                                                                         const ClutterAnimationMode *modes);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_keyframe_transition_set                 (ClutterKeyframeTransition  *transition,
                                                                         GType                       gtype,
                                                                         guint                       n_key_frames,
                                                                         ...);

CLUTTER_AVAILABLE_IN_1_12
void                    clutter_keyframe_transition_set_key_frame       (ClutterKeyframeTransition  *transition,
                                                                         guint                       index_,
                                                                         double                      key,
                                                                         ClutterAnimationMode        mode,
                                                                         const GValue               *value);
CLUTTER_AVAILABLE_IN_1_12
void                    clutter_keyframe_transition_get_key_frame       (ClutterKeyframeTransition  *transition,
                                                                         guint                       index_,
                                                                         double                     *key,
                                                                         ClutterAnimationMode       *mode,
                                                                         GValue                     *value);
CLUTTER_AVAILABLE_IN_1_12
guint                   clutter_keyframe_transition_get_n_key_frames    (ClutterKeyframeTransition  *transition);

CLUTTER_AVAILABLE_IN_1_12
void                    clutter_keyframe_transition_clear               (ClutterKeyframeTransition  *transition);

G_END_DECLS

#endif /* __CLUTTER_KEYFRAME_TRANSITION_H__ */
