/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Øyvind Kolås   <pippin@o-hand.com>
 *
 * Copyright (C) 2007 OpenedHand
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

#ifndef __CLUTTER_SHADER_H__
#define __CLUTTER_SHADER_H__

#include <clutter/clutter-types.h>
#include <clutter/clutter-shader-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_SHADER         (clutter_shader_get_type ())
#define CLUTTER_SHADER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CLUTTER_TYPE_SHADER, ClutterShader))
#define CLUTTER_SHADER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CLUTTER_TYPE_SHADER, ClutterShaderClass))
#define CLUTTER_IS_SHADER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CLUTTER_TYPE_SHADER))
#define CLUTTER_IS_SHADER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CLUTTER_TYPE_SHADER))
#define CLUTTER_SHADER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CLUTTER_TYPE_SHADER, ClutterShaderClass))

/**
 * CLUTTER_SHADER_ERROR:
 *
 * Error domain for #ClutterShader errors
 *
 * Since: 0.6
 *
 * Deprecated: 1.8
 */
#define CLUTTER_SHADER_ERROR        (clutter_shader_error_quark ())

/**
 * ClutterShaderError:
 * @CLUTTER_SHADER_ERROR_NO_ASM: No ASM shaders support
 * @CLUTTER_SHADER_ERROR_NO_GLSL: No GLSL shaders support
 * @CLUTTER_SHADER_ERROR_COMPILE: Compilation error
 *
 * #ClutterShader error enumeration
 *
 * Since: 0.6
 *
 * Deprecated: 1.8
 */
typedef enum {
  CLUTTER_SHADER_ERROR_NO_ASM,
  CLUTTER_SHADER_ERROR_NO_GLSL,
  CLUTTER_SHADER_ERROR_COMPILE
} ClutterShaderError;

typedef struct _ClutterShaderPrivate ClutterShaderPrivate;
typedef struct _ClutterShaderClass   ClutterShaderClass;

/**
 * ClutterShader:
 *
 * The #ClutterShader structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 0.6
 *
 * Deprecated: 1.8: Use #ClutterShaderEffect instead
 */
struct _ClutterShader
{
  /*< private >*/
  GObject               parent;
  ClutterShaderPrivate *priv;
};

/**
 * ClutterShaderClass:
 *
 * The #ClutterShaderClass structure contains only private data
 *
 * Since: 0.6
 *
 * Deprecated: 1.8: Use #ClutterShaderEffectClass instead
 */
struct _ClutterShaderClass
{
  /*< private >*/
  GObjectClass parent_class;
};

CLUTTER_DEPRECATED_IN_1_8
GQuark clutter_shader_error_quark (void);

CLUTTER_DEPRECATED_IN_1_8
GType clutter_shader_get_type (void) G_GNUC_CONST;

CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
ClutterShader *       clutter_shader_new                      (void);

CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
void                  clutter_shader_set_is_enabled           (ClutterShader      *shader,
                                                               gboolean            enabled);
CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
gboolean              clutter_shader_get_is_enabled           (ClutterShader      *shader);

CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
gboolean              clutter_shader_compile                  (ClutterShader      *shader,
                                                               GError            **error);
CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
void                  clutter_shader_release                  (ClutterShader      *shader);
CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
gboolean              clutter_shader_is_compiled              (ClutterShader      *shader);

CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
void                  clutter_shader_set_vertex_source        (ClutterShader      *shader,
                                                               const gchar        *data,
                                                               gssize              length);
CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
void                  clutter_shader_set_fragment_source      (ClutterShader      *shader,
                                                               const gchar        *data,
                                                               gssize              length);
CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
const gchar *         clutter_shader_get_vertex_source        (ClutterShader      *shader);
CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
const gchar *         clutter_shader_get_fragment_source      (ClutterShader      *shader);

CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
void                  clutter_shader_set_uniform              (ClutterShader      *shader,
                                                               const gchar        *name,
                                                               const GValue       *value);

CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
CoglHandle            clutter_shader_get_cogl_program         (ClutterShader      *shader);
CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
CoglHandle            clutter_shader_get_cogl_fragment_shader (ClutterShader      *shader);
CLUTTER_DEPRECATED_IN_1_8_FOR(ClutterShaderEffect)
CoglHandle            clutter_shader_get_cogl_vertex_shader   (ClutterShader      *shader);

/* ClutterActor methods */

CLUTTER_DEPRECATED_IN_1_8_FOR(clutter_actor_add_effect)
gboolean              clutter_actor_set_shader                (ClutterActor       *self,
                                                               ClutterShader      *shader);

CLUTTER_DEPRECATED_IN_1_8_FOR(clutter_actor_get_effect)
ClutterShader *       clutter_actor_get_shader                (ClutterActor       *self);

CLUTTER_DEPRECATED_IN_1_8_FOR(clutter_shader_effect_set_uniform_value)
void                  clutter_actor_set_shader_param          (ClutterActor       *self,
                                                               const gchar        *param,
                                                               const GValue       *value);

CLUTTER_DEPRECATED_IN_1_8_FOR(clutter_shader_effect_set_uniform)
void                  clutter_actor_set_shader_param_int      (ClutterActor       *self,
                                                               const gchar        *param,
                                                               gint                value);

CLUTTER_DEPRECATED_IN_1_8_FOR(clutter_shader_effect_set_uniform)
void                  clutter_actor_set_shader_param_float    (ClutterActor       *self,
                                                               const gchar        *param,
                                                               gfloat              value);

G_END_DECLS

#endif /* __CLUTTER_SHADER_H__ */
