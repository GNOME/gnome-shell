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
 * SECTION:clutter-brightness-effect
 * @short_description: Increase/decrease brightness of actor.
 * @see_also: #ClutterEffect, #ClutterOffscreenEffect
 *
 * #ClutterBrightnessEffect is a sub-class of #ClutterEffect that changes the
 * overall brightness of a #ClutterActor.
 *
 * #ClutterBrightnessEffect is available since Clutter 1.10
 */

#define CLUTTER_BRIGHTNESS_EFFECT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BRIGHTNESS_EFFECT, ClutterBrightnessEffectClass))
#define CLUTTER_IS_BRIGHTNESS_EFFECT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BRIGHTNESS_EFFECT))
#define CLUTTER_BRIGHTNESS_EFFECT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BRIGHTNESS_EFFECT, ClutterBrightnessEffectClass))

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-brightness-effect.h"

#include "clutter-actor.h"
#include "clutter-feature.h"
#include "clutter-offscreen-effect.h"
#include "clutter-private.h"

struct _ClutterBrightnessEffect
{
  ClutterOffscreenEffect parent_instance;

  /* a back pointer to our actor, so that we can query it */
  ClutterActor *actor;

  CoglHandle shader;
  CoglHandle program;

  /* Brightness changes. */
  ClutterColor brightness;

  gint tex_uniform;
  gint brightness_uniform;

  guint is_compiled : 1;
};

struct _ClutterBrightnessEffectClass
{
  ClutterOffscreenEffectClass parent_class;
};

/* Brightness effects in GLSL.
 */
static const gchar *brightness_glsl_shader =
"uniform sampler2D tex;\n"
"uniform vec3 brightness;\n"
"\n"
"void main ()\n"
"{\n"
"  vec4 color = cogl_color_in * texture2D (tex, vec2 (cogl_tex_coord_in[0].xy));\n"
"  vec3 effect = vec3 (color);\n"
"\n"
"  effect = clamp (effect + brightness, 0.0, 1.0);\n"
"\n"
"\n"
"  cogl_color_out = vec4 (effect, color.a);\n"
"}\n";

/* No brightness change. */
static const ClutterColor same_brightness = { 0x7f, 0x7f, 0x7f, 0xff };

enum
{
  PROP_0,

  PROP_BRIGHTNESS,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE (ClutterBrightnessEffect,
               clutter_brightness_effect,
               CLUTTER_TYPE_OFFSCREEN_EFFECT);

static gboolean
clutter_brightness_effect_pre_paint (ClutterEffect *effect)
{
  ClutterBrightnessEffect *self = CLUTTER_BRIGHTNESS_EFFECT (effect);
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
      cogl_shader_source (self->shader, brightness_glsl_shader);

      self->is_compiled = FALSE;
      self->tex_uniform = -1;
      self->brightness_uniform = -1;
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

          g_warning (G_STRLOC ": Unable to compile the brightness effects shader: %s",
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
          self->brightness_uniform =
            cogl_program_get_uniform_location (self->program, "brightness");
        }
    }

  parent_class = CLUTTER_EFFECT_CLASS (clutter_brightness_effect_parent_class);
  return parent_class->pre_paint (effect);
}

static void
clutter_brightness_effect_paint_target (ClutterOffscreenEffect *effect)
{
  ClutterBrightnessEffect *self = CLUTTER_BRIGHTNESS_EFFECT (effect);
  ClutterOffscreenEffectClass *parent;
  CoglHandle material;

  if (self->program == COGL_INVALID_HANDLE)
    goto out;

  if (self->tex_uniform > -1)
    cogl_program_set_uniform_1i (self->program, self->tex_uniform, 0);

  if (self->brightness_uniform > -1)
    {
      float brightness[3] = {
        (self->brightness.red / 127.0) - 1.0,
        (self->brightness.green / 127.0) - 1.0,
        (self->brightness.blue / 127.0) - 1.0
      };
      cogl_program_set_uniform_float (self->program, self->brightness_uniform,
                                      3, 1,
                                      brightness);
    }

  material = clutter_offscreen_effect_get_target (effect);
  cogl_material_set_user_program (material, self->program);

out:
  parent = CLUTTER_OFFSCREEN_EFFECT_CLASS (clutter_brightness_effect_parent_class);
  parent->paint_target (effect);
}

static void
clutter_brightness_effect_dispose (GObject *gobject)
{
  ClutterBrightnessEffect *self = CLUTTER_BRIGHTNESS_EFFECT (gobject);

  if (self->program != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (self->program);

      self->program = COGL_INVALID_HANDLE;
      self->shader = COGL_INVALID_HANDLE;
    }

  self->actor = NULL;

  G_OBJECT_CLASS (clutter_brightness_effect_parent_class)->dispose (gobject);
}

static void
clutter_brightness_effect_set_property (GObject      *gobject,
                                        guint        prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ClutterBrightnessEffect *effect = CLUTTER_BRIGHTNESS_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_BRIGHTNESS:
      clutter_brightness_effect_set_brightness (effect,
                                                clutter_value_get_color (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_brightness_effect_get_property (GObject    *gobject,
                                        guint      prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ClutterBrightnessEffect *effect = CLUTTER_BRIGHTNESS_EFFECT (gobject);
  ClutterColor brightness;

  switch (prop_id)
    {
    case PROP_BRIGHTNESS:
        clutter_brightness_effect_get_brightness (effect, &brightness);
        clutter_value_set_color (value, &brightness);
        break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_brightness_effect_class_init (ClutterBrightnessEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->paint_target = clutter_brightness_effect_paint_target;

  effect_class->pre_paint = clutter_brightness_effect_pre_paint;

  gobject_class->set_property = clutter_brightness_effect_set_property;
  gobject_class->get_property = clutter_brightness_effect_get_property;
  gobject_class->dispose = clutter_brightness_effect_dispose;

  /**
   * ClutterBrightnessEffect:brightness:
   *
   * The brightness change to apply to the actor
   *
   * Since: 1.10
   */
  obj_props[PROP_BRIGHTNESS] =
    clutter_param_spec_color ("brightness",
                              P_("Brightness"),
                              P_("The brightness change to apply"),
                              &same_brightness,
                              G_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
clutter_brightness_effect_init (ClutterBrightnessEffect *self)
{
  self->brightness = same_brightness;
}

/**
 * clutter_brightness_effect_new:
 *
 * Creates a new #ClutterBrightnessEffect to be used with
 * clutter_actor_add_effect()
 *
 * Return value: (transfer full): the newly created #ClutterBrightnessEffect or
 *   or %NULL.  Use g_object_unref() when done.
 *
 * Since: 1.10
 */
ClutterEffect *
clutter_brightness_effect_new (void)
{
  return g_object_new (CLUTTER_TYPE_BRIGHTNESS_EFFECT,
                       NULL);
}

/**
 * clutter_brightness_effect_set_brightness:
 * @effect: a #ClutterBrightnessEffect
 * @brightness: ClutterColor governing the brightness change.
 *
 * Add each of the red, green, blue components of the @brightness to
 * the red, greeen, or blue components of the actor's colors.
 *
 * Since: 1.10
 */
void
clutter_brightness_effect_set_brightness (ClutterBrightnessEffect *effect,
                                          const ClutterColor      *brightness)
{
  g_return_if_fail (CLUTTER_IS_BRIGHTNESS_EFFECT (effect));
  if (clutter_color_equal (&effect->brightness, brightness))
    return;

  effect->brightness = *brightness;

  if (effect->actor != NULL)
    clutter_actor_queue_redraw (effect->actor);
}

/**
 * clutter_brightness_effect_get_brightness:
 * @effect: a #ClutterBrightnessEffect
 * @brightness: (out caller-allocates): return location for the brightness.
 *
 * Retrieves the brightness value used by @effect
 *
 * Since: 1.10
 */
void
clutter_brightness_effect_get_brightness (ClutterBrightnessEffect *effect,
                                          ClutterColor            *brightness)
{
  g_return_if_fail (CLUTTER_IS_BRIGHTNESS_EFFECT (effect));
  g_return_if_fail (brightness != NULL);

  *brightness = effect->brightness;
}
