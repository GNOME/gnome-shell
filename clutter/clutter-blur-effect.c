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
#include "config.h"
#endif

#include "clutter-blur-effect.h"

#include "cogl/cogl.h"

#include "clutter-debug.h"
#include "clutter-offscreen-effect.h"
#include "clutter-private.h"

#define BLUR_PADDING    2

/* FIXME - lame shader; we should really have a decoupled
 * horizontal/vertical two pass shader for the gaussian blur
 */
static const gchar *box_blur_glsl_shader =
"uniform sampler2D tex;\n"
"uniform float x_step, y_step;\n"
"\n"
"vec4 get_rgba_rel (sampler2D source, float dx, float dy)\n"
"{\n"
"  return texture2D (tex, cogl_tex_coord_in[0].st + vec2 (dx, dy) * 2.0);\n"
"}\n"
"\n"
"void main ()\n"
"{\n"
"  vec4 color = cogl_color_in * texture2D (tex, vec2 (cogl_tex_coord_in[0].xy));\n"
"  color += get_rgba_rel (tex, -x_step, -y_step);\n"
"  color += get_rgba_rel (tex,  0.0,    -y_step);\n"
"  color += get_rgba_rel (tex,  x_step, -y_step);\n"
"  color += get_rgba_rel (tex, -x_step,  0.0);\n"
"  color += get_rgba_rel (tex,  0.0,     0.0);\n"
"  color += get_rgba_rel (tex,  x_step,  0.0);\n"
"  color += get_rgba_rel (tex, -x_step,  y_step);\n"
"  color += get_rgba_rel (tex,  0.0,     y_step);\n"
"  color += get_rgba_rel (tex,  x_step,  y_step);\n"
"  cogl_color_out = color / 9.0;\n"
"}";

struct _ClutterBlurEffect
{
  ClutterOffscreenEffect parent_instance;

  /* a back pointer to our actor, so that we can query it */
  ClutterActor *actor;

  /* the parameters; x_step and y_step depend on
   * the actor's allocation
   */
  gfloat x_step;
  gfloat y_step;

  CoglHandle shader;
  CoglHandle program;

  gint tex_uniform;
  gint x_step_uniform;
  gint y_step_uniform;

  guint is_compiled : 1;
};

struct _ClutterBlurEffectClass
{
  ClutterOffscreenEffectClass parent_class;
};

G_DEFINE_TYPE (ClutterBlurEffect,
               clutter_blur_effect,
               CLUTTER_TYPE_OFFSCREEN_EFFECT);

static int
next_p2 (int a)
{
  int rval = 1;

  while (rval < a)
    rval <<= 1;

  return rval;
}

static gboolean
clutter_blur_effect_pre_paint (ClutterEffect *effect)
{
  ClutterBlurEffect *self = CLUTTER_BLUR_EFFECT (effect);
  ClutterEffectClass *parent_class;
  ClutterActorBox allocation;
  gfloat width, height;

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

  clutter_actor_get_allocation_box (self->actor, &allocation);
  clutter_actor_box_get_size (&allocation, &width, &height);

  self->x_step = 1.0f / (float) next_p2 (width);
  self->y_step = 1.0f / (float) next_p2 (height);

  if (self->shader == COGL_INVALID_HANDLE)
    {
      self->shader = cogl_create_shader (COGL_SHADER_TYPE_FRAGMENT);
      cogl_shader_source (self->shader, box_blur_glsl_shader);

      self->is_compiled = FALSE;
      self->tex_uniform = -1;
      self->x_step_uniform = -1;
      self->y_step_uniform = -1;
    }

  if (self->program == COGL_INVALID_HANDLE)
    self->program = cogl_create_program ();

  if (!self->is_compiled)
    {
      g_assert (self->shader != COGL_INVALID_HANDLE);
      g_assert (self->program != COGL_INVALID_HANDLE);

      cogl_shader_compile (self->shader);
      if (!cogl_shader_is_compiled (self->shader))
        {
          gchar *log_buf = cogl_shader_get_info_log (self->shader);

          g_warning (G_STRLOC ": Unable to compile the box blur shader: %s",
                     log_buf);
          g_free (log_buf);

          cogl_handle_unref (self->shader);
          cogl_handle_unref (self->program);

          self->shader = COGL_INVALID_HANDLE;
          self->program = COGL_INVALID_HANDLE;
        }
      else
        {
          cogl_program_attach_shader (self->program, self->shader);
          cogl_program_link (self->program);

          cogl_handle_unref (self->shader);

          self->is_compiled = TRUE;

          self->tex_uniform =
            cogl_program_get_uniform_location (self->program, "tex");
          self->x_step_uniform =
            cogl_program_get_uniform_location (self->program, "x_step");
          self->y_step_uniform =
            cogl_program_get_uniform_location (self->program, "y_step");
        }
    }

  parent_class = CLUTTER_EFFECT_CLASS (clutter_blur_effect_parent_class);
  return parent_class->pre_paint (effect);
}

static void
clutter_blur_effect_paint_target (ClutterOffscreenEffect *effect)
{
  ClutterBlurEffect *self = CLUTTER_BLUR_EFFECT (effect);
  ClutterOffscreenEffectClass *parent;
  CoglHandle material;

  if (self->program == COGL_INVALID_HANDLE)
    goto out;

  if (self->tex_uniform > -1)
    cogl_program_set_uniform_1i (self->program, self->tex_uniform, 0);

  if (self->x_step_uniform > -1)
    cogl_program_set_uniform_1f (self->program,
                                 self->x_step_uniform,
                                 self->x_step);

  if (self->y_step_uniform > -1)
    cogl_program_set_uniform_1f (self->program,
                                 self->y_step_uniform,
                                 self->y_step);

  material = clutter_offscreen_effect_get_target (effect);
  cogl_material_set_user_program (material, self->program);

out:
  parent = CLUTTER_OFFSCREEN_EFFECT_CLASS (clutter_blur_effect_parent_class);
  parent->paint_target (effect);
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

  if (self->program != COGL_INVALID_HANDLE)
    {
      cogl_handle_unref (self->program);

      self->program = COGL_INVALID_HANDLE;
      self->shader = COGL_INVALID_HANDLE;
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
