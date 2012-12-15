/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:shell-glsl-quad
 * @short_description: Draw a rectangle using GLSL
 *
 * A #ShellGLSLQuad draws one single rectangle, sized to the allocation
 * box, but allows running custom GLSL to the vertex and fragment
 * stages of the graphic pipeline.
 *
 * To ease writing the shader, a single texture layer is also used.
 */

#include "config.h"

#define CLUTTER_ENABLE_EXPERIMENTAL_API
#define COGL_ENABLE_EXPERIMENTAL_API

#include <cogl/cogl.h>
#include "shell-glsl-quad.h"

G_DEFINE_TYPE (ShellGLSLQuad, shell_glsl_quad, CLUTTER_TYPE_ACTOR);

struct _ShellGLSLQuadPrivate
{
  CoglPipeline  *pipeline;
  CoglTexture2D *texture;
};

static void
shell_glsl_quad_paint (ClutterActor *actor)
{
  ShellGLSLQuad *self = SHELL_GLSL_QUAD (actor);
  ShellGLSLQuadPrivate *priv;
  guint8 paint_opacity;
  ClutterActorBox box;

  priv = self->priv;

  paint_opacity = clutter_actor_get_paint_opacity (actor);
  clutter_actor_get_allocation_box (actor, &box);

  /* semi-transparent black */
  cogl_pipeline_set_color4ub (priv->pipeline,
                              0, 0, 0,
                              paint_opacity);
  cogl_framebuffer_draw_rectangle (cogl_get_draw_framebuffer (),
                                   priv->pipeline,
                                   box.x1, box.y1,
                                   box.x2, box.y2);
}


/**
 * shell_glsl_quad_add_glsl_snippet:
 * @quad: a #ShellGLSLQuad
 * @hook: where to insert the code
 * @declarations: GLSL declarations
 * @code: GLSL code
 * @is_replace: wheter Cogl code should be replaced by the custom shader
 *
 * Adds a GLSL snippet to the pipeline used for drawing the actor texture.
 * See #CoglSnippet for details.
 *
 * This is only valid inside the a call to the build_pipeline() virtual
 * function.
 */
void
shell_glsl_quad_add_glsl_snippet (ShellGLSLQuad    *quad,
                                  ShellSnippetHook  hook,
                                  const char       *declarations,
                                  const char       *code,
                                  gboolean          is_replace)
{
  ShellGLSLQuadClass *klass = SHELL_GLSL_QUAD_GET_CLASS (quad);
  CoglSnippet *snippet;

  g_return_if_fail (klass->base_pipeline != NULL);

  if (is_replace)
    {
      snippet = cogl_snippet_new (hook, declarations, NULL);
      cogl_snippet_set_replace (snippet, code);
    }
  else
    {
      snippet = cogl_snippet_new (hook, declarations, code);
    }

  if (hook == SHELL_SNIPPET_HOOK_VERTEX ||
      hook == SHELL_SNIPPET_HOOK_FRAGMENT)
    cogl_pipeline_add_snippet (klass->base_pipeline, snippet);
  else
    cogl_pipeline_add_layer_snippet (klass->base_pipeline, 0, snippet);

  cogl_object_unref (snippet);
}

static void
shell_glsl_quad_dispose (GObject *gobject)
{
  ShellGLSLQuad *self = SHELL_GLSL_QUAD (gobject);
  ShellGLSLQuadPrivate *priv;

  priv = self->priv;

  g_clear_pointer (&priv->pipeline, cogl_object_unref);
  g_clear_pointer (&priv->texture, cogl_object_unref);

  G_OBJECT_CLASS (shell_glsl_quad_parent_class)->dispose (gobject);
}

static void
shell_glsl_quad_init (ShellGLSLQuad *quad)
{
  quad->priv = G_TYPE_INSTANCE_GET_PRIVATE (quad, SHELL_TYPE_GLSL_QUAD, ShellGLSLQuadPrivate);
}

static void
shell_glsl_quad_constructed (GObject *object)
{
  ShellGLSLQuad *self;
  ShellGLSLQuadClass *klass;
  CoglContext *ctx =
    clutter_backend_get_cogl_context (clutter_get_default_backend ());
  static const uint8_t tex_data[] = { 0, 0, 0, 0 };

  G_OBJECT_CLASS (shell_glsl_quad_parent_class)->constructed (object);

  /* Note that, differently from ClutterBlurEffect, we are calling
     this inside constructed, not init, so klass points to the most-derived
     GTypeClass, not ShellGLSLQuadClass.
  */
  klass = SHELL_GLSL_QUAD_GET_CLASS (object);
  self = SHELL_GLSL_QUAD (object);

  if (G_UNLIKELY (klass->base_pipeline == NULL))
    {
      klass->base_pipeline = cogl_pipeline_new (ctx);

      if (klass->build_pipeline != NULL)
        klass->build_pipeline (self);
    }

  self->priv->pipeline = cogl_pipeline_copy (klass->base_pipeline);

  self->priv->texture = cogl_texture_2d_new_from_data (ctx, 1, 1,
                                                       COGL_PIXEL_FORMAT_RGBA_8888,
                                                       0, tex_data, NULL);
  cogl_pipeline_set_layer_texture (self->priv->pipeline, 0,
                                   COGL_TEXTURE (self->priv->texture));
}

static void
shell_glsl_quad_class_init (ShellGLSLQuadClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = shell_glsl_quad_constructed;
  gobject_class->dispose = shell_glsl_quad_dispose;

  actor_class->paint = shell_glsl_quad_paint;

  g_type_class_add_private (klass, sizeof (ShellGLSLQuadPrivate));
}
