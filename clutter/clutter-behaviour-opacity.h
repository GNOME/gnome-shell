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

#ifndef __CLUTTER_BEHAVIOUR_OPACITY_H__
#define __CLUTTER_BEHAVIOUR_OPACITY_H__

#include <clutter/clutter-alpha.h>
#include <clutter/clutter-behaviour.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_BEHAVIOUR_OPACITY (clutter_behaviour_opacity_get_type ())

#define CLUTTER_BEHAVIOUR_OPACITY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_OPACITY, ClutterBehaviourOpacity))

#define CLUTTER_BEHAVIOUR_OPACITY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_BEHAVIOUR_OPACITY, ClutterBehaviourOpacityClass))

#define CLUTTER_IS_BEHAVIOUR_OPACITY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_OPACITY))

#define CLUTTER_IS_BEHAVIOUR_OPACITY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_BEHAVIOUR_OPACITY))

#define CLUTTER_BEHAVIOUR_OPACITY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_BEHAVIOUR_OPACITY, ClutterBehaviourOpacityClass))

typedef struct _ClutterBehaviourOpacity        ClutterBehaviourOpacity;
typedef struct _ClutterBehaviourOpacityPrivate ClutterBehaviourOpacityPrivate;
typedef struct _ClutterBehaviourOpacityClass   ClutterBehaviourOpacityClass;

/**
 * ClutterBehaviourOpacity:
 *
 * The #ClutterBehaviourOpacity structure contains only private data and
 * should be accessed using the provided API
 *
 * Since: 0.2
 */
struct _ClutterBehaviourOpacity
{
  /*< private >*/
  ClutterBehaviour             parent;
  ClutterBehaviourOpacityPrivate *priv;
};

/**
 * ClutterBehaviourOpacityClass:
 *
 * The #ClutterBehaviourOpacityClas structure contains only private data
 *
 * Since: 0.2
 */
struct _ClutterBehaviourOpacityClass
{
  /*< private >*/
  ClutterBehaviourClass   parent_class;
};

GType clutter_behaviour_opacity_get_type (void) G_GNUC_CONST;

ClutterBehaviour *clutter_behaviour_opacity_new (ClutterAlpha *alpha,
                                                 guint8        opacity_start,
                                                 guint8        opacity_end);

void clutter_behaviour_opacity_set_bounds (ClutterBehaviourOpacity *behaviour,
                                           guint8                   opacity_start,
                                           guint8                   opacity_end);
void clutter_behaviour_opacity_get_bounds (ClutterBehaviourOpacity *behaviour,
                                           guint8                  *opacity_start,
                                           guint8                  *opacity_end);

G_END_DECLS


#endif /* __CLUTTER_BEHAVIOUR_OPACITY_H__ */
