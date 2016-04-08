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
 *
 * Deprecated: 1.12
 */
typedef struct _ClutterAnimatorKey     ClutterAnimatorKey;

/**
 * ClutterAnimator:
 *
 * The #ClutterAnimator structure contains only private data and
 * should be accessed using the provided API
 *
 * Since: 1.2
 *
 * Deprecated: 1.12
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
 *
 * Deprecated: 1.12
 */
struct _ClutterAnimatorClass
{
  /*< private >*/
  GObjectClass parent_class;

  /* padding for future expansion */
  gpointer _padding_dummy[16];
};

CLUTTER_DEPRECATED_IN_1_12
GType                clutter_animator_get_type                   (void) G_GNUC_CONST;

CLUTTER_DEPRECATED_IN_1_12
ClutterAnimator *    clutter_animator_new                        (void);
CLUTTER_DEPRECATED_IN_1_12
ClutterAnimator *    clutter_animator_set_key                    (ClutterAnimator      *animator,
                                                                  GObject              *object,
                                                                  const gchar          *property_name,
                                                                  guint                 mode,
                                                                  gdouble               progress,
                                                                  const GValue         *value);
CLUTTER_DEPRECATED_IN_1_12
void                 clutter_animator_set                        (ClutterAnimator      *animator,
                                                                  gpointer              first_object,
                                                                  const gchar          *first_property_name,
                                                                  guint                 first_mode,
                                                                  gdouble               first_progress,
                                                                  ...) G_GNUC_NULL_TERMINATED;
CLUTTER_DEPRECATED_IN_1_12
GList              * clutter_animator_get_keys                   (ClutterAnimator      *animator,
                                                                  GObject              *object,
                                                                  const gchar          *property_name,
                                                                  gdouble               progress);

CLUTTER_DEPRECATED_IN_1_12
void                 clutter_animator_remove_key                 (ClutterAnimator      *animator,
                                                                  GObject              *object, 
                                                                  const gchar          *property_name,
                                                                  gdouble               progress);

CLUTTER_DEPRECATED_IN_1_12
ClutterTimeline *    clutter_animator_start                      (ClutterAnimator      *animator);

CLUTTER_DEPRECATED_IN_1_12
gboolean             clutter_animator_compute_value              (ClutterAnimator      *animator,
                                                                  GObject              *object,
                                                                  const gchar          *property_name,
                                                                  gdouble               progress,
                                                                  GValue               *value);

CLUTTER_DEPRECATED_IN_1_12
ClutterTimeline *    clutter_animator_get_timeline               (ClutterAnimator      *animator);
CLUTTER_DEPRECATED_IN_1_12
void                 clutter_animator_set_timeline               (ClutterAnimator      *animator,
                                                                  ClutterTimeline      *timeline);
CLUTTER_DEPRECATED_IN_1_12
guint                clutter_animator_get_duration               (ClutterAnimator      *animator);
CLUTTER_DEPRECATED_IN_1_12
void                 clutter_animator_set_duration               (ClutterAnimator      *animator,
                                                                  guint                 duration);

CLUTTER_DEPRECATED_IN_1_12
gboolean             clutter_animator_property_get_ease_in       (ClutterAnimator      *animator,
                                                                  GObject              *object,
                                                                  const gchar          *property_name);
CLUTTER_DEPRECATED_IN_1_12
void                 clutter_animator_property_set_ease_in       (ClutterAnimator      *animator,
                                                                  GObject              *object,
                                                                  const gchar          *property_name,
                                                                  gboolean              ease_in);

CLUTTER_DEPRECATED_IN_1_12
ClutterInterpolation clutter_animator_property_get_interpolation (ClutterAnimator      *animator,
                                                                  GObject              *object,
                                                                  const gchar          *property_name);
CLUTTER_DEPRECATED_IN_1_12
void                 clutter_animator_property_set_interpolation (ClutterAnimator      *animator,
                                                                  GObject              *object,
                                                                  const gchar          *property_name,
                                                                  ClutterInterpolation  interpolation);

CLUTTER_DEPRECATED_IN_1_12
GType           clutter_animator_key_get_type           (void) G_GNUC_CONST;
CLUTTER_DEPRECATED_IN_1_12
GObject *       clutter_animator_key_get_object         (const ClutterAnimatorKey *key);
CLUTTER_DEPRECATED_IN_1_12
const gchar *   clutter_animator_key_get_property_name  (const ClutterAnimatorKey *key);
CLUTTER_DEPRECATED_IN_1_12
GType           clutter_animator_key_get_property_type  (const ClutterAnimatorKey *key);
CLUTTER_DEPRECATED_IN_1_12
gulong          clutter_animator_key_get_mode           (const ClutterAnimatorKey *key);
CLUTTER_DEPRECATED_IN_1_12
gdouble         clutter_animator_key_get_progress       (const ClutterAnimatorKey *key);
CLUTTER_DEPRECATED_IN_1_12
gboolean        clutter_animator_key_get_value          (const ClutterAnimatorKey *key,
                                                         GValue                   *value);

G_END_DECLS

#endif /* __CLUTTER_ANIMATOR_H__ */
