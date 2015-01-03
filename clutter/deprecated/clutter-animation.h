/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2008  Intel Corporation.
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
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_ANIMATION_H__
#define __CLUTTER_ANIMATION_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ANIMATION                  (clutter_animation_get_type ())
#define CLUTTER_ANIMATION(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ANIMATION, ClutterAnimation))
#define CLUTTER_IS_ANIMATION(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ANIMATION))
#define CLUTTER_ANIMATION_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_ANIMATION, ClutterAnimationClass))
#define CLUTTER_IS_ANIMATION_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_ANIMATION))
#define CLUTTER_ANIMATION_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_ANIMATION, ClutterAnimationClass))

typedef struct _ClutterAnimationPrivate         ClutterAnimationPrivate;
typedef struct _ClutterAnimationClass           ClutterAnimationClass;

/**
 * ClutterAnimation:
 *
 * The #ClutterAnimation structure contains only private data and should
 * be accessed using the provided functions.
 *
 * Since: 1.0
 *
 * Deprecated: 1.12: Use the implicit animation on #ClutterActor
 */
struct _ClutterAnimation
{
  /*< private >*/
  GObject parent_instance;

  ClutterAnimationPrivate *priv;
};

/**
 * ClutterAnimationClass:
 * @started: class handler for the #ClutterAnimation::started signal
 * @completed: class handler for the #ClutterAnimation::completed signal
 *
 * The #ClutterAnimationClass structure contains only private data and
 * should be accessed using the provided functions.
 *
 * Since: 1.0
 *
 * Deprecated: 1.12: Use the implicit animation on #ClutterActor
 */
struct _ClutterAnimationClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  void (* started)   (ClutterAnimation *animation);
  void (* completed) (ClutterAnimation *animation);

  /*< private >*/
  /* padding for future expansion */
  void (*_clutter_reserved1) (void);
  void (*_clutter_reserved2) (void);
  void (*_clutter_reserved3) (void);
  void (*_clutter_reserved4) (void);
  void (*_clutter_reserved5) (void);
  void (*_clutter_reserved6) (void);
  void (*_clutter_reserved7) (void);
  void (*_clutter_reserved8) (void);
};

CLUTTER_DEPRECATED_IN_1_12
GType clutter_animation_get_type (void) G_GNUC_CONST;

CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_property_transition_new)
ClutterAnimation *      clutter_animation_new                   (void);

CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_transition_set_animatable)
void                    clutter_animation_set_object            (ClutterAnimation     *animation,
                                                                 GObject              *object);
CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_transition_get_animatable)
GObject *               clutter_animation_get_object            (ClutterAnimation     *animation);
CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_timeline_set_progress_mode)
void                    clutter_animation_set_mode              (ClutterAnimation     *animation,
                                                                 gulong                mode);
CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_timeline_get_progress_mode)
gulong                  clutter_animation_get_mode              (ClutterAnimation     *animation);
CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_timeline_set_duration)
void                    clutter_animation_set_duration          (ClutterAnimation     *animation,
                                                                 guint                 msecs);
CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_timeline_get_duration)
guint                   clutter_animation_get_duration          (ClutterAnimation     *animation);
CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_timeline_set_repeat_count)
void                    clutter_animation_set_loop              (ClutterAnimation     *animation,
                                                                 gboolean              loop);
CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_timeline_get_repeat_count)
gboolean                clutter_animation_get_loop              (ClutterAnimation     *animation);
CLUTTER_DEPRECATED_IN_1_12
void                    clutter_animation_set_timeline          (ClutterAnimation     *animation,
                                                                 ClutterTimeline      *timeline);
CLUTTER_DEPRECATED_IN_1_12
ClutterTimeline *       clutter_animation_get_timeline          (ClutterAnimation     *animation);
CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_animation_set_timeline)
void                    clutter_animation_set_alpha             (ClutterAnimation     *animation,
                                                                 ClutterAlpha         *alpha);
CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_animation_get_timeline)
ClutterAlpha *          clutter_animation_get_alpha             (ClutterAnimation     *animation);
CLUTTER_DEPRECATED_IN_1_12
ClutterAnimation *      clutter_animation_bind                  (ClutterAnimation     *animation,
                                                                 const gchar          *property_name,
                                                                 const GValue         *final);
CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_transition_set_interval)
ClutterAnimation *      clutter_animation_bind_interval         (ClutterAnimation     *animation,
                                                                 const gchar          *property_name,
                                                                 ClutterInterval      *interval);
CLUTTER_DEPRECATED_IN_1_12
gboolean                clutter_animation_has_property          (ClutterAnimation     *animation,
                                                                 const gchar          *property_name);
CLUTTER_DEPRECATED_IN_1_12
ClutterAnimation *      clutter_animation_update                (ClutterAnimation     *animation,
                                                                 const gchar          *property_name,
                                                                 const GValue         *final);
CLUTTER_DEPRECATED_IN_1_12
void                    clutter_animation_update_interval       (ClutterAnimation     *animation,
                                                                 const gchar          *property_name,
                                                                 ClutterInterval      *interval);
CLUTTER_DEPRECATED_IN_1_12
void                    clutter_animation_unbind_property       (ClutterAnimation     *animation,
                                                                 const gchar          *property_name);
CLUTTER_DEPRECATED_IN_1_12
ClutterInterval     *   clutter_animation_get_interval          (ClutterAnimation     *animation,
                                                                 const gchar          *property_name);
CLUTTER_DEPRECATED_IN_1_12
void                    clutter_animation_completed             (ClutterAnimation     *animation);

/*
 * ClutterActor API
 */

CLUTTER_DEPRECATED_IN_1_12
ClutterAnimation *      clutter_actor_animate                   (ClutterActor         *actor,
                                                                 gulong                mode,
                                                                 guint                 duration,
                                                                 const gchar          *first_property_name,
                                                                 ...) G_GNUC_NULL_TERMINATED;
CLUTTER_DEPRECATED_IN_1_12
ClutterAnimation *      clutter_actor_animate_with_timeline     (ClutterActor         *actor,
                                                                 gulong                mode,
                                                                 ClutterTimeline      *timeline,
                                                                 const gchar          *first_property_name,
                                                                 ...) G_GNUC_NULL_TERMINATED;
CLUTTER_DEPRECATED_IN_1_12
ClutterAnimation *      clutter_actor_animatev                  (ClutterActor         *actor,
                                                                 gulong                mode,
                                                                 guint                 duration,
                                                                 gint                  n_properties,
                                                                 const gchar * const   properties[],
                                                                 const GValue         *values);
CLUTTER_DEPRECATED_IN_1_12
ClutterAnimation *      clutter_actor_animate_with_timelinev    (ClutterActor         *actor,
                                                                 gulong                mode,
                                                                 ClutterTimeline      *timeline,
                                                                 gint                  n_properties,
                                                                 const gchar * const   properties[],
                                                                 const GValue         *values);
CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_animate_with_timeline)
ClutterAnimation *      clutter_actor_animate_with_alpha        (ClutterActor         *actor,
                                                                 ClutterAlpha         *alpha,
                                                                 const gchar          *first_property_name,
                                                                 ...) G_GNUC_NULL_TERMINATED;
CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_animate_with_timelinev)
ClutterAnimation *      clutter_actor_animate_with_alphav       (ClutterActor         *actor,
                                                                 ClutterAlpha         *alpha,
                                                                 gint                  n_properties,
                                                                 const gchar * const   properties[],
                                                                 const GValue         *values);

CLUTTER_DEPRECATED_IN_1_12
ClutterAnimation *      clutter_actor_get_animation             (ClutterActor         *actor);
CLUTTER_DEPRECATED_IN_1_12
void                    clutter_actor_detach_animation          (ClutterActor         *actor);

G_END_DECLS

#endif /* __CLUTTER_ANIMATION_DEPRECATED_H__ */
