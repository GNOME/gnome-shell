/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Endless Mobile
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "shell-wobbly-effect.h"

struct _ShellWobblyEffectPrivate
{
  int bend_x;

  int tex_width, tex_height;
  CoglPipeline *pipeline;

  int tex_width_uniform;
  int bend_x_uniform;
};
typedef struct _ShellWobblyEffectPrivate ShellWobblyEffectPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ShellWobblyEffect, shell_wobbly_effect, CLUTTER_TYPE_OFFSCREEN_EFFECT);

static const gchar *wobbly_decls =
  "uniform int tex_width;\n"
  "uniform int bend_x;\n";
static const gchar *wobbly_pre =
  "float bend_x_coord = (float(bend_x) / tex_width);\n"
  "float interp = (1 - cos(cogl_tex_coord.y * 3.1415926)) / 2;\n"
  "cogl_tex_coord.x -= (interp * bend_x_coord);\n";

/* XXX - clutter_effect_get_paint_volume is fucking terrible
 * and I want to kill myself */
static gboolean
shell_wobbly_effect_get_paint_volume (ClutterEffect      *effect,
                                      ClutterPaintVolume *volume)
{
  ShellWobblyEffect *self = SHELL_WOBBLY_EFFECT (effect);
  ShellWobblyEffectPrivate *priv = shell_wobbly_effect_get_instance_private (self);

  float cur_width;
  cur_width = clutter_paint_volume_get_width (volume);
  cur_width += ABS (priv->bend_x);
  clutter_paint_volume_set_width (volume, cur_width);

  /* Also modify the origin if it bends to the left. */
  if (priv->bend_x < 0)
    {
      ClutterVertex origin;
      clutter_paint_volume_get_origin (volume, &origin);
      origin.x += priv->bend_x;
      clutter_paint_volume_set_origin (volume, &origin);
    }

  return TRUE;
}

static gboolean
shell_wobbly_effect_pre_paint (ClutterEffect *effect)
{
  ShellWobblyEffect *self = SHELL_WOBBLY_EFFECT (effect);
  ShellWobblyEffectPrivate *priv = shell_wobbly_effect_get_instance_private (self);

  if (!clutter_actor_meta_get_enabled (CLUTTER_ACTOR_META (effect)))
    return FALSE;

  /* If we're not doing any bending, we're not needed. */
  if (priv->bend_x == 0)
    return FALSE;

  if (!clutter_feature_available (CLUTTER_FEATURE_SHADERS_GLSL))
    {
      /* if we don't have support for GLSL shaders then we
       * forcibly disable the ActorMeta
       */
      g_warning ("Unable to use the ShellWobblyEffect: the "
                 "graphics hardware or the current GL driver does not "
                 "implement support for the GLSL shading language. The "
                 "effect will be disabled.");
      clutter_actor_meta_set_enabled (CLUTTER_ACTOR_META (effect), FALSE);
      return FALSE;
    }

  if (!CLUTTER_EFFECT_CLASS (shell_wobbly_effect_parent_class)->pre_paint (effect))
    return FALSE;

  ClutterOffscreenEffect *offscreen_effect = CLUTTER_OFFSCREEN_EFFECT (effect);
  CoglObject *texture;

  texture = clutter_offscreen_effect_get_texture (offscreen_effect);
  cogl_pipeline_set_layer_texture (priv->pipeline, 0, texture);

  priv->tex_width = cogl_texture_get_width (texture);
  priv->tex_height = cogl_texture_get_height (texture);

  cogl_pipeline_set_uniform_1i (priv->pipeline, priv->tex_width_uniform, priv->tex_width);

  return TRUE;
}

static void
shell_wobbly_effect_paint_target (ClutterOffscreenEffect *effect)
{
  ShellWobblyEffect *self = SHELL_WOBBLY_EFFECT (effect);
  ShellWobblyEffectPrivate *priv = shell_wobbly_effect_get_instance_private (self);
  CoglFramebuffer *fb = cogl_get_draw_framebuffer ();
  ClutterActor *actor;
  guint8 paint_opacity;

  actor = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (effect));
  paint_opacity = clutter_actor_get_paint_opacity (actor);

  cogl_pipeline_set_color4ub (priv->pipeline,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity,
                              paint_opacity);

  cogl_framebuffer_draw_rectangle (fb, priv->pipeline,
                                   0, 0, priv->tex_width, priv->tex_height);
}

static void
shell_wobbly_effect_dispose (GObject *object)
{
  ShellWobblyEffect *self = SHELL_WOBBLY_EFFECT (object);
  ShellWobblyEffectPrivate *priv = shell_wobbly_effect_get_instance_private (self);

  g_clear_pointer (&priv->pipeline, cogl_object_unref);

  G_OBJECT_CLASS (shell_wobbly_effect_parent_class)->dispose (object);
}

static void
shell_wobbly_effect_class_init (ShellWobblyEffectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterEffectClass *effect_class = CLUTTER_EFFECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);

  offscreen_class->paint_target = shell_wobbly_effect_paint_target;
  effect_class->pre_paint = shell_wobbly_effect_pre_paint;
  effect_class->get_paint_volume = shell_wobbly_effect_get_paint_volume;
  object_class->dispose = shell_wobbly_effect_dispose;
}

static void
update_uniforms (ShellWobblyEffect *self)
{
  ShellWobblyEffectPrivate *priv = shell_wobbly_effect_get_instance_private (self);
  cogl_pipeline_set_uniform_1i (priv->pipeline, priv->bend_x_uniform, priv->bend_x);
}

static void
shell_wobbly_effect_init (ShellWobblyEffect *self)
{
  static CoglPipeline *pipeline_template;

  ShellWobblyEffectPrivate *priv = shell_wobbly_effect_get_instance_private (self);

  if (G_UNLIKELY (pipeline_template == NULL))
    {
      CoglSnippet *snippet;
      CoglContext *ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());

      pipeline_template = cogl_pipeline_new (ctx);

      snippet = cogl_snippet_new (COGL_SNIPPET_HOOK_TEXTURE_LOOKUP, wobbly_decls, NULL);
      cogl_snippet_set_pre (snippet, wobbly_pre);
      cogl_pipeline_add_layer_snippet (pipeline_template, 0, snippet);
      cogl_object_unref (snippet);

      cogl_pipeline_set_layer_null_texture (pipeline_template,
                                            0, /* layer number */
                                            COGL_TEXTURE_TYPE_2D);
    }

  priv->pipeline = cogl_pipeline_copy (pipeline_template);
  priv->tex_width_uniform = cogl_pipeline_get_uniform_location (priv->pipeline, "tex_width");
  priv->bend_x_uniform = cogl_pipeline_get_uniform_location (priv->pipeline, "bend_x");

  update_uniforms (self);
}

void
shell_wobbly_effect_set_bend_x (ShellWobblyEffect *self,
                                int                bend_x)
{
  ShellWobblyEffectPrivate *priv = shell_wobbly_effect_get_instance_private (self);

  if (priv->bend_x == bend_x)
    return;

  priv->bend_x = bend_x;
  update_uniforms (self);
  clutter_effect_queue_repaint (CLUTTER_EFFECT (self));
}
