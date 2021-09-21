/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2010-2012 Inclusive Design Research Centre, OCAD University.
 *
 * This program is free software; you can redistribute it and/or
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
 * SECTION:shell-invert-lightness-effect
 * @short_description: A colorization effect where lightness is inverted but
 * color is not.
 * @see_also: #ClutterEffect, #ClutterOffscreenEffect
 *
 * #ShellInvertLightnessEffect is a sub-class of #ClutterEffect that enhances
 * the appearance of a clutter actor.  Specifically it inverts the lightness
 * of a #ClutterActor (e.g., darker colors become lighter, white becomes black,
 * and white, black).
 */

#define SHELL_INVERT_LIGHTNESS_EFFECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_INVERT_LIGHTNESS_EFFECT, ShellInvertLightnessEffectClass))
#define SHELL_IS_INVERT_EFFECT_CLASS(klass)           (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_INVERT_LIGHTNESS_EFFECT))
#define SHELL_INVERT_LIGHTNESS_EFFECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_INVERT_LIGHTNESS_EFFEC, ShellInvertLightnessEffectClass))

#include "shell-invert-lightness-effect.h"

#include <cogl/cogl.h>

struct _ShellInvertLightnessEffect
{
  ClutterOffscreenEffect parent_instance;

  CoglPipeline *pipeline;
};

struct _ShellInvertLightnessEffectClass
{
  ClutterOffscreenEffectClass parent_class;

  CoglPipeline *base_pipeline;
};

/* Lightness inversion in GLSL.
 */
static const gchar *invert_lightness_source =
  "cogl_texel = texture2D (cogl_sampler, cogl_tex_coord.st);\n"
  "vec3 effect = vec3 (cogl_texel);\n"
  "\n"
  "float maxColor = max (cogl_texel.r, max (cogl_texel.g, cogl_texel.b));\n"
  "float minColor = min (cogl_texel.r, min (cogl_texel.g, cogl_texel.b));\n"
  "float lightness = (maxColor + minColor) / 2.0;\n"
  "\n"
  "float delta = (1.0 - lightness) - lightness;\n"
  "effect.rgb = (effect.rgb + delta);\n"
  "\n"
  "cogl_texel = vec4 (effect, cogl_texel.a);\n";

G_DEFINE_TYPE (ShellInvertLightnessEffect,
               shell_invert_lightness_effect,
               CLUTTER_TYPE_OFFSCREEN_EFFECT);

static CoglPipeline *
shell_glsl_effect_create_pipeline (ClutterOffscreenEffect *effect,
                                   CoglTexture            *texture)
{
  ShellInvertLightnessEffect *self = SHELL_INVERT_LIGHTNESS_EFFECT (effect);

  cogl_pipeline_set_layer_texture (self->pipeline, 0, texture);

  return cogl_object_ref (self->pipeline);
}

static void
shell_invert_lightness_effect_dispose (GObject *gobject)
{
  ShellInvertLightnessEffect *self = SHELL_INVERT_LIGHTNESS_EFFECT (gobject);

  if (self->pipeline != NULL)
    {
      cogl_object_unref (self->pipeline);
      self->pipeline = NULL;
    }

  G_OBJECT_CLASS (shell_invert_lightness_effect_parent_class)->dispose (gobject);
}

static void
shell_invert_lightness_effect_class_init (ShellInvertLightnessEffectClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->create_pipeline = shell_glsl_effect_create_pipeline;

  gobject_class->dispose = shell_invert_lightness_effect_dispose;
}

static void
shell_invert_lightness_effect_init (ShellInvertLightnessEffect *self)
{
  ShellInvertLightnessEffectClass *klass;
  klass = SHELL_INVERT_LIGHTNESS_EFFECT_GET_CLASS (self);

  if (G_UNLIKELY (klass->base_pipeline == NULL))
    {
      CoglSnippet *snippet;
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());

      klass->base_pipeline = cogl_pipeline_new (ctx);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                                  NULL,
                                  NULL);
      cogl_snippet_set_replace (snippet, invert_lightness_source);
      cogl_pipeline_add_layer_snippet (klass->base_pipeline, 0, snippet);
      cogl_object_unref (snippet);

      cogl_pipeline_set_layer_null_texture (klass->base_pipeline, 0);
    }

  self->pipeline = cogl_pipeline_copy (klass->base_pipeline);
}

/**
 * shell_invert_lightness_effect_new:
 *
 * Creates a new #ShellInvertLightnessEffect to be used with
 * clutter_actor_add_effect()
 *
 * Return value: (transfer full): the newly created
 * #ShellInvertLightnessEffect or %NULL.  Use g_object_unref() when done.
 */
ClutterEffect *
shell_invert_lightness_effect_new (void)
{
  return g_object_new (SHELL_TYPE_INVERT_LIGHTNESS_EFFECT, NULL);
}
