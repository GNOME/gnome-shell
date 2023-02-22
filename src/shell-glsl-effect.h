/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_GLSL_EFFECT_H__
#define __SHELL_GLSL_EFFECT_H__

#include "st.h"

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

#define SHELL_TYPE_GLSL_EFFECT (shell_glsl_effect_get_type ())
G_DECLARE_DERIVABLE_TYPE (ShellGLSLEffect, shell_glsl_effect,
                          SHELL, GLSL_EFFECT, ClutterOffscreenEffect)

struct _ShellGLSLEffectClass
{
  ClutterOffscreenEffectClass parent_class;

  CoglPipeline *base_pipeline;

  void (*build_pipeline) (ShellGLSLEffect *effect);
};

void shell_glsl_effect_add_glsl_snippet (ShellGLSLEffect  *effect,
                                         ShellSnippetHook  hook,
                                         const char       *declarations,
                                         const char       *code,
                                         gboolean          is_replace);

int  shell_glsl_effect_get_uniform_location (ShellGLSLEffect *effect,
                                             const char      *name);
void shell_glsl_effect_set_uniform_float    (ShellGLSLEffect *effect,
                                             int              uniform,
                                             int              n_components,
                                             int              total_count,
                                             const float     *value);
void shell_glsl_effect_set_uniform_matrix   (ShellGLSLEffect *effect,
                                             int              uniform,
                                             gboolean         transpose,
                                             int              dimensions,
                                             int              total_count,
                                             const float     *value);

#endif /* __SHELL_GLSL_EFFECT_H__ */
