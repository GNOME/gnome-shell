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

#include "shell-enum-types.h"

/**
 * SECTION:shell-blur-effect
 * @short_description: Blur effect for actors
 *
 * #ShellBlurEffect is a blur implementation based on Clutter. It also has
 * an optional brightness property.
 *
 * # Modes
 *
 * #ShellBlurEffect can work in @SHELL_BLUR_MODE_BACKGROUND and @SHELL_BLUR_MODE_ACTOR
 * modes. The actor mode blurs the actor itself, and all of its children. The
 * background mode blurs the pixels beneath the actor, but not the actor itself.
 *
 * @SHELL_BLUR_MODE_BACKGROUND can be computationally expensive, since the contents
 * beneath the actor cannot be cached, so beware of the performance implications
 * of using this blur mode.
 */

static const gchar *brightness_glsl_declarations =
"uniform float brightness;                                                 \n";

static const gchar *brightness_glsl =
"  cogl_color_out.rgb *= brightness;                                       \n";

#define MIN_DOWNSCALE_SIZE 256.f
#define MAX_SIGMA 6.f

typedef enum
{
  ACTOR_PAINTED = 1 << 0,
  BLUR_APPLIED = 1 << 1,
} CacheFlags;

typedef struct
{
  CoglFramebuffer *framebuffer;
  CoglPipeline *pipeline;
  CoglTexture *texture;
} FramebufferData;

struct _ShellBlurEffect
{
  ClutterEffect parent_instance;

  ClutterActor *actor;

  unsigned int tex_width;
  unsigned int tex_height;

  /* The cached contents */
  FramebufferData actor_fb;
  CacheFlags cache_flags;

  FramebufferData background_fb;
  FramebufferData brightness_fb;
  int brightness_uniform;

  ShellBlurMode mode;
  float downscale_factor;
  float brightness;
  int sigma;
};

G_DEFINE_TYPE (ShellBlurEffect, shell_blur_effect, CLUTTER_TYPE_EFFECT)

enum {
  PROP_0,
  PROP_SIGMA,
  PROP_BRIGHTNESS,
  PROP_MODE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = { NULL, };

static CoglPipeline*
create_base_pipeline (void)
{
  static CoglPipeline *base_pipeline = NULL;

  if (G_UNLIKELY (base_pipeline == NULL))
    {
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());

      base_pipeline = cogl_pipeline_new (ctx);
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
  static CoglPipeline *brightness_pipeline = NULL;

  if (G_UNLIKELY (brightness_pipeline == NULL))
    {
      CoglSnippet *snippet;

      brightness_pipeline = create_base_pipeline ();

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                  brightness_glsl_declarations,
                                  brightness_glsl);
      cogl_pipeline_add_snippet (brightness_pipeline, snippet);
      cogl_object_unref (snippet);
    }

  return cogl_pipeline_copy (brightness_pipeline);
}


static void
update_brightness (ShellBlurEffect *self,
                   uint8_t          paint_opacity)
{
  cogl_pipeline_set_color4ub (self->brightness_fb.pipeline,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);

  if (self->brightness_uniform > -1)
    {
      cogl_pipeline_set_uniform_1f (self->brightness_fb.pipeline,
                                    self->brightness_uniform,
                                    self->brightness);
    }
}

static void
setup_projection_matrix (CoglFramebuffer *framebuffer,
                         float            width,
                         float            height)
{
  graphene_matrix_t projection;

  graphene_matrix_init_translate (&projection,
                                  &GRAPHENE_POINT3D_INIT (-width / 2.0,
                                                          -height / 2.0,
                                                          0.f));
  graphene_matrix_scale (&projection, 2.0 / width, -2.0 / height, 1.f);

  cogl_framebuffer_set_projection_matrix (framebuffer, &projection);
}

static gboolean
update_fbo (FramebufferData *data,
            unsigned int     width,
            unsigned int     height,
            float            downscale_factor)
{
  CoglContext *ctx =
    clutter_backend_get_cogl_context (clutter_get_default_backend ());

  g_clear_pointer (&data->texture, cogl_object_unref);
  g_clear_object (&data->framebuffer);

  float new_width = floorf (width / downscale_factor);
  float new_height = floorf (height / downscale_factor);

  data->texture = cogl_texture_2d_new_with_size (ctx, new_width, new_height);
  if (!data->texture)
    return FALSE;

  cogl_pipeline_set_layer_texture (data->pipeline, 0, data->texture);

  data->framebuffer =
    COGL_FRAMEBUFFER (cogl_offscreen_new_with_texture (data->texture));
  if (!data->framebuffer)
    {
      g_warning ("%s: Unable to create an Offscreen buffer", G_STRLOC);
      return FALSE;
    }

  setup_projection_matrix (data->framebuffer, new_width, new_height);

  return TRUE;
}

static gboolean
update_actor_fbo (ShellBlurEffect *self,
                  unsigned int     width,
                  unsigned int     height,
                  float            downscale_factor)
{
  if (self->tex_width == width &&
      self->tex_height == height &&
      self->downscale_factor == downscale_factor &&
      self->actor_fb.framebuffer)
    {
      return TRUE;
    }

  self->cache_flags &= ~ACTOR_PAINTED;

  return update_fbo (&self->actor_fb, width, height, downscale_factor);
}

static gboolean
update_brightness_fbo (ShellBlurEffect *self,
                       unsigned int     width,
                       unsigned int     height,
                       float            downscale_factor)
{
  if (self->tex_width == width &&
      self->tex_height == height &&
      self->downscale_factor == downscale_factor &&
      self->brightness_fb.framebuffer)
    {
      return TRUE;
    }

  return update_fbo (&self->brightness_fb,
                     width, height,
                     downscale_factor);
}

static gboolean
update_background_fbo (ShellBlurEffect *self,
                       unsigned int     width,
                       unsigned int     height)
{
  if (self->tex_width == width &&
      self->tex_height == height &&
      self->background_fb.framebuffer)
    {
      return TRUE;
    }

  return update_fbo (&self->background_fb, width, height, 1.0);
}

static void
clear_framebuffer_data (FramebufferData *fb_data)
{
  g_clear_pointer (&fb_data->texture, cogl_object_unref);
  g_clear_object (&fb_data->framebuffer);
}

static float
calculate_downscale_factor (float width,
                            float height,
                            float sigma)
{
  float downscale_factor = 1.0;
  float scaled_width = width;
  float scaled_height = height;
  float scaled_sigma = sigma;

  /* This is the algorithm used by Firefox; keep downscaling until either the
   * blur radius is lower than the threshold, or the downscaled texture is too
   * small.
   */
  while (scaled_sigma > MAX_SIGMA &&
         scaled_width > MIN_DOWNSCALE_SIZE &&
         scaled_height > MIN_DOWNSCALE_SIZE)
    {
      downscale_factor *= 2.f;

      scaled_width = width / downscale_factor;
      scaled_height = height / downscale_factor;
      scaled_sigma = sigma / downscale_factor;
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
  clear_framebuffer_data (&self->actor_fb);
  clear_framebuffer_data (&self->background_fb);
  clear_framebuffer_data (&self->brightness_fb);

  /* we keep a back pointer here, to avoid going through the ActorMeta */
  self->actor = clutter_actor_meta_get_actor (meta);
}

static void
update_actor_box (ShellBlurEffect     *self,
                  ClutterPaintContext *paint_context,
                  ClutterActorBox     *source_actor_box)
{
  ClutterStageView *stage_view;
  float box_scale_factor = 1.0f;
  float origin_x, origin_y;
  float width, height;

  switch (self->mode)
    {
    case SHELL_BLUR_MODE_ACTOR:
      clutter_actor_get_allocation_box (self->actor, source_actor_box);
      break;

    case SHELL_BLUR_MODE_BACKGROUND:
      stage_view = clutter_paint_context_get_stage_view (paint_context);

      clutter_actor_get_transformed_position (self->actor, &origin_x, &origin_y);
      clutter_actor_get_transformed_size (self->actor, &width, &height);

      if (stage_view)
        {
          cairo_rectangle_int_t stage_view_layout;

          box_scale_factor = clutter_stage_view_get_scale (stage_view);
          clutter_stage_view_get_layout (stage_view, &stage_view_layout);

          origin_x -= stage_view_layout.x;
          origin_y -= stage_view_layout.y;
        }
      else
        {
          /* If we're drawing off stage, just assume scale = 1, this won't work
           * with stage-view scaling though.
           */
        }

      clutter_actor_box_set_origin (source_actor_box, origin_x, origin_y);
      clutter_actor_box_set_size (source_actor_box, width, height);

      clutter_actor_box_scale (source_actor_box, box_scale_factor);
      break;
    }

  clutter_actor_box_clamp_to_pixel (source_actor_box);
}

static void
add_blurred_pipeline (ShellBlurEffect  *self,
                      ClutterPaintNode *node,
                      uint8_t           paint_opacity)
{
  g_autoptr (ClutterPaintNode) pipeline_node = NULL;
  float width, height;

  /* Use the untransformed actor size here, since the framebuffer itself already
   * has the actor transform matrix applied.
   */
  clutter_actor_get_size (self->actor, &width, &height);

  update_brightness (self, paint_opacity);

  pipeline_node = clutter_pipeline_node_new (self->brightness_fb.pipeline);
  clutter_paint_node_set_static_name (pipeline_node, "ShellBlurEffect (final)");
  clutter_paint_node_add_child (node, pipeline_node);

  clutter_paint_node_add_rectangle (pipeline_node,
                                    &(ClutterActorBox) {
                                      0.f, 0.f,
                                      width,
                                      height,
                                    });
}

static ClutterPaintNode *
create_blur_nodes (ShellBlurEffect  *self,
                   ClutterPaintNode *node,
                   uint8_t           paint_opacity)
{
  g_autoptr (ClutterPaintNode) brightness_node = NULL;
  g_autoptr (ClutterPaintNode) blur_node = NULL;
  float width;
  float height;

  clutter_actor_get_size (self->actor, &width, &height);

  update_brightness (self, paint_opacity);
  brightness_node = clutter_layer_node_new_to_framebuffer (self->brightness_fb.framebuffer,
                                                           self->brightness_fb.pipeline);
  clutter_paint_node_set_static_name (brightness_node, "ShellBlurEffect (brightness)");
  clutter_paint_node_add_child (node, brightness_node);
  clutter_paint_node_add_rectangle (brightness_node,
                                    &(ClutterActorBox) {
                                      0.f, 0.f,
                                      width, height,
                                    });

  blur_node = clutter_blur_node_new (self->tex_width / self->downscale_factor,
                                     self->tex_height / self->downscale_factor,
                                     self->sigma / self->downscale_factor);
  clutter_paint_node_set_static_name (blur_node, "ShellBlurEffect (blur)");
  clutter_paint_node_add_child (brightness_node, blur_node);
  clutter_paint_node_add_rectangle (blur_node,
                                    &(ClutterActorBox) {
                                      0.f, 0.f,
                                      cogl_texture_get_width (self->brightness_fb.texture),
                                      cogl_texture_get_height (self->brightness_fb.texture),
                                    });

  self->cache_flags |= BLUR_APPLIED;

  return g_steal_pointer (&blur_node);
}

static void
paint_background (ShellBlurEffect     *self,
                  ClutterPaintNode    *node,
                  ClutterPaintContext *paint_context,
                  ClutterActorBox     *source_actor_box)
{
  g_autoptr (ClutterPaintNode) background_node = NULL;
  g_autoptr (ClutterPaintNode) blit_node = NULL;
  CoglFramebuffer *src;
  float transformed_x;
  float transformed_y;
  float transformed_width;
  float transformed_height;

  clutter_actor_box_get_origin (source_actor_box,
                                &transformed_x,
                                &transformed_y);
  clutter_actor_box_get_size (source_actor_box,
                              &transformed_width,
                              &transformed_height);

  /* Background layer node */
  background_node =
    clutter_layer_node_new_to_framebuffer (self->background_fb.framebuffer,
                                           self->background_fb.pipeline);
  clutter_paint_node_set_static_name (background_node, "ShellBlurEffect (background)");
  clutter_paint_node_add_child (node, background_node);
  clutter_paint_node_add_rectangle (background_node,
                                    &(ClutterActorBox) {
                                      0.f, 0.f,
                                      self->tex_width / self->downscale_factor,
                                      self->tex_height / self->downscale_factor,
                                    });

  /* Blit node */
  src = clutter_paint_context_get_framebuffer (paint_context);
  blit_node = clutter_blit_node_new (src);
  clutter_paint_node_set_static_name (blit_node, "ShellBlurEffect (blit)");
  clutter_paint_node_add_child (background_node, blit_node);
  clutter_blit_node_add_blit_rectangle (CLUTTER_BLIT_NODE (blit_node),
                                        transformed_x,
                                        transformed_y,
                                        0, 0,
                                        transformed_width,
                                        transformed_height);
}

static gboolean
update_framebuffers (ShellBlurEffect     *self,
                     ClutterPaintContext *paint_context,
                     ClutterActorBox     *source_actor_box)
{
  gboolean updated = FALSE;
  float downscale_factor;
  float height = -1;
  float width = -1;

  clutter_actor_box_get_size (source_actor_box, &width, &height);

  downscale_factor = calculate_downscale_factor (width, height, self->sigma);

  updated = update_actor_fbo (self, width, height, downscale_factor) &&
            update_brightness_fbo (self, width, height, downscale_factor);

  if (self->mode == SHELL_BLUR_MODE_BACKGROUND)
    updated = updated && update_background_fbo (self, width, height);

  self->tex_width = width;
  self->tex_height = height;
  self->downscale_factor = downscale_factor;

  return updated;
}

static void
add_actor_node (ShellBlurEffect  *self,
                ClutterPaintNode *node,
                int               opacity)
{
  g_autoptr (ClutterPaintNode) actor_node = NULL;

  actor_node = clutter_actor_node_new (self->actor, opacity);
  clutter_paint_node_add_child (node, actor_node);
}

static void
paint_actor_offscreen (ShellBlurEffect         *self,
                       ClutterPaintNode        *node,
                       ClutterEffectPaintFlags  flags)
{
  gboolean actor_dirty;

  actor_dirty = (flags & CLUTTER_EFFECT_PAINT_ACTOR_DIRTY) != 0;

  /* The actor offscreen framebuffer is updated already */
  if (actor_dirty || !(self->cache_flags & ACTOR_PAINTED))
    {
      g_autoptr (ClutterPaintNode) transform_node = NULL;
      g_autoptr (ClutterPaintNode) layer_node = NULL;
      graphene_matrix_t transform;

      /* Layer node */
      layer_node = clutter_layer_node_new_to_framebuffer (self->actor_fb.framebuffer,
                                                          self->actor_fb.pipeline);
      clutter_paint_node_set_static_name (layer_node, "ShellBlurEffect (actor offscreen)");
      clutter_paint_node_add_child (node, layer_node);
      clutter_paint_node_add_rectangle (layer_node,
                                        &(ClutterActorBox) {
                                          0.f, 0.f,
                                          self->tex_width / self->downscale_factor,
                                          self->tex_height / self->downscale_factor,
                                        });

      /* Transform node */
      graphene_matrix_init_scale (&transform,
                                  1.f / self->downscale_factor,
                                  1.f / self->downscale_factor,
                                  1.f);
      transform_node = clutter_transform_node_new (&transform);
      clutter_paint_node_set_static_name (transform_node, "ShellBlurEffect (downscale)");
      clutter_paint_node_add_child (layer_node, transform_node);

      /* Actor node */
      add_actor_node (self, transform_node, 255);

      self->cache_flags |= ACTOR_PAINTED;
    }
  else
    {
      g_autoptr (ClutterPaintNode) pipeline_node = NULL;

      pipeline_node = clutter_pipeline_node_new (self->actor_fb.pipeline);
      clutter_paint_node_set_static_name (pipeline_node,
                                          "ShellBlurEffect (actor texture)");
      clutter_paint_node_add_child (node, pipeline_node);
      clutter_paint_node_add_rectangle (pipeline_node,
                                        &(ClutterActorBox) {
                                          0.f, 0.f,
                                          self->tex_width / self->downscale_factor,
                                          self->tex_height / self->downscale_factor,
                                        });
    }
}

static gboolean
needs_repaint (ShellBlurEffect         *self,
               ClutterEffectPaintFlags  flags)
{
  gboolean actor_cached;
  gboolean blur_cached;
  gboolean actor_dirty;

  actor_dirty = (flags & CLUTTER_EFFECT_PAINT_ACTOR_DIRTY) != 0;
  blur_cached = (self->cache_flags & BLUR_APPLIED) != 0;
  actor_cached = (self->cache_flags & ACTOR_PAINTED) != 0;

  switch (self->mode)
    {
    case SHELL_BLUR_MODE_ACTOR:
      return actor_dirty || !blur_cached || !actor_cached;

    case SHELL_BLUR_MODE_BACKGROUND:
      return TRUE;
    }

  return TRUE;
}

static void
shell_blur_effect_paint_node (ClutterEffect           *effect,
                              ClutterPaintNode        *node,
                              ClutterPaintContext     *paint_context,
                              ClutterEffectPaintFlags  flags)
{
  ShellBlurEffect *self = SHELL_BLUR_EFFECT (effect);
  uint8_t paint_opacity;

  g_assert (self->actor != NULL);

  if (self->sigma > 0)
    {
      g_autoptr (ClutterPaintNode) blur_node = NULL;

      switch (self->mode)
        {
        case SHELL_BLUR_MODE_ACTOR:
          paint_opacity = clutter_actor_get_paint_opacity (self->actor);
          break;

        case SHELL_BLUR_MODE_BACKGROUND:
          paint_opacity = 255;
          break;

        default:
          g_assert_not_reached();
          break;
        }

      if (needs_repaint (self, flags))
        {
          ClutterActorBox source_actor_box;

          update_actor_box (self, paint_context, &source_actor_box);

          /* Failing to create or update the offscreen framebuffers prevents
           * the entire effect to be applied.
           */
          if (!update_framebuffers (self, paint_context, &source_actor_box))
            goto fail;

          blur_node = create_blur_nodes (self, node, paint_opacity);

          switch (self->mode)
            {
            case SHELL_BLUR_MODE_ACTOR:
              paint_actor_offscreen (self, blur_node, flags);
              break;

            case SHELL_BLUR_MODE_BACKGROUND:
              paint_background (self, blur_node, paint_context, &source_actor_box);
              break;
            }
        }
      else
        {
          /* Use the cached pipeline if no repaint is needed */
          add_blurred_pipeline (self, node, paint_opacity);
        }

      /* Background blur needs to paint the actor after painting the blurred
       * background.
       */
      switch (self->mode)
        {
        case SHELL_BLUR_MODE_ACTOR:
          break;

        case SHELL_BLUR_MODE_BACKGROUND:
          add_actor_node (self, node, -1);
          break;
        }

      return;
    }

fail:
  /* When no blur is applied, or the offscreen framebuffers
   * couldn't be created, fallback to simply painting the actor.
   */
  add_actor_node (self, node, -1);
}

static void
shell_blur_effect_finalize (GObject *object)
{
  ShellBlurEffect *self = (ShellBlurEffect *)object;

  clear_framebuffer_data (&self->actor_fb);
  clear_framebuffer_data (&self->background_fb);
  clear_framebuffer_data (&self->brightness_fb);

  g_clear_pointer (&self->actor_fb.pipeline, cogl_object_unref);
  g_clear_pointer (&self->background_fb.pipeline, cogl_object_unref);
  g_clear_pointer (&self->brightness_fb.pipeline, cogl_object_unref);

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
    case PROP_SIGMA:
      g_value_set_int (value, self->sigma);
      break;

    case PROP_BRIGHTNESS:
      g_value_set_float (value, self->brightness);
      break;

    case PROP_MODE:
      g_value_set_enum (value, self->mode);
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
    case PROP_SIGMA:
      shell_blur_effect_set_sigma (self, g_value_get_int (value));
      break;

    case PROP_BRIGHTNESS:
      shell_blur_effect_set_brightness (self, g_value_get_float (value));
      break;

    case PROP_MODE:
      shell_blur_effect_set_mode (self, g_value_get_enum (value));
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

  effect_class->paint_node = shell_blur_effect_paint_node;

  properties[PROP_SIGMA] =
    g_param_spec_int ("sigma",
                      "Sigma",
                      "Sigma",
                      0, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_BRIGHTNESS] =
    g_param_spec_float ("brightness",
                        "Brightness",
                        "Brightness",
                        0.f, 1.f, 1.f,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_MODE] =
    g_param_spec_enum ("mode",
                       "Blur mode",
                       "Blur mode",
                       SHELL_TYPE_BLUR_MODE,
                       SHELL_BLUR_MODE_ACTOR,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
shell_blur_effect_init (ShellBlurEffect *self)
{
  self->mode = SHELL_BLUR_MODE_ACTOR;
  self->sigma = 0;
  self->brightness = 1.f;

  self->actor_fb.pipeline = create_base_pipeline ();
  self->background_fb.pipeline = create_base_pipeline ();
  self->brightness_fb.pipeline = create_brightness_pipeline ();
  self->brightness_uniform =
    cogl_pipeline_get_uniform_location (self->brightness_fb.pipeline, "brightness");
}

ShellBlurEffect *
shell_blur_effect_new (void)
{
  return g_object_new (SHELL_TYPE_BLUR_EFFECT, NULL);
}

int
shell_blur_effect_get_sigma (ShellBlurEffect *self)
{
  g_return_val_if_fail (SHELL_IS_BLUR_EFFECT (self), -1);

  return self->sigma;
}

void
shell_blur_effect_set_sigma (ShellBlurEffect *self,
                             int              sigma)
{
  g_return_if_fail (SHELL_IS_BLUR_EFFECT (self));

  if (self->sigma == sigma)
    return;

  self->sigma = sigma;
  self->cache_flags &= ~BLUR_APPLIED;

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SIGMA]);
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
  self->cache_flags &= ~BLUR_APPLIED;

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_BRIGHTNESS]);
}

ShellBlurMode
shell_blur_effect_get_mode (ShellBlurEffect *self)
{
  g_return_val_if_fail (SHELL_IS_BLUR_EFFECT (self), -1);

  return self->mode;
}

void
shell_blur_effect_set_mode (ShellBlurEffect *self,
                            ShellBlurMode    mode)
{
  g_return_if_fail (SHELL_IS_BLUR_EFFECT (self));

  if (self->mode == mode)
    return;

  self->mode = mode;
  self->cache_flags &= ~BLUR_APPLIED;

  switch (mode)
    {
    case SHELL_BLUR_MODE_ACTOR:
      clear_framebuffer_data (&self->background_fb);
      break;

    case SHELL_BLUR_MODE_BACKGROUND:
    default:
      /* Do nothing */
      break;
    }

  if (self->actor)
    clutter_effect_queue_repaint (CLUTTER_EFFECT (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODE]);
}
