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
 * #ClutterColorizeEffect is a sub-class of #ClutterEffect that
 * colorizes an actor with the given tint.
 *
 * #ClutterColorizeEffect uses the programmable pipeline of the GPU
 * so it is only available on graphics hardware that supports this
 * feature.
 *
 * #ClutterColorizeEffect is available since Clutter 1.4
 */

#define CLUTTER_COLORIZE_EFFECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_COLORIZE_EFFECT, ClutterColorizeEffectClass))
#define CLUTTER_IS_COLORIZE_EFFECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_COLORIZE_EFFECT))
#define CLUTTER_COLORIZE_EFFECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_COLORIZE_EFFECT, ClutterColorizeEffectClass))

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-colorize-effect.h"

#include "cogl/cogl.h"

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-private.h"
#include "clutter-shader-effect.h"

typedef struct _ClutterColorizeEffectClass    ClutterColorizeEffectClass;

struct _ClutterColorizeEffect
{
  ClutterShaderEffect parent_instance;

  /* a back pointer to our actor, so that we can query it */
  ClutterActor *actor;

  /* the tint of the colorization */
  ClutterColor tint;
};

struct _ClutterColorizeEffectClass
{
  ClutterShaderEffectClass parent_class;
};

/* the magic gray vec3 has been taken from the NTSC conversion weights
 * as defined by:
 *
 *   "OpenGL Superbible, 4th Edition"
 *   -- Richard S. Wright Jr, Benjamin Lipchak, Nicholas Haemel
 *   Addison-Wesley
 */
static const gchar *colorize_glsl_shader =
"uniform sampler2D tex;\n"
"uniform vec3 tint;\n"
"\n"
"void main ()\n"
"{\n"
"  vec4 color = gl_Color * texture2D (tex, vec2 (gl_TexCoord[0].xy));\n"
"  float gray = dot (color.rgb, vec3 (0.299, 0.587, 0.114));\n"
"  gl_FragColor = vec4 (gray * tint, color.a);\n"
"}\n";

/* a lame sepia */
static const ClutterColor default_tint = { 255, 204, 153, 255 };

enum
{
  PROP_0,

  PROP_TINT
};

G_DEFINE_TYPE (ClutterColorizeEffect,
               clutter_colorize_effect,
               CLUTTER_TYPE_SHADER_EFFECT);

static gboolean
clutter_colorize_effect_pre_paint (ClutterEffect *effect)
{
  ClutterColorizeEffect *self = CLUTTER_COLORIZE_EFFECT (effect);
  ClutterShaderEffect *shader_effect;
  ClutterEffectClass *parent_class;
  float tint_r, tint_g, tint_b;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  shader_effect = CLUTTER_SHADER_EFFECT (effect);

  clutter_shader_effect_set_shader_source (shader_effect, colorize_glsl_shader);

  /* we want normalized values here */
  tint_r = self->tint.red   / 255.0f;
  tint_g = self->tint.green / 255.0f;
  tint_b = self->tint.blue  / 255.0f;

  /* bind the uniforms to the factor property */
  clutter_shader_effect_set_uniform (shader_effect,
                                     "tex",
                                     G_TYPE_INT, 1,
                                     0);
  clutter_shader_effect_set_uniform (shader_effect,
                                     "tint",
                                     G_TYPE_FLOAT, 3,
                                     tint_r,
                                     tint_g,
                                     tint_b);

  parent_class = CLUTTER_EFFECT_CLASS (clutter_colorize_effect_parent_class);
  return parent_class->pre_paint (effect);
}

static void
clutter_colorize_effect_finalize (GObject *gobject)
{
  G_OBJECT_CLASS (clutter_colorize_effect_parent_class)->finalize (gobject);
}

static void
clutter_colorize_effect_set_property (GObject      *gobject,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ClutterColorizeEffect *effect = CLUTTER_COLORIZE_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_TINT:
      clutter_colorize_effect_set_tint (effect,
                                        clutter_value_get_color (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_colorize_effect_get_property (GObject    *gobject,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ClutterColorizeEffect *effect = CLUTTER_COLORIZE_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_TINT:
      clutter_value_set_color (value, &effect->tint);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_colorize_effect_class_init (ClutterColorizeEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  effect_class->pre_paint = clutter_colorize_effect_pre_paint;

  gobject_class->set_property = clutter_colorize_effect_set_property;
  gobject_class->get_property = clutter_colorize_effect_get_property;
  gobject_class->finalize = clutter_colorize_effect_finalize;

  /**
   * ClutterColorizeEffect:tint:
   *
   * The tint to apply to the actor
   *
   * Since: 1.4
   */
  pspec = clutter_param_spec_color ("tint",
                                    "Tint",
                                    "The tint to apply",
                                    &default_tint,
                                    CLUTTER_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_TINT, pspec);
}

static void
clutter_colorize_effect_init (ClutterColorizeEffect *self)
{
  self->tint = default_tint;
}

/**
 * clutter_colorize_effect_new:
 * @tint: the color to be used
 *
 * Creates a new #ClutterColorizeEffect to be used with
 * clutter_actor_set_effect()
 *
 * Return value: the newly created #ClutterColorizeEffect or %NULL
 *
 * Since: 1.4
 */
ClutterEffect *
clutter_colorize_effect_new (const ClutterColor *tint)
{
  return g_object_new (CLUTTER_TYPE_COLORIZE_EFFECT,
                       "tint", tint,
                       NULL);
}

/**
 * clutter_colorize_effect_set_tint:
 * @effect: a #ClutterColorizeEffect
 * @tint: the color to be used
 *
 * Sets the tint to be used when colorizing
 *
 * Since: 1.4
 */
void
clutter_colorize_effect_set_tint (ClutterColorizeEffect *effect,
                                  const ClutterColor    *tint)
{
  g_return_if_fail (CLUTTER_IS_COLORIZE_EFFECT (effect));

  effect->tint = *tint;

  if (effect->actor != NULL)
    clutter_actor_queue_redraw (effect->actor);

  g_object_notify (G_OBJECT (effect), "tint");
}

/**
 * clutter_colorize_effect_get_tint:
 * @effect: a #ClutterColorizeEffect
 * @tint: (out): return location for the color used
 *
 * Retrieves the tint used by @effect
 *
 * Since: 1.4
 */
void
clutter_colorize_effect_get_tint (ClutterColorizeEffect *effect,
                                  ClutterColor          *tint)
{
  g_return_if_fail (CLUTTER_IS_COLORIZE_EFFECT (effect));
  g_return_if_fail (tint != NULL);

  *tint = effect->tint;
}
