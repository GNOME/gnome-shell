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

  gint tex_width;
  gint tex_height;

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

static gboolean
shell_invert_lightness_effect_pre_paint (ClutterEffect *effect)
{
  ShellInvertLightnessEffect *self = SHELL_INVERT_LIGHTNESS_EFFECT (effect);
  ClutterEffectClass *parent_class;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  if (!clutter_feature_available (CLUTTER_FEATURE_SHADERS_GLSL))
    {
      /* if we don't have support for GLSL shaders then we
       * forcibly disable the ActorMeta
       */
      g_warning ("Unable to use the ShellInvertLightnessEffect: the "
                 "graphics hardware or the current GL driver does not "
                 "implement support for the GLSL shading language.");
      clutter_actor_meta_set_enabled (CLUTTER_ACTOR_META (self), FALSE);
      return FALSE;
    }

  parent_class =
    CLUTTER_EFFECT_CLASS (shell_invert_lightness_effect_parent_class);
  if (parent_class->pre_paint (effect))
    {
      ClutterOffscreenEffect *offscreen_effect =
        CLUTTER_OFFSCREEN_EFFECT (effect);
      CoglHandle texture;

      texture = clutter_offscreen_effect_get_texture (offscreen_effect);
      self->tex_width = cogl_texture_get_width (texture);
      self->tex_height = cogl_texture_get_height (texture);

      cogl_pipeline_set_layer_texture (self->pipeline, 0, texture);

      return TRUE;
    }
  else
    return FALSE;
}

static void
shell_invert_lightness_effect_paint_target (ClutterOffscreenEffect *effect)
{
  ShellInvertLightnessEffect *self = SHELL_INVERT_LIGHTNESS_EFFECT (effect);
  ClutterActor *actor;
  guint8 paint_opacity;
  CoglFramebuffer *fb = cogl_get_draw_framebuffer ();

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));
  paint_opacity = clutter_actor_get_paint_opacity (actor);

  cogl_pipeline_set_color4ub (self->pipeline,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);
  cogl_framebuffer_draw_rectangle (fb, self->pipeline,
                                   0, 0, self->tex_width, self->tex_height);
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
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->paint_target = shell_invert_lightness_effect_paint_target;

  effect_class->pre_paint = shell_invert_lightness_effect_pre_paint;

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

      cogl_pipeline_set_layer_null_texture (klass->base_pipeline,
                                            0, /* layer number */
                                            COGL_TEXTURE_TYPE_2D);
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
