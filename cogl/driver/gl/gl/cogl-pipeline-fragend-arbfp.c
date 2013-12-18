/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-debug.h"
#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-state-private.h"
#include "cogl-pipeline-layer-private.h"

#ifdef COGL_PIPELINE_FRAGEND_ARBFP

#include "cogl-context-private.h"
#include "cogl-object-private.h"

#include "cogl-texture-private.h"
#include "cogl-blend-string.h"
#include "cogl-journal-private.h"
#include "cogl-color-private.h"
#include "cogl-profile.h"
#include "cogl-program-private.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>

/* This might not be defined on GLES */
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D                           0x806F
#endif

const CoglPipelineFragend _cogl_pipeline_arbfp_fragend;

typedef struct _UnitState
{
  int constant_id; /* The program.local[] index */
  unsigned int dirty_combine_constant:1;
  unsigned int has_combine_constant:1;

  unsigned int sampled:1;
} UnitState;

typedef struct
{
  int ref_count;

  CoglHandle user_program;
  /* XXX: only valid during codegen */
  GString *source;
  GLuint gl_program;
  UnitState *unit_state;
  int next_constant_id;

  /* Age of the program the last time the uniforms were flushed. This
     is used to detect when we need to flush all of the uniforms */
  unsigned int user_program_age;

  /* We need to track the last pipeline that an ARBfp program was used
   * with so know if we need to update any program.local parameters. */
  CoglPipeline *last_used_for_pipeline;

  CoglPipelineCacheEntry *cache_entry;
} CoglPipelineShaderState;

static CoglUserDataKey shader_state_key;

static CoglPipelineShaderState *
shader_state_new (int n_layers,
                  CoglPipelineCacheEntry *cache_entry)
{
  CoglPipelineShaderState *shader_state;

  shader_state = g_slice_new0 (CoglPipelineShaderState);
  shader_state->ref_count = 1;
  shader_state->unit_state = g_new0 (UnitState, n_layers);
  shader_state->cache_entry = cache_entry;

  return shader_state;
}

static CoglPipelineShaderState *
get_shader_state (CoglPipeline *pipeline)
{
  return cogl_object_get_user_data (COGL_OBJECT (pipeline), &shader_state_key);
}

static void
destroy_shader_state (void *user_data,
                      void *instance)
{
  CoglPipelineShaderState *shader_state = user_data;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* If the shader state was last used for this pipeline then clear it
     so that if same address gets used again for a new pipeline then
     we won't think it's the same pipeline and avoid updating the
     constants */
  if (shader_state->last_used_for_pipeline == instance)
    shader_state->last_used_for_pipeline = NULL;

  if (shader_state->cache_entry &&
      shader_state->cache_entry->pipeline != instance)
    shader_state->cache_entry->usage_count--;

  if (--shader_state->ref_count == 0)
    {
      if (shader_state->gl_program)
        {
          GE (ctx, glDeletePrograms (1, &shader_state->gl_program));
          shader_state->gl_program = 0;
        }

      g_free (shader_state->unit_state);

      g_slice_free (CoglPipelineShaderState, shader_state);
    }
}

static void
set_shader_state (CoglPipeline *pipeline, CoglPipelineShaderState *shader_state)
{
  if (shader_state)
    {
      shader_state->ref_count++;

      /* If we're not setting the state on the template pipeline then
       * mark it as a usage of the pipeline cache entry */
      if (shader_state->cache_entry &&
          shader_state->cache_entry->pipeline != pipeline)
        shader_state->cache_entry->usage_count++;
    }

  _cogl_object_set_user_data (COGL_OBJECT (pipeline),
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

static void
_cogl_pipeline_fragend_arbfp_start (CoglPipeline *pipeline,
                                    int n_layers,
                                    unsigned long pipelines_difference)
{
  CoglPipelineShaderState *shader_state;
  CoglPipeline *authority;
  CoglPipelineCacheEntry *cache_entry = NULL;
  CoglProgram *user_program = cogl_pipeline_get_user_program (pipeline);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Now lookup our ARBfp backend private state */
  shader_state = get_shader_state (pipeline);

  /* If we have a valid shader_state then we are all set and don't
   * need to generate a new program. */
  if (shader_state)
    return;

  /* If we don't have an associated arbfp program yet then find the
   * arbfp-authority (the oldest ancestor whose state will result in
   * the same program being generated as for this pipeline).
   *
   * We always make sure to associate new programs with the
   * arbfp-authority to maximize the chance that other pipelines can
   * share it.
   */
  authority = _cogl_pipeline_find_equivalent_parent
    (pipeline,
     _cogl_pipeline_get_state_for_fragment_codegen (ctx) &
     ~COGL_PIPELINE_STATE_LAYERS,
     _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx));
  shader_state = get_shader_state (authority);
  if (shader_state)
    {
      /* If we are going to share our program state with an arbfp-authority
       * then add a reference to the program state associated with that
       * arbfp-authority... */
      set_shader_state (pipeline, shader_state);
      return;
    }

  /* If we haven't yet found an existing program then before we resort to
   * generating a new arbfp program we see if we can find a suitable
   * program in the pipeline_cache. */
  if (G_LIKELY (!(COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_PROGRAM_CACHES))))
    {
      cache_entry =
        _cogl_pipeline_cache_get_fragment_template (ctx->pipeline_cache,
                                                    authority);

      shader_state = get_shader_state (cache_entry->pipeline);

      if (shader_state)
        shader_state->ref_count++;
    }

  /* If we still haven't got a shader state then we'll have to create
     a new one */
  if (shader_state == NULL)
    {
      shader_state = shader_state_new (n_layers, cache_entry);

      shader_state->user_program = user_program;
      if (user_program == COGL_INVALID_HANDLE)
        {
          /* We reuse a single grow-only GString for code-gen */
          g_string_set_size (ctx->codegen_source_buffer, 0);
          shader_state->source = ctx->codegen_source_buffer;
          g_string_append (shader_state->source,
                           "!!ARBfp1.0\n"
                           "TEMP output;\n"
                           "TEMP tmp0, tmp1, tmp2, tmp3, tmp4;\n"
                           "PARAM half = {.5, .5, .5, .5};\n"
                           "PARAM one = {1, 1, 1, 1};\n"
                           "PARAM two = {2, 2, 2, 2};\n"
                           "PARAM minus_one = {-1, -1, -1, -1};\n");
        }
    }

  set_shader_state (pipeline, shader_state);

  shader_state->ref_count--;

  /* Since we have already resolved the arbfp-authority at this point
   * we might as well also associate any program we find from the cache
   * with the authority too... */
  if (authority != pipeline)
    set_shader_state (authority, shader_state);

  /* If we found a template then we'll attach it to that too so that
     next time a similar pipeline is used it can use the same state */
  if (cache_entry)
    set_shader_state (cache_entry->pipeline, shader_state);
}

static const char *
texture_type_to_arbfp_string (CoglTextureType texture_type)
{
  switch (texture_type)
    {
#if 0 /* TODO */
    case COGL_TEXTURE_TYPE_1D:
      return "1D";
#endif
    case COGL_TEXTURE_TYPE_2D:
      return "2D";
    case COGL_TEXTURE_TYPE_3D:
      return "3D";
    case COGL_TEXTURE_TYPE_RECTANGLE:
      return "RECT";
    }

  g_warn_if_reached ();

  return "2D";
}

static void
setup_texture_source (CoglPipelineShaderState *shader_state,
                      int unit_index,
                      CoglTextureType texture_type)
{
  if (!shader_state->unit_state[unit_index].sampled)
    {
      if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_TEXTURING)))
        g_string_append_printf (shader_state->source,
                                "TEMP texel%d;\n"
                                "MOV texel%d, one;\n",
                                unit_index,
                                unit_index);
      else
        g_string_append_printf (shader_state->source,
                                "TEMP texel%d;\n"
                                "TEX texel%d,fragment.texcoord[%d],"
                                "texture[%d],%s;\n",
                                unit_index,
                                unit_index,
                                unit_index,
                                unit_index,
                                texture_type_to_arbfp_string (texture_type));
      shader_state->unit_state[unit_index].sampled = TRUE;
    }
}

typedef enum _CoglPipelineFragendARBfpArgType
{
  COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_SIMPLE,
  COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_CONSTANT,
  COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_TEXTURE
} CoglPipelineFragendARBfpArgType;

typedef struct _CoglPipelineFragendARBfpArg
{
  const char *name;

  CoglPipelineFragendARBfpArgType type;

  /* for type = TEXTURE */
  int texture_unit;
  CoglTextureType texture_type;

  /* for type = CONSTANT */
  int constant_id;

  const char *swizzle;

} CoglPipelineFragendARBfpArg;

static void
append_arg (GString *source, const CoglPipelineFragendARBfpArg *arg)
{
  switch (arg->type)
    {
    case COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_TEXTURE:
      g_string_append_printf (source, "texel%d%s",
                              arg->texture_unit, arg->swizzle);
      break;
    case COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_CONSTANT:
      g_string_append_printf (source, "program.local[%d]%s",
                              arg->constant_id, arg->swizzle);
      break;
    case COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_SIMPLE:
      g_string_append_printf (source, "%s%s",
                              arg->name, arg->swizzle);
      break;
    }
}

/* Note: we are trying to avoid duplicating strings during codegen
 * which is why we have the slightly awkward
 * CoglPipelineFragendARBfpArg mechanism. */
static void
setup_arg (CoglPipeline *pipeline,
           CoglPipelineLayer *layer,
           CoglBlendStringChannelMask mask,
           int arg_index,
           CoglPipelineCombineSource src,
           GLint op,
           CoglPipelineFragendARBfpArg *arg)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);
  static const char *tmp_name[3] = { "tmp0", "tmp1", "tmp2" };

  switch (src)
    {
    case COGL_PIPELINE_COMBINE_SOURCE_TEXTURE:
      arg->type = COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_TEXTURE;
      arg->name = "texel%d";
      arg->texture_unit = _cogl_pipeline_layer_get_unit_index (layer);
      setup_texture_source (shader_state,
                            arg->texture_unit,
                            _cogl_pipeline_layer_get_texture_type (layer));
      break;
    case COGL_PIPELINE_COMBINE_SOURCE_CONSTANT:
      {
        int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
        UnitState *unit_state = &shader_state->unit_state[unit_index];

        unit_state->constant_id = shader_state->next_constant_id++;
        unit_state->has_combine_constant = TRUE;
        unit_state->dirty_combine_constant = TRUE;

        arg->type = COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_CONSTANT;
        arg->name = "program.local[%d]";
        arg->constant_id = unit_state->constant_id;
        break;
      }
    case COGL_PIPELINE_COMBINE_SOURCE_PRIMARY_COLOR:
      arg->type = COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_SIMPLE;
      arg->name = "fragment.color.primary";
      break;
    case COGL_PIPELINE_COMBINE_SOURCE_PREVIOUS:
      arg->type = COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_SIMPLE;
      if (_cogl_pipeline_layer_get_unit_index (layer) == 0)
        arg->name = "fragment.color.primary";
      else
        arg->name = "output";
      break;
    default: /* Sample the texture attached to a specific layer */
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
            arg->type = COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_SIMPLE;
            arg->name = "output";
          }
        else
          {
            CoglTextureType texture_type;

            arg->type = COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_TEXTURE;
            arg->name = "texture[%d]";
            arg->texture_unit =
              _cogl_pipeline_layer_get_unit_index (other_layer);
            texture_type = _cogl_pipeline_layer_get_texture_type (other_layer);
            setup_texture_source (shader_state,
                                  arg->texture_unit,
                                  texture_type);
          }
      }
      break;
    }

  arg->swizzle = "";

  switch (op)
    {
    case COGL_PIPELINE_COMBINE_OP_SRC_COLOR:
      break;
    case COGL_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_COLOR:
      g_string_append_printf (shader_state->source,
                              "SUB tmp%d, one, ",
                              arg_index);
      append_arg (shader_state->source, arg);
      g_string_append_printf (shader_state->source, ";\n");
      arg->type = COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_SIMPLE;
      arg->name = tmp_name[arg_index];
      arg->swizzle = "";
      break;
    case COGL_PIPELINE_COMBINE_OP_SRC_ALPHA:
      /* avoid a swizzle if we know RGB are going to be masked
       * in the end anyway */
      if (mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        arg->swizzle = ".a";
      break;
    case COGL_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_ALPHA:
      g_string_append_printf (shader_state->source,
                              "SUB tmp%d, one, ",
                              arg_index);
      append_arg (shader_state->source, arg);
      /* avoid a swizzle if we know RGB are going to be masked
       * in the end anyway */
      if (mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        g_string_append_printf (shader_state->source, ".a;\n");
      else
        g_string_append_printf (shader_state->source, ";\n");
      arg->type = COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_SIMPLE;
      arg->name = tmp_name[arg_index];
      break;
    default:
      g_error ("Unknown texture combine operator %d", op);
      break;
    }
}

static CoglBool
fragend_arbfp_args_equal (CoglPipelineFragendARBfpArg *arg0,
                          CoglPipelineFragendARBfpArg *arg1)
{
  if (arg0->type != arg1->type)
    return FALSE;

  if (arg0->name != arg1->name &&
      strcmp (arg0->name, arg1->name) != 0)
    return FALSE;

  if (arg0->type == COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_TEXTURE &&
      arg0->texture_unit != arg1->texture_unit)
    return FALSE;
  /* Note we don't have to check the target; a texture unit can only
   * have one target enabled at a time. */

  if (arg0->type == COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_CONSTANT &&
      arg0->constant_id != arg1->constant_id)
    return FALSE;

  if (arg0->swizzle != arg1->swizzle &&
      strcmp (arg0->swizzle, arg1->swizzle) != 0)
    return FALSE;

  return TRUE;
}

static void
append_function (CoglPipeline *pipeline,
                 CoglBlendStringChannelMask mask,
                 GLint function,
                 CoglPipelineFragendARBfpArg *args,
                 int n_args)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);
  const char *mask_name;

  switch (mask)
    {
    case COGL_BLEND_STRING_CHANNEL_MASK_RGB:
      mask_name = ".rgb";
      break;
    case COGL_BLEND_STRING_CHANNEL_MASK_ALPHA:
      mask_name = ".a";
      break;
    case COGL_BLEND_STRING_CHANNEL_MASK_RGBA:
      mask_name = "";
      break;
    default:
      g_error ("Unknown channel mask %d", mask);
      mask_name = "";
    }

  switch (function)
    {
    case COGL_PIPELINE_COMBINE_FUNC_ADD:
      g_string_append_printf (shader_state->source,
                              "ADD_SAT output%s, ",
                              mask_name);
      break;
    case COGL_PIPELINE_COMBINE_FUNC_MODULATE:
      /* Note: no need to saturate since we can assume operands
       * have values in the range [0,1] */
      g_string_append_printf (shader_state->source, "MUL output%s, ",
                              mask_name);
      break;
    case COGL_PIPELINE_COMBINE_FUNC_REPLACE:
      /* Note: no need to saturate since we can assume operand
       * has a value in the range [0,1] */
      g_string_append_printf (shader_state->source, "MOV output%s, ",
                              mask_name);
      break;
    case COGL_PIPELINE_COMBINE_FUNC_SUBTRACT:
      g_string_append_printf (shader_state->source,
                              "SUB_SAT output%s, ",
                              mask_name);
      break;
    case COGL_PIPELINE_COMBINE_FUNC_ADD_SIGNED:
      g_string_append_printf (shader_state->source, "ADD tmp3%s, ",
                              mask_name);
      append_arg (shader_state->source, &args[0]);
      g_string_append (shader_state->source, ", ");
      append_arg (shader_state->source, &args[1]);
      g_string_append (shader_state->source, ";\n");
      g_string_append_printf (shader_state->source,
                              "SUB_SAT output%s, tmp3, half",
                              mask_name);
      n_args = 0;
      break;
    case COGL_PIPELINE_COMBINE_FUNC_DOT3_RGB:
    /* These functions are the same except that GL_DOT3_RGB never
     * updates the alpha channel.
     *
     * NB: GL_DOT3_RGBA is a bit special because it effectively forces
     * an RGBA mask and we end up ignoring any separate alpha channel
     * function.
     */
    case COGL_PIPELINE_COMBINE_FUNC_DOT3_RGBA:
      {
        const char *tmp4 = "tmp4";

        /* The maths for this was taken from Mesa;
         * apparently:
         *
         * tmp3 = 2*src0 - 1
         * tmp4 = 2*src1 - 1
         * output = DP3 (tmp3, tmp4)
         *
         * is the same as:
         *
         * output = 4 * DP3 (src0 - 0.5, src1 - 0.5)
         */

        g_string_append (shader_state->source, "MAD tmp3, two, ");
        append_arg (shader_state->source, &args[0]);
        g_string_append (shader_state->source, ", minus_one;\n");

        if (!fragend_arbfp_args_equal (&args[0], &args[1]))
          {
            g_string_append (shader_state->source, "MAD tmp4, two, ");
            append_arg (shader_state->source, &args[1]);
            g_string_append (shader_state->source, ", minus_one;\n");
          }
        else
          tmp4 = "tmp3";

        g_string_append_printf (shader_state->source,
                                "DP3_SAT output%s, tmp3, %s",
                                mask_name, tmp4);
        n_args = 0;
      }
      break;
    case COGL_PIPELINE_COMBINE_FUNC_INTERPOLATE:
      /* Note: no need to saturate since we can assume operands
       * have values in the range [0,1] */

      /* NB: GL_INTERPOLATE = arg0*arg2 + arg1*(1-arg2)
       * but LRP dst, a, b, c = b*a + c*(1-a) */
      g_string_append_printf (shader_state->source, "LRP output%s, ",
                              mask_name);
      append_arg (shader_state->source, &args[2]);
      g_string_append (shader_state->source, ", ");
      append_arg (shader_state->source, &args[0]);
      g_string_append (shader_state->source, ", ");
      append_arg (shader_state->source, &args[1]);
      n_args = 0;
      break;
    default:
      g_error ("Unknown texture combine function %d", function);
      g_string_append_printf (shader_state->source, "MUL_SAT output%s, ",
                              mask_name);
      n_args = 2;
      break;
    }

  if (n_args > 0)
    append_arg (shader_state->source, &args[0]);
  if (n_args > 1)
    {
      g_string_append (shader_state->source, ", ");
      append_arg (shader_state->source, &args[1]);
    }
  g_string_append (shader_state->source, ";\n");
}

static void
append_masked_combine (CoglPipeline *arbfp_authority,
                       CoglPipelineLayer *layer,
                       CoglBlendStringChannelMask mask,
                       CoglPipelineCombineFunc function,
                       CoglPipelineCombineSource *src,
                       CoglPipelineCombineOp *op)
{
  int i;
  int n_args;
  CoglPipelineFragendARBfpArg args[3];

  n_args = _cogl_get_n_args_for_combine_func (function);

  for (i = 0; i < n_args; i++)
    {
      setup_arg (arbfp_authority,
                 layer,
                 mask,
                 i,
                 src[i],
                 op[i],
                 &args[i]);
    }

  append_function (arbfp_authority,
                   mask,
                   function,
                   args,
                   n_args);
}

static CoglBool
_cogl_pipeline_fragend_arbfp_add_layer (CoglPipeline *pipeline,
                                        CoglPipelineLayer *layer,
                                        unsigned long layers_difference)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);
  CoglPipelineLayer *combine_authority =
    _cogl_pipeline_layer_get_authority (layer,
                                        COGL_PIPELINE_LAYER_STATE_COMBINE);
  CoglPipelineLayerBigState *big_state = combine_authority->big_state;

  /* Notes...
   *
   * We are ignoring the issue of texture indirection limits until
   * someone complains (Ref Section 3.11.6 in the ARB_fragment_program
   * spec)
   *
   * There always five TEMPs named tmp0, tmp1 and tmp2, tmp3 and tmp4
   * available and these constants: 'one' = {1, 1, 1, 1}, 'half'
   * {.5, .5, .5, .5}, 'two' = {2, 2, 2, 2}, 'minus_one' = {-1, -1,
   * -1, -1}
   *
   * tmp0-2 are intended for dealing with some of the texture combine
   * operands (e.g. GL_ONE_MINUS_SRC_COLOR) tmp3/4 are for dealing
   * with the GL_ADD_SIGNED texture combine and the GL_DOT3_RGB[A]
   * functions.
   *
   * Each layer outputs to the TEMP called "output", and reads from
   * output if it needs to refer to GL_PREVIOUS. (we detect if we are
   * layer0 so we will read fragment.color for GL_PREVIOUS in that
   * case)
   *
   * We aim to do all the channels together if the same function is
   * used for RGB as for A.
   *
   * We aim to avoid string duplication / allocations during codegen.
   *
   * We are careful to only saturate when writing to output.
   */

  if (!shader_state->source)
    return TRUE;

  if (!_cogl_pipeline_layer_needs_combine_separate (combine_authority))
    {
      append_masked_combine (pipeline,
                             layer,
                             COGL_BLEND_STRING_CHANNEL_MASK_RGBA,
                             big_state->texture_combine_rgb_func,
                             big_state->texture_combine_rgb_src,
                             big_state->texture_combine_rgb_op);
    }
  else if (big_state->texture_combine_rgb_func ==
           COGL_PIPELINE_COMBINE_FUNC_DOT3_RGBA)
    {
      /* GL_DOT3_RGBA Is a bit weird as a GL_COMBINE_RGB function
       * since if you use it, it overrides your ALPHA function...
       */
      append_masked_combine (pipeline,
                             layer,
                             COGL_BLEND_STRING_CHANNEL_MASK_RGBA,
                             big_state->texture_combine_rgb_func,
                             big_state->texture_combine_rgb_src,
                             big_state->texture_combine_rgb_op);
    }
  else
    {
      append_masked_combine (pipeline,
                             layer,
                             COGL_BLEND_STRING_CHANNEL_MASK_RGB,
                             big_state->texture_combine_rgb_func,
                             big_state->texture_combine_rgb_src,
                             big_state->texture_combine_rgb_op);
      append_masked_combine (pipeline,
                             layer,
                             COGL_BLEND_STRING_CHANNEL_MASK_ALPHA,
                             big_state->texture_combine_alpha_func,
                             big_state->texture_combine_alpha_src,
                             big_state->texture_combine_alpha_op);
    }

  return TRUE;
}

static CoglBool
_cogl_pipeline_fragend_arbfp_passthrough (CoglPipeline *pipeline)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);

  if (!shader_state->source)
    return TRUE;

  g_string_append (shader_state->source,
                   "MOV output, fragment.color.primary;\n");
  return TRUE;
}

typedef struct _UpdateConstantsState
{
  int unit;
  CoglBool update_all;
  CoglPipelineShaderState *shader_state;
} UpdateConstantsState;

static CoglBool
update_constants_cb (CoglPipeline *pipeline,
                     int layer_index,
                     void *user_data)
{
  UpdateConstantsState *state = user_data;
  CoglPipelineShaderState *shader_state = state->shader_state;
  UnitState *unit_state = &shader_state->unit_state[state->unit++];

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (unit_state->has_combine_constant &&
      (state->update_all || unit_state->dirty_combine_constant))
    {
      float constant[4];
      _cogl_pipeline_get_layer_combine_constant (pipeline,
                                                 layer_index,
                                                 constant);
      GE (ctx, glProgramLocalParameter4fv (GL_FRAGMENT_PROGRAM_ARB,
                                           unit_state->constant_id,
                                           constant));
      unit_state->dirty_combine_constant = FALSE;
    }
  return TRUE;
}

static CoglBool
_cogl_pipeline_fragend_arbfp_end (CoglPipeline *pipeline,
                                  unsigned long pipelines_difference)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);
  GLuint gl_program;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (shader_state->source)
    {
      GLenum gl_error;
      COGL_STATIC_COUNTER (fragend_arbfp_compile_counter,
                           "arbfp compile counter",
                           "Increments each time a new ARBfp "
                           "program is compiled",
                           0 /* no application private data */);

      COGL_COUNTER_INC (_cogl_uprof_context, fragend_arbfp_compile_counter);

      g_string_append (shader_state->source,
                       "MOV result.color,output;\n");
      g_string_append (shader_state->source, "END\n");

      if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_SHOW_SOURCE)))
        g_message ("pipeline program:\n%s", shader_state->source->str);

      GE (ctx, glGenPrograms (1, &shader_state->gl_program));

      GE (ctx, glBindProgram (GL_FRAGMENT_PROGRAM_ARB,
                              shader_state->gl_program));

      while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
        ;
      ctx->glProgramString (GL_FRAGMENT_PROGRAM_ARB,
                            GL_PROGRAM_FORMAT_ASCII_ARB,
                            shader_state->source->len,
                            shader_state->source->str);
      if (ctx->glGetError () != GL_NO_ERROR)
        {
          g_warning ("\n%s\n%s",
                     shader_state->source->str,
                     ctx->glGetString (GL_PROGRAM_ERROR_STRING_ARB));
        }

      shader_state->source = NULL;
    }

  if (shader_state->user_program != COGL_INVALID_HANDLE)
    {
      /* An arbfp program should contain exactly one shader which we
         can use directly */
      CoglProgram *program = shader_state->user_program;
      CoglShader *shader = program->attached_shaders->data;

      gl_program = shader->gl_handle;
    }
  else
    gl_program = shader_state->gl_program;

  GE (ctx, glBindProgram (GL_FRAGMENT_PROGRAM_ARB, gl_program));
  _cogl_use_fragment_program (0, COGL_PIPELINE_PROGRAM_TYPE_ARBFP);

  if (shader_state->user_program == COGL_INVALID_HANDLE)
    {
      UpdateConstantsState state;
      state.unit = 0;
      state.shader_state = shader_state;
      /* If this arbfp program was last used with a different pipeline
       * then we need to ensure we update all program.local params */
      state.update_all =
        pipeline != shader_state->last_used_for_pipeline;
      cogl_pipeline_foreach_layer (pipeline,
                                   update_constants_cb,
                                   &state);
    }
  else
    {
      CoglProgram *program = shader_state->user_program;
      CoglBool program_changed;

      /* If the shader has changed since it was last flushed then we
         need to update all uniforms */
      program_changed = program->age != shader_state->user_program_age;

      _cogl_program_flush_uniforms (program, gl_program, program_changed);

      shader_state->user_program_age = program->age;
    }

  /* We need to track what pipeline used this arbfp program last since
   * we will need to update program.local params when switching
   * between different pipelines. */
  shader_state->last_used_for_pipeline = pipeline;

  return TRUE;
}

static void
_cogl_pipeline_fragend_arbfp_pipeline_pre_change_notify (
                                                   CoglPipeline *pipeline,
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
_cogl_pipeline_fragend_arbfp_layer_pre_change_notify (
                                                CoglPipeline *owner,
                                                CoglPipelineLayer *layer,
                                                CoglPipelineLayerState change)
{
  CoglPipelineShaderState *shader_state = get_shader_state (owner);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!shader_state)
    return;

  if ((change & _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx)))
    {
      dirty_shader_state (owner);
      return;
    }

  if (change & COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT)
    {
      int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
      shader_state->unit_state[unit_index].dirty_combine_constant = TRUE;
    }

  /* TODO: we could be saving snippets of texture combine code along
   * with each layer and then when a layer changes we would just free
   * the snippet. */
  return;
}

const CoglPipelineFragend _cogl_pipeline_arbfp_fragend =
{
  _cogl_pipeline_fragend_arbfp_start,
  _cogl_pipeline_fragend_arbfp_add_layer,
  _cogl_pipeline_fragend_arbfp_passthrough,
  _cogl_pipeline_fragend_arbfp_end,
  _cogl_pipeline_fragend_arbfp_pipeline_pre_change_notify,
  NULL,
  _cogl_pipeline_fragend_arbfp_layer_pre_change_notify
};

#endif /* COGL_PIPELINE_FRAGEND_ARBFP */

