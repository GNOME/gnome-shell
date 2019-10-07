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

/*
 * ShellBlurEffect:
 *
 * Applies a gaussian blur to the actor. The gaussian blur is implemented in
 * 2 passes: vertical and horizontal. Thus, it is a linear shader.
 *
 * The actor is drawn into a downscaled framebuffer; the blur passes are applied
 * on the downscaled actor contents; and finally, the blurred contents are drawn
 * upscaled again.
 *
 * At last, this blur implementation cuts down the number of sampling operations
 * by exploiting the hardware interpolation that is performed when sampling between
 * pixel boundaries. This technique is described at:
 *
 * http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
 *
 * At last, ShellBlurEffect has an optional brighness value.
 */

static const gchar *gaussian_blur_glsl_declarations =
"uniform float blur_radius;                                        \n"
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
"  int horizontal = 1 - vertical;                                  \n"
"                                                                  \n"
"  vec4 ret = vec4 (0);                                            \n"
"  vec2 uv = vec2(cogl_tex_coord.st);                              \n"
"                                                                  \n"
"  float half_radius = radius / 2.0;                               \n"
"  int n_steps = max(int(ceil(half_radius)), 1);                   \n"
"                                                                  \n"
"  for (int i = 0; i < n_steps; i += 2) {                          \n"
"    float weight0 = gaussian (half_radius, float(i));             \n"
"    float weight1 = gaussian (half_radius, float(i) + 1);         \n"
"    float weight = weight0 + weight1;                             \n"
"                                                                  \n"
"    float step0 = float(i) * pixel_step;                          \n"
"    float step1 = float(i + 1) * pixel_step;                      \n"
"                                                                  \n"
"    float foffset = (step0 * weight0 + step1 * weight1) / weight; \n"
"    vec2 offset = vec2(foffset * float(horizontal),               \n"
"                       foffset * float(vertical));                \n"
"                                                                  \n"
"    vec4 c = texture2D(cogl_sampler, uv + offset);                \n"
"    total += weight;                                              \n"
"    ret += c * weight;                                            \n"
"                                                                  \n"
"    c = texture2D(cogl_sampler, uv - offset);                     \n"
"    total += weight;                                              \n"
"    ret += c * weight;                                            \n"
"  }                                                               \n"
"                                                                  \n"
"  cogl_texel = vec4 (ret / total);                                \n";

static const gchar *brightness_glsl_declarations =
"uniform float brightness;                                         \n";

static const gchar *brightness_glsl =
"  cogl_color_out.rgb *= brightness;                               \n";

#define MIN_DOWNSCALE_SIZE 256.f
#define MAX_BLUR_RADIUS 10.f

typedef enum
{
  VERTICAL,
  HORIZONTAL,
} BlurType;

typedef struct
{
  CoglFramebuffer *framebuffer;
  CoglTexture *texture;

  BlurType type;

  CoglPipeline *pipeline;
  int blur_radius_uniform;
  int pixel_step_uniform;
  int vertical_uniform;
} BlurData;

struct _ShellBlurEffect
{
  ClutterEffect parent_instance;

  ClutterActor *actor;
  uint8_t old_opacity_override;

  BlurData blur[2];

  unsigned int tex_width;
  unsigned int tex_height;

  /* The cached contents */
  CoglFramebuffer *framebuffer;
  CoglTexture *texture;

  CoglPipeline *pipeline;
  int brightness_uniform;

  float downscale_factor;
  float brightness;
  int blur_radius;
};

G_DEFINE_TYPE (ShellBlurEffect, shell_blur_effect, CLUTTER_TYPE_EFFECT)

enum {
  PROP_0,
  PROP_BLUR_RADIUS,
  PROP_BRIGHTNESS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = { NULL, };

static CoglPipeline*
create_blur_pipeline (void)
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


static CoglPipeline*
create_brightness_pipeline (void)
{
  static CoglPipeline *base_pipeline = NULL;

  if (G_UNLIKELY (base_pipeline == NULL))
    {
      CoglSnippet *snippet;
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());

      base_pipeline = cogl_pipeline_new (ctx);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                  brightness_glsl_declarations,
                                  brightness_glsl);
      cogl_pipeline_add_snippet (base_pipeline, snippet);
      cogl_object_unref (snippet);

      cogl_pipeline_set_layer_null_texture (base_pipeline, 0);
      cogl_pipeline_set_layer_filters (base_pipeline,
                                       0,
                                       COGL_PIPELINE_FILTER_LINEAR,
                                       COGL_PIPELINE_FILTER_LINEAR);
    }

  return cogl_pipeline_copy (base_pipeline);
}

static void
setup_blur (BlurData *blur,
            BlurType  type)
{
  blur->type = type;
  blur->pipeline = create_blur_pipeline ();

  blur->blur_radius_uniform =
    cogl_pipeline_get_uniform_location (blur->pipeline, "blur_radius");
  blur->pixel_step_uniform =
    cogl_pipeline_get_uniform_location (blur->pipeline, "pixel_step");
  blur->vertical_uniform =
    cogl_pipeline_get_uniform_location (blur->pipeline, "vertical");
}

static void
update_blur_uniforms (ShellBlurEffect *self,
                      BlurData        *blur)
{
  gboolean is_vertical = blur->type == VERTICAL;

  if (blur->pixel_step_uniform > -1)
    {
      float pixel_step;

      if (is_vertical)
        pixel_step = 1.f / cogl_texture_get_height (blur->texture);
      else
        pixel_step = 1.f / cogl_texture_get_width (blur->texture);

      cogl_pipeline_set_uniform_1f (blur->pipeline,
                                    blur->pixel_step_uniform,
                                    pixel_step);
    }

  if (blur->blur_radius_uniform > -1)
    {
      cogl_pipeline_set_uniform_1f (blur->pipeline,
                                    blur->blur_radius_uniform,
                                    self->blur_radius / self->downscale_factor);
    }

  if (blur->vertical_uniform > -1)
    {
      cogl_pipeline_set_uniform_1i (blur->pipeline,
                                    blur->vertical_uniform,
                                    is_vertical);
    }
}

static void
update_brightness_uniform (ShellBlurEffect *self)
{
  if (self->brightness_uniform > -1)
    {
      cogl_pipeline_set_uniform_1f (self->pipeline,
                                    self->brightness_uniform,
                                    self->brightness);
    }
}

static gboolean
update_fbo (CoglFramebuffer **framebuffer,
            CoglTexture     **texture,
            CoglPipeline     *pipeline,
            unsigned int      width,
            unsigned int      height,
            float             downscale_factor)
{
  CoglContext *ctx =
    clutter_backend_get_cogl_context (clutter_get_default_backend ());

  g_clear_pointer (texture, cogl_object_unref);
  g_clear_pointer (framebuffer, cogl_object_unref);

  *texture =
    cogl_texture_2d_new_with_size (ctx,
                                   width / downscale_factor,
                                   height / downscale_factor);
  if (!*texture)
    return FALSE;

  cogl_pipeline_set_layer_texture (pipeline, 0, *texture);

  *framebuffer = cogl_offscreen_new_with_texture (*texture);
  if (!*framebuffer)
    {
      g_warning ("%s: Unable to create an Offscreen buffer", G_STRLOC);
      return FALSE;
    }

  return TRUE;
}

static gboolean
update_cache_fbo (ClutterEffect *effect,
                  unsigned int   width,
                  unsigned int   height,
                  float          downscale_factor)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (effect);

  if (self->tex_width == width &&
      self->tex_height == height &&
      self->framebuffer)
    {
      return TRUE;
    }

  return update_fbo (&self->framebuffer,
                     &self->texture,
                     self->pipeline,
                     width, height,
                     downscale_factor);
}

static gboolean
update_blur_fbo (ClutterEffect *effect,
                 BlurData      *blur,
                 unsigned int   width,
                 unsigned int   height,
                 float          downscale_factor)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (effect);

  if (self->tex_width == width &&
      self->tex_height == height &&
      blur->framebuffer)
    {
      return TRUE;
    }

  return update_fbo (&blur->framebuffer,
                     &blur->texture,
                     blur->pipeline,
                     width, height,
                     downscale_factor);
}

static void
clear_blur (BlurData *blur)
{
  g_clear_pointer (&blur->texture, cogl_object_unref);
  g_clear_pointer (&blur->framebuffer, cogl_object_unref);
}

static void
setup_projection_matrix (CoglFramebuffer *framebuffer,
                         float            width,
                         float            height,
                         float            downscale_factor)
{
  CoglMatrix projection;
  float downscaled_width = width / downscale_factor;
  float downscaled_height = height / downscale_factor;

  cogl_matrix_init_identity (&projection);
  cogl_matrix_scale (&projection,
                     2.0 / downscaled_width,
                     -2.0 / downscaled_height,
                     1.f);
  cogl_matrix_translate (&projection,
                         -downscaled_width / 2.0,
                         -downscaled_height / 2.0,
                         0);

  cogl_framebuffer_set_projection_matrix (framebuffer, &projection);
}

static float
calculate_downscale_factor (float width,
                            float height,
                            float blur_radius)
{
  float downscale_factor = 1.0;
  float scaled_width = width;
  float scaled_height = height;
  float scaled_radius = blur_radius;

  /* This is the algorithm used by Firefox; keep downscaling until either the
   * blur radius is lower than the threshold, or the downscaled texture is too
   * small.
   */
  while (scaled_radius > MAX_BLUR_RADIUS &&
         scaled_width > MIN_DOWNSCALE_SIZE &&
         scaled_height > MIN_DOWNSCALE_SIZE)
    {
      downscale_factor *= 2.f;

      scaled_width = width / downscale_factor;
      scaled_height = height / downscale_factor;
      scaled_radius = blur_radius / downscale_factor;
    }

  return downscale_factor;
}

static void
shell_blur_effect_set_actor (ClutterActorMeta *meta,
                             ClutterActor     *actor)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (meta);
  ClutterActorMetaClass *meta_class;

  meta_class = CLUTTER_ACTOR_META_CLASS (shell_blur_effect_parent_class);
  meta_class->set_actor (meta, actor);

  /* clear out the previous state */
  clear_blur (&self->blur[VERTICAL]);
  clear_blur (&self->blur[HORIZONTAL]);

  g_clear_pointer (&self->texture, cogl_object_unref);
  g_clear_pointer (&self->framebuffer, cogl_object_unref);

  /* we keep a back pointer here, to avoid going through the ActorMeta */
  self->actor = clutter_actor_meta_get_actor (meta);
}

static gboolean
shell_blur_effect_pre_paint (ClutterEffect *effect)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (effect);
  ClutterActorBox box;
  CoglColor transparent;
  BlurData *vblur;
  BlurData *hblur;
  float width = -1;
  float height = -1;
  float resource_scale;
  float ceiled_resource_scale;
  float downscale_factor;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  if (!self->actor)
    return FALSE;

  if (clutter_actor_get_resource_scale (self->actor, &resource_scale))
    ceiled_resource_scale = ceilf (resource_scale);
  else
    g_assert_not_reached ();

  clutter_actor_get_allocation_box (self->actor, &box);
  clutter_actor_box_scale (&box, ceiled_resource_scale);
  clutter_actor_box_get_size (&box, &width, &height);

  width = ceilf (width);
  height = ceilf (height);
  downscale_factor = calculate_downscale_factor (width, height, self->blur_radius);

  if (!update_blur_fbo (effect, &self->blur[VERTICAL], width, height, downscale_factor) ||
      !update_blur_fbo (effect, &self->blur[HORIZONTAL], width, height, downscale_factor) ||
      !update_cache_fbo (effect, width, height, downscale_factor))
    {
      return FALSE;
    }

  self->tex_width = width;
  self->tex_height = height;
  self->downscale_factor = downscale_factor;

  self->old_opacity_override = clutter_actor_get_opacity_override (self->actor);
  clutter_actor_set_opacity_override (self->actor, 0xff);

  /* Draw the actor contents at the vertical blur framebuffer */
  vblur = &self->blur[VERTICAL];
  hblur = &self->blur[HORIZONTAL];

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  cogl_push_framebuffer (vblur->framebuffer);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  setup_projection_matrix (vblur->framebuffer, width, height, downscale_factor);
  setup_projection_matrix (hblur->framebuffer, width, height, downscale_factor);
  setup_projection_matrix (self->framebuffer, width, height, downscale_factor);

  cogl_color_init_from_4ub (&transparent, 0, 0, 0, 0);
  cogl_framebuffer_clear (vblur->framebuffer,
                          COGL_BUFFER_BIT_COLOR |
                          COGL_BUFFER_BIT_DEPTH,
                          &transparent);
  cogl_framebuffer_clear (hblur->framebuffer,
                          COGL_BUFFER_BIT_COLOR |
                          COGL_BUFFER_BIT_DEPTH,
                          &transparent);
  cogl_framebuffer_clear (self->framebuffer,
                          COGL_BUFFER_BIT_COLOR |
                          COGL_BUFFER_BIT_DEPTH,
                          &transparent);

  cogl_framebuffer_push_matrix (vblur->framebuffer);

  cogl_framebuffer_scale (vblur->framebuffer,
                          1.f / downscale_factor,
                          1.f / downscale_factor,
                          1.f);

  return TRUE;
}

static void
paint_texture (ShellBlurEffect *self)
{
  CoglFramebuffer *framebuffer = cogl_get_draw_framebuffer ();
  ClutterActor *actor;
  CoglMatrix modelview;
  BlurData *vblur;
  BlurData *hblur;
  guint8 paint_opacity;
  float resource_scale;

  vblur = &self->blur[VERTICAL];
  hblur = &self->blur[HORIZONTAL];

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (self));
  paint_opacity = clutter_actor_get_paint_opacity (actor);

  /* Pass 1:
   *
   * Draw the actor contents (which is in the vblur framebuffer
   * at this point) into the hblur framebuffer. This will run the
   * vertical blur fragment shader, and will output a vertically
   * blurred image.
   */
  update_blur_uniforms (self, vblur);
  cogl_pipeline_set_color4ub (vblur->pipeline,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);

  cogl_framebuffer_draw_textured_rectangle (hblur->framebuffer,
                                            vblur->pipeline,
                                            0, 0,
                                            cogl_texture_get_width (vblur->texture),
                                            cogl_texture_get_height (vblur->texture),
                                            0.f, 0.f,
                                            1.f, 1.f);

  /* Pass 2:
   *
   * Now the opposite; draw the vertically blurred image using the
   * horizontal blur pipeline into the cache framebuffer.
   */
  update_blur_uniforms (self, hblur);
  cogl_pipeline_set_color4ub (hblur->pipeline,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);

  cogl_framebuffer_draw_textured_rectangle (self->framebuffer,
                                            hblur->pipeline,
                                            0, 0,
                                            cogl_texture_get_width (hblur->texture),
                                            cogl_texture_get_height (hblur->texture),
                                            0.f, 0.f,
                                            1.f, 1.f);

  /* Now self->texture contains the vertically AND horizontally blurred
   * texture. Paint that texture into the onscreen framebuffer, this time
   * without any blur shader applied.
   */
  cogl_framebuffer_push_matrix (framebuffer);
  cogl_framebuffer_get_modelview_matrix (framebuffer, &modelview);

  if (clutter_actor_get_resource_scale (self->actor, &resource_scale) &&
      resource_scale != 1.0f)
    {
      float paint_scale = 1.0f / resource_scale;
      cogl_matrix_scale (&modelview, paint_scale, paint_scale, 1);
    }

  cogl_framebuffer_set_modelview_matrix (framebuffer, &modelview);

  update_brightness_uniform (self);
  cogl_framebuffer_draw_textured_rectangle (framebuffer,
                                            self->pipeline,
                                            0, 0,
                                            self->tex_width,
                                            self->tex_height,
                                            0.f, 0.f,
                                            1.f, 1.f);

  cogl_framebuffer_pop_matrix (framebuffer);
}

static void
shell_blur_effect_paint (ClutterEffect           *effect,
                         ClutterEffectPaintFlags  flags)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (effect);

  if (!self->framebuffer || (flags & CLUTTER_EFFECT_PAINT_ACTOR_DIRTY))
    {
      /* Chain up to the parent paint method which will call the pre and
         post paint functions to update the image */
      CLUTTER_EFFECT_CLASS (shell_blur_effect_parent_class)->paint (effect, flags);
    }
  else
    {
      /* If we've already got a cached image and the actor hasn't been redrawn
       * then we can just use the cached image in the FBO.
       */
      paint_texture (self);
    }
}

static void
shell_blur_effect_post_paint (ClutterEffect *effect)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (effect);

  cogl_framebuffer_pop_matrix (self->blur[VERTICAL].framebuffer);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  cogl_pop_framebuffer ();
  G_GNUC_END_IGNORE_DEPRECATIONS;

  clutter_actor_set_opacity_override (self->actor, self->old_opacity_override);

  paint_texture (self);
}

static gboolean
shell_blur_effect_modify_paint_volume (ClutterEffect      *effect,
                                       ClutterPaintVolume *volume)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (effect);
  graphene_point3d_t origin;
  float width;
  float height;

  clutter_paint_volume_get_origin (volume, &origin);
  width = clutter_paint_volume_get_width (volume);
  height = clutter_paint_volume_get_height (volume);

  origin.y -= self->blur_radius;
  origin.x -= self->blur_radius;

  height += 2 * self->blur_radius;
  width += 2 * self->blur_radius;

  clutter_paint_volume_set_origin (volume, &origin);
  clutter_paint_volume_set_width (volume, width);
  clutter_paint_volume_set_height (volume, height);

  return TRUE;
}

static void
shell_blur_effect_finalize (GObject *object)
{
  ShellBlurEffect *self = (ShellBlurEffect *)object;

  clear_blur (&self->blur[VERTICAL]);
  clear_blur (&self->blur[HORIZONTAL]);

  g_clear_pointer (&self->blur[VERTICAL].pipeline, cogl_object_unref);
  g_clear_pointer (&self->blur[HORIZONTAL].pipeline, cogl_object_unref);

  g_clear_pointer (&self->texture, cogl_object_unref);
  g_clear_pointer (&self->pipeline, cogl_object_unref);
  g_clear_pointer (&self->framebuffer, cogl_object_unref);

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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
shell_blur_effect_class_init (ShellBlurEffectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorMetaClass *meta_class = CLUTTER_ACTOR_META_CLASS (klass);
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);

  object_class->finalize = shell_blur_effect_finalize;
  object_class->get_property = shell_blur_effect_get_property;
  object_class->set_property = shell_blur_effect_set_property;

  meta_class->set_actor = shell_blur_effect_set_actor;

  effect_class->pre_paint = shell_blur_effect_pre_paint;
  effect_class->paint = shell_blur_effect_paint;
  effect_class->post_paint = shell_blur_effect_post_paint;
  effect_class->modify_paint_volume = shell_blur_effect_modify_paint_volume;

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

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
shell_blur_effect_init (ShellBlurEffect *self)
{
  self->blur_radius = 0;
  self->brightness = 1.f;

  self->pipeline = create_brightness_pipeline ();
  self->brightness_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline, "brightness");

  setup_blur (&self->blur[VERTICAL], VERTICAL);
  setup_blur (&self->blur[HORIZONTAL], HORIZONTAL);
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
