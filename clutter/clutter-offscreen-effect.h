/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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

#ifndef __CLUTTER_OFFSCREEN_EFFECT_H__
#define __CLUTTER_OFFSCREEN_EFFECT_H__

#include <cogl/cogl.h>
#include <clutter/clutter-effect.h>
#include <clutter/clutter-cogl-compat.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_OFFSCREEN_EFFECT           (clutter_offscreen_effect_get_type ())
#define CLUTTER_OFFSCREEN_EFFECT(o)             (G_TYPE_CHECK_INSTANCE_CAST ((o), CLUTTER_TYPE_OFFSCREEN_EFFECT, ClutterOffscreenEffect))
#define CLUTTER_IS_OFFSCREEN_EFFECT(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), CLUTTER_TYPE_OFFSCREEN_EFFECT))
#define CLUTTER_OFFSCREEN_EFFECT_CLASS(k)       (G_TYPE_CHECK_CLASS_CAST ((k), CLUTTER_TYPE_OFFSCREEN_EFFECT, ClutterOffscreenEffectClass))
#define CLUTTER_IS_OFFSCREEN_EFFECT_CLASS(k)    (G_TYPE_CHECK_CLASS_TYPE ((k), CLUTTER_TYPE_OFFSCREEN_EFFECT))
#define CLUTTER_OFFSCREEN_EFFECT_GET_CLASS(o)   (G_TYPE_INSTANCE_GET_CLASS ((o), CLUTTER_TYPE_OFFSCREEN_EFFECT, ClutterOffscreenEffectClass))

typedef struct _ClutterOffscreenEffect          ClutterOffscreenEffect;
typedef struct _ClutterOffscreenEffectPrivate   ClutterOffscreenEffectPrivate;
typedef struct _ClutterOffscreenEffectClass     ClutterOffscreenEffectClass;

/**
 * ClutterOffscreenEffect:
 *
 * The #ClutterOffscreenEffect structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 1.4
 */
struct _ClutterOffscreenEffect
{
  /*< private >*/
  ClutterEffect parent_instance;

  ClutterOffscreenEffectPrivate *priv;
};

/**
 * ClutterOffscreenEffectClass:
 * @create_texture: virtual function
 * @paint_target: virtual function
 *
 * The #ClutterOffscreenEffectClass structure contains only private data
 *
 * Since: 1.4
 */
struct _ClutterOffscreenEffectClass
{
  /*< private >*/
  ClutterEffectClass parent_class;

  /*< public >*/
  CoglHandle (* create_texture) (ClutterOffscreenEffect *effect,
                                 gfloat                  width,
                                 gfloat                  height);
  void       (* paint_target)   (ClutterOffscreenEffect *effect);

  /*< private >*/
  void (* _clutter_offscreen1) (void);
  void (* _clutter_offscreen2) (void);
  void (* _clutter_offscreen3) (void);
  void (* _clutter_offscreen4) (void);
  void (* _clutter_offscreen5) (void);
  void (* _clutter_offscreen6) (void);
  void (* _clutter_offscreen7) (void);
};

CLUTTER_AVAILABLE_IN_1_4
GType clutter_offscreen_effect_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_4
CoglMaterial *  clutter_offscreen_effect_get_target             (ClutterOffscreenEffect *effect);

CLUTTER_AVAILABLE_IN_1_10
CoglHandle      clutter_offscreen_effect_get_texture            (ClutterOffscreenEffect *effect);

CLUTTER_AVAILABLE_IN_1_4
void            clutter_offscreen_effect_paint_target           (ClutterOffscreenEffect *effect);
CLUTTER_AVAILABLE_IN_1_4
CoglHandle      clutter_offscreen_effect_create_texture         (ClutterOffscreenEffect *effect,
                                                                 gfloat                  width,
                                                                 gfloat                  height);

CLUTTER_DEPRECATED_IN_1_14_FOR (clutter_offscreen_effect_get_target_rect)
gboolean        clutter_offscreen_effect_get_target_size        (ClutterOffscreenEffect *effect,
                                                                 gfloat                 *width,
                                                                 gfloat                 *height);

CLUTTER_AVAILABLE_IN_1_14
gboolean        clutter_offscreen_effect_get_target_rect        (ClutterOffscreenEffect *effect,
                                                                 ClutterRect            *rect);

G_END_DECLS

#endif /* __CLUTTER_OFFSCREEN_EFFECT_H__ */
