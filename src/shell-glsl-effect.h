/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_GLSL_QUAD_H__
#define __SHELL_GLSL_QUAD_H__

#include "st.h"
#include <gtk/gtk.h>

/**
 * ShellSnippetHook:
 * Temporary hack to work around Cogl not exporting CoglSnippetHook in
 * the 1.0 API. Don't use.
 */
typedef enum {
  /* Per pipeline vertex hooks */
  SHELL_SNIPPET_HOOK_VERTEX = 0,
  SHELL_SNIPPET_HOOK_VERTEX_TRANSFORM,

  /* Per pipeline fragment hooks */
  SHELL_SNIPPET_HOOK_FRAGMENT = 2048,

  /* Per layer vertex hooks */
  SHELL_SNIPPET_HOOK_TEXTURE_COORD_TRANSFORM = 4096,

  /* Per layer fragment hooks */
  SHELL_SNIPPET_HOOK_LAYER_FRAGMENT = 6144,
  SHELL_SNIPPET_HOOK_TEXTURE_LOOKUP
} ShellSnippetHook;

#define SHELL_TYPE_GLSL_QUAD (shell_glsl_quad_get_type ())
G_DECLARE_DERIVABLE_TYPE (ShellGLSLQuad, shell_glsl_quad,
                          SHELL, GLSL_QUAD, ClutterActor)

struct _ShellGLSLQuadClass
{
  ClutterActorClass parent_class;

  CoglPipeline *base_pipeline;

  void (*build_pipeline) (ShellGLSLQuad *effect);
};

void shell_glsl_quad_add_glsl_snippet (ShellGLSLQuad    *quad,
                                       ShellSnippetHook  hook,
                                       const char       *declarations,
                                       const char       *code,
                                       gboolean          is_replace);

int  shell_glsl_quad_get_uniform_location (ShellGLSLQuad *quad,
                                           const char    *name);
void shell_glsl_quad_set_uniform_float    (ShellGLSLQuad *quad,
                                           int            uniform,
                                           int            n_components,
                                           int            total_count,
                                           const float   *value);

#endif /* __SHELL_GLSL_QUAD_H__ */
