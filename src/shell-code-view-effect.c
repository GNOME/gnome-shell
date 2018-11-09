/*
 * shell-code-view-effect.c
 *
 * Based on clutter-desaturate-effect.c.
 *
 * Copyright (C) 2010  Intel Corporation.
 * Copyright (C) 2018  Endless Mobile, Inc.
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
 * Authors:
 *   Emmanuele Bassi <ebassi@linux.intel.com>
 *   Cosimo Cecchi <cosimo@endlessm.com>
 */

#include <math.h>

#include "shell-code-view-effect.h"

#include <cogl/cogl.h>

#include <stdlib.h>
#include <string.h>

typedef struct
{
  ClutterColor gradient_colors[5];
  gfloat gradient_points[5];

  gint gradient_colors_uniform;
  gint gradient_points_uniform;

  CoglPipeline *pipeline;
} ShellCodeViewEffectPrivate;

struct _ShellCodeViewEffectClass
{
  ClutterOffscreenEffectClass parent_class;

  CoglPipeline *base_pipeline;
};

/* the magic gray vec3 has been taken from the NTSC conversion weights
 * as defined by:
 *
 *   "OpenGL Superbible, 4th edition"
 *   -- Richard S. Wright Jr, Benjamin Lipchak, Nicholas Haemel
 *   Addison-Wesley
 */
static const gchar *glsl_declarations =
  "uniform vec3 colors[5];\n"
  "uniform float points[5];\n"
  "\n"
  "vec4 gradient_map (const vec4 color)\n"
  "{\n"
  "  if (color.a != 1.0)\n"
  "  {\n"
  "    return color;\n"
  "  }\n"
  "  const vec3 gray_conv = vec3 (0.299, 0.587, 0.114);\n"
  "  float desaturated = dot (color.rgb, gray_conv);\n"
  "  vec4 color_out = color;\n"
  "  if (desaturated <= points[1])\n"
  "  {\n"
  "    color_out.rgb = mix (colors[0], colors[1], (desaturated - points[0]) / (points[1] - points[0]));\n"
  "  }\n"
  "  else if (desaturated <= points[2])\n"
  "  {\n"
  "    color_out.rgb = mix (colors[1], colors[2], (desaturated - points[1]) / (points[2] - points[1]));\n"
  "  }\n"
  "  else if (desaturated <= points[3])\n"
  "  {\n"
  "    color_out.rgb = mix (colors[2], colors[3], (desaturated - points[2]) / (points[3] - points[2]));\n"
  "  }\n"
  "  else\n"
  "  {\n"
  "    color_out.rgb = mix (colors[3], colors[4], (desaturated - points[3]) / (points[4] - points[3]));\n"
  "  }\n"
  "  return color_out;\n"
  "}\n";

static const gchar *glsl_source =
  "  cogl_color_out.rgba = gradient_map (cogl_color_out.rgba);";


G_DEFINE_TYPE_WITH_PRIVATE (ShellCodeViewEffect,
                            shell_code_view_effect,
                            CLUTTER_TYPE_OFFSCREEN_EFFECT)

static gboolean
shell_code_view_effect_pre_paint (ClutterEffect       *effect,
                                  ClutterPaintContext *paint_context)
{
  ShellCodeViewEffect *self = SHELL_CODE_VIEW_EFFECT (effect);
  ShellCodeViewEffectPrivate *priv = shell_code_view_effect_get_instance_private (self);
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

  parent_class = CLUTTER_EFFECT_CLASS (shell_code_view_effect_parent_class);
  if (parent_class->pre_paint (effect, paint_context))
    {
      ClutterOffscreenEffect *offscreen_effect = CLUTTER_OFFSCREEN_EFFECT (effect);
      CoglHandle texture;

      texture = clutter_offscreen_effect_get_texture (offscreen_effect);
      cogl_pipeline_set_layer_texture (priv->pipeline, 0, texture);

      return TRUE;
    }
  else
    return FALSE;
}

static void
shell_code_view_effect_paint_target (ClutterOffscreenEffect *effect,
                                     ClutterPaintContext    *paint_context)
{
  ShellCodeViewEffect *self = SHELL_CODE_VIEW_EFFECT (effect);
  ShellCodeViewEffectPrivate *priv = shell_code_view_effect_get_instance_private (self);
  CoglFramebuffer *fb;
  ClutterActor *actor;
  CoglHandle texture;
  guint8 paint_opacity;

  texture = clutter_offscreen_effect_get_texture (effect);
  cogl_pipeline_set_layer_texture (priv->pipeline, 0, texture);

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));
  paint_opacity = clutter_actor_get_paint_opacity (actor);

  fb = clutter_paint_context_get_framebuffer (paint_context);
  cogl_pipeline_set_color4ub (priv->pipeline,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);
  cogl_framebuffer_draw_rectangle (fb, priv->pipeline,
                                   0, 0,
                                   cogl_texture_get_width (texture),
                                   cogl_texture_get_height (texture));
}

static void
shell_code_view_effect_dispose (GObject *gobject)
{
  ShellCodeViewEffect *self = SHELL_CODE_VIEW_EFFECT (gobject);
  ShellCodeViewEffectPrivate *priv = shell_code_view_effect_get_instance_private (self);

  g_clear_pointer (&priv->pipeline, cogl_object_unref);

  G_OBJECT_CLASS (shell_code_view_effect_parent_class)->dispose (gobject);
}

static void
update_gradient_uniforms (ShellCodeViewEffect *self)
{
  ShellCodeViewEffectPrivate *priv = shell_code_view_effect_get_instance_private (self);

  if (!(priv->gradient_points_uniform > -1) || !(priv->gradient_colors_uniform > -1))
    return;

  cogl_pipeline_set_uniform_float (priv->pipeline,
                                   priv->gradient_points_uniform,
                                   1, /* n_components */
                                   5, /* count */
                                   priv->gradient_points);

  float colors[15];

  int i = 0;
  for (int j = 0; j < 5; j++)
    {
      colors[i++] = priv->gradient_colors[j].red / 255.0;
      colors[i++] = priv->gradient_colors[j].green / 255.0;
      colors[i++] = priv->gradient_colors[j].blue / 255.0;
    }

  cogl_pipeline_set_uniform_float (priv->pipeline,
                                   priv->gradient_colors_uniform,
                                   3, /* n_components */
                                   5, /* count */
                                   colors);
}

static void
shell_code_view_effect_class_init (ShellCodeViewEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->paint_target = shell_code_view_effect_paint_target;

  effect_class->pre_paint = shell_code_view_effect_pre_paint;

  gobject_class->dispose = shell_code_view_effect_dispose;
}

static void
shell_code_view_effect_init (ShellCodeViewEffect *self)
{
  ShellCodeViewEffectClass *klass = SHELL_CODE_VIEW_EFFECT_GET_CLASS (self);
  ShellCodeViewEffectPrivate *priv = shell_code_view_effect_get_instance_private (self);

  if (G_UNLIKELY (klass->base_pipeline == NULL))
    {
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());
      CoglSnippet *snippet;

      klass->base_pipeline = cogl_pipeline_new (ctx);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                  glsl_declarations,
                                  glsl_source);
      cogl_pipeline_add_snippet (klass->base_pipeline, snippet);
      cogl_object_unref (snippet);

      cogl_pipeline_set_layer_null_texture (klass->base_pipeline, 0);
    }

  priv->pipeline = cogl_pipeline_copy (klass->base_pipeline);

  priv->gradient_colors_uniform =
    cogl_pipeline_get_uniform_location (priv->pipeline, "colors");
  priv->gradient_points_uniform =
    cogl_pipeline_get_uniform_location (priv->pipeline, "points");

  update_gradient_uniforms (self);
}

/**
 * shell_code_view_effect_set_gradient_stops:
 * @effect: a #ShellCodeViewEffect
 * @gradient_colors: (array length=gradient_len) (element-type utf8): gradient colors
 * @gradient_points: (array length=gradient_len) (element-type gfloat): gradient points
 * @gradient_len: length of gradient stops
 *
 * Set the gradient colors and stop points for this effect.
 */
void
shell_code_view_effect_set_gradient_stops (ShellCodeViewEffect *effect,
                                           gchar **gradient_colors,
                                           gfloat *gradient_points,
                                           gsize gradient_len)
{
  ShellCodeViewEffectPrivate *priv = shell_code_view_effect_get_instance_private (effect);
  gint i;

  g_return_if_fail (gradient_colors != NULL);
  g_return_if_fail (gradient_points != NULL);
  g_return_if_fail (gradient_len == 5);

  memcpy (priv->gradient_points, gradient_points, sizeof (gfloat) * gradient_len);
  for (i = 0; i < gradient_len; i++)
    clutter_color_from_string (&priv->gradient_colors[i], gradient_colors[i]);

  update_gradient_uniforms (effect);
}

/**
 * shell_code_view_effect_new:
 *
 * Creates a new #ShellCodeViewEffect to be used with
 * clutter_actor_add_effect()
 *
 * Return value: the newly created #ShellCodeViewEffect or %NULL
 */
ClutterEffect *
shell_code_view_effect_new (void)
{
  return g_object_new (SHELL_TYPE_CODE_VIEW_EFFECT, NULL);
}
