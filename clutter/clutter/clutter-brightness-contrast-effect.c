/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010-2012 Inclusive Design Research Centre, OCAD University.
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
 *   Joseph Scheuhammer <clown@alum.mit.edu>
 */

/**
 * SECTION:clutter-brightness-contrast-effect
 * @short_description: Increase/decrease brightness and/or contrast of actor.
 * @see_also: #ClutterEffect, #ClutterOffscreenEffect
 *
 * #ClutterBrightnessContrastEffect is a sub-class of #ClutterEffect that
 * changes the overall brightness of a #ClutterActor.
 *
 * #ClutterBrightnessContrastEffect is available since Clutter 1.10
 */

#define CLUTTER_BRIGHTNESS_CONTRAST_EFFECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_BRIGHTNESS_CONTRAST_EFFECT, ClutterBrightnessContrastEffectClass))
#define CLUTTER_IS_BRIGHTNESS_CONTRAST_EFFECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_BRIGHTNESS_CONTRAST_EFFECT))
#define CLUTTER_BRIGHTNESS_CONTRAST_EFFECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_BRIGHTNESS_CONTRAST_EFFECT, ClutterBrightnessContrastEffectClass))

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include <math.h>

#define CLUTTER_ENABLE_EXPERIMENTAL_API

#include "clutter-brightness-contrast-effect.h"

#include <cogl/cogl.h>

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-offscreen-effect.h"
#include "clutter-private.h"

struct _ClutterBrightnessContrastEffect
{
  ClutterOffscreenEffect parent_instance;

  /* Brightness and contrast changes. */
  gfloat brightness_red;
  gfloat brightness_green;
  gfloat brightness_blue;

  gfloat contrast_red;
  gfloat contrast_green;
  gfloat contrast_blue;

  gint brightness_multiplier_uniform;
  gint brightness_offset_uniform;
  gint contrast_uniform;

  gint tex_width;
  gint tex_height;

  CoglPipeline *pipeline;
};

struct _ClutterBrightnessContrastEffectClass
{
  ClutterOffscreenEffectClass parent_class;

  CoglPipeline *base_pipeline;
};

/* Brightness effects in GLSL.
 */
static const gchar *brightness_contrast_decls =
  "uniform vec3 brightness_multiplier;\n"
  "uniform vec3 brightness_offset;\n"
  "uniform vec3 contrast;\n";

static const gchar *brightness_contrast_source =
  /* Apply the brightness. The brightness_offset is multiplied by the
     alpha to keep the color pre-multiplied */
  "cogl_color_out.rgb = (cogl_color_out.rgb * brightness_multiplier +\n"
  "                      brightness_offset * cogl_color_out.a);\n"
  /* Apply the contrast */
  "cogl_color_out.rgb = ((cogl_color_out.rgb - 0.5 * cogl_color_out.a) *\n"
  "                      contrast + 0.5 * cogl_color_out.a);\n";

static const ClutterColor no_brightness_change = { 0x7f, 0x7f, 0x7f, 0xff };
static const ClutterColor no_contrast_change = { 0x7f, 0x7f, 0x7f, 0xff };
static const gfloat no_change = 0.0f;

enum
{
  PROP_0,

  PROP_BRIGHTNESS,
  PROP_CONTRAST,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE (ClutterBrightnessContrastEffect,
               clutter_brightness_contrast_effect,
               CLUTTER_TYPE_OFFSCREEN_EFFECT);

static gboolean
will_have_no_effect (ClutterBrightnessContrastEffect *self)
{
  return (self->brightness_red == no_change &&
          self->brightness_green == no_change &&
          self->brightness_blue == no_change &&
          self->contrast_red == no_change &&
          self->contrast_green == no_change &&
          self->contrast_blue == no_change);
}

static gboolean
clutter_brightness_contrast_effect_pre_paint (ClutterEffect *effect)
{
  ClutterBrightnessContrastEffect *self = CLUTTER_BRIGHTNESS_CONTRAST_EFFECT (effect);
  ClutterEffectClass *parent_class;

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  if (will_have_no_effect (self))
    return FALSE;

  if (!clutter_feature_available (CLUTTER_FEATURE_SHADERS_GLSL))
    {
      /* if we don't have support for GLSL shaders then we
       * forcibly disable the ActorMeta
       */
      g_warning ("Unable to use the ClutterBrightnessContrastEffect: the "
                 "graphics hardware or the current GL driver does not "
                 "implement support for the GLSL shading language. The "
                 "effect will be disabled.");
      clutter_actor_meta_set_enabled (CLUTTER_ACTOR_META (effect), FALSE);
      return FALSE;
    }

  parent_class =
    CLUTTER_EFFECT_CLASS (clutter_brightness_contrast_effect_parent_class);
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
clutter_brightness_contrast_effect_paint_target (ClutterOffscreenEffect *effect)
{
  ClutterBrightnessContrastEffect *self = CLUTTER_BRIGHTNESS_CONTRAST_EFFECT (effect);
  ClutterActor *actor;
  guint8 paint_opacity;

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));
  paint_opacity = clutter_actor_get_paint_opacity (actor);

  cogl_pipeline_set_color4ub (self->pipeline,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);
  cogl_push_source (self->pipeline);

  cogl_rectangle (0, 0, self->tex_width, self->tex_height);

  cogl_pop_source ();
}

static void
clutter_brightness_contrast_effect_dispose (GObject *gobject)
{
  ClutterBrightnessContrastEffect *self = CLUTTER_BRIGHTNESS_CONTRAST_EFFECT (gobject);

  if (self->pipeline != NULL)
    {
      cogl_object_unref (self->pipeline);
      self->pipeline = NULL;
    }

  G_OBJECT_CLASS (clutter_brightness_contrast_effect_parent_class)->dispose (gobject);
}

static void
clutter_brightness_contrast_effect_set_property (GObject      *gobject,
                                                 guint        prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec)
{
  ClutterBrightnessContrastEffect *effect = CLUTTER_BRIGHTNESS_CONTRAST_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_BRIGHTNESS:
      {
        const ClutterColor *color = clutter_value_get_color (value);
        clutter_brightness_contrast_effect_set_brightness_full (effect,
                                                                color->red / 127.0f - 1.0f,
                                                                color->green / 127.0f - 1.0f,
                                                                color->blue / 127.0f - 1.0f);
      }
      break;

    case PROP_CONTRAST:
      {
        const ClutterColor *color = clutter_value_get_color (value);
        clutter_brightness_contrast_effect_set_contrast_full (effect,
                                                              color->red / 127.0f - 1.0f,
                                                              color->green / 127.0f - 1.0f,
                                                              color->blue / 127.0f - 1.0f);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_brightness_contrast_effect_get_property (GObject    *gobject,
                                                 guint      prop_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec)
{
  ClutterBrightnessContrastEffect *effect = CLUTTER_BRIGHTNESS_CONTRAST_EFFECT (gobject);
  ClutterColor color;

  switch (prop_id)
    {
    case PROP_BRIGHTNESS:
      {
        color.red = (effect->brightness_red + 1.0f) * 127.0f;
        color.green = (effect->brightness_green + 1.0f) * 127.0f;
        color.blue = (effect->brightness_blue + 1.0f) * 127.0f;
        color.alpha = 0xff;

        clutter_value_set_color (value, &color);
      }
      break;

    case PROP_CONTRAST:
      {
        color.red = (effect->contrast_red + 1.0f) * 127.0f;
        color.green = (effect->contrast_green + 1.0f) * 127.0f;
        color.blue = (effect->contrast_blue + 1.0f) * 127.0f;
        color.alpha = 0xff;

        clutter_value_set_color (value, &color);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_brightness_contrast_effect_class_init (ClutterBrightnessContrastEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->paint_target = clutter_brightness_contrast_effect_paint_target;

  effect_class->pre_paint = clutter_brightness_contrast_effect_pre_paint;

  gobject_class->set_property = clutter_brightness_contrast_effect_set_property;
  gobject_class->get_property = clutter_brightness_contrast_effect_get_property;
  gobject_class->dispose = clutter_brightness_contrast_effect_dispose;

  /**
   * ClutterBrightnessContrastEffect:brightness:
   *
   * The brightness change to apply to the effect.
   *
   * This property uses a #ClutterColor to represent the changes to each
   * color channel. The range is [ 0, 255 ], with 127 as the value used
   * to indicate no change; values smaller than 127 indicate a decrease
   * in brightness, and values larger than 127 indicate an increase in
   * brightness.
   *
   * Since: 1.10
   */
  obj_props[PROP_BRIGHTNESS] =
    clutter_param_spec_color ("brightness",
                              P_("Brightness"),
                              P_("The brightness change to apply"),
                              &no_brightness_change,
                              CLUTTER_PARAM_READWRITE);

  /**
   * ClutterBrightnessContrastEffect:contrast:
   *
   * The contrast change to apply to the effect.
   *
   * This property uses a #ClutterColor to represent the changes to each
   * color channel. The range is [ 0, 255 ], with 127 as the value used
   * to indicate no change; values smaller than 127 indicate a decrease
   * in contrast, and values larger than 127 indicate an increase in
   * contrast.
   *
   * Since: 1.10
   */
  obj_props[PROP_CONTRAST] =
    clutter_param_spec_color ("contrast",
                              P_("Contrast"),
                              P_("The contrast change to apply"),
                              &no_contrast_change,
                              CLUTTER_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
get_brightness_values (gfloat  value,
                       gfloat *multiplier,
                       gfloat *offset)
{
  if (value < 0.0f)
    {
      *multiplier = 1.0f + value;
      *offset = 0.0f;
    }
  else
    {
      *multiplier = 1.0f - value;
      *offset = value;
    }
}

static inline void
update_uniforms (ClutterBrightnessContrastEffect *self)
{
  if (self->brightness_multiplier_uniform > -1 &&
      self->brightness_offset_uniform > -1)
    {
      float brightness_multiplier[3];
      float brightness_offset[3];

      get_brightness_values (self->brightness_red,
                             brightness_multiplier + 0,
                             brightness_offset + 0);
      get_brightness_values (self->brightness_green,
                             brightness_multiplier + 1,
                             brightness_offset + 1);
      get_brightness_values (self->brightness_blue,
                             brightness_multiplier + 2,
                             brightness_offset + 2);

      cogl_pipeline_set_uniform_float (self->pipeline,
                                       self->brightness_multiplier_uniform,
                                       3, /* n_components */
                                       1, /* count */
                                       brightness_multiplier);
      cogl_pipeline_set_uniform_float (self->pipeline,
                                       self->brightness_offset_uniform,
                                       3, /* n_components */
                                       1, /* count */
                                       brightness_offset);
    }

  if (self->contrast_uniform > -1)
    {
      float contrast[3] = {
        tan ((self->contrast_red + 1) * G_PI_4),
        tan ((self->contrast_green + 1) * G_PI_4),
        tan ((self->contrast_blue + 1) * G_PI_4)
      };

      cogl_pipeline_set_uniform_float (self->pipeline,
                                       self->contrast_uniform,
                                       3, /* n_components */
                                       1, /* count */
                                       contrast);
    }
}

static void
clutter_brightness_contrast_effect_init (ClutterBrightnessContrastEffect *self)
{
  ClutterBrightnessContrastEffectClass *klass;

  self->brightness_red = no_change;
  self->brightness_green = no_change;
  self->brightness_blue = no_change;

  self->contrast_red = no_change;
  self->contrast_green = no_change;
  self->contrast_blue = no_change;

  klass = CLUTTER_BRIGHTNESS_CONTRAST_EFFECT_GET_CLASS (self);

  if (G_UNLIKELY (klass->base_pipeline == NULL))
    {
      CoglSnippet *snippet;
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());

      klass->base_pipeline = cogl_pipeline_new (ctx);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                  brightness_contrast_decls,
                                  brightness_contrast_source);
      cogl_pipeline_add_snippet (klass->base_pipeline, snippet);
      cogl_object_unref (snippet);

      cogl_pipeline_set_layer_null_texture (klass->base_pipeline,
                                            0, /* layer number */
                                            COGL_TEXTURE_TYPE_2D);
    }

  self->pipeline = cogl_pipeline_copy (klass->base_pipeline);

  self->brightness_multiplier_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline,
                                        "brightness_multiplier");
  self->brightness_offset_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline,
                                        "brightness_offset");
  self->contrast_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline, "contrast");

  update_uniforms (self);
}

/**
 * clutter_brightness_contrast_effect_new:
 *
 * Creates a new #ClutterBrightnessContrastEffect to be used with
 * clutter_actor_add_effect()
 *
 * Return value: (transfer full): the newly created
 *   #ClutterBrightnessContrastEffect or %NULL.  Use g_object_unref() when
 *   done.
 *
 * Since: 1.10
 */
ClutterEffect *
clutter_brightness_contrast_effect_new (void)
{
  return g_object_new (CLUTTER_TYPE_BRIGHTNESS_CONTRAST_EFFECT, NULL);
}

/**
 * clutter_brightness_contrast_effect_set_brightness_full:
 * @effect: a #ClutterBrightnessContrastEffect
 * @red: red component of the change in brightness
 * @green: green component of the change in brightness
 * @blue: blue component of the change in brightness
 *
 * The range for each component is [-1.0, 1.0] where 0.0 designates no change,
 * values below 0.0 mean a decrease in brightness, and values above indicate
 * an increase.
 *
 * Since: 1.10
 */
void
clutter_brightness_contrast_effect_set_brightness_full (ClutterBrightnessContrastEffect *effect,
                                                        gfloat                           red,
                                                        gfloat                           green,
                                                        gfloat                           blue)
{
  g_return_if_fail (CLUTTER_IS_BRIGHTNESS_CONTRAST_EFFECT (effect));

  if (red == effect->brightness_red &&
      green == effect->brightness_green &&
      blue == effect->brightness_blue)
    return;

  effect->brightness_red = red;
  effect->brightness_green = green;
  effect->brightness_blue = blue;

  update_uniforms (effect);

  clutter_effect_queue_repaint (CLUTTER_EFFECT (effect));

  g_object_notify_by_pspec (G_OBJECT (effect), obj_props[PROP_BRIGHTNESS]);
}

/**
 * clutter_brightness_contrast_effect_get_brightness:
 * @effect: a #ClutterBrightnessContrastEffect
 * @red: (out) (allow-none): return location for red component of the
 *    change in brightness
 * @green: (out) (allow-none): return location for green component of the
 *    change in brightness
 * @blue: (out) (allow-none): return location for blue component of the
 *    change in brightness
 *
 * Retrieves the change in brightness used by @effect.
 *
 * Since: 1.10
 */
void
clutter_brightness_contrast_effect_get_brightness (ClutterBrightnessContrastEffect *effect,
                                                   gfloat                          *red,
                                                   gfloat                          *green,
                                                   gfloat                          *blue)
{
  g_return_if_fail (CLUTTER_IS_BRIGHTNESS_CONTRAST_EFFECT (effect));

  if (red != NULL)
    *red = effect->brightness_red;

  if (green != NULL)
    *green = effect->brightness_green;

  if (blue != NULL)
    *blue = effect->brightness_blue;
}

/**
 * clutter_brightness_contrast_effect_set_brightness:
 * @effect: a #ClutterBrightnessContrastEffect
 * @brightness:  the brightness change for all three components (r, g, b)
 *
 * The range of @brightness is [-1.0, 1.0], where 0.0 designates no change;
 * a value below 0.0 indicates a decrease in brightness; and a value
 * above 0.0 indicates an increase of brightness.
 *
 * Since: 1.10
 */
void
clutter_brightness_contrast_effect_set_brightness (ClutterBrightnessContrastEffect *effect,
                                                   gfloat                           brightness)
{
  clutter_brightness_contrast_effect_set_brightness_full (effect,
                                                          brightness,
                                                          brightness,
                                                          brightness);
}

/**
 * clutter_brightness_contrast_effect_set_contrast_full:
 * @effect: a #ClutterBrightnessContrastEffect
 * @red: red component of the change in contrast
 * @green: green component of the change in contrast
 * @blue: blue component of the change in contrast
 *
 * The range for each component is [-1.0, 1.0] where 0.0 designates no change,
 * values below 0.0 mean a decrease in contrast, and values above indicate
 * an increase.
 *
 * Since: 1.10
 */
void
clutter_brightness_contrast_effect_set_contrast_full (ClutterBrightnessContrastEffect *effect,
                                                      gfloat                          red,
                                                      gfloat                          green,
                                                      gfloat                          blue)
{
  g_return_if_fail (CLUTTER_IS_BRIGHTNESS_CONTRAST_EFFECT (effect));

  if (red == effect->contrast_red &&
      green == effect->contrast_green &&
      blue == effect->contrast_blue)
    return;

  effect->contrast_red = red;
  effect->contrast_green = green;
  effect->contrast_blue = blue;

  update_uniforms (effect);

  clutter_effect_queue_repaint (CLUTTER_EFFECT (effect));

  g_object_notify_by_pspec (G_OBJECT (effect), obj_props[PROP_CONTRAST]);
}

/**
 * clutter_brightness_contrast_effect_get_contrast:
 * @effect: a #ClutterBrightnessContrastEffect
 * @red: (out) (allow-none): return location for red component of the
 *    change in contrast
 * @green: (out) (allow-none): return location for green component of the
 *    change in contrast
 * @blue: (out) (allow-none): return location for blue component of the
 *    change in contrast
 *
 * Retrieves the contrast value used by @effect.
 *
 * Since: 1.10
 */
void
clutter_brightness_contrast_effect_get_contrast (ClutterBrightnessContrastEffect *effect,
                                                 gfloat                          *red,
                                                 gfloat                          *green,
                                                 gfloat                          *blue)
{
  g_return_if_fail (CLUTTER_IS_BRIGHTNESS_CONTRAST_EFFECT (effect));

  if (red != NULL)
    *red = effect->contrast_red;

  if (green != NULL)
    *green = effect->contrast_green;

  if (blue != NULL)
    *blue = effect->contrast_blue;
}

/**
 * clutter_brightness_contrast_effect_set_contrast:
 * @effect: a #ClutterBrightnessContrastEffect
 * @contrast: contrast change for all three channels
 *
 * The range for @contrast is [-1.0, 1.0], where 0.0 designates no change;
 * a value below 0.0 indicates a decrease in contrast; and a value above
 * 0.0 indicates an increase.
 *
 * Since: 1.10
 */
void
clutter_brightness_contrast_effect_set_contrast (ClutterBrightnessContrastEffect *effect,
                                                 gfloat                           contrast)
{
  clutter_brightness_contrast_effect_set_contrast_full (effect,
                                                        contrast,
                                                        contrast,
                                                        contrast);
}
