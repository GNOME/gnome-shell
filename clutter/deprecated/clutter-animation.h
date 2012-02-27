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

#ifndef __CLUTTER_ANIMATION_DEPRECATED_H__
#define __CLUTTER_ANIMATION_DEPRECATED_H__

#include <clutter/clutter-animation.h>

G_BEGIN_DECLS

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_animation_set_timeline)
void                    clutter_animation_set_alpha             (ClutterAnimation     *animation,
                                                                 ClutterAlpha         *alpha);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_animation_get_timeline)
ClutterAlpha *          clutter_animation_get_alpha             (ClutterAnimation     *animation);

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

G_END_DECLS

#endif /* __CLUTTER_ANIMATION_DEPRECATED_H__ */
