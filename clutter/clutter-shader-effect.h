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

#ifndef __CLUTTER_SHADER_EFFECT_H__
#define __CLUTTER_SHADER_EFFECT_H__

#include <clutter/clutter-offscreen-effect.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_SHADER_EFFECT              (clutter_shader_effect_get_type ())
#define CLUTTER_SHADER_EFFECT(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_SHADER_EFFECT, ClutterShaderEffect))
#define CLUTTER_IS_SHADER_EFFECT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_SHADER_EFFECT))
#define CLUTTER_SHADER_EFFECT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_SHADER_EFFECT, ClutterShaderEffectClass))
#define CLUTTER_IS_SHADER_EFFECT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_SHADER_EFFECT))
#define CLUTTER_SHADER_EFFECT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_SHADER_EFFECT, ClutterShaderEffectClass))

typedef struct _ClutterShaderEffect             ClutterShaderEffect;
typedef struct _ClutterShaderEffectPrivate      ClutterShaderEffectPrivate;
typedef struct _ClutterShaderEffectClass        ClutterShaderEffectClass;

/**
 * ClutterShaderEffect:
 *
 * The #ClutterShaderEffect structure contains
 * only private data and should be accessed using the provided API
 *
 * Since: 1.4
 */
struct _ClutterShaderEffect
{
  /*< private >*/
  ClutterOffscreenEffect parent_instance;

  ClutterShaderEffectPrivate *priv;
};

/**
 * ClutterShaderEffectClass:
 * @get_static_shader_source: Returns the GLSL source code to use for
 *  instances of this shader effect. Note that this function is only
 *  called once per subclass of #ClutterShaderEffect regardless of how
 *  many instances are used. It is expected that subclasses will return
 *  a copy of a static string from this function.
 *
 * The #ClutterShaderEffectClass structure contains
 * only private data
 *
 * Since: 1.4
 */
struct _ClutterShaderEffectClass
{
  /*< private >*/
  ClutterOffscreenEffectClass parent_class;

  /*< public >*/
  gchar * (* get_static_shader_source) (ClutterShaderEffect *effect);

  /*< private >*/
  /* padding */
  void (*_clutter_shader1) (void);
  void (*_clutter_shader2) (void);
  void (*_clutter_shader3) (void);
  void (*_clutter_shader4) (void);
  void (*_clutter_shader5) (void);
};

CLUTTER_AVAILABLE_IN_1_4
GType clutter_shader_effect_get_type (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_4
ClutterEffect * clutter_shader_effect_new               (ClutterShaderType    shader_type);

CLUTTER_AVAILABLE_IN_1_4
gboolean        clutter_shader_effect_set_shader_source (ClutterShaderEffect *effect,
                                                         const gchar         *source);

CLUTTER_AVAILABLE_IN_1_4
void            clutter_shader_effect_set_uniform       (ClutterShaderEffect *effect,
                                                         const gchar         *name,
                                                         GType                gtype,
                                                         gsize                n_values,
                                                         ...);
CLUTTER_AVAILABLE_IN_1_4
void            clutter_shader_effect_set_uniform_value (ClutterShaderEffect *effect,
                                                         const gchar         *name,
                                                         const GValue        *value);

CLUTTER_AVAILABLE_IN_1_4
CoglHandle      clutter_shader_effect_get_shader        (ClutterShaderEffect *effect);
CLUTTER_AVAILABLE_IN_1_4
CoglHandle      clutter_shader_effect_get_program       (ClutterShaderEffect *effect);

G_END_DECLS

#endif /* __CLUTTER_SHADER_EFFECT_H__ */
