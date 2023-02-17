/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:shell-glsl-effect
 * @short_description: An offscreen effect using GLSL
 *
 * A #ShellGLSLEffect is a #ClutterOffscreenEffect that allows
 * running custom GLSL to the vertex and fragment stages of the
 * graphic pipeline.
 */

#include "config.h"

#include <cogl/cogl.h>
#include "shell-glsl-effect.h"

typedef struct _ShellGLSLEffectPrivate ShellGLSLEffectPrivate;
struct _ShellGLSLEffectPrivate
{
  CoglPipeline  *pipeline;
};

G_DEFINE_TYPE_WITH_PRIVATE (ShellGLSLEffect, shell_glsl_effect, CLUTTER_TYPE_OFFSCREEN_EFFECT);

static CoglPipeline *
shell_glsl_effect_create_pipeline (ClutterOffscreenEffect *effect,
                                   CoglTexture            *texture)
{
  ShellGLSLEffect *self = SHELL_GLSL_EFFECT (effect);
  ShellGLSLEffectPrivate *priv = shell_glsl_effect_get_instance_private (self);

  cogl_pipeline_set_layer_texture (priv->pipeline, 0, texture);

  return cogl_object_ref (priv->pipeline);
}

/**
 * shell_glsl_effect_add_glsl_snippet:
 * @effect: a #ShellGLSLEffect
 * @hook: where to insert the code
 * @declarations: GLSL declarations
 * @code: GLSL code
 * @is_replace: whether Cogl code should be replaced by the custom shader
 *
 * Adds a GLSL snippet to the pipeline used for drawing the effect texture.
 * See #CoglSnippet for details.
 *
 * This is only valid inside the a call to the build_pipeline() virtual
 * function.
 */
void
shell_glsl_effect_add_glsl_snippet (ShellGLSLEffect  *effect,
                                    ShellSnippetHook  hook,
                                    const char       *declarations,
                                    const char       *code,
                                    gboolean          is_replace)
{
  ShellGLSLEffectClass *klass = SHELL_GLSL_EFFECT_GET_CLASS (effect);
  CoglSnippet *snippet;

  g_return_if_fail (klass->base_pipeline != NULL);

  if (is_replace)
    {
      snippet = cogl_snippet_new ((CoglSnippetHook)hook, declarations, NULL);
      cogl_snippet_set_replace (snippet, code);
    }
  else
    {
      snippet = cogl_snippet_new ((CoglSnippetHook)hook, declarations, code);
    }

  if (hook == SHELL_SNIPPET_HOOK_VERTEX ||
      hook == SHELL_SNIPPET_HOOK_FRAGMENT)
    cogl_pipeline_add_snippet (klass->base_pipeline, snippet);
  else
    cogl_pipeline_add_layer_snippet (klass->base_pipeline, 0, snippet);

  cogl_object_unref (snippet);
}

static void
shell_glsl_effect_dispose (GObject *gobject)
{
  ShellGLSLEffect *self = SHELL_GLSL_EFFECT (gobject);
  ShellGLSLEffectPrivate *priv;

  priv = shell_glsl_effect_get_instance_private (self);

  g_clear_pointer (&priv->pipeline, cogl_object_unref);

  G_OBJECT_CLASS (shell_glsl_effect_parent_class)->dispose (gobject);
}

static void
shell_glsl_effect_init (ShellGLSLEffect *effect)
{
}

static void
shell_glsl_effect_constructed (GObject *object)
{
  ShellGLSLEffect *self;
  ShellGLSLEffectClass *klass;
  ShellGLSLEffectPrivate *priv;
  CoglContext *ctx =
    clutter_backend_get_cogl_context (clutter_get_default_backend ());

  G_OBJECT_CLASS (shell_glsl_effect_parent_class)->constructed (object);

  /* Note that, differently from ClutterBlurEffect, we are calling
     this inside constructed, not init, so klass points to the most-derived
     GTypeClass, not ShellGLSLEffectClass.
  */
  klass = SHELL_GLSL_EFFECT_GET_CLASS (object);
  self = SHELL_GLSL_EFFECT (object);
  priv = shell_glsl_effect_get_instance_private (self);

  if (G_UNLIKELY (klass->base_pipeline == NULL))
    {
      klass->base_pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_set_blend (klass->base_pipeline, "RGB = ADD (SRC_COLOR * (SRC_COLOR[A]), DST_COLOR * (1-SRC_COLOR[A]))", NULL);

      if (klass->build_pipeline != NULL)
        klass->build_pipeline (self);
    }

  priv->pipeline = cogl_pipeline_copy (klass->base_pipeline);

  cogl_pipeline_set_layer_null_texture (klass->base_pipeline, 0);
}

static void
shell_glsl_effect_class_init (ShellGLSLEffectClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterOffscreenEffectClass *offscreen_class;

  offscreen_class = CLUTTER_OFFSCREEN_EFFECT_CLASS (klass);
  offscreen_class->create_pipeline = shell_glsl_effect_create_pipeline;

  gobject_class->constructed = shell_glsl_effect_constructed;
  gobject_class->dispose = shell_glsl_effect_dispose;
}

/**
 * shell_glsl_effect_get_uniform_location:
 * @effect: a #ShellGLSLEffect
 * @name: the uniform name
 *
 * Returns: the location of the uniform named @name, that can be
 *          passed to shell_glsl_effect_set_uniform_float().
 */
int
shell_glsl_effect_get_uniform_location (ShellGLSLEffect *effect,
                                        const char      *name)
{
  ShellGLSLEffectPrivate *priv = shell_glsl_effect_get_instance_private (effect);
  return cogl_pipeline_get_uniform_location (priv->pipeline, name);
}

/**
 * shell_glsl_effect_set_uniform_float:
 * @effect: a #ShellGLSLEffect
 * @uniform: the uniform location (as returned by shell_glsl_effect_get_uniform_location())
 * @n_components: the number of components in the uniform (eg. 3 for a vec3)
 * @total_count: the total number of floats in @value
 * @value: (array length=total_count): the array of floats to set @uniform
 */
void
shell_glsl_effect_set_uniform_float (ShellGLSLEffect *effect,
                                     int              uniform,
                                     int              n_components,
                                     int              total_count,
                                     const float     *value)
{
  ShellGLSLEffectPrivate *priv = shell_glsl_effect_get_instance_private (effect);
  cogl_pipeline_set_uniform_float (priv->pipeline, uniform,
                                   n_components, total_count / n_components,
                                   value);
}

/**
 * shell_glsl_effect_set_uniform_matrix:
 * @effect: a #ShellGLSLEffect
 * @uniform: the uniform location (as returned by shell_glsl_effect_get_uniform_location())
 * @transpose: Whether to transpose the matrix
 * @dimensions: the number of components in the uniform (eg. 3 for a vec3)
 * @total_count: the total number of floats in @value
 * @value: (array length=total_count): the array of floats to set @uniform
 */
void
shell_glsl_effect_set_uniform_matrix (ShellGLSLEffect *effect,
                                      int              uniform,
                                      gboolean         transpose,
                                      int              dimensions,
                                      int              total_count,
                                      const float     *value)
{
  ShellGLSLEffectPrivate *priv = shell_glsl_effect_get_instance_private (effect);
  cogl_pipeline_set_uniform_matrix (priv->pipeline, uniform,
                                    dimensions,
                                    total_count / (dimensions * dimensions),
                                    transpose, value);
}
