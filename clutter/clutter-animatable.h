/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009  Intel Corporation.
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

#ifndef __CLUTTER_ANIMATABLE_H__
#define __CLUTTER_ANIMATABLE_H__

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#include <clutter/clutter-animation.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_ANIMATABLE                 (clutter_animatable_get_type ())
#define CLUTTER_ANIMATABLE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_ANIMATABLE, ClutterAnimatable))
#define CLUTTER_IS_ANIMATABLE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_ANIMATABLE))
#define CLUTTER_ANIMATABLE_GET_IFACE(obj)       (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CLUTTER_TYPE_ANIMATABLE, ClutterAnimatableIface))

typedef struct _ClutterAnimatable               ClutterAnimatable; /* dummy typedef */
typedef struct _ClutterAnimatableIface          ClutterAnimatableIface;

/**
 * ClutterAnimatableIface:
 * @animate_property: virtual function for animating a property
 *
 * Base interface for #GObject<!-- -->s that can be animated by a
 * a #ClutterAnimation.
 *
 * Since: 1.0
 */
struct _ClutterAnimatableIface
{
  /*< private >*/
  GTypeInterface parent_iface;

  /*< public >*/
  gboolean (* animate_property) (ClutterAnimatable *animatable,
                                 ClutterAnimation  *animation,
                                 const gchar       *property_name,
                                 const GValue      *initial_value,
                                 const GValue      *final_value,
                                 gdouble            progress,
                                 GValue            *value);
};

GType clutter_animatable_get_type (void) G_GNUC_CONST;

gboolean clutter_animatable_animate_property (ClutterAnimatable *animatable,
                                              ClutterAnimation  *animation,
                                              const gchar       *property_name,
                                              const GValue      *initial_value,
                                              const GValue      *final_value,
                                              gdouble            progress,
                                              GValue            *value);

G_END_DECLS

#endif /* __CLUTTER_ANIMATABLE_H__ */
