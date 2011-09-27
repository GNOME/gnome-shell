/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010, 2011 Inclusive Design Research Centre, OCAD University.
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
 *   Joseph Scheuhammer <clown@alum.mit.edu>
 */

/**
 * SECTION:clutter-invert-lightness-effect
 * @short_description: A colorization effect where lightness is inverted but
 * color is not.
 * @see_also: #ClutterEffect, #ClutterOffscreenEffect
 *
 * #ClutterInvertLightnessEffect is a sub-class of #ClutterEffect that enhances
 * the appearance of a clutter actor.  Specifically it inverts the lightness
 * of a #ClutterActor (e.g., darker colors become lighter, white becomes black,
 * and white, black).
 *
 * #ClutterInvertLightnessEffect is available since Clutter 1.10
 */

#define CLUTTER_INVERT_LIGHTNESS_EFFECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_INVERT_LIGHTNESS_EFFECT, ClutterInvertLightnessEffectClass))
#define CLUTTER_IS_INVERT_EFFECT_CLASS(klass)           (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_INVERT_LIGHTNESS_EFFECT))
#define CLUTTER_INVERT_LIGHTNESS_EFFECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_INVERT_LIGHTNESS_EFFEC, ClutterInvertLightnessEffectClass))

#include "clutter-invert-lightness-effect.h"

#include "clutter-actor.h"
#include "clutter-feature.h"
#include "clutter-offscreen-effect.h"

struct _ClutterInvertLightnessEffect
{
  ClutterOffscreenEffect parent_instance;

  /* a back pointer to our actor, so that we can query it */
  ClutterActor *actor;

  CoglHandle shader;
  CoglHandle program;

  gint tex_uniform;

  guint is_compiled : 1;
};

struct _ClutterInvertLightnessEffectClass
{
  ClutterOffscreenEffectClass parent_class;
};

/* Lightness inversion in GLSL.
 */
static const gchar *invert_lightness_glsl_shader =
"uniform sampler2D tex;\n"
"\n"
"void main ()\n"
"{\n"
"  vec4 color = cogl_color_in * texture2D (tex, vec2 (cogl_tex_coord_in[0].xy));\n"
"  vec3 effect = vec3 (color);\n"
"\n"
"  float maxColor = max (color.r, max (color.g, color.b));\n"
"  float minColor = min (color.r, min (color.g, color.b));\n"
"  float lightness = (maxColor + minColor) / 2.0;\n"
"\n"
"  float delta = (1.0 - lightness) - lightness;\n"
"  effect.rgb = (effect.rgb + delta);\n"
"\n"
"  cogl_color_out = vec4 (effect, color.a);\n"
"}\n";

G_DEFINE_TYPE (ClutterInvertLightnessEffect,
               clutter_invert_lightness_effect,
               CLUTTER_TYPE_OFFSCREEN_EFFECT);

static gboolean
clutter_invert_lightness_effect_pre_paint (ClutterEffect *effect)
{
  ClutterInvertLightnessEffect *self = CLUTTER_INVERT_LIGHTNESS_EFFECT (effect);
  ClutterEffectClass *parent_class;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  self->actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));
  if (self->actor == NULL)
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
      cogl_shader_source (self->shader, invert_lightness_glsl_shader);

      self->is_compiled = FALSE;
      self->tex_uniform = -1;
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

          g_warning (G_STRLOC ": Unable to compile the invert-lightness effects shader: %s",
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
        }
    }

  parent_class = CLUTTER_EFFECT_CLASS (clutter_invert_lightness_effect_parent_class);
  return parent_class->pre_paint (effect);
}

static void
clutter_invert_lightness_effect_paint_target (ClutterOffscreenEffect *effect)
{
  ClutterInvertLightnessEffect *self = CLUTTER_INVERT_LIGHTNESS_EFFECT (effect);
  ClutterOffscreenEffectClass *parent;
  CoglHandle material;

  if (self->program == COGL_INVALID_HANDLE)
    goto out;

  if (self->tex_uniform > -1)
    cogl_program_set_uniform_1i (self->program, self->tex_uniform, 0);

  material = clutter_offscreen_effect_get_target (effect);
  cogl_material_set_user_program (material, self->program);

out:
  parent = CLUTTER_OFFSCREEN_EFFECT_CLASS (clutter_invert_lightness_effect_parent_class);
  parent->paint_target (effect);
}

static void
clutter_invert_lightness_effect_dispose (GObject *gobject)
{
  ClutterInvertLightnessEffect *self = CLUTTER_INVERT_LIGHTNESS_EFFECT (gobject);

  if (self->program != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (self->program);

      self->program = COGL_INVALID_HANDLE;
      self->shader = COGL_INVALID_HANDLE;
    }

  self->actor = NULL;

  G_OBJECT_CLASS (clutter_invert_lightness_effect_parent_class)->dispose (gobject);
}

static void
clutter_invert_lightness_effect_class_init (ClutterInvertLightnessEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->paint_target = clutter_invert_lightness_effect_paint_target;

  effect_class->pre_paint = clutter_invert_lightness_effect_pre_paint;

  gobject_class->dispose = clutter_invert_lightness_effect_dispose;
}

static void
clutter_invert_lightness_effect_init (ClutterInvertLightnessEffect *self)
{
}

/**
 * clutter_invert_lightness_effect_new:
 *
 * Creates a new #ClutterInvertLightnessEffect to be used with
 * clutter_actor_add_effect()
 *
 * Return value: (transfer full): the newly created
 *   #ClutterInvertLightnessEffect or %NULL.  Use g_object_unref() when done.
 *
 * Since: 1.10
 */
ClutterEffect *
clutter_invert_lightness_effect_new (void)
{
  return g_object_new (CLUTTER_TYPE_INVERT_LIGHTNESS_EFFECT,
                       NULL);
}
