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
 * SECTION:clutter-blur-effect
 * @short_description: A blur effect
 * @see_also: #ClutterEffect, #ClutterOffscreenEffect
 *
 * #ClutterBlurEffect is a sub-class of #ClutterEffect that allows blurring a
 * actor and its contents.
 *
 * #ClutterBlurEffect uses the programmable pipeline of the GPU and an
 * offscreen buffer, so it is only available on graphics hardware that supports
 * these two features.
 *
 * #ClutterBlurEffect is available since Clutter 1.4
 */

#define CLUTTER_BLUR_EFFECT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BLUR_EFFECT, ClutterBlurEffectClass))
#define CLUTTER_IS_BLUR_EFFECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BLUR_EFFECT))
#define CLUTTER_BLUR_EFFECT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BLUR_EFFECT, ClutterBlurEffectClass))

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-blur-effect.h"

#include "cogl/cogl.h"

#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-shader-effect.h"

typedef struct _ClutterBlurEffectClass  ClutterBlurEffectClass;

/* FIXME - lame shader; we should really have a decoupled
 * horizontal/vertical two pass shader for the gaussian blur
 */
static const gchar *box_blur_glsl_shader =
"uniform sampler2D tex;\n"
"uniform float x_step, y_step;\n"
"\n"
"vec4 get_rgba_rel (sampler2D source, float dx, float dy)\n"
"{\n"
"  return texture2D (tex, gl_TexCoord[0].st + vec2 (dx, dy) * 2.0);\n"
"}\n"
"\n"
"void main ()\n"
"{\n"
"  vec4 color = gl_Color * texture2D (tex, vec2 (gl_TexCoord[0].xy));\n"
"  float count = 1.0;\n"
"  color += get_rgba_rel (tex, -x_step, -y_step); count++;\n"
"  color += get_rgba_rel (tex, -x_step, 0.0);     count++;\n"
"  color += get_rgba_rel (tex, 0.0, -y_step);     count++;\n"
"  color += get_rgba_rel (tex, 0.0, 0.0);         count++;\n"
"  color += get_rgba_rel (tex, 0.0, y_step);      count++;\n"
"  color += get_rgba_rel (tex, x_step, -y_step);  count++;\n"
"  color += get_rgba_rel (tex, x_step, 0.0);      count++;\n"
"  color += get_rgba_rel (tex, x_step, y_step);   count++;\n"
"  gl_FragColor = color / count;\n"
"}";

struct _ClutterBlurEffect
{
  ClutterShaderEffect parent_instance;

  /* a back pointer to our actor, so that we can query it */
  ClutterActor *actor;

  /* the parameters; x_step and y_step depend on
   * the actor's allocation
   */
  gfloat x_step;
  gfloat y_step;
};

struct _ClutterBlurEffectClass
{
  ClutterShaderEffectClass parent_class;
};

enum
{
  PROP_0
};

G_DEFINE_TYPE (ClutterBlurEffect,
               clutter_blur_effect,
               CLUTTER_TYPE_SHADER_EFFECT);

static int
next_p2 (int a)
{
  int rval = 1;

  while (rval < a)
    rval <<= 1;

  return rval;
}

static gboolean
clutter_blur_effect_pre_paint (ClutterEffect *effect)
{
  ClutterBlurEffect *self = CLUTTER_BLUR_EFFECT (effect);
  ClutterShaderEffect *shader_effect;
  ClutterEffectClass *parent_class;
  ClutterActorBox allocation;
  gfloat width, height;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  self->actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));
  if (self->actor == NULL)
    return FALSE;

  clutter_actor_get_allocation_box (self->actor, &allocation);
  clutter_actor_box_get_size (&allocation, &width, &height);

  self->x_step = 1.0f / (float) next_p2 (width);
  self->y_step = 1.0f / (float) next_p2 (height);

  shader_effect = CLUTTER_SHADER_EFFECT (effect);

  clutter_shader_effect_set_shader_source (shader_effect, box_blur_glsl_shader);

  clutter_shader_effect_set_uniform (shader_effect,
                                     "tex",
                                     G_TYPE_INT, 1,
                                     0);
  clutter_shader_effect_set_uniform (shader_effect,
                                     "x_step",
                                     G_TYPE_FLOAT, 1,
                                     self->x_step);
  clutter_shader_effect_set_uniform (shader_effect,
                                     "y_step",
                                     G_TYPE_FLOAT, 1,
                                     self->y_step);

  parent_class = CLUTTER_EFFECT_CLASS (clutter_blur_effect_parent_class);
  return parent_class->pre_paint (effect);
}

static void
clutter_blur_effect_finalize (GObject *gobject)
{
  G_OBJECT_CLASS (clutter_blur_effect_parent_class)->finalize (gobject);
}

static void
clutter_blur_effect_set_property (GObject      *gobject,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_blur_effect_get_property (GObject    *gobject,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_blur_effect_class_init (ClutterBlurEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  effect_class->pre_paint = clutter_blur_effect_pre_paint;

  gobject_class->set_property = clutter_blur_effect_set_property;
  gobject_class->get_property = clutter_blur_effect_get_property;
  gobject_class->finalize = clutter_blur_effect_finalize;
}

static void
clutter_blur_effect_init (ClutterBlurEffect *self)
{
}

/**
 * clutter_blur_effect_new:
 *
 * Creates a new #ClutterBlurEffect to be used with
 * clutter_actor_add_effect()
 *
 * Return value: the newly created #ClutterBlurEffect or %NULL
 *
 * Since: 1.4
 */
ClutterEffect *
clutter_blur_effect_new (void)
{
  return g_object_new (CLUTTER_TYPE_BLUR_EFFECT, NULL);
}
