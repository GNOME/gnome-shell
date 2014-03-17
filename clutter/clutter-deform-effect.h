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

#ifndef __CLUTTER_DEFORM_EFFECT_H__
#define __CLUTTER_DEFORM_EFFECT_H__

#include <cogl/cogl.h>
#include <clutter/clutter-offscreen-effect.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_DEFORM_EFFECT              (clutter_deform_effect_get_type ())
#define CLUTTER_DEFORM_EFFECT(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_DEFORM_EFFECT, ClutterDeformEffect))
#define CLUTTER_IS_DEFORM_EFFECT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_DEFORM_EFFECT))
#define CLUTTER_DEFORM_EFFECT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_DEFORM_EFFECT, ClutterDeformEffectClass))
#define CLUTTER_IS_DEFORM_EFFECT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_DEFORM_EFFECT))
#define CLUTTER_DEFORM_EFFECT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_DEFORM_EFFECT, ClutterDeformEffectClass))

typedef struct _ClutterDeformEffect             ClutterDeformEffect;
typedef struct _ClutterDeformEffectPrivate      ClutterDeformEffectPrivate;
typedef struct _ClutterDeformEffectClass        ClutterDeformEffectClass;

/**
 * ClutterDeformEffect:
 *
 * The <structname>ClutterDeformEffect</structname> structure contains
 * only private data and should be accessed using the provided API
 *
 * Since: 1.4
 */
struct _ClutterDeformEffect
{
  /*< private >*/
  ClutterOffscreenEffect parent_instance;

  ClutterDeformEffectPrivate *priv;
};

/**
 * ClutterDeformEffectClass:
 * @deform_vertex: virtual function; sub-classes should override this
 *   function to compute the deformation of each vertex
 *
 * The <structname>ClutterDeformEffectClass</structname> structure contains
 * only private data
 *
 * Since: 1.4
 */
struct _ClutterDeformEffectClass
{
  /*< private >*/
  ClutterOffscreenEffectClass parent_class;

  /*< public >*/
  void (* deform_vertex) (ClutterDeformEffect *effect,
                          gfloat               width,
                          gfloat               height,
                          CoglTextureVertex   *vertex);

  /*< private >*/
  void (*_clutter_deform1) (void);
  void (*_clutter_deform2) (void);
  void (*_clutter_deform3) (void);
  void (*_clutter_deform4) (void);
  void (*_clutter_deform5) (void);
  void (*_clutter_deform6) (void);
  void (*_clutter_deform7) (void);
};

CLUTTER_AVAILABLE_IN_1_4
GType clutter_deform_effect_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_4
void            clutter_deform_effect_set_back_material (ClutterDeformEffect *effect,
                                                         CoglHandle           material);
CLUTTER_AVAILABLE_IN_1_4
CoglHandle      clutter_deform_effect_get_back_material (ClutterDeformEffect *effect);
CLUTTER_AVAILABLE_IN_1_4
void            clutter_deform_effect_set_n_tiles       (ClutterDeformEffect *effect,
                                                         guint                x_tiles,
                                                         guint                y_tiles);
CLUTTER_AVAILABLE_IN_1_4
void            clutter_deform_effect_get_n_tiles       (ClutterDeformEffect *effect,
                                                         guint               *x_tiles,
                                                         guint               *y_tiles);

CLUTTER_AVAILABLE_IN_1_4
void            clutter_deform_effect_invalidate        (ClutterDeformEffect *effect);

G_END_DECLS

#endif /* __CLUTTER_DEFORM_EFFECT_H__ */
