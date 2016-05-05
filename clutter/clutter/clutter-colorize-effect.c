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
 * SECTION:clutter-colorize-effect
 * @short_description: A colorization effect
 * @see_also: #ClutterEffect, #ClutterOffscreenEffect
 *
 * #ClutterColorizeEffect is a sub-class of #ClutterEffect that
 * colorizes an actor with the given tint.
 *
 * #ClutterColorizeEffect is available since Clutter 1.4
 */

#define CLUTTER_COLORIZE_EFFECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_COLORIZE_EFFECT, ClutterColorizeEffectClass))
#define CLUTTER_IS_COLORIZE_EFFECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_COLORIZE_EFFECT))
#define CLUTTER_COLORIZE_EFFECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_COLORIZE_EFFECT, ClutterColorizeEffectClass))

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#define CLUTTER_ENABLE_EXPERIMENTAL_API

#include "clutter-colorize-effect.h"

#include "cogl/cogl.h"

#include "clutter-debug.h"
#include "clutter-enum-types.h"
#include "clutter-offscreen-effect.h"
#include "clutter-private.h"

struct _ClutterColorizeEffect
{
  ClutterOffscreenEffect parent_instance;

  /* the tint of the colorization */
  ClutterColor tint;

  gint tint_uniform;

  gint tex_width;
  gint tex_height;

  CoglPipeline *pipeline;
};

struct _ClutterColorizeEffectClass
{
  ClutterOffscreenEffectClass parent_class;

  CoglPipeline *base_pipeline;
};

/* the magic gray vec3 has been taken from the NTSC conversion weights
 * as defined by:
 *
 *   "OpenGL Superbible, 4th Edition"
 *   -- Richard S. Wright Jr, Benjamin Lipchak, Nicholas Haemel
 *   Addison-Wesley
 */
static const gchar *colorize_glsl_declarations =
"uniform vec3 tint;\n";

static const gchar *colorize_glsl_source =
"float gray = dot (cogl_color_out.rgb, vec3 (0.299, 0.587, 0.114));\n"
"cogl_color_out.rgb = gray * tint;\n";

/* a lame sepia */
static const ClutterColor default_tint = { 255, 204, 153, 255 };

enum
{
  PROP_0,

  PROP_TINT,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE (ClutterColorizeEffect,
               clutter_colorize_effect,
               CLUTTER_TYPE_OFFSCREEN_EFFECT);

static gboolean
clutter_colorize_effect_pre_paint (ClutterEffect *effect)
{
  ClutterColorizeEffect *self = CLUTTER_COLORIZE_EFFECT (effect);
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

  parent_class = CLUTTER_EFFECT_CLASS (clutter_colorize_effect_parent_class);
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
clutter_colorize_effect_paint_target (ClutterOffscreenEffect *effect)
{
  ClutterColorizeEffect *self = CLUTTER_COLORIZE_EFFECT (effect);
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
clutter_colorize_effect_dispose (GObject *gobject)
{
  ClutterColorizeEffect *self = CLUTTER_COLORIZE_EFFECT (gobject);

  if (self->pipeline != NULL)
    {
      cogl_object_unref (self->pipeline);
      self->pipeline = NULL;
    }

  G_OBJECT_CLASS (clutter_colorize_effect_parent_class)->dispose (gobject);
}

static void
clutter_colorize_effect_set_property (GObject      *gobject,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ClutterColorizeEffect *effect = CLUTTER_COLORIZE_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_TINT:
      clutter_colorize_effect_set_tint (effect,
                                        clutter_value_get_color (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_colorize_effect_get_property (GObject    *gobject,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ClutterColorizeEffect *effect = CLUTTER_COLORIZE_EFFECT (gobject);

  switch (prop_id)
    {
    case PROP_TINT:
      clutter_value_set_color (value, &effect->tint);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_colorize_effect_class_init (ClutterColorizeEffectClass *klass)
{
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->paint_target = clutter_colorize_effect_paint_target;

  effect_class->pre_paint = clutter_colorize_effect_pre_paint;

  gobject_class->set_property = clutter_colorize_effect_set_property;
  gobject_class->get_property = clutter_colorize_effect_get_property;
  gobject_class->dispose = clutter_colorize_effect_dispose;

  /**
   * ClutterColorizeEffect:tint:
   *
   * The tint to apply to the actor
   *
   * Since: 1.4
   */
  obj_props[PROP_TINT] =
    clutter_param_spec_color ("tint",
                              P_("Tint"),
                              P_("The tint to apply"),
                              &default_tint,
                              CLUTTER_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}

static void
update_tint_uniform (ClutterColorizeEffect *self)
{
  if (self->tint_uniform > -1)
    {
      float tint[3] = {
        self->tint.red / 255.0,
        self->tint.green / 255.0,
        self->tint.blue / 255.0
      };

      cogl_pipeline_set_uniform_float (self->pipeline,
                                       self->tint_uniform,
                                       3, /* n_components */
                                       1, /* count */
                                       tint);
    }
}

static void
clutter_colorize_effect_init (ClutterColorizeEffect *self)
{
  ClutterColorizeEffectClass *klass = CLUTTER_COLORIZE_EFFECT_GET_CLASS (self);

  if (G_UNLIKELY (klass->base_pipeline == NULL))
    {
      CoglSnippet *snippet;
      CoglContext *ctx =
        clutter_backend_get_cogl_context (clutter_get_default_backend ());

      klass->base_pipeline = cogl_pipeline_new (ctx);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_FRAGMENT,
                                  colorize_glsl_declarations,
                                  colorize_glsl_source);
      cogl_pipeline_add_snippet (klass->base_pipeline, snippet);
      cogl_object_unref (snippet);

      cogl_pipeline_set_layer_null_texture (klass->base_pipeline,
                                            0, /* layer number */
                                            COGL_TEXTURE_TYPE_2D);
    }

  self->pipeline = cogl_pipeline_copy (klass->base_pipeline);

  self->tint_uniform =
    cogl_pipeline_get_uniform_location (self->pipeline, "tint");

  self->tint = default_tint;

  update_tint_uniform (self);
}

/**
 * clutter_colorize_effect_new:
 * @tint: the color to be used
 *
 * Creates a new #ClutterColorizeEffect to be used with
 * clutter_actor_add_effect()
 *
 * Return value: the newly created #ClutterColorizeEffect or %NULL
 *
 * Since: 1.4
 */
ClutterEffect *
clutter_colorize_effect_new (const ClutterColor *tint)
{
  return g_object_new (CLUTTER_TYPE_COLORIZE_EFFECT,
                       "tint", tint,
                       NULL);
}

/**
 * clutter_colorize_effect_set_tint:
 * @effect: a #ClutterColorizeEffect
 * @tint: the color to be used
 *
 * Sets the tint to be used when colorizing
 *
 * Since: 1.4
 */
void
clutter_colorize_effect_set_tint (ClutterColorizeEffect *effect,
                                  const ClutterColor    *tint)
{
  g_return_if_fail (CLUTTER_IS_COLORIZE_EFFECT (effect));

  effect->tint = *tint;

  update_tint_uniform (effect);

  clutter_effect_queue_repaint (CLUTTER_EFFECT (effect));

  g_object_notify_by_pspec (G_OBJECT (effect), obj_props[PROP_TINT]);
}

/**
 * clutter_colorize_effect_get_tint:
 * @effect: a #ClutterColorizeEffect
 * @tint: (out caller-allocates): return location for the color used
 *
 * Retrieves the tint used by @effect
 *
 * Since: 1.4
 */
void
clutter_colorize_effect_get_tint (ClutterColorizeEffect *effect,
                                  ClutterColor          *tint)
{
  g_return_if_fail (CLUTTER_IS_COLORIZE_EFFECT (effect));
  g_return_if_fail (tint != NULL);

  *tint = effect->tint;
}
