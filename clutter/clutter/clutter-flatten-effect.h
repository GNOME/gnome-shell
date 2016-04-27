/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011  Intel Corporation.
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
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifndef __CLUTTER_FLATTEN_EFFECT_H__
#define __CLUTTER_FLATTEN_EFFECT_H__

#include <clutter/clutter-offscreen-effect.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_FLATTEN_EFFECT                                     \
  (_clutter_flatten_effect_get_type())
#define CLUTTER_FLATTEN_EFFECT(obj)                                     \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               CLUTTER_TYPE_FLATTEN_EFFECT,             \
                               ClutterFlattenEffect))
#define CLUTTER_FLATTEN_EFFECT_CLASS(klass)                             \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            CLUTTER_TYPE_FLATTEN_EFFECT,                \
                            ClutterFlattenEffectClass))
#define CLUTTER_IS_FLATTEN_EFFECT(obj)                                  \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                   \
                               CLUTTER_TYPE_FLATTEN_EFFECT))
#define CLUTTER_IS_FLATTEN_EFFECT_CLASS(klass)                          \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                    \
                            CLUTTER_TYPE_FLATTEN_EFFECT))
#define CLUTTER_FLATTEN_EFFECT_GET_CLASS(obj)                           \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              CLUTTER_FLATTEN_EFFECT,                   \
                              ClutterFlattenEffectClass))

typedef struct _ClutterFlattenEffect        ClutterFlattenEffect;
typedef struct _ClutterFlattenEffectClass   ClutterFlattenEffectClass;
typedef struct _ClutterFlattenEffectPrivate ClutterFlattenEffectPrivate;

struct _ClutterFlattenEffectClass
{
  ClutterOffscreenEffectClass parent_class;
};

struct _ClutterFlattenEffect
{
  ClutterOffscreenEffect parent;
};

GType _clutter_flatten_effect_get_type (void) G_GNUC_CONST;

ClutterEffect *_clutter_flatten_effect_new (void);

G_END_DECLS

#endif /* __CLUTTER_FLATTEN_EFFECT_H__ */
