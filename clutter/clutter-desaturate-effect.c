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

/**
 * SECTION:clutter-desaturate-effect
 * @short_description: A desaturation effect
 * @see_also: #ClutterEffect, #ClutterOffscreenEffect
 *
 * #ClutterDesaturateEffect is a sub-class of #ClutterEffect that
 * desaturates the color of an actor and its contents. The strenght
 * of the desaturation effect is controllable and animatable through
 * the #ClutterDesaturateEffect:factor property.
 *
 * #ClutterDesaturateEffect is available since Clutter 1.4
 */

#define CLUTTER_DESATURATE_EFFECT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_DESATURATE_EFFECT, ClutterDesaturateEffectClass))
#define CLUTTER_IS_DESATURATE_EFFECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_DESATURATE_EFFECT))
#define CLUTTER_DESATURATE_EFFECT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_DESATURATE_EFFECT, ClutterDesaturateEffectClass))

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "clutter-desaturate-effect.h"

#include "cogl/cogl.h"

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-offscreen-effect.h"
#include "clutter-private.h"

struct _ClutterDesaturateEffect
{
  ClutterOffscreenEffect parent_instance;

  /* the desaturation factor, also known as "strength" */
  gdouble factor;

  CoglHandle shader;
  CoglHandle program;

  gint tex_uniform;
  gint factor_uniform;

  guint is_compiled : 1;
};

struct _ClutterDesaturateEffectClass
{
  ClutterOffscreenEffectClass parent_class;
};

/* the magic gray vec3 has been taken from the NTSC conversion weights
 * as defined by:
 *
 *   "OpenGL Superbible, 4th edition"
 *   -- Richard S. Wright Jr, Benjamin Lipchak, Nicholas Haemel
 *   Addison-Wesley
 */
static const gchar *desaturate_glsl_shader =
"uniform sampler2D tex;\n"
"uniform float factor;\n"
"\n"
"vec3 desaturate (const vec3 color, const float desaturation)\n"
"{\n"
"  const vec3 gray_conv = vec3 (0.299, 0.587, 0.114);\n"
"  vec3 gray = vec3 (dot (gray_conv, color));\n"
"  return vec3 (mix (color.rgb, gray, desaturation));\n"
"}\n"
"\n"
"void main ()\n"
"{\n"
"  vec4 color = cogl_color_in * texture2D (tex, vec2 (cogl_tex_coord_in[0].xy));\n"
"  color.rgb = desaturate (color.rgb, factor);\n"
"  cogl_color_out = color;\n"
"}\n";

enum
{
  PROP_0,

  PROP_FACTOR,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE (ClutterDesaturateEffect,
               clutter_desaturate_effect,
               CLUTTER_TYPE_OFFSCREEN_EFFECT);

static gboolean
clutter_desaturate_effect_pre_paint (ClutterEffect *effect)
{
  ClutterDesaturateEffect *self = CLUTTER_DESATURATE_EFFECT (effect);
  ClutterEffectClass *parent_class;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  if (!clutter_feature_available (CLUTTER_FEATURE_SHADERS_GLSL))
    {
      /* if we don't have support for GLSL shaders then we
       * forcibly disable the ActorMeta
       */
      g_warning ("Unable to use the ShaderEffect: the graphics hardware "
                 "or the current GL driver does not implement support "
                 "for the GLSL shading language.");
      clutter_actor_meta_set_enabled (CLUTTER_ACTOR_META (effect), FALSE);
      return FALSE;
    }

  if (self->shader == COGL_INVALID_HANDLE)
    {
      self->shader = cogl_create_shader (COGL_SHADER_TYPE_FRAGMENT);
      cogl_shader_source (self->shader, desaturate_glsl_shader);

      self->is_compiled = FALSE;
      self->tex_uniform = -1;
      self->factor_uniform = -1;
    }

  if (self->program == COGL_INVALID_HANDLE)
    self->program = cogl_create_program ();

  if (!self->is_compiled)
    {
      g_assert (self->shader != COGL_INVALID_HANDLE);
      g_assert (self->program != COGL_INVALID_HANDLE);

      cogl_shader_compile (self->shader);
      if (!cogl_shader_is_compiled (self->shader))
        {
          gchar *log_buf = cogl_shader_get_info_log (self->shader);

          g_warning (G_STRLOC ": Unable to compile the desaturate shader: %s",
                     log_buf);
          g_free (log_buf);

          cogl_handle_unref (self->shader);
          cogl_handle_unref (self->program);

          self->shader = COGL_INVALID_HANDLE;
          self->program = COGL_INVALID_HANDLE;
        }
      else
        {
          cogl_program_attach_shader (self->program, self->shader);
          cogl_program_link (self->program);

          cogl_handle_unref (self->shader);

          self->is_compiled = TRUE;

          self->tex_uniform =
            cogl_program_get_uniform_location (self->program, "tex");
          self->factor_uniform =
            cogl_program_get_uniform_location (self->program, "factor");
        }
    }

  parent_class = CLUTTER_EFFECT_CLASS (clutter_desaturate_effect_parent_class);
  return parent_class->pre_paint (effect);
}

static void
clutter_desaturate_effect_paint_target (ClutterOffscreenEffect *effect)
{
  ClutterDesaturateEffect *self = CLUTTER_DESATURATE_EFFECT (effect);
  ClutterOffscreenEffectClass *parent;
  CoglHandle material;

  if (self->program == COGL_INVALID_HANDLE)
    goto out;

  if (self->tex_uniform > -1)
    cogl_program_set_uniform_1i (self->program, self->tex_uniform, 0);

  if (self->factor_uniform > -1)
    cogl_program_set_uniform_1f (self->program,
                                 self->factor_uniform,
                                 self->factor);

  material = clutter_offscreen_effect_get_target (effect);
  cogl_material_set_user_program (material, self->program);

out:
  parent = CLUTTER_OFFSCREEN_EFFECT_CLASS (clutter_desaturate_effect_parent_class);
  parent->paint_target (effect);
}

static void
clutter_desaturate_effect_dispose (GObject *gobject)
{
  ClutterDesaturateEffect *self = CLUTTER_DESATURATE_EFFECT (gobject);

  if (self->program != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (self->program);

      self->program = COGL_INVALID_HANDLE;
      self->shader = COGL_INVALID_HANDLE;
    }

  G_OBJECT_CLASS (clutter_desaturate_effect_parent_class)->dispose (gobject);
}

static void
clutter_desaturate_effect_set_property (GObject      *gobject,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ClutterDesaturateEffect *effect = CLUTTER_DESATURATE_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_FACTOR:
      clutter_desaturate_effect_set_factor (effect,
                                            g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_desaturate_effect_get_property (GObject    *gobject,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ClutterDesaturateEffect *effect = CLUTTER_DESATURATE_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_FACTOR:
      g_value_set_double (value, effect->factor);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_desaturate_effect_class_init (ClutterDesaturateEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->paint_target = clutter_desaturate_effect_paint_target;

  effect_class->pre_paint = clutter_desaturate_effect_pre_paint;

  /**
   * ClutterDesaturateEffect:factor:
   *
   * The desaturation factor, between 0.0 (no desaturation) and 1.0 (full
   * desaturation).
   *
   * Since: 1.4
   */
  obj_props[PROP_FACTOR] =
    g_param_spec_double ("factor",
                         P_("Factor"),
                         P_("The desaturation factor"),
                         0.0, 1.0,
                         1.0,
                         CLUTTER_PARAM_READWRITE);

  gobject_class->dispose = clutter_desaturate_effect_dispose;
  gobject_class->set_property = clutter_desaturate_effect_set_property;
  gobject_class->get_property = clutter_desaturate_effect_get_property;

  g_object_class_install_properties (gobject_class,
                                     PROP_LAST,
                                     obj_props);
}

static void
clutter_desaturate_effect_init (ClutterDesaturateEffect *self)
{
  self->factor = 1.0;
}

/**
 * clutter_desaturate_effect_new:
 * @factor: the desaturation factor, between 0.0 and 1.0
 *
 * Creates a new #ClutterDesaturateEffect to be used with
 * clutter_actor_add_effect()
 *
 * Return value: the newly created #ClutterDesaturateEffect or %NULL
 *
 * Since: 1.4
 */
ClutterEffect *
clutter_desaturate_effect_new (gdouble factor)
{
  g_return_val_if_fail (factor >= 0.0 && factor <= 1.0, NULL);

  return g_object_new (CLUTTER_TYPE_DESATURATE_EFFECT,
                       "factor", factor,
                       NULL);
}

/**
 * clutter_desaturate_effect_set_factor:
 * @effect: a #ClutterDesaturateEffect
 * @factor: the desaturation factor, between 0.0 and 1.0
 *
 * Sets the desaturation factor for @effect, with 0.0 being "do not desaturate"
 * and 1.0 being "fully desaturate"
 *
 * Since: 1.4
 */
void
clutter_desaturate_effect_set_factor (ClutterDesaturateEffect *effect,
                                      gdouble                  factor)
{
  g_return_if_fail (CLUTTER_IS_DESATURATE_EFFECT (effect));
  g_return_if_fail (factor >= 0.0 && factor <= 1.0);

  if (fabsf (effect->factor - factor) >= 0.00001)
    {
      effect->factor = factor;

      clutter_effect_queue_repaint (CLUTTER_EFFECT (effect));

      g_object_notify_by_pspec (G_OBJECT (effect), obj_props[PROP_FACTOR]);
    }
}

/**
 * clutter_desaturate_effect_get_factor:
 * @effect: a #ClutterDesaturateEffect
 *
 * Retrieves the desaturation factor of @effect
 *
 * Return value: the desaturation factor
 *
 * Since: 1.4
 */
gdouble
clutter_desaturate_effect_get_factor (ClutterDesaturateEffect *effect)
{
  g_return_val_if_fail (CLUTTER_IS_DESATURATE_EFFECT (effect), 0.0);

  return effect->factor;
}
