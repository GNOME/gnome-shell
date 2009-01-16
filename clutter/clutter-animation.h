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

#include <clutter/clutter-actor.h>
#include <clutter/clutter-alpha.h>
#include <clutter/clutter-interval.h>
#include <clutter/clutter-timeline.h>
#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ANIMATION                  (clutter_animation_get_type ())
#define CLUTTER_ANIMATION(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ANIMATION, ClutterAnimation))
#define CLUTTER_IS_ANIMATION(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ANIMATION))
#define CLUTTER_ANIMATION_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_ANIMATION, ClutterAnimationClass))
#define CLUTTER_IS_ANIMATION_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_ANIMATION))
#define CLUTTER_ANIMATION_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_ANIMATION, ClutterAnimationClass))

typedef struct _ClutterAnimation                ClutterAnimation;
typedef struct _ClutterAnimationPrivate         ClutterAnimationPrivate;
typedef struct _ClutterAnimationClass           ClutterAnimationClass;

/**
 * ClutterAnimation:
 *
 * The #ClutterAnimation structure contains only private data and should
 * be accessed using the provided functions.
 *
 * Since: 1.0
 */
struct _ClutterAnimation
{
  /*< private >*/
  GInitiallyUnowned parent_instance;

  ClutterAnimationPrivate *priv;
};

/**
 * ClutterAnimationClass:
 * @completed: class handler for the #ClutterAnimation::completed signal
 *
 * The #ClutterAnimationClass structure contains only private data and
 * should be accessed using the provided functions.
 *
 * Since: 1.0
 */
struct _ClutterAnimationClass
{
  /*< private >*/
  GInitiallyUnownedClass parent_class;

  /*< public >*/
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

GType                clutter_animation_get_type        (void) G_GNUC_CONST;

ClutterAnimation *   clutter_animation_new             (void);

void                 clutter_animation_set_object      (ClutterAnimation     *animation,
                                                        GObject              *object);
GObject *            clutter_animation_get_object      (ClutterAnimation     *animation);
void                 clutter_animation_set_mode        (ClutterAnimation     *animation,
                                                        gulong                mode);
gulong               clutter_animation_get_mode        (ClutterAnimation     *animation);
void                 clutter_animation_set_duration    (ClutterAnimation     *animation,
                                                        gint                  msecs);
guint                clutter_animation_get_duration    (ClutterAnimation     *animation);
void                 clutter_animation_set_loop        (ClutterAnimation     *animation,
                                                        gboolean              loop);
gboolean             clutter_animation_get_loop        (ClutterAnimation     *animation);
void                 clutter_animation_set_timeline    (ClutterAnimation     *animation,
                                                        ClutterTimeline      *timeline);
ClutterTimeline *    clutter_animation_get_timeline    (ClutterAnimation     *animation);
void                 clutter_animation_set_alpha       (ClutterAnimation     *animation,
                                                        ClutterAlpha         *alpha);
ClutterAlpha *       clutter_animation_get_alpha       (ClutterAnimation     *animation);

void                 clutter_animation_bind_property   (ClutterAnimation     *animation,
                                                        const gchar          *property_name,
                                                        ClutterInterval      *interval);
gboolean             clutter_animation_has_property    (ClutterAnimation     *animation,
                                                        const gchar          *property_name);
void                 clutter_animation_update_property (ClutterAnimation     *animation,
                                                        const gchar          *property_name,
                                                        ClutterInterval      *interval);
void                 clutter_animation_unbind_property (ClutterAnimation     *animation,
                                                        const gchar          *property_name);
ClutterInterval     *clutter_animation_get_interval    (ClutterAnimation     *animation,
                                                        const gchar          *property_name);

ClutterAnimation *   clutter_actor_animate               (ClutterActor         *actor,
                                                          gulong                mode,
                                                          guint                 duration,
                                                          const gchar          *first_property_name,
                                                          ...) G_GNUC_NULL_TERMINATED;
ClutterAnimation *   clutter_actor_animate_with_timeline (ClutterActor         *actor,
                                                          gulong                mode,
                                                          ClutterTimeline      *timeline,
                                                          const gchar          *first_property_name,
                                                          ...) G_GNUC_NULL_TERMINATED;
ClutterAnimation *   clutter_actor_animate_with_alpha    (ClutterActor         *actor,
                                                          ClutterAlpha         *alpha,
                                                          const gchar          *first_property_name,
                                                          ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /* __CLUTTER_ANIMATION_H__ */
