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
 * SECTION:clutter-desaturate-effect
 * @short_description: A desaturation effect
 * @see_also: #ClutterEffect, #ClutterOffscreenEffect
 *
 * #ShellGridDesaturateEffect is a sub-class of #ClutterEffect that
 * desaturates the color of an actor and its contents. The strenght
 * of the desaturation effect is controllable and animatable through
 * the #ShellGridDesaturateEffect:factor property.
 *
 * #ShellGridDesaturateEffect is available since Clutter 1.4
 */

#include <math.h>

#include "shell-grid-desaturate-effect.h"

#include <cogl/cogl.h>

static CoglPipeline *base_pipeline = NULL;

struct _ShellGridDesaturateEffect
{
  ClutterOffscreenEffect parent_instance;

  /* the desaturation factor, also known as "strength" */
  gdouble factor;

  /* an area not to be shaded */
  graphene_rect_t unshaded_rect;

  gint factor_uniform;
  gint unshaded_uniform;

  gint tex_width;
  gint tex_height;

  gboolean unshaded_uniform_dirty;

  CoglPipeline *pipeline;
};

/* the magic gray vec3 has been taken from the NTSC conversion weights
 * as defined by:
 *
 *   "OpenGL Superbible, 4th edition"
 *   -- Richard S. Wright Jr, Benjamin Lipchak, Nicholas Haemel
 *   Addison-Wesley
 */
static const gchar *desaturate_glsl_declarations =
  "uniform float factor;\n"
  "uniform vec4 unshaded;\n"
  "\n"
  "vec3 desaturate (const vec3 color, const float desaturation)\n"
  "{\n"
  "  if ((cogl_tex_coord0_in[0] > unshaded[0]) && (cogl_tex_coord0_in[0] < unshaded[2]) &&\n"
  "      (cogl_tex_coord0_in[1] > unshaded[1]) && (cogl_tex_coord0_in[1] < unshaded[3]))\n"
  "    return color;\n"
  "  const vec3 gray_conv = vec3 (0.299, 0.587, 0.114);\n"
  "  vec3 gray = vec3 (dot (gray_conv, color));\n"
  "  return vec3 (mix (color.rgb, gray, desaturation));\n"
  "}\n";

static const gchar *desaturate_glsl_source =
  "  cogl_color_out.rgb = desaturate (cogl_color_out.rgb, factor);\n";

enum
{
  PROP_0,

  PROP_FACTOR,
  PROP_UNSHADED_RECT,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE (ShellGridDesaturateEffect,
               shell_grid_desaturate_effect,
               CLUTTER_TYPE_OFFSCREEN_EFFECT);

static void update_unshaded_uniform (ShellGridDesaturateEffect *self);

static gboolean
shell_grid_desaturate_effect_pre_paint (ClutterEffect       *effect,
                                        ClutterPaintContext *paint_context)
{
  ShellGridDesaturateEffect *self = SHELL_GRID_DESATURATE_EFFECT (effect);
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

  parent_class = CLUTTER_EFFECT_CLASS (shell_grid_desaturate_effect_parent_class);
  if (parent_class->pre_paint (effect, paint_context))
    {
      ClutterOffscreenEffect *offscreen_effect =
        CLUTTER_OFFSCREEN_EFFECT (effect);
      CoglHandle texture;

      texture = clutter_offscreen_effect_get_texture (offscreen_effect);
      self->tex_width = cogl_texture_get_width (texture);
      self->tex_height = cogl_texture_get_height (texture);

      if (self->unshaded_uniform_dirty)
        update_unshaded_uniform (self);

      cogl_pipeline_set_layer_texture (self->pipeline, 0, texture);

      return TRUE;
    }
  else
    return FALSE;
}

static void
shell_grid_desaturate_effect_paint_target (ClutterOffscreenEffect *effect,
                                           ClutterPaintContext    *paint_context)
{
  ShellGridDesaturateEffect *self = SHELL_GRID_DESATURATE_EFFECT (effect);
  CoglFramebuffer *framebuffer;
  ClutterActor *actor;
  CoglHandle texture;
  guint8 paint_opacity;

  texture = clutter_offscreen_effect_get_texture (effect);
  cogl_pipeline_set_layer_texture (self->pipeline, 0, texture);

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));
  paint_opacity = clutter_actor_get_paint_opacity (actor);

  cogl_pipeline_set_color4ub (self->pipeline,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);

  framebuffer = clutter_paint_context_get_framebuffer (paint_context);
  cogl_framebuffer_draw_rectangle (framebuffer,
                                   self->pipeline,
                                   0, 0,
                                   cogl_texture_get_width (texture),
                                   cogl_texture_get_height (texture));
}

static void
shell_grid_desaturate_effect_dispose (GObject *gobject)
{
  ShellGridDesaturateEffect *self = SHELL_GRID_DESATURATE_EFFECT (gobject);

  if (self->pipeline != NULL)
    {
      cogl_object_unref (self->pipeline);
      self->pipeline = NULL;
    }

  G_OBJECT_CLASS (shell_grid_desaturate_effect_parent_class)->dispose (gobject);
}

static void
shell_grid_desaturate_effect_set_property (GObject      *gobject,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  ShellGridDesaturateEffect *effect = SHELL_GRID_DESATURATE_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_FACTOR:
      shell_grid_desaturate_effect_set_factor (effect,
                                               g_value_get_double (value));
      break;

    case PROP_UNSHADED_RECT:
      shell_grid_desaturate_effect_set_unshaded_rect (effect,
                                                      g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
shell_grid_desaturate_effect_get_property (GObject    *gobject,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  ShellGridDesaturateEffect *effect = SHELL_GRID_DESATURATE_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_FACTOR:
      g_value_set_double (value, effect->factor);
      break;

    case PROP_UNSHADED_RECT:
      g_value_set_boxed (value, &effect->unshaded_rect);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
update_factor_uniform (ShellGridDesaturateEffect *self)
{
  if (self->factor_uniform > -1)
    cogl_pipeline_set_uniform_1f (self->pipeline,
                                  self->factor_uniform,
                                  self->factor);
}

static void
update_unshaded_uniform (ShellGridDesaturateEffect *self)
{
  float values[4] = { 0., 0., 0., 0.};
  ClutterOffscreenEffect *offscreen_effect = CLUTTER_OFFSCREEN_EFFECT (self);
  CoglHandle texture;
  float width, height;

  if (self->unshaded_uniform == -1)
    return;

  texture = clutter_offscreen_effect_get_texture (offscreen_effect);
  if (!texture)
    goto out;

  width = cogl_texture_get_width (texture);
  height = cogl_texture_get_height (texture);

  values[0] = MIN (1.0, self->unshaded_rect.origin.x / width);
  values[1] = MIN (1.0, self->unshaded_rect.origin.y / height);
  values[2] = MIN (1.0, (self->unshaded_rect.origin.x + self->unshaded_rect.size.width) / width);
  values[3] = MIN (1.0, (self->unshaded_rect.origin.y + self->unshaded_rect.size.height) / height);

  self->unshaded_uniform_dirty = FALSE;

 out:
  cogl_pipeline_set_uniform_float (self->pipeline,
                                   self->unshaded_uniform,
                                   4, 1,
                                   values);
}

static void
shell_grid_desaturate_effect_class_init (ShellGridDesaturateEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->paint_target = shell_grid_desaturate_effect_paint_target;

  effect_class->pre_paint = shell_grid_desaturate_effect_pre_paint;

  /**
   * ShellGridDesaturateEffect:factor:
   *
   * The desaturation factor, between 0.0 (no desaturation) and 1.0 (full
   * desaturation).
   */
  obj_props[PROP_FACTOR] =
    g_param_spec_double ("factor",
                         "Factor",
                         "The desaturation factor",
                         0.0, 1.0,
                         1.0,
                         G_PARAM_READWRITE);

  /**
   * ShellGridDesaturateEffect:unshaded-rect:
   *
   * The unshaded rectangle.
   */
  obj_props[PROP_UNSHADED_RECT] =
    g_param_spec_boxed ("unshaded-rect",
                        "Unshaded rect",
                        "The unshaded rectangle",
                        GRAPHENE_TYPE_RECT,
                        G_PARAM_READWRITE);

  gobject_class->dispose = shell_grid_desaturate_effect_dispose;
  gobject_class->set_property = shell_grid_desaturate_effect_set_property;
  gobject_class->get_property = shell_grid_desaturate_effect_get_property;

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
shell_grid_desaturate_effect_init (ShellGridDesaturateEffect *self)
{
  if (G_UNLIKELY (base_pipeline == NULL))
    {
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());
      CoglSnippet *snippet;

      base_pipeline = cogl_pipeline_new (ctx);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                  desaturate_glsl_declarations,
                                  desaturate_glsl_source);
      cogl_pipeline_add_snippet (base_pipeline, snippet);
      cogl_object_unref (snippet);

      cogl_pipeline_set_layer_null_texture (base_pipeline, 0);
    }

  self->pipeline = cogl_pipeline_copy (base_pipeline);

  self->factor_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline, "factor");

  self->factor = 1.0;

  update_factor_uniform (self);

  self->unshaded_rect = GRAPHENE_RECT_INIT_ZERO;

  self->unshaded_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline, "unshaded");

  update_unshaded_uniform (self);
}

/**
 * shell_grid_desaturate_effect_new:
 * @factor: the desaturation factor, between 0.0 and 1.0
 *
 * Creates a new #ShellGridDesaturateEffect to be used with
 * clutter_actor_add_effect()
 *
 * Return value: the newly created #ShellGridDesaturateEffect or %NULL
 */
ClutterEffect *
shell_grid_desaturate_effect_new (gdouble factor)
{
  g_return_val_if_fail (factor >= 0.0 && factor <= 1.0, NULL);

  return g_object_new (CLUTTER_TYPE_DESATURATE_EFFECT,
                       "factor", factor,
                       NULL);
}

/**
 * shell_grid_desaturate_effect_set_factor:
 * @effect: a #ShellGridDesaturateEffect
 * @factor: the desaturation factor, between 0.0 and 1.0
 *
 * Sets the desaturation factor for @effect, with 0.0 being "do not desaturate"
 * and 1.0 being "fully desaturate"
 */
void
shell_grid_desaturate_effect_set_factor (ShellGridDesaturateEffect *effect,
                                         gdouble                    factor)
{
  g_return_if_fail (SHELL_IS_GRID_DESATURATE_EFFECT (effect));
  g_return_if_fail (factor >= 0.0 && factor <= 1.0);

  if (fabsf (effect->factor - factor) >= 0.00001)
    {
      effect->factor = factor;
      update_factor_uniform (effect);

      clutter_effect_queue_repaint (CLUTTER_EFFECT (effect));

      g_object_notify_by_pspec (G_OBJECT (effect), obj_props[PROP_FACTOR]);
    }
}

/**
 * shell_grid_desaturate_effect_get_unshaded_rect:
 * @effect: a #ShellGridDesaturateEffect
 *
 * Retrieves the unshaded area of @effect
 *
 * Return value: (transfer none): the unshaded area
 */
const graphene_rect_t *
shell_grid_desaturate_effect_get_unshaded_rect (ShellGridDesaturateEffect *effect)
{
  g_return_val_if_fail (SHELL_IS_GRID_DESATURATE_EFFECT (effect), NULL);

  return &effect->unshaded_rect;
}

/**
 * shell_grid_desaturate_effect_set_unshaded_rect:
 * @effect: a #ShellGridDesaturateEffect
 * @rect: (allow-none): the unshaded area
 *
 * Sets an unshaded area to the effect
 */
void
shell_grid_desaturate_effect_set_unshaded_rect (ShellGridDesaturateEffect *effect,
                                                graphene_rect_t           *rect)
{
  g_return_if_fail (SHELL_IS_GRID_DESATURATE_EFFECT (effect));

  if (!graphene_rect_equal (rect, &effect->unshaded_rect))
    {
      effect->unshaded_rect = *rect;
      effect->unshaded_uniform_dirty = TRUE;

      clutter_effect_queue_repaint (CLUTTER_EFFECT (effect));

      g_object_notify_by_pspec (G_OBJECT (effect), obj_props[PROP_UNSHADED_RECT]);
    }
}

/**
 * shell_grid_desaturate_effect_get_factor:
 * @effect: a #ShellGridDesaturateEffect
 *
 * Retrieves the desaturation factor of @effect
 *
 * Return value: the desaturation factor
 */
gdouble
shell_grid_desaturate_effect_get_factor (ShellGridDesaturateEffect *effect)
{
  g_return_val_if_fail (SHELL_IS_GRID_DESATURATE_EFFECT (effect), 0.0);

  return effect->factor;
}
