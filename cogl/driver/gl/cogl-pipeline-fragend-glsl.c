/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010,2013 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-layer-private.h"
#include "cogl-blend-string.h"
#include "cogl-snippet-private.h"
#include "cogl-list.h"

#ifdef COGL_PIPELINE_FRAGEND_GLSL

#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-shader-private.h"
#include "cogl-program-private.h"
#include "cogl-pipeline-cache.h"
#include "cogl-pipeline-fragend-glsl-private.h"
#include "cogl-glsl-shader-private.h"

#include <glib.h>

/*
 * GL/GLES compatability defines for pipeline thingies:
 */

/* This might not be defined on GLES */
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D                           0x806F
#endif

const CoglPipelineFragend _cogl_pipeline_glsl_backend;

typedef struct _UnitState
{
  unsigned int sampled:1;
  unsigned int combine_constant_used:1;
} UnitState;

typedef struct _LayerData
{
  CoglList link;

  /* Layer index for the for the previous layer. This isn't
     necessarily the same as this layer's index - 1 because the
     indices can have gaps. If this is the first layer then it will be
     -1 */
  int previous_layer_index;

  CoglPipelineLayer *layer;
} LayerData;

typedef struct
{
  int ref_count;

  GLuint gl_shader;
  GString *header, *source;
  UnitState *unit_state;

  CoglBool ref_point_coord;

  /* List of layers that we haven't generated code for yet. These are
     in reverse order. As soon as we're about to generate code for
     layer we'll remove it from the list so we don't generate it
     again */
  CoglList layers;
} CoglPipelineShaderState;

static CoglUserDataKey shader_state_key;

static void
ensure_layer_generated (CoglPipeline *pipeline,
                        int layer_num);

static CoglPipelineShaderState *
shader_state_new (int n_layers)
{
  CoglPipelineShaderState *shader_state;

  shader_state = g_slice_new0 (CoglPipelineShaderState);
  shader_state->ref_count = 1;
  shader_state->unit_state = g_new0 (UnitState, n_layers);
  shader_state->ref_point_coord = FALSE;

  return shader_state;
}

static CoglPipelineShaderState *
get_shader_state (CoglPipeline *pipeline)
{
  return cogl_object_get_user_data (COGL_OBJECT (pipeline), &shader_state_key);
}

static void
destroy_shader_state (void *user_data)
{
  CoglPipelineShaderState *shader_state = user_data;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (--shader_state->ref_count == 0)
    {
      if (shader_state->gl_shader)
        GE( ctx, glDeleteShader (shader_state->gl_shader) );

      g_free (shader_state->unit_state);

      g_slice_free (CoglPipelineShaderState, shader_state);
    }
}

static void
set_shader_state (CoglPipeline *pipeline, CoglPipelineShaderState *shader_state)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &shader_state_key,
                             shader_state,
                             destroy_shader_state);
}

static void
dirty_shader_state (CoglPipeline *pipeline)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &shader_state_key,
                             NULL,
                             NULL);
}

GLuint
_cogl_pipeline_fragend_glsl_get_shader (CoglPipeline *pipeline)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);

  if (shader_state)
    return shader_state->gl_shader;
  else
    return 0;
}

static CoglPipelineSnippetList *
get_fragment_snippets (CoglPipeline *pipeline)
{
  pipeline =
    _cogl_pipeline_get_authority (pipeline,
                                  COGL_PIPELINE_STATE_FRAGMENT_SNIPPETS);

  return &pipeline->big_state->fragment_snippets;
}

static CoglPipelineSnippetList *
get_layer_fragment_snippets (CoglPipelineLayer *layer)
{
  unsigned long state = COGL_PIPELINE_LAYER_STATE_FRAGMENT_SNIPPETS;
  layer = _cogl_pipeline_layer_get_authority (layer, state);

  return &layer->big_state->fragment_snippets;
}

static CoglBool
has_replace_hook (CoglPipelineLayer *layer,
                  CoglSnippetHook hook)
{
  GList *l;

  for (l = get_layer_fragment_snippets (layer)->entries; l; l = l->next)
    {
      CoglSnippet *snippet = l->data;

      if (snippet->hook == hook && snippet->replace)
        return TRUE;
    }

  return FALSE;
}

static CoglBool
add_layer_declaration_cb (CoglPipelineLayer *layer,
                          void *user_data)
{
  CoglPipelineShaderState *shader_state = user_data;
  CoglTextureType texture_type =
    _cogl_pipeline_layer_get_texture_type (layer);
  const char *target_string;

  _cogl_gl_util_get_texture_target_string (texture_type, &target_string, NULL);

  g_string_append_printf (shader_state->header,
                          "uniform sampler%s cogl_sampler%i;\n",
                          target_string,
                          layer->index);

  return TRUE;
}

static void
add_layer_declarations (CoglPipeline *pipeline,
                        CoglPipelineShaderState *shader_state)
{
  /* We always emit sampler uniforms in case there will be custom
   * layer snippets that want to sample arbitrary layers. */

  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         add_layer_declaration_cb,
                                         shader_state);
}

static void
add_global_declarations (CoglPipeline *pipeline,
                         CoglPipelineShaderState *shader_state)
{
  CoglSnippetHook hook = COGL_SNIPPET_HOOK_FRAGMENT_GLOBALS;
  CoglPipelineSnippetList *snippets = get_fragment_snippets (pipeline);

  /* Add the global data hooks. All of the code in these snippets is
   * always added and only the declarations data is used */

  _cogl_pipeline_snippet_generate_declarations (shader_state->header,
                                                hook,
                                                snippets);
}

static void
_cogl_pipeline_fragend_glsl_start (CoglPipeline *pipeline,
                                   int n_layers,
                                   unsigned long pipelines_difference)
{
  CoglPipelineShaderState *shader_state;
  CoglPipeline *authority;
  CoglPipeline *template_pipeline = NULL;
  CoglProgram *user_program = cogl_pipeline_get_user_program (pipeline);
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Now lookup our glsl backend private state */
  shader_state = get_shader_state (pipeline);

  if (shader_state == NULL)
    {
      /* If we don't have an associated glsl shader yet then find the
       * glsl-authority (the oldest ancestor whose state will result in
       * the same shader being generated as for this pipeline).
       *
       * We always make sure to associate new shader with the
       * glsl-authority to maximize the chance that other pipelines can
       * share it.
       */
      authority = _cogl_pipeline_find_equivalent_parent
        (pipeline,
         _cogl_pipeline_get_state_for_fragment_codegen (ctx) &
         ~COGL_PIPELINE_STATE_LAYERS,
         _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx));

      shader_state = get_shader_state (authority);

      /* If we don't have an existing program associated with the
       * glsl-authority then start generating code for a new shader...
       */
      if (shader_state == NULL)
        {
          /* Check if there is already a similar cached pipeline whose
             shader state we can share */
          if (G_LIKELY (!(COGL_DEBUG_ENABLED
                          (COGL_DEBUG_DISABLE_PROGRAM_CACHES))))
            {
              template_pipeline =
                _cogl_pipeline_cache_get_fragment_template (ctx->pipeline_cache,
                                                            authority);

              shader_state = get_shader_state (template_pipeline);
            }

          if (shader_state)
            shader_state->ref_count++;
          else
            shader_state = shader_state_new (n_layers);

          set_shader_state (authority, shader_state);

          if (template_pipeline)
            {
              shader_state->ref_count++;
              set_shader_state (template_pipeline, shader_state);
            }
        }

      /* If the pipeline isn't actually its own glsl-authority
       * then take a reference to the program state associated
       * with the glsl-authority... */
      if (authority != pipeline)
        {
          shader_state->ref_count++;
          set_shader_state (pipeline, shader_state);
        }
    }

  if (user_program)
    {
      /* If the user program contains a fragment shader then we don't need
         to generate one */
      if (_cogl_program_has_fragment_shader (user_program))
        {
          if (shader_state->gl_shader)
            {
              GE( ctx, glDeleteShader (shader_state->gl_shader) );
              shader_state->gl_shader = 0;
            }
          return;
        }
    }

  if (shader_state->gl_shader)
    return;

  /* If we make it here then we have a glsl_shader_state struct
     without a gl_shader either because this is the first time we've
     encountered it or because the user program has changed */

  /* We reuse two grow-only GStrings for code-gen. One string
     contains the uniform and attribute declarations while the
     other contains the main function. We need two strings
     because we need to dynamically declare attributes as the
     add_layer callback is invoked */
  g_string_set_size (ctx->codegen_header_buffer, 0);
  g_string_set_size (ctx->codegen_source_buffer, 0);
  shader_state->header = ctx->codegen_header_buffer;
  shader_state->source = ctx->codegen_source_buffer;
  _cogl_list_init (&shader_state->layers);

  add_layer_declarations (pipeline, shader_state);
  add_global_declarations (pipeline, shader_state);

  g_string_append (shader_state->source,
                   "void\n"
                   "cogl_generated_source ()\n"
                   "{\n");

  for (i = 0; i < n_layers; i++)
    {
      shader_state->unit_state[i].sampled = FALSE;
      shader_state->unit_state[i].combine_constant_used = FALSE;
    }
}

static void
add_constant_lookup (CoglPipelineShaderState *shader_state,
                     CoglPipeline *pipeline,
                     CoglPipelineLayer *layer,
                     const char *swizzle)
{
  g_string_append_printf (shader_state->header,
                          "_cogl_layer_constant_%i.%s",
                          layer->index, swizzle);
}

static void
ensure_texture_lookup_generated (CoglPipelineShaderState *shader_state,
                                 CoglPipeline *pipeline,
                                 CoglPipelineLayer *layer)
{
  int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
  CoglPipelineSnippetData snippet_data;
  CoglTextureType texture_type;
  const char *target_string, *tex_coord_swizzle;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (shader_state->unit_state[unit_index].sampled)
    return;

  texture_type =
    _cogl_pipeline_layer_get_texture_type (layer);
  _cogl_gl_util_get_texture_target_string (texture_type,
                                           &target_string,
                                           &tex_coord_swizzle);

  shader_state->unit_state[unit_index].sampled = TRUE;

  g_string_append_printf (shader_state->header,
                          "vec4 cogl_texel%i;\n",
                          layer->index);

  g_string_append_printf (shader_state->source,
                          "  cogl_texel%i = cogl_texture_lookup%i ("
                          "cogl_sampler%i, ",
                          layer->index,
                          layer->index,
                          layer->index);

  if (cogl_pipeline_get_layer_point_sprite_coords_enabled (pipeline,
                                                           layer->index))
    {
      shader_state->ref_point_coord = TRUE;
      g_string_append_printf (shader_state->source,
                              "vec4 (gl_PointCoord, 0.0, 1.0)");
    }
  else
    g_string_append_printf (shader_state->source,
                            "cogl_tex_coord%i_in",
                            layer->index);

  g_string_append (shader_state->source, ");\n");

  /* There's no need to generate the real texture lookup if it's going
     to be replaced */
  if (!has_replace_hook (layer, COGL_SNIPPET_HOOK_TEXTURE_LOOKUP))
    {
      g_string_append_printf (shader_state->header,
                              "vec4\n"
                              "cogl_real_texture_lookup%i (sampler%s tex,\n"
                              "                            vec4 coords)\n"
                              "{\n"
                              "  return ",
                              layer->index,
                              target_string);

      if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_TEXTURING)))
        g_string_append (shader_state->header,
                         "vec4 (1.0, 1.0, 1.0, 1.0);\n");
      else
        g_string_append_printf (shader_state->header,
                                "texture%s (tex, coords.%s);\n",
                                target_string, tex_coord_swizzle);

      g_string_append (shader_state->header, "}\n");
    }

  /* Wrap the texture lookup in any snippets that have been hooked */
  memset (&snippet_data, 0, sizeof (snippet_data));
  snippet_data.snippets = get_layer_fragment_snippets (layer);
  snippet_data.hook = COGL_SNIPPET_HOOK_TEXTURE_LOOKUP;
  snippet_data.chain_function = g_strdup_printf ("cogl_real_texture_lookup%i",
                                                 layer->index);
  snippet_data.final_name = g_strdup_printf ("cogl_texture_lookup%i",
                                             layer->index);
  snippet_data.function_prefix = g_strdup_printf ("cogl_texture_lookup_hook%i",
                                                  layer->index);
  snippet_data.return_type = "vec4";
  snippet_data.return_variable = "cogl_texel";
  snippet_data.arguments = "cogl_sampler, cogl_tex_coord";
  snippet_data.argument_declarations =
    g_strdup_printf ("sampler%s cogl_sampler, vec4 cogl_tex_coord",
                     target_string);
  snippet_data.source_buf = shader_state->header;

  _cogl_pipeline_snippet_generate_code (&snippet_data);

  g_free ((char *) snippet_data.chain_function);
  g_free ((char *) snippet_data.final_name);
  g_free ((char *) snippet_data.function_prefix);
  g_free ((char *) snippet_data.argument_declarations);
}

static void
add_arg (CoglPipelineShaderState *shader_state,
         CoglPipeline *pipeline,
         CoglPipelineLayer *layer,
         int previous_layer_index,
         CoglPipelineCombineSource src,
         CoglPipelineCombineOp operand,
         const char *swizzle)
{
  GString *shader_source = shader_state->header;
  char alpha_swizzle[5] = "aaaa";

  g_string_append_c (shader_source, '(');

  if (operand == COGL_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_COLOR ||
      operand == COGL_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_ALPHA)
    g_string_append_printf (shader_source,
                            "vec4(1.0, 1.0, 1.0, 1.0).%s - ",
                            swizzle);

  /* If the operand is reading from the alpha then replace the swizzle
     with the same number of copies of the alpha */
  if (operand == COGL_PIPELINE_COMBINE_OP_SRC_ALPHA ||
      operand == COGL_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_ALPHA)
    {
      alpha_swizzle[strlen (swizzle)] = '\0';
      swizzle = alpha_swizzle;
    }

  switch (src)
    {
    case COGL_PIPELINE_COMBINE_SOURCE_TEXTURE:
      g_string_append_printf (shader_source,
                              "cogl_texel%i.%s",
                              layer->index,
                              swizzle);
      break;

    case COGL_PIPELINE_COMBINE_SOURCE_CONSTANT:
      add_constant_lookup (shader_state,
                           pipeline,
                           layer,
                           swizzle);
      break;

    case COGL_PIPELINE_COMBINE_SOURCE_PREVIOUS:
      if (previous_layer_index >= 0)
        {
          g_string_append_printf (shader_source,
                                  "cogl_layer%i.%s",
                                  previous_layer_index,
                                  swizzle);
          break;
        }
      /* flow through */
    case COGL_PIPELINE_COMBINE_SOURCE_PRIMARY_COLOR:
      g_string_append_printf (shader_source, "cogl_color_in.%s", swizzle);
      break;

    default:
      {
        int layer_num = src - COGL_PIPELINE_COMBINE_SOURCE_TEXTURE0;
        CoglPipelineGetLayerFlags flags = COGL_PIPELINE_GET_LAYER_NO_CREATE;
        CoglPipelineLayer *other_layer =
          _cogl_pipeline_get_layer_with_flags (pipeline, layer_num, flags);

        if (other_layer == NULL)
          {
            static CoglBool warning_seen = FALSE;
            if (!warning_seen)
              {
                g_warning ("The application is trying to use a texture "
                           "combine with a layer number that does not exist");
                warning_seen = TRUE;
              }
            g_string_append_printf (shader_source,
                                    "vec4 (1.0, 1.0, 1.0, 1.0).%s",
                                    swizzle);
          }
        else
          g_string_append_printf (shader_source,
                                  "cogl_texel%i.%s",
                                  other_layer->index,
                                  swizzle);
      }
      break;
    }

  g_string_append_c (shader_source, ')');
}

static void
ensure_arg_generated (CoglPipeline *pipeline,
                      CoglPipelineLayer *layer,
                      int previous_layer_index,
                      CoglPipelineCombineSource src)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);

  switch (src)
    {
    case COGL_PIPELINE_COMBINE_SOURCE_PRIMARY_COLOR:
      /* This doesn't involve any other layers */
      break;

    case COGL_PIPELINE_COMBINE_SOURCE_CONSTANT:
      {
        int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
        /* Create a sampler uniform for this layer if we haven't already */
        if (!shader_state->unit_state[unit_index].combine_constant_used)
          {
            g_string_append_printf (shader_state->header,
                                    "uniform vec4 _cogl_layer_constant_%i;\n",
                                    layer->index);
            shader_state->unit_state[unit_index].combine_constant_used = TRUE;
          }
      }
      break;

    case COGL_PIPELINE_COMBINE_SOURCE_PREVIOUS:
      if (previous_layer_index >= 0)
        ensure_layer_generated (pipeline, previous_layer_index);
      break;

    case COGL_PIPELINE_COMBINE_SOURCE_TEXTURE:
      ensure_texture_lookup_generated (shader_state,
                                       pipeline,
                                       layer);
      break;

    default:
      if (src >= COGL_PIPELINE_COMBINE_SOURCE_TEXTURE0)
        {
          int layer_num = src - COGL_PIPELINE_COMBINE_SOURCE_TEXTURE0;
          CoglPipelineGetLayerFlags flags = COGL_PIPELINE_GET_LAYER_NO_CREATE;
          CoglPipelineLayer *other_layer =
            _cogl_pipeline_get_layer_with_flags (pipeline, layer_num, flags);

          if (other_layer)
            ensure_texture_lookup_generated (shader_state,
                                             pipeline,
                                             other_layer);
        }
      break;
    }
}

static void
ensure_args_for_func (CoglPipeline *pipeline,
                      CoglPipelineLayer *layer,
                      int previous_layer_index,
                      CoglPipelineCombineFunc function,
                      CoglPipelineCombineSource *src)
{
  int n_args = _cogl_get_n_args_for_combine_func (function);
  int i;

  for (i = 0; i < n_args; i++)
    ensure_arg_generated (pipeline, layer, previous_layer_index, src[i]);
}

static void
append_masked_combine (CoglPipeline *pipeline,
                       CoglPipelineLayer *layer,
                       int previous_layer_index,
                       const char *swizzle,
                       CoglPipelineCombineFunc function,
                       CoglPipelineCombineSource *src,
                       CoglPipelineCombineOp *op)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);
  GString *shader_source = shader_state->header;

  g_string_append_printf (shader_state->header,
                          "  cogl_layer.%s = ",
                          swizzle);

  switch (function)
    {
    case COGL_PIPELINE_COMBINE_FUNC_REPLACE:
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[0], op[0], swizzle);
      break;

    case COGL_PIPELINE_COMBINE_FUNC_MODULATE:
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " * ");
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[1], op[1], swizzle);
      break;

    case COGL_PIPELINE_COMBINE_FUNC_ADD:
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " + ");
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[1], op[1], swizzle);
      break;

    case COGL_PIPELINE_COMBINE_FUNC_ADD_SIGNED:
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " + ");
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[1], op[1], swizzle);
      g_string_append_printf (shader_source,
                              " - vec4(0.5, 0.5, 0.5, 0.5).%s",
                              swizzle);
      break;

    case COGL_PIPELINE_COMBINE_FUNC_SUBTRACT:
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " - ");
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[1], op[1], swizzle);
      break;

    case COGL_PIPELINE_COMBINE_FUNC_INTERPOLATE:
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[0], op[0], swizzle);
      g_string_append (shader_source, " * ");
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[2], op[2], swizzle);
      g_string_append (shader_source, " + ");
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[1], op[1], swizzle);
      g_string_append_printf (shader_source,
                              " * (vec4(1.0, 1.0, 1.0, 1.0).%s - ",
                              swizzle);
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[2], op[2], swizzle);
      g_string_append_c (shader_source, ')');
      break;

    case COGL_PIPELINE_COMBINE_FUNC_DOT3_RGB:
    case COGL_PIPELINE_COMBINE_FUNC_DOT3_RGBA:
      g_string_append (shader_source, "vec4(4.0 * ((");
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[0], op[0], "r");
      g_string_append (shader_source, " - 0.5) * (");
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[1], op[1], "r");
      g_string_append (shader_source, " - 0.5) + (");
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[0], op[0], "g");
      g_string_append (shader_source, " - 0.5) * (");
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[1], op[1], "g");
      g_string_append (shader_source, " - 0.5) + (");
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[0], op[0], "b");
      g_string_append (shader_source, " - 0.5) * (");
      add_arg (shader_state, pipeline, layer, previous_layer_index,
               src[1], op[1], "b");
      g_string_append_printf (shader_source, " - 0.5))).%s", swizzle);
      break;
    }

  g_string_append_printf (shader_source, ";\n");
}

static void
ensure_layer_generated (CoglPipeline *pipeline,
                        int layer_index)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);
  CoglPipelineLayer *combine_authority;
  CoglPipelineLayerBigState *big_state;
  CoglPipelineLayer *layer;
  CoglPipelineSnippetData snippet_data;
  LayerData *layer_data;

  /* Find the layer that corresponds to this layer_num */
  _cogl_list_for_each (layer_data, &shader_state->layers, link)
    {
      layer = layer_data->layer;

      if (layer->index == layer_index)
        goto found;
    }

  /* If we didn't find it then we can assume the layer has already
     been generated */
  return;

 found:

  /* Remove the layer from the list so we don't generate it again */
  _cogl_list_remove (&layer_data->link);

  combine_authority =
    _cogl_pipeline_layer_get_authority (layer,
                                        COGL_PIPELINE_LAYER_STATE_COMBINE);
  big_state = combine_authority->big_state;

  /* Make a global variable for the result of the layer code */
  g_string_append_printf (shader_state->header,
                          "vec4 cogl_layer%i;\n",
                          layer_index);

  /* Skip the layer generation if there is a snippet that replaces the
     default layer code. This is important because generating this
     code may cause the code for other layers to be generated and
     stored in the global variable. If this code isn't actually used
     then the global variables would be uninitialised and they may be
     used from other layers */
  if (!has_replace_hook (layer, COGL_SNIPPET_HOOK_LAYER_FRAGMENT))
    {
      ensure_args_for_func (pipeline,
                            layer,
                            layer_data->previous_layer_index,
                            big_state->texture_combine_rgb_func,
                            big_state->texture_combine_rgb_src);
      ensure_args_for_func (pipeline,
                            layer,
                            layer_data->previous_layer_index,
                            big_state->texture_combine_alpha_func,
                            big_state->texture_combine_alpha_src);

      g_string_append_printf (shader_state->header,
                              "vec4\n"
                              "cogl_real_generate_layer%i ()\n"
                              "{\n"
                              "  vec4 cogl_layer;\n",
                              layer_index);

      if (!_cogl_pipeline_layer_needs_combine_separate (combine_authority) ||
          /* GL_DOT3_RGBA Is a bit weird as a GL_COMBINE_RGB function
           * since if you use it, it overrides your ALPHA function...
           */
          big_state->texture_combine_rgb_func ==
          COGL_PIPELINE_COMBINE_FUNC_DOT3_RGBA)
        append_masked_combine (pipeline,
                               layer,
                               layer_data->previous_layer_index,
                               "rgba",
                               big_state->texture_combine_rgb_func,
                               big_state->texture_combine_rgb_src,
                               big_state->texture_combine_rgb_op);
      else
        {
          append_masked_combine (pipeline,
                                 layer,
                                 layer_data->previous_layer_index,
                                 "rgb",
                                 big_state->texture_combine_rgb_func,
                                 big_state->texture_combine_rgb_src,
                                 big_state->texture_combine_rgb_op);
          append_masked_combine (pipeline,
                                 layer,
                                 layer_data->previous_layer_index,
                                 "a",
                                 big_state->texture_combine_alpha_func,
                                 big_state->texture_combine_alpha_src,
                                 big_state->texture_combine_alpha_op);
        }

      g_string_append (shader_state->header,
                       "  return cogl_layer;\n"
                       "}\n");
    }

  /* Wrap the layer code in any snippets that have been hooked */
  memset (&snippet_data, 0, sizeof (snippet_data));
  snippet_data.snippets = get_layer_fragment_snippets (layer);
  snippet_data.hook = COGL_SNIPPET_HOOK_LAYER_FRAGMENT;
  snippet_data.chain_function = g_strdup_printf ("cogl_real_generate_layer%i",
                                                 layer_index);
  snippet_data.final_name = g_strdup_printf ("cogl_generate_layer%i",
                                             layer_index);
  snippet_data.function_prefix = g_strdup_printf ("cogl_generate_layer%i",
                                                  layer_index);
  snippet_data.return_type = "vec4";
  snippet_data.return_variable = "cogl_layer";
  snippet_data.source_buf = shader_state->header;

  _cogl_pipeline_snippet_generate_code (&snippet_data);

  g_free ((char *) snippet_data.chain_function);
  g_free ((char *) snippet_data.final_name);
  g_free ((char *) snippet_data.function_prefix);

  g_string_append_printf (shader_state->source,
                          "  cogl_layer%i = cogl_generate_layer%i ();\n",
                          layer_index,
                          layer_index);

  g_slice_free (LayerData, layer_data);
}

static CoglBool
_cogl_pipeline_fragend_glsl_add_layer (CoglPipeline *pipeline,
                                        CoglPipelineLayer *layer,
                                        unsigned long layers_difference)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);
  LayerData *layer_data;

  if (!shader_state->source)
    return TRUE;

  /* Store the layers in reverse order */
  layer_data = g_slice_new (LayerData);
  layer_data->layer = layer;

  if (_cogl_list_empty (&shader_state->layers))
    {
      layer_data->previous_layer_index = -1;
    }
  else
    {
      LayerData *first =
        _cogl_container_of (shader_state->layers.next, first, link);
      layer_data->previous_layer_index = first->layer->index;
    }

  _cogl_list_insert (&shader_state->layers, &layer_data->link);

  return TRUE;
}

/* GLES2 and GL3 don't have alpha testing so we need to implement it
   in the shader */

#if defined(HAVE_COGL_GLES2) || defined(HAVE_COGL_GL)

static void
add_alpha_test_snippet (CoglPipeline *pipeline,
                        CoglPipelineShaderState *shader_state)
{
  CoglPipelineAlphaFunc alpha_func;

  alpha_func = cogl_pipeline_get_alpha_test_function (pipeline);

  if (alpha_func == COGL_PIPELINE_ALPHA_FUNC_ALWAYS)
    /* Do nothing */
    return;

  if (alpha_func == COGL_PIPELINE_ALPHA_FUNC_NEVER)
    {
      /* Always discard the fragment */
      g_string_append (shader_state->source,
                       "  discard;\n");
      return;
    }

  /* For all of the other alpha functions we need a uniform for the
     reference */

  g_string_append (shader_state->header,
                   "uniform float _cogl_alpha_test_ref;\n");

  g_string_append (shader_state->source,
                   "  if (cogl_color_out.a ");

  switch (alpha_func)
    {
    case COGL_PIPELINE_ALPHA_FUNC_LESS:
      g_string_append (shader_state->source, ">=");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_EQUAL:
      g_string_append (shader_state->source, "!=");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_LEQUAL:
      g_string_append (shader_state->source, ">");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_GREATER:
      g_string_append (shader_state->source, "<=");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_NOTEQUAL:
      g_string_append (shader_state->source, "==");
      break;
    case COGL_PIPELINE_ALPHA_FUNC_GEQUAL:
      g_string_append (shader_state->source, "< ");
      break;

    case COGL_PIPELINE_ALPHA_FUNC_ALWAYS:
    case COGL_PIPELINE_ALPHA_FUNC_NEVER:
      g_assert_not_reached ();
      break;
    }

  g_string_append (shader_state->source,
                   " _cogl_alpha_test_ref)\n    discard;\n");
}

#endif /*  HAVE_COGL_GLES2 */

static CoglBool
_cogl_pipeline_fragend_glsl_end (CoglPipeline *pipeline,
                                 unsigned long pipelines_difference)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (shader_state->source)
    {
      const char *source_strings[2];
      GLint lengths[2];
      GLint compile_status;
      GLuint shader;
      CoglPipelineSnippetData snippet_data;
      const char *version_string;

      COGL_STATIC_COUNTER (fragend_glsl_compile_counter,
                           "glsl fragment compile counter",
                           "Increments each time a new GLSL "
                           "fragment shader is compiled",
                           0 /* no application private data */);
      COGL_COUNTER_INC (_cogl_uprof_context, fragend_glsl_compile_counter);

      /* We only need to generate code to calculate the fragment value
         for the last layer. If the value of this layer depends on any
         previous layers then it will recursively generate the code
         for those layers */
      if (!_cogl_list_empty (&shader_state->layers))
        {
          CoglPipelineLayer *last_layer;
          LayerData *layer_data, *tmp;

          layer_data = _cogl_container_of (shader_state->layers.next,
                                           layer_data,
                                           link);
          last_layer = layer_data->layer;

          ensure_layer_generated (pipeline, last_layer->index);
          g_string_append_printf (shader_state->source,
                                  "  cogl_color_out = cogl_layer%i;\n",
                                  last_layer->index);

          _cogl_list_for_each_safe (layer_data,
                                    tmp,
                                    &shader_state->layers,
                                    link)
            g_slice_free (LayerData, layer_data);
        }
      else
        g_string_append (shader_state->source,
                         "  cogl_color_out = cogl_color_in;\n");

#if defined(HAVE_COGL_GLES2) || defined (HAVE_COGL_GL)
      if (!(ctx->private_feature_flags & COGL_PRIVATE_FEATURE_ALPHA_TEST))
        add_alpha_test_snippet (pipeline, shader_state);
#endif

      /* Close the function surrounding the generated fragment processing */
      g_string_append (shader_state->source, "}\n");

      /* Add all of the hooks for fragment processing */
      memset (&snippet_data, 0, sizeof (snippet_data));
      snippet_data.snippets = get_fragment_snippets (pipeline);
      snippet_data.hook = COGL_SNIPPET_HOOK_FRAGMENT;
      snippet_data.chain_function = "cogl_generated_source";
      snippet_data.final_name = "main";
      snippet_data.function_prefix = "cogl_fragment_hook";
      snippet_data.source_buf = shader_state->source;
      _cogl_pipeline_snippet_generate_code (&snippet_data);

      GE_RET( shader, ctx, glCreateShader (GL_FRAGMENT_SHADER) );

      lengths[0] = shader_state->header->len;
      source_strings[0] = shader_state->header->str;
      lengths[1] = shader_state->source->len;
      source_strings[1] = shader_state->source->str;

      if (shader_state->ref_point_coord &&
          !(ctx->private_feature_flags & COGL_PRIVATE_FEATURE_GL_EMBEDDED))
        version_string = "#version 120\n";
      else
        version_string = NULL;

      _cogl_glsl_shader_set_source_with_boilerplate (ctx,
                                                     version_string,
                                                     shader, GL_FRAGMENT_SHADER,
                                                     pipeline,
                                                     2, /* count */
                                                     source_strings, lengths);

      GE( ctx, glCompileShader (shader) );
      GE( ctx, glGetShaderiv (shader, GL_COMPILE_STATUS, &compile_status) );

      if (!compile_status)
        {
          GLint len = 0;
          char *shader_log;

          GE( ctx, glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &len) );
          shader_log = g_alloca (len);
          GE( ctx, glGetShaderInfoLog (shader, len, &len, shader_log) );
          g_warning ("Shader compilation failed:\n%s", shader_log);
        }

      shader_state->header = NULL;
      shader_state->source = NULL;
      shader_state->gl_shader = shader;
    }

  return TRUE;
}

static void
_cogl_pipeline_fragend_glsl_pre_change_notify (CoglPipeline *pipeline,
                                               CoglPipelineState change,
                                               const CoglColor *new_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if ((change & _cogl_pipeline_get_state_for_fragment_codegen (ctx)))
    dirty_shader_state (pipeline);
}

/* NB: layers are considered immutable once they have any dependants
 * so although multiple pipelines can end up depending on a single
 * static layer, we can guarantee that if a layer is being *changed*
 * then it can only have one pipeline depending on it.
 *
 * XXX: Don't forget this is *pre* change, we can't read the new value
 * yet!
 */
static void
_cogl_pipeline_fragend_glsl_layer_pre_change_notify (
                                                CoglPipeline *owner,
                                                CoglPipelineLayer *layer,
                                                CoglPipelineLayerState change)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if ((change & _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx)))
    {
      dirty_shader_state (owner);
      return;
    }

  /* TODO: we could be saving snippets of texture combine code along
   * with each layer and then when a layer changes we would just free
   * the snippet. */
}

const CoglPipelineFragend _cogl_pipeline_glsl_fragend =
{
  _cogl_pipeline_fragend_glsl_start,
  _cogl_pipeline_fragend_glsl_add_layer,
  NULL, /* passthrough */
  _cogl_pipeline_fragend_glsl_end,
  _cogl_pipeline_fragend_glsl_pre_change_notify,
  NULL, /* pipeline_set_parent_notify */
  _cogl_pipeline_fragend_glsl_layer_pre_change_notify
};

#endif /* COGL_PIPELINE_FRAGEND_GLSL */

