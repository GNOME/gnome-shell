/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#pragma once

#include "st.h"

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
                                         CoglSnippetHook   hook,
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
