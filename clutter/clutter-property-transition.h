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

#ifndef __CLUTTER_PROPERTY_TRANSITION_H__
#define __CLUTTER_PROPERTY_TRANSITION_H__

#include <clutter/clutter-types.h>
#include <clutter/clutter-transition.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_PROPERTY_TRANSITION                (clutter_property_transition_get_type ())
#define CLUTTER_PROPERTY_TRANSITION(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_PROPERTY_TRANSITION, ClutterPropertyTransition))
#define CLUTTER_IS_PROPERTY_TRANSITION(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_PROPERTY_TRANSITION))
#define CLUTTER_PROPERTY_TRANSITION_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_PROPERTY_TRANSITION, ClutterPropertyTransitionClass))
#define CLUTTER_IS_PROPERTY_TRANSITION_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_PROPERTY_TRANSITION))
#define CLUTTER_PROPERTY_TRANSITION_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_PROPERTY_TRANSITION, ClutterPropertyTransitionClass))

typedef struct _ClutterPropertyTransitionPrivate        ClutterPropertyTransitionPrivate;
typedef struct _ClutterPropertyTransitionClass          ClutterPropertyTransitionClass;

/**
 * ClutterPropertyTransition:
 *
 * The #ClutterPropertyTransition structure contains
 * private data and should only be accessed using the provided API.
 *
 * Since: 1.10
 */
struct _ClutterPropertyTransition
{
  /*< private >*/
  ClutterTransition parent_instance;

  ClutterPropertyTransitionPrivate *priv;
};

/**
 * ClutterPropertyTransitionClass:
 *
 * The #ClutterPropertyTransitionClass structure
 * contains private data.
 *
 * Since: 1.10
 */
struct _ClutterPropertyTransitionClass
{
  /*< private >*/
  ClutterTransitionClass parent_class;

  gpointer _padding[8];
};

CLUTTER_AVAILABLE_IN_1_10
GType clutter_property_transition_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_10
ClutterTransition *     clutter_property_transition_new                 (const char                *property_name);
CLUTTER_AVAILABLE_IN_1_10
void                    clutter_property_transition_set_property_name   (ClutterPropertyTransition *transition,
                                                                         const char                *property_name);
CLUTTER_AVAILABLE_IN_1_10
const char *            clutter_property_transition_get_property_name   (ClutterPropertyTransition *transition);

G_END_DECLS

#endif /* __CLUTTER_PROPERTY_TRANSITION_H__ */
