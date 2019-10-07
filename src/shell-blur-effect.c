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

static const gchar *gaussian_blur_glsl =
"uniform sampler2D tex;                                            \n"
"uniform float blur_radius;                                        \n"
"uniform float pixel_step;                                         \n"
"uniform int vertical;                                             \n"
"                                                                  \n"
"float gaussian (float sigma, float x) {                           \n"
"  return exp ( - (x * x) / (2.0 * sigma * sigma));                \n"
"}                                                                 \n"
"                                                                  \n"
"void main() {                                                     \n"
"  float radius = blur_radius * 2.30348;                           \n"
"  float total = 0.0;                                              \n"
"  vec4 ret = vec4 (0);                                            \n"
"  vec2 uv = vec2(cogl_tex_coord0_in.st);                          \n"
"                                                                  \n"
"  int half_radius = max(int(radius / 2.0), 1);                    \n"
"                                                                  \n"
"  if (vertical != 0) {                                            \n"
"    for (int y = -half_radius; y < half_radius; y++) {            \n"
"      float fy = gaussian (radius / 2.0, float(y));               \n"
"      float offset_y = float(y) * pixel_step;                     \n"
"                                                                  \n"
"      vec4 c = texture2D(tex, uv + vec2(0.0, offset_y));          \n"
"      total += fy;                                                \n"
"      ret += c * fy;                                              \n"
"    }                                                             \n"
"  } else {                                                        \n"
"    for (int x = -half_radius; x < half_radius; x++) {            \n"
"      float fx = gaussian (radius / 2.0, float(x));               \n"
"      float offset_x = float(x) * pixel_step;                     \n"
"                                                                  \n"
"      vec4 c = texture2D(tex, uv + vec2(offset_x, 0.0));          \n"
"      total += fx;                                                \n"
"      ret += c * fx;                                              \n"
"    }                                                             \n"
"  }                                                               \n"
"                                                                  \n"
"  cogl_color_out = vec4 (ret / total);                            \n"
"}                                                                 \n";

struct _ShellBlurEffect
{
  ClutterShaderEffect parent_instance;

  gboolean vertical;
  int blur_radius;
};

G_DEFINE_TYPE (ShellBlurEffect, shell_blur_effect, CLUTTER_TYPE_SHADER_EFFECT)

enum {
  PROP_0,
  PROP_BLUR_RADIUS,
  PROP_VERTICAL,
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = { NULL, };

static gboolean
shell_blur_effect_pre_paint (ClutterEffect *effect)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (effect);
  gboolean success;

  success = CLUTTER_EFFECT_CLASS (shell_blur_effect_parent_class)->pre_paint (effect);

  if (success)
    {
      ClutterRect rect;
      float pixel_step;

      clutter_offscreen_effect_get_target_rect (CLUTTER_OFFSCREEN_EFFECT (self),
                                                &rect);
      if (self->vertical)
        pixel_step = 1.f / rect.size.height;
      else
        pixel_step = 1.f / rect.size.width;

      clutter_shader_effect_set_uniform (CLUTTER_SHADER_EFFECT (self),
                                         "blur_radius",
                                         G_TYPE_FLOAT,
                                         1, (float) self->blur_radius);

      clutter_shader_effect_set_uniform (CLUTTER_SHADER_EFFECT (self),
                                         "pixel_step",
                                         G_TYPE_FLOAT,
                                         1, pixel_step);

      clutter_shader_effect_set_uniform (CLUTTER_SHADER_EFFECT (self),
                                         "vertical",
                                         G_TYPE_INT,
                                         1, self->vertical);
    }

  return success;
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
      self->blur_radius = g_value_get_int (value);
      break;

    case PROP_VERTICAL:
      self->vertical = g_value_get_boolean (value);
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

  object_class->get_property = shell_blur_effect_get_property;
  object_class->set_property = shell_blur_effect_set_property;

  effect_class->pre_paint = shell_blur_effect_pre_paint;
  effect_class->modify_paint_volume = shell_blur_effect_modify_paint_volume;

  properties[PROP_BLUR_RADIUS] =
    g_param_spec_int ("blur-radius",
                      "Blur radius",
                      "Blur radius",
                      0, G_MAXINT, 0,
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
  self->vertical = FALSE;

  clutter_shader_effect_set_shader_source (CLUTTER_SHADER_EFFECT (self),
                                           gaussian_blur_glsl);
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
