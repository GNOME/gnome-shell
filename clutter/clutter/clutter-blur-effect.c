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
 * #ClutterBlurEffect is available since Clutter 1.4
 */

#define CLUTTER_BLUR_EFFECT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BLUR_EFFECT, ClutterBlurEffectClass))
#define CLUTTER_IS_BLUR_EFFECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BLUR_EFFECT))
#define CLUTTER_BLUR_EFFECT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BLUR_EFFECT, ClutterBlurEffectClass))

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#define CLUTTER_ENABLE_EXPERIMENTAL_API

#include "clutter-blur-effect.h"

#include "cogl/cogl.h"

#include "clutter-debug.h"
#include "clutter-offscreen-effect.h"
#include "clutter-private.h"

#define BLUR_PADDING    2

/* FIXME - lame shader; we should really have a decoupled
 * horizontal/vertical two pass shader for the gaussian blur
 */
static const gchar *box_blur_glsl_declarations =
"uniform vec2 pixel_step;\n";
#define SAMPLE(offx, offy) \
  "cogl_texel += texture2D (cogl_sampler, cogl_tex_coord.st + pixel_step * " \
  "vec2 (" G_STRINGIFY (offx) ", " G_STRINGIFY (offy) "));\n"
static const gchar *box_blur_glsl_shader =
"  cogl_texel = texture2D (cogl_sampler, cogl_tex_coord.st);\n"
  SAMPLE (-1.0, -1.0)
  SAMPLE ( 0.0, -1.0)
  SAMPLE (+1.0, -1.0)
  SAMPLE (-1.0,  0.0)
  SAMPLE (+1.0,  0.0)
  SAMPLE (-1.0, +1.0)
  SAMPLE ( 0.0, +1.0)
  SAMPLE (+1.0, +1.0)
"  cogl_texel /= 9.0;\n";
#undef SAMPLE

struct _ClutterBlurEffect
{
  ClutterOffscreenEffect parent_instance;

  /* a back pointer to our actor, so that we can query it */
  ClutterActor *actor;

  gint pixel_step_uniform;

  gint tex_width;
  gint tex_height;

  CoglPipeline *pipeline;
};

struct _ClutterBlurEffectClass
{
  ClutterOffscreenEffectClass parent_class;

  CoglPipeline *base_pipeline;
};

G_DEFINE_TYPE (ClutterBlurEffect,
               clutter_blur_effect,
               CLUTTER_TYPE_OFFSCREEN_EFFECT);

static gboolean
clutter_blur_effect_pre_paint (ClutterEffect *effect)
{
  ClutterBlurEffect *self = CLUTTER_BLUR_EFFECT (effect);
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

  parent_class = CLUTTER_EFFECT_CLASS (clutter_blur_effect_parent_class);
  if (parent_class->pre_paint (effect))
    {
      ClutterOffscreenEffect *offscreen_effect =
        CLUTTER_OFFSCREEN_EFFECT (effect);
      CoglHandle texture;

      texture = clutter_offscreen_effect_get_texture (offscreen_effect);
      self->tex_width = cogl_texture_get_width (texture);
      self->tex_height = cogl_texture_get_height (texture);

      if (self->pixel_step_uniform > -1)
        {
          gfloat pixel_step[2];

          pixel_step[0] = 1.0f / self->tex_width;
          pixel_step[1] = 1.0f / self->tex_height;

          cogl_pipeline_set_uniform_float (self->pipeline,
                                           self->pixel_step_uniform,
                                           2, /* n_components */
                                           1, /* count */
                                           pixel_step);
        }

      cogl_pipeline_set_layer_texture (self->pipeline, 0, texture);

      return TRUE;
    }
  else
    return FALSE;
}

static void
clutter_blur_effect_paint_target (ClutterOffscreenEffect *effect)
{
  ClutterBlurEffect *self = CLUTTER_BLUR_EFFECT (effect);
  guint8 paint_opacity;

  paint_opacity = clutter_actor_get_paint_opacity (self->actor);

  cogl_pipeline_set_color4ub (self->pipeline,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);
  cogl_push_source (self->pipeline);

  cogl_rectangle (0, 0, self->tex_width, self->tex_height);

  cogl_pop_source ();
}

static gboolean
clutter_blur_effect_get_paint_volume (ClutterEffect      *effect,
                                      ClutterPaintVolume *volume)
{
  gfloat cur_width, cur_height;
  ClutterVertex origin;

  clutter_paint_volume_get_origin (volume, &origin);
  cur_width = clutter_paint_volume_get_width (volume);
  cur_height = clutter_paint_volume_get_height (volume);

  origin.x -= BLUR_PADDING;
  origin.y -= BLUR_PADDING;
  cur_width += 2 * BLUR_PADDING;
  cur_height += 2 * BLUR_PADDING;
  clutter_paint_volume_set_origin (volume, &origin);
  clutter_paint_volume_set_width (volume, cur_width);
  clutter_paint_volume_set_height (volume, cur_height);

  return TRUE;
}

static void
clutter_blur_effect_dispose (GObject *gobject)
{
  ClutterBlurEffect *self = CLUTTER_BLUR_EFFECT (gobject);

  if (self->pipeline != NULL)
    {
      cogl_object_unref (self->pipeline);
      self->pipeline = NULL;
    }

  G_OBJECT_CLASS (clutter_blur_effect_parent_class)->dispose (gobject);
}

static void
clutter_blur_effect_class_init (ClutterBlurEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  gobject_class->dispose = clutter_blur_effect_dispose;

  effect_class->pre_paint = clutter_blur_effect_pre_paint;
  effect_class->get_paint_volume = clutter_blur_effect_get_paint_volume;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->paint_target = clutter_blur_effect_paint_target;
}

static void
clutter_blur_effect_init (ClutterBlurEffect *self)
{
  ClutterBlurEffectClass *klass = CLUTTER_BLUR_EFFECT_GET_CLASS (self);

  if (G_UNLIKELY (klass->base_pipeline == NULL))
    {
      CoglSnippet *snippet;
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());

      klass->base_pipeline = cogl_pipeline_new (ctx);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP,
                                  box_blur_glsl_declarations,
                                  NULL);
      cogl_snippet_set_replace (snippet, box_blur_glsl_shader);
      cogl_pipeline_add_layer_snippet (klass->base_pipeline, 0, snippet);
      cogl_object_unref (snippet);

      cogl_pipeline_set_layer_null_texture (klass->base_pipeline,
                                            0, /* layer number */
                                            COGL_TEXTURE_TYPE_2D);
    }

  self->pipeline = cogl_pipeline_copy (klass->base_pipeline);

  self->pixel_step_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline, "pixel_step");
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
