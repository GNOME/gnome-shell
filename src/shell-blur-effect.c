/* shell-blur-effect.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "shell-blur-effect.h"

static const gchar *gaussian_blur_glsl_declarations =
"uniform float blur_radius;                                        \n"
"uniform float brightness;                                         \n"
"uniform float pixel_step;                                         \n"
"uniform int vertical;                                             \n"
"                                                                  \n"
"float gaussian (float sigma, float x) {                           \n"
"  return exp ( - (x * x) / (2.0 * sigma * sigma));                \n"
"}                                                                 \n"
"                                                                  \n";

static const gchar *gaussian_blur_glsl =
"  float radius = blur_radius * 2.30348;                           \n"
"  float total = 0.0;                                              \n"
"  vec4 ret = vec4 (0);                                            \n"
"  vec2 uv = vec2(cogl_tex_coord.st);                              \n"
"                                                                  \n"
"  int half_radius = max(int(radius / 2.0), 1);                    \n"
"                                                                  \n"
"  if (vertical != 0) {                                            \n"
"    for (int y = -half_radius; y < half_radius; y++) {            \n"
"      float fy = gaussian (radius / 2.0, float(y));               \n"
"      float offset_y = float(y) * pixel_step;                     \n"
"                                                                  \n"
"      vec4 c = texture2D(cogl_sampler, uv + vec2(0.0, offset_y)); \n"
"      total += fy;                                                \n"
"      ret += c * fy;                                              \n"
"    }                                                             \n"
"  } else {                                                        \n"
"    for (int x = -half_radius; x < half_radius; x++) {            \n"
"      float fx = gaussian (radius / 2.0, float(x));               \n"
"      float offset_x = float(x) * pixel_step;                     \n"
"                                                                  \n"
"      vec4 c = texture2D(cogl_sampler, uv + vec2(offset_x, 0.0)); \n"
"      total += fx;                                                \n"
"      ret += c * fx;                                              \n"
"    }                                                             \n"
"  }                                                               \n"
"                                                                  \n"
"  cogl_texel = vec4 (ret / total);                                \n"
"  cogl_texel.rgb *= brightness;                                   \n";

#define DOWNSCALE_FACTOR 3.0

struct _ShellBlurEffect
{
  ClutterOffscreenEffect parent_instance;

  CoglPipeline *pipeline;
  int blur_radius_uniform;
  int brightness_uniform;
  int pixel_step_uniform;
  int vertical_uniform;

  unsigned int tex_width;
  unsigned int tex_height;

  gboolean vertical;
  float brightness;
  int blur_radius;
};

G_DEFINE_TYPE (ShellBlurEffect, shell_blur_effect, CLUTTER_TYPE_OFFSCREEN_EFFECT)

enum {
  PROP_0,
  PROP_BLUR_RADIUS,
  PROP_BRIGHTNESS,
  PROP_VERTICAL,
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = { NULL, };

static CoglPipeline*
create_pipeline (void)
{
  static CoglPipeline *base_pipeline = NULL;

  if (G_UNLIKELY (base_pipeline == NULL))
    {
      CoglSnippet *snippet;
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());

      base_pipeline = cogl_pipeline_new (ctx);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                                  gaussian_blur_glsl_declarations,
                                  NULL);
      cogl_snippet_set_replace (snippet, gaussian_blur_glsl);
      cogl_pipeline_add_layer_snippet (base_pipeline, 0, snippet);
      cogl_object_unref (snippet);

      cogl_pipeline_set_layer_null_texture (base_pipeline, 0);
      cogl_pipeline_set_layer_filters (base_pipeline,
                                       0,
                                       COGL_PIPELINE_FILTER_LINEAR,
                                       COGL_PIPELINE_FILTER_LINEAR);
      cogl_pipeline_set_layer_wrap_mode (base_pipeline,
                                         0,
                                         COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE);
    }

  return cogl_pipeline_copy (base_pipeline);
}

static gboolean
shell_blur_effect_pre_paint (ClutterEffect *effect)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (effect);
  ClutterOffscreenEffect *offscreen_effect =
    CLUTTER_OFFSCREEN_EFFECT (self);
  gboolean success;

  success = CLUTTER_EFFECT_CLASS (shell_blur_effect_parent_class)->pre_paint (effect);

  if (success)
    {
      CoglFramebuffer *offscreen;
      CoglHandle texture;

      texture = clutter_offscreen_effect_get_texture (offscreen_effect);
      self->tex_width = cogl_texture_get_width (texture) * DOWNSCALE_FACTOR;
      self->tex_height = cogl_texture_get_height (texture) * DOWNSCALE_FACTOR;

      if (self->pixel_step_uniform > -1)
        {
          ClutterRect rect;
          float pixel_step;

          clutter_offscreen_effect_get_target_rect (CLUTTER_OFFSCREEN_EFFECT (self),
                                                    &rect);
          if (self->vertical)
            pixel_step = 1.f / rect.size.height;
          else
            pixel_step = 1.f / rect.size.width;

          cogl_pipeline_set_uniform_1f (self->pipeline,
                                        self->pixel_step_uniform,
                                        pixel_step / DOWNSCALE_FACTOR);
        }

      if (self->blur_radius_uniform > -1)
        {
          cogl_pipeline_set_uniform_1f (self->pipeline,
                                        self->blur_radius_uniform,
                                        self->blur_radius / DOWNSCALE_FACTOR);
        }

      if (self->brightness_uniform > -1)
        {
          cogl_pipeline_set_uniform_1f (self->pipeline,
                                        self->brightness_uniform,
                                        self->brightness);
        }

      if (self->vertical_uniform > -1)
        {
          cogl_pipeline_set_uniform_1i (self->pipeline,
                                        self->vertical_uniform,
                                        self->vertical);
        }

      cogl_pipeline_set_layer_texture (self->pipeline, 0, texture);

      /* Texture is downscaled, draw downscaled as well */
      offscreen = cogl_get_draw_framebuffer ();
      cogl_framebuffer_scale (offscreen,
                              1.0 / DOWNSCALE_FACTOR,
                              1.0 / DOWNSCALE_FACTOR,
                              0.0);
    }

  return success;
}

static void
shell_blur_effect_paint_target (ClutterOffscreenEffect *effect)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (effect);
  CoglFramebuffer *framebuffer = cogl_get_draw_framebuffer ();
  ClutterActor *actor;
  float blur_radius_offset_h;
  float blur_radius_offset_v;
  guint8 paint_opacity;

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (self));
  paint_opacity = clutter_actor_get_paint_opacity (actor);

  cogl_pipeline_set_color4ub (self->pipeline,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);

  blur_radius_offset_h =
    self->blur_radius / (float) self->tex_width / DOWNSCALE_FACTOR;
  blur_radius_offset_v =
    self->blur_radius / (float) self->tex_height / DOWNSCALE_FACTOR;

  cogl_framebuffer_draw_textured_rectangle (framebuffer,
                                            self->pipeline,
                                            0, 0,
                                            self->tex_width,
                                            self->tex_height,
                                            blur_radius_offset_h,
                                            blur_radius_offset_v,
                                            1.f - blur_radius_offset_h,
                                            1.f - blur_radius_offset_v);
}

static void
shell_blur_effect_post_paint (ClutterEffect *effect)
{
  CoglFramebuffer *offscreen = cogl_get_draw_framebuffer ();

  cogl_framebuffer_scale (offscreen,
                          DOWNSCALE_FACTOR,
                          DOWNSCALE_FACTOR,
                          0.f);

  CLUTTER_EFFECT_CLASS (shell_blur_effect_parent_class)->post_paint (effect);
}

static CoglTexture*
shell_blur_effect_create_texture (ClutterOffscreenEffect *effect,
                                  float                   width,
                                  float                   height)
{
  CoglContext *ctx =
    clutter_backend_get_cogl_context (clutter_get_default_backend ());

  return cogl_texture_2d_new_with_size (ctx, width / 2.0, height / 2.0);
}

static gboolean
shell_blur_effect_modify_paint_volume (ClutterEffect      *effect,
                                       ClutterPaintVolume *volume)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (effect);
  ClutterVertex origin;
  float width;
  float height;

  clutter_paint_volume_get_origin (volume, &origin);
  width = clutter_paint_volume_get_width (volume);
  height = clutter_paint_volume_get_height (volume);

  if (self->vertical)
    {
      origin.y -= self->blur_radius;
      height += 2 * self->blur_radius;
    }
  else
    {
      origin.x -= self->blur_radius;
      width += 2 * self->blur_radius;
    }

  clutter_paint_volume_set_origin (volume, &origin);
  clutter_paint_volume_set_width (volume, width);
  clutter_paint_volume_set_height (volume, height);

  return TRUE;
}

static void
shell_blur_effect_finalize (GObject *object)
{
  ShellBlurEffect *self = (ShellBlurEffect *)object;

  g_clear_pointer (&self->pipeline, cogl_object_unref);

  G_OBJECT_CLASS (shell_blur_effect_parent_class)->finalize (object);
}

static void
shell_blur_effect_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (object);

  switch (prop_id)
    {
    case PROP_BLUR_RADIUS:
      g_value_set_int (value, self->blur_radius);
      break;

    case PROP_BRIGHTNESS:
      g_value_set_int (value, self->brightness);
      break;

    case PROP_VERTICAL:
      g_value_set_boolean (value, self->vertical);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
shell_blur_effect_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (object);

  switch (prop_id)
    {
    case PROP_BLUR_RADIUS:
      shell_blur_effect_set_blur_radius (self, g_value_get_int (value));
      break;

    case PROP_BRIGHTNESS:
      shell_blur_effect_set_brightness (self, g_value_get_float (value));
      break;

    case PROP_VERTICAL:
      shell_blur_effect_set_vertical (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
shell_blur_effect_class_init (ShellBlurEffectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class =
    CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);

  object_class->finalize = shell_blur_effect_finalize;
  object_class->get_property = shell_blur_effect_get_property;
  object_class->set_property = shell_blur_effect_set_property;

  effect_class->pre_paint = shell_blur_effect_pre_paint;
  effect_class->post_paint = shell_blur_effect_post_paint;
  effect_class->modify_paint_volume = shell_blur_effect_modify_paint_volume;

  offscreen_class->paint_target = shell_blur_effect_paint_target;
  offscreen_class->create_texture = shell_blur_effect_create_texture;

  properties[PROP_BLUR_RADIUS] =
    g_param_spec_int ("blur-radius",
                      "Blur radius",
                      "Blur radius",
                      0, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_BRIGHTNESS] =
    g_param_spec_float ("brightness",
                        "Brightness",
                        "Brightness",
                        0.f, 1.f, 1.f,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_VERTICAL] =
    g_param_spec_boolean ("vertical",
                          "Vertical",
                          "Whether the blur is vertical or horizontal",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
shell_blur_effect_init (ShellBlurEffect *self)
{
  self->blur_radius = 0;
  self->brightness = 1.f;
  self->vertical = FALSE;
  self->pipeline = create_pipeline ();

  self->blur_radius_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline, "blur_radius");
  self->brightness_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline, "brightness");
  self->pixel_step_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline, "pixel_step");
  self->vertical_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline, "vertical");
}

ShellBlurEffect *
shell_blur_effect_new (void)
{
  return g_object_new (SHELL_TYPE_BLUR_EFFECT, NULL);
}

int
shell_blur_effect_get_blur_radius (ShellBlurEffect *self)
{
  g_return_val_if_fail (SHELL_IS_BLUR_EFFECT (self), -1);

  return self->blur_radius;
}

void
shell_blur_effect_set_blur_radius (ShellBlurEffect *self,
                                   int              radius)
{
  g_return_if_fail (SHELL_IS_BLUR_EFFECT (self));

  if (self->blur_radius == radius)
    return;

  self->blur_radius = radius;
  clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BLUR_RADIUS]);
}

float
shell_blur_effect_get_brightness (ShellBlurEffect *self)
{
  g_return_val_if_fail (SHELL_IS_BLUR_EFFECT (self), FALSE);

  return self->brightness;
}

void
shell_blur_effect_set_brightness (ShellBlurEffect *self,
                                  float            brightness)
{
  g_return_if_fail (SHELL_IS_BLUR_EFFECT (self));

  if (self->brightness == brightness)
    return;

  self->brightness = brightness;
  clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BRIGHTNESS]);
}

gboolean
shell_blur_effect_get_vertical (ShellBlurEffect *self)
{
  g_return_val_if_fail (SHELL_IS_BLUR_EFFECT (self), FALSE);

  return self->vertical;
}

void
shell_blur_effect_set_vertical (ShellBlurEffect *self,
                                gboolean         vertical)
{
  g_return_if_fail (SHELL_IS_BLUR_EFFECT (self));

  if (self->vertical == vertical)
    return;

  self->vertical = vertical;
  clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VERTICAL]);
}
