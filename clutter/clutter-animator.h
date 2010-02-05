/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010 Intel Corporation
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
 *   Øyvind Kolås <pippin@linux.intel.com>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_ANIMATOR_H__
#define __CLUTTER_ANIMATOR_H__

#include <clutter/clutter-types.h>
#include <clutter/clutter-timeline.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ANIMATOR                   (clutter_animator_get_type ())
#define CLUTTER_TYPE_ANIMATOR_KEY               (clutter_animator_key_get_type ())

#define CLUTTER_ANIMATOR(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ANIMATOR, ClutterAnimator))
#define CLUTTER_ANIMATOR_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_ANIMATOR, ClutterAnimatorClass))
#define CLUTTER_IS_ANIMATOR(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ANIMATOR))
#define CLUTTER_IS_ANIMATOR_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_ANIMATOR))
#define CLUTTER_ANIMATOR_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_ANIMATOR, ClutterAnimatorClass))

/* ClutterAnimator is typedef in clutter-types.h */

typedef struct _ClutterAnimatorClass   ClutterAnimatorClass;
typedef struct _ClutterAnimatorPrivate ClutterAnimatorPrivate;

/**
 * ClutterAnimatorKey:
 *
 * A key frame inside a #ClutterAnimator
 *
 * Since: 1.2
 */
typedef struct _ClutterAnimatorKey     ClutterAnimatorKey;

/**
 * ClutterInterpolation:
 * @CLUTTER_INTERPOLATION_LINEAR:
 * @CLUTTER_INTERPOLATION_CUBIC:
 *
 * The mode of interpolation between key frames
 *
 * Since: 1.2
 */
typedef enum {
  CLUTTER_INTERPOLATION_LINEAR,
  CLUTTER_INTERPOLATION_CUBIC
} ClutterInterpolation;

/**
 * ClutterAnimator:
 *
 * The #ClutterAnimator structure contains only private data and
 * should be accessed using the provided API
 *
 * Since: 1.2
 */
struct _ClutterAnimator
{
  /*< private >*/
  GObject parent_instance;

  ClutterAnimatorPrivate *priv;
};

/**
 * ClutterAnimatorClass:
 *
 * The #ClutterAnimatorClass structure contains only private data
 *
 * Since: 1.2
 */
struct _ClutterAnimatorClass
{
  /*< private >*/
  GObjectClass parent_class;

  /* padding for future expansion */
  gpointer _padding_dummy[16];
};

GType                clutter_animator_get_type                   (void) G_GNUC_CONST;

ClutterAnimator *    clutter_animator_new                        (void);
ClutterAnimator *    clutter_animator_set_key                    (ClutterAnimator      *animator,
                                                                  GObject              *object,
                                                                  const gchar          *property_name,
                                                                  guint                 mode,
                                                                  gdouble               progress,
                                                                  const GValue         *value);
void                 clutter_animator_set                        (ClutterAnimator      *animator,
                                                                  gpointer              first_object,
                                                                  const gchar          *first_property_name,
                                                                  guint                 first_mode,
                                                                  gdouble               first_progress,
                                                                  ...);
GList              * clutter_animator_get_keys                   (ClutterAnimator      *animator,
                                                                  GObject              *object,
                                                                  const gchar          *property_name,
                                                                  gdouble               progress);

void                 clutter_animator_remove_key                 (ClutterAnimator      *animator,
                                                                  GObject              *object, 
                                                                  const gchar          *property_name,
                                                                  gdouble               progress);

ClutterTimeline *    clutter_animator_run                        (ClutterAnimator      *animator);
ClutterTimeline *    clutter_animator_get_timeline               (ClutterAnimator      *animator);
void                 clutter_animator_set_timeline               (ClutterAnimator      *animator,
                                                                  ClutterTimeline      *timeline);
guint                clutter_animator_get_duration               (ClutterAnimator      *animator);
void                 clutter_animator_set_duration               (ClutterAnimator      *animator,
                                                                  guint                 duration);

gboolean             clutter_animator_property_get_ease_in       (ClutterAnimator      *animator,
                                                                  GObject              *object,
                                                                  const gchar          *property_name);
void                 clutter_animator_property_set_ease_in       (ClutterAnimator      *animator,
                                                                  GObject              *object,
                                                                  const gchar          *property_name,
                                                                  gboolean              ease_in);

ClutterInterpolation clutter_animator_property_get_interpolation (ClutterAnimator      *animator,
                                                                  GObject              *object,
                                                                  const gchar          *property_name,
                                                                  ClutterInterpolation  interpolation);
void                 clutter_animator_property_set_interpolation (ClutterAnimator      *animator,
                                                                  GObject              *object,
                                                                  const gchar          *property_name,
                                                                  ClutterInterpolation  interpolation);

GType                 clutter_animator_key_get_type          (void) G_GNUC_CONST;
GObject *             clutter_animator_key_get_object        (ClutterAnimatorKey  *animator_key);
G_CONST_RETURN gchar *clutter_animator_key_get_property_name (ClutterAnimatorKey  *animator_key);
gulong                clutter_animator_key_get_mode          (ClutterAnimatorKey  *animator_key);
gdouble               clutter_animator_key_get_progress      (ClutterAnimatorKey  *animator_key);
void                  clutter_animator_key_get_value         (ClutterAnimatorKey  *animator_key,
                                                              GValue              *value);

G_END_DECLS

#endif /* __CLUTTER_ANIMATOR_H__ */
