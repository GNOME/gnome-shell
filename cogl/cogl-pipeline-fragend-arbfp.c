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
#include "cogl-pipeline-private.h"

#ifdef COGL_PIPELINE_FRAGEND_ARBFP

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl-handle.h"

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

typedef struct _UnitState
{
  int constant_id; /* The program.local[] index */
  unsigned int dirty_combine_constant:1;

  unsigned int sampled:1;
} UnitState;

typedef struct _ArbfpProgramState
{
  int ref_count;

  /* XXX: only valid during codegen */
  CoglPipeline *arbfp_authority;

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
} ArbfpProgramState;

typedef struct _CoglPipelineFragendARBfpPrivate
{
  ArbfpProgramState *arbfp_program_state;
} CoglPipelineFragendARBfpPrivate;

const CoglPipelineFragend _cogl_pipeline_arbfp_fragend;


static ArbfpProgramState *
arbfp_program_state_new (int n_layers)
{
  ArbfpProgramState *state = g_slice_new0 (ArbfpProgramState);
  state->ref_count = 1;
  state->unit_state = g_new0 (UnitState, n_layers);
  return state;
}

static ArbfpProgramState *
arbfp_program_state_ref (ArbfpProgramState *state)
{
  state->ref_count++;
  return state;
}

void
arbfp_program_state_unref (ArbfpProgramState *state)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  g_return_if_fail (state->ref_count > 0);

  state->ref_count--;
  if (state->ref_count == 0)
    {
      if (state->gl_program)
        {
          GE (ctx, glDeletePrograms (1, &state->gl_program));
          state->gl_program = 0;
        }

      g_free (state->unit_state);

      g_slice_free (ArbfpProgramState, state);
    }
}

static CoglPipelineFragendARBfpPrivate *
get_arbfp_priv (CoglPipeline *pipeline)
{
  if (!(pipeline->fragend_priv_set_mask & COGL_PIPELINE_FRAGEND_ARBFP_MASK))
    return NULL;

  return pipeline->fragend_privs[COGL_PIPELINE_FRAGEND_ARBFP];
}

static void
set_arbfp_priv (CoglPipeline *pipeline, CoglPipelineFragendARBfpPrivate *priv)
{
  if (priv)
    {
      pipeline->fragend_privs[COGL_PIPELINE_FRAGEND_ARBFP] = priv;
      pipeline->fragend_priv_set_mask |= COGL_PIPELINE_FRAGEND_ARBFP_MASK;
    }
  else
    pipeline->fragend_priv_set_mask &= ~COGL_PIPELINE_FRAGEND_ARBFP_MASK;
}

static ArbfpProgramState *
get_arbfp_program_state (CoglPipeline *pipeline)
{
  CoglPipelineFragendARBfpPrivate *priv = get_arbfp_priv (pipeline);
  if (!priv)
    return NULL;
  return priv->arbfp_program_state;
}

static gboolean
_cogl_pipeline_fragend_arbfp_start (CoglPipeline *pipeline,
                                    int n_layers,
                                    unsigned long pipelines_difference,
                                    int n_tex_coord_attribs)
{
  CoglPipelineFragendARBfpPrivate *priv;
  CoglPipeline *authority;
  CoglPipelineFragendARBfpPrivate *authority_priv;
  ArbfpProgramState *arbfp_program_state;
  CoglHandle user_program;

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* First validate that we can handle the current state using ARBfp
   */

  if (!cogl_features_available (COGL_FEATURE_SHADERS_ARBFP))
    return FALSE;

  /* TODO: support fog */
  if (ctx->legacy_fog_state.enabled)
    return FALSE;

  user_program = cogl_pipeline_get_user_program (pipeline);
  if (user_program != COGL_INVALID_HANDLE)
    {
      /* If the program doesn't have a fragment shader then some other
         vertend will handle the vertex shader state and we still need
         to generate a fragment program */
      if (!_cogl_program_has_fragment_shader (user_program))
        user_program = COGL_INVALID_HANDLE;
      /* If the user program does have a fragment shader then we can
         only handle it if it's in ARBfp */
      else if (_cogl_program_get_language (user_program) !=
               COGL_SHADER_LANGUAGE_ARBFP)
        return FALSE;
    }

  /* Now lookup our ARBfp backend private state (allocating if
   * necessary) */
  priv = get_arbfp_priv (pipeline);
  if (!priv)
    {
      priv = g_slice_new0 (CoglPipelineFragendARBfpPrivate);
      set_arbfp_priv (pipeline, priv);
    }

  /* If we have a valid arbfp_program_state pointer then we are all
   * set and don't need to generate a new program. */
  if (priv->arbfp_program_state)
    return TRUE;

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
  authority_priv = get_arbfp_priv (authority);
  if (authority_priv &&
      authority_priv->arbfp_program_state)
    {
      /* If we are going to share our program state with an arbfp-authority
       * then steal a reference to the program state associated with that
       * arbfp-authority... */
      priv->arbfp_program_state =
        arbfp_program_state_ref (authority_priv->arbfp_program_state);
      return TRUE;
    }

  if (!authority_priv)
    {
      authority_priv = g_slice_new0 (CoglPipelineFragendARBfpPrivate);
      set_arbfp_priv (authority, authority_priv);
    }

  /* If we haven't yet found an existing program then before we resort to
   * generating a new arbfp program we see if we can find a suitable
   * program in the arbfp_cache. */
  if (G_LIKELY (!(COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_PROGRAM_CACHES))))
    {
      arbfp_program_state = g_hash_table_lookup (ctx->arbfp_cache, authority);
      if (arbfp_program_state)
        {
          priv->arbfp_program_state =
            arbfp_program_state_ref (arbfp_program_state);

          /* Since we have already resolved the arbfp-authority at this point
           * we might as well also associate any program we find from the cache
           * with the authority too... */
          if (authority_priv != priv)
            authority_priv->arbfp_program_state =
              arbfp_program_state_ref (arbfp_program_state);
          return TRUE;
        }
    }

  /* If we still haven't found an existing program then start
   * generating code for a new program...
   */

  arbfp_program_state = arbfp_program_state_new (n_layers);

  priv->arbfp_program_state = arbfp_program_state_ref (arbfp_program_state);

  /* Since we have already resolved the arbfp-authority at this point we might
   * as well also associate any program we generate with the authority too...
   */
  if (authority_priv != priv)
    authority_priv->arbfp_program_state =
      arbfp_program_state_ref (arbfp_program_state);

  arbfp_program_state->user_program = user_program;
  if (user_program == COGL_INVALID_HANDLE)
    {
      int i;

      /* We reuse a single grow-only GString for code-gen */
      g_string_set_size (ctx->codegen_source_buffer, 0);
      arbfp_program_state->source = ctx->codegen_source_buffer;
      g_string_append (arbfp_program_state->source,
                       "!!ARBfp1.0\n"
                       "TEMP output;\n"
                       "TEMP tmp0, tmp1, tmp2, tmp3, tmp4;\n"
                       "PARAM half = {.5, .5, .5, .5};\n"
                       "PARAM one = {1, 1, 1, 1};\n"
                       "PARAM two = {2, 2, 2, 2};\n"
                       "PARAM minus_one = {-1, -1, -1, -1};\n");

      /* At the end of code-gen we'll add the program to a cache and
       * we'll use the authority pipeline as the basis for key into
       * that cache... */
      arbfp_program_state->arbfp_authority = authority;

      for (i = 0; i < n_layers; i++)
        {
          arbfp_program_state->unit_state[i].sampled = FALSE;
          arbfp_program_state->unit_state[i].dirty_combine_constant = FALSE;
        }
      arbfp_program_state->next_constant_id = 0;
    }

  return TRUE;
}

unsigned int
_cogl_pipeline_fragend_arbfp_hash (const void *data)
{
  unsigned int fragment_state;
  unsigned int layer_fragment_state;

  _COGL_GET_CONTEXT (ctx, 0);

  fragment_state =
    _cogl_pipeline_get_state_for_fragment_codegen (ctx);
  layer_fragment_state =
    _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx);

  return _cogl_pipeline_hash ((CoglPipeline *)data,

                              fragment_state, layer_fragment_state,
                              0);
}

gboolean
_cogl_pipeline_fragend_arbfp_equal (const void *a, const void *b)
{
  unsigned int fragment_state;
  unsigned int layer_fragment_state;

  _COGL_GET_CONTEXT (ctx, 0);

  fragment_state =
    _cogl_pipeline_get_state_for_fragment_codegen (ctx);
  layer_fragment_state =
    _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx);

  return _cogl_pipeline_equal ((CoglPipeline *)a, (CoglPipeline *)b,
                               fragment_state, layer_fragment_state,
                               0);
}

static const char *
gl_target_to_arbfp_string (GLenum gl_target)
{
  if (gl_target == GL_TEXTURE_1D)
    return "1D";
  else if (gl_target == GL_TEXTURE_2D)
    return "2D";
#ifdef GL_ARB_texture_rectangle
  else if (gl_target == GL_TEXTURE_RECTANGLE_ARB)
    return "RECT";
#endif
  else if (gl_target == GL_TEXTURE_3D)
    return "3D";
  else
    return "2D";
}

static void
setup_texture_source (ArbfpProgramState *arbfp_program_state,
                      int unit_index,
                      GLenum gl_target)
{
  if (!arbfp_program_state->unit_state[unit_index].sampled)
    {
      if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_TEXTURING)))
        g_string_append_printf (arbfp_program_state->source,
                                "TEMP texel%d;\n"
                                "MOV texel%d, one;\n",
                                unit_index,
                                unit_index);
      else
        g_string_append_printf (arbfp_program_state->source,
                                "TEMP texel%d;\n"
                                "TEX texel%d,fragment.texcoord[%d],"
                                "texture[%d],%s;\n",
                                unit_index,
                                unit_index,
                                unit_index,
                                unit_index,
                                gl_target_to_arbfp_string (gl_target));
      arbfp_program_state->unit_state[unit_index].sampled = TRUE;
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
  GLenum texture_target;

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
           GLint src,
           GLint op,
           CoglPipelineFragendARBfpArg *arg)
{
  ArbfpProgramState *arbfp_program_state = get_arbfp_program_state (pipeline);
  static const char *tmp_name[3] = { "tmp0", "tmp1", "tmp2" };
  GLenum gl_target;
  CoglHandle texture;

  switch (src)
    {
    case COGL_PIPELINE_COMBINE_SOURCE_TEXTURE:
      arg->type = COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_TEXTURE;
      arg->name = "texel%d";
      arg->texture_unit = _cogl_pipeline_layer_get_unit_index (layer);
      texture = _cogl_pipeline_layer_get_texture (layer);
      cogl_texture_get_gl_texture (texture, NULL, &gl_target);
      setup_texture_source (arbfp_program_state, arg->texture_unit, gl_target);
      break;
    case COGL_PIPELINE_COMBINE_SOURCE_CONSTANT:
      {
        int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
        UnitState *unit_state = &arbfp_program_state->unit_state[unit_index];

        unit_state->constant_id = arbfp_program_state->next_constant_id++;
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
    default: /* GL_TEXTURE0..N */
      arg->type = COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_TEXTURE;
      arg->name = "texture[%d]";
      arg->texture_unit = src - GL_TEXTURE0;
      texture = _cogl_pipeline_layer_get_texture (layer);
      cogl_texture_get_gl_texture (texture, NULL, &gl_target);
      setup_texture_source (arbfp_program_state, arg->texture_unit, gl_target);
    }

  arg->swizzle = "";

  switch (op)
    {
    case COGL_PIPELINE_COMBINE_OP_SRC_COLOR:
      break;
    case COGL_PIPELINE_COMBINE_OP_ONE_MINUS_SRC_COLOR:
      g_string_append_printf (arbfp_program_state->source,
                              "SUB tmp%d, one, ",
                              arg_index);
      append_arg (arbfp_program_state->source, arg);
      g_string_append_printf (arbfp_program_state->source, ";\n");
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
      g_string_append_printf (arbfp_program_state->source,
                              "SUB tmp%d, one, ",
                              arg_index);
      append_arg (arbfp_program_state->source, arg);
      /* avoid a swizzle if we know RGB are going to be masked
       * in the end anyway */
      if (mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        g_string_append_printf (arbfp_program_state->source, ".a;\n");
      else
        g_string_append_printf (arbfp_program_state->source, ";\n");
      arg->type = COGL_PIPELINE_FRAGEND_ARBFP_ARG_TYPE_SIMPLE;
      arg->name = tmp_name[arg_index];
      break;
    default:
      g_error ("Unknown texture combine operator %d", op);
      break;
    }
}

static gboolean
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
  ArbfpProgramState *arbfp_program_state = get_arbfp_program_state (pipeline);
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
      g_string_append_printf (arbfp_program_state->source,
                              "ADD_SAT output%s, ",
                              mask_name);
      break;
    case COGL_PIPELINE_COMBINE_FUNC_MODULATE:
      /* Note: no need to saturate since we can assume operands
       * have values in the range [0,1] */
      g_string_append_printf (arbfp_program_state->source, "MUL output%s, ",
                              mask_name);
      break;
    case COGL_PIPELINE_COMBINE_FUNC_REPLACE:
      /* Note: no need to saturate since we can assume operand
       * has a value in the range [0,1] */
      g_string_append_printf (arbfp_program_state->source, "MOV output%s, ",
                              mask_name);
      break;
    case COGL_PIPELINE_COMBINE_FUNC_SUBTRACT:
      g_string_append_printf (arbfp_program_state->source,
                              "SUB_SAT output%s, ",
                              mask_name);
      break;
    case COGL_PIPELINE_COMBINE_FUNC_ADD_SIGNED:
      g_string_append_printf (arbfp_program_state->source, "ADD tmp3%s, ",
                              mask_name);
      append_arg (arbfp_program_state->source, &args[0]);
      g_string_append (arbfp_program_state->source, ", ");
      append_arg (arbfp_program_state->source, &args[1]);
      g_string_append (arbfp_program_state->source, ";\n");
      g_string_append_printf (arbfp_program_state->source,
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

        g_string_append (arbfp_program_state->source, "MAD tmp3, two, ");
        append_arg (arbfp_program_state->source, &args[0]);
        g_string_append (arbfp_program_state->source, ", minus_one;\n");

        if (!fragend_arbfp_args_equal (&args[0], &args[1]))
          {
            g_string_append (arbfp_program_state->source, "MAD tmp4, two, ");
            append_arg (arbfp_program_state->source, &args[1]);
            g_string_append (arbfp_program_state->source, ", minus_one;\n");
          }
        else
          tmp4 = "tmp3";

        g_string_append_printf (arbfp_program_state->source,
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
      g_string_append_printf (arbfp_program_state->source, "LRP output%s, ",
                              mask_name);
      append_arg (arbfp_program_state->source, &args[2]);
      g_string_append (arbfp_program_state->source, ", ");
      append_arg (arbfp_program_state->source, &args[0]);
      g_string_append (arbfp_program_state->source, ", ");
      append_arg (arbfp_program_state->source, &args[1]);
      n_args = 0;
      break;
    default:
      g_error ("Unknown texture combine function %d", function);
      g_string_append_printf (arbfp_program_state->source, "MUL_SAT output%s, ",
                              mask_name);
      n_args = 2;
      break;
    }

  if (n_args > 0)
    append_arg (arbfp_program_state->source, &args[0]);
  if (n_args > 1)
    {
      g_string_append (arbfp_program_state->source, ", ");
      append_arg (arbfp_program_state->source, &args[1]);
    }
  g_string_append (arbfp_program_state->source, ";\n");
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

static gboolean
_cogl_pipeline_fragend_arbfp_add_layer (CoglPipeline *pipeline,
                                        CoglPipelineLayer *layer,
                                        unsigned long layers_difference)
{
  ArbfpProgramState *arbfp_program_state = get_arbfp_program_state (pipeline);
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

  if (!arbfp_program_state->source)
    return TRUE;

  if (!_cogl_pipeline_need_texture_combine_separate (combine_authority))
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

gboolean
_cogl_pipeline_fragend_arbfp_passthrough (CoglPipeline *pipeline)
{
  ArbfpProgramState *arbfp_program_state = get_arbfp_program_state (pipeline);

  if (!arbfp_program_state->source)
    return TRUE;

  g_string_append (arbfp_program_state->source,
                   "MOV output, fragment.color.primary;\n");
  return TRUE;
}

typedef struct _UpdateConstantsState
{
  int unit;
  gboolean update_all;
  ArbfpProgramState *arbfp_program_state;
} UpdateConstantsState;

static gboolean
update_constants_cb (CoglPipeline *pipeline,
                     int layer_index,
                     void *user_data)
{
  UpdateConstantsState *state = user_data;
  ArbfpProgramState *arbfp_program_state = state->arbfp_program_state;
  UnitState *unit_state = &arbfp_program_state->unit_state[state->unit++];

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (state->update_all || unit_state->dirty_combine_constant)
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

static gboolean
_cogl_pipeline_fragend_arbfp_end (CoglPipeline *pipeline,
                                  unsigned long pipelines_difference)
{
  ArbfpProgramState *arbfp_program_state = get_arbfp_program_state (pipeline);
  GLuint gl_program;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (arbfp_program_state->source)
    {
      GLenum gl_error;
      COGL_STATIC_COUNTER (fragend_arbfp_compile_counter,
                           "arbfp compile counter",
                           "Increments each time a new ARBfp "
                           "program is compiled",
                           0 /* no application private data */);

      COGL_COUNTER_INC (_cogl_uprof_context, fragend_arbfp_compile_counter);

      g_string_append (arbfp_program_state->source,
                       "MOV result.color,output;\n");
      g_string_append (arbfp_program_state->source, "END\n");

      if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_SHOW_SOURCE)))
        g_message ("pipeline program:\n%s", arbfp_program_state->source->str);

      GE (ctx, glGenPrograms (1, &arbfp_program_state->gl_program));

      GE (ctx, glBindProgram (GL_FRAGMENT_PROGRAM_ARB,
                         arbfp_program_state->gl_program));

      while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
        ;
      ctx->glProgramString (GL_FRAGMENT_PROGRAM_ARB,
                            GL_PROGRAM_FORMAT_ASCII_ARB,
                            arbfp_program_state->source->len,
                            arbfp_program_state->source->str);
      if (ctx->glGetError () != GL_NO_ERROR)
        {
          g_warning ("\n%s\n%s",
                     arbfp_program_state->source->str,
                     ctx->glGetString (GL_PROGRAM_ERROR_STRING_ARB));
        }

      arbfp_program_state->source = NULL;

      if (G_LIKELY (!(COGL_DEBUG_ENABLED (COGL_DEBUG_DISABLE_PROGRAM_CACHES))))
        {
          CoglPipeline *key;

          /* XXX: I wish there was a way to insert into a GHashTable
           * with a pre-calculated hash value since there is a cost to
           * calculating the hash of a CoglPipeline and in this case
           * we know we have already called _cogl_pipeline_hash during
           * _cogl_pipeline_fragend_arbfp_backend_start so we could pass the
           * value through to here to avoid hashing it again.
           */

          /* XXX: Any keys referenced by the hash table need to remain
           * valid all the while that there are corresponding values,
           * so for now we simply make a copy of the current authority
           * pipeline.
           *
           * FIXME: A problem with this is that our key into the cache
           * may hold references to some arbitrary user textures which
           * will now be kept alive indefinitly which is a shame. A
           * better solution will be to derive a special "key
           * pipeline" from the authority which derives from the base
           * Cogl pipeline (to avoid affecting the lifetime of any
           * other pipelines) and only takes a copy of the state that
           * relates to the arbfp program and references small dummy
           * textures instead of potentially large user textures. */
          key = cogl_pipeline_copy (arbfp_program_state->arbfp_authority);
          arbfp_program_state_ref (arbfp_program_state);
          g_hash_table_insert (ctx->arbfp_cache, key, arbfp_program_state);
          if (G_UNLIKELY (g_hash_table_size (ctx->arbfp_cache) > 50))
            {
              static gboolean seen = FALSE;
              if (!seen)
                g_warning ("Over 50 separate ARBfp programs have been "
                           "generated which is very unusual, so something "
                           "is probably wrong!\n");
              seen = TRUE;
            }
        }

      /* The authority is only valid during codegen since the program
       * state may have a longer lifetime than the original authority
       * it is created for. */
      arbfp_program_state->arbfp_authority = NULL;
    }

  if (arbfp_program_state->user_program != COGL_INVALID_HANDLE)
    {
      /* An arbfp program should contain exactly one shader which we
         can use directly */
      CoglProgram *program = arbfp_program_state->user_program;
      CoglShader *shader = program->attached_shaders->data;

      gl_program = shader->gl_handle;
    }
  else
    gl_program = arbfp_program_state->gl_program;

  GE (ctx, glBindProgram (GL_FRAGMENT_PROGRAM_ARB, gl_program));
  _cogl_use_fragment_program (0, COGL_PIPELINE_PROGRAM_TYPE_ARBFP);

  if (arbfp_program_state->user_program == COGL_INVALID_HANDLE)
    {
      UpdateConstantsState state;
      state.unit = 0;
      state.arbfp_program_state = arbfp_program_state;
      /* If this arbfp program was last used with a different pipeline
       * then we need to ensure we update all program.local params */
      state.update_all =
        pipeline != arbfp_program_state->last_used_for_pipeline;
      cogl_pipeline_foreach_layer (pipeline,
                                   update_constants_cb,
                                   &state);
    }
  else
    {
      CoglProgram *program = arbfp_program_state->user_program;
      gboolean program_changed;

      /* If the shader has changed since it was last flushed then we
         need to update all uniforms */
      program_changed = program->age != arbfp_program_state->user_program_age;

      _cogl_program_flush_uniforms (program, gl_program, program_changed);

      arbfp_program_state->user_program_age = program->age;
    }

  /* We need to track what pipeline used this arbfp program last since
   * we will need to update program.local params when switching
   * between different pipelines. */
  arbfp_program_state->last_used_for_pipeline = pipeline;

  return TRUE;
}

static void
dirty_arbfp_program_state (CoglPipeline *pipeline)
{
  CoglPipelineFragendARBfpPrivate *priv;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  priv = get_arbfp_priv (pipeline);
  if (!priv)
    return;

  if (priv->arbfp_program_state)
    {
      arbfp_program_state_unref (priv->arbfp_program_state);
      priv->arbfp_program_state = NULL;
    }
}

static void
_cogl_pipeline_fragend_arbfp_pipeline_pre_change_notify (
                                                   CoglPipeline *pipeline,
                                                   CoglPipelineState change,
                                                   const CoglColor *new_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if ((change & _cogl_pipeline_get_state_for_fragment_codegen (ctx)))
    dirty_arbfp_program_state (pipeline);
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
  CoglPipelineFragendARBfpPrivate *priv = get_arbfp_priv (owner);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!priv)
    return;

  if ((change & _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx)))
    {
      dirty_arbfp_program_state (owner);
      return;
    }

  if (change & COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT)
    {
      ArbfpProgramState *arbfp_program_state = get_arbfp_program_state (owner);

      if (arbfp_program_state)
        {
          int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
          arbfp_program_state->unit_state[unit_index].dirty_combine_constant =
            TRUE;
        }
    }

  /* TODO: we could be saving snippets of texture combine code along
   * with each layer and then when a layer changes we would just free
   * the snippet. */
  return;
}

static void
_cogl_pipeline_fragend_arbfp_free_priv (CoglPipeline *pipeline)
{
  CoglPipelineFragendARBfpPrivate *priv = get_arbfp_priv (pipeline);
  if (priv)
    {
      if (priv->arbfp_program_state)
        arbfp_program_state_unref (priv->arbfp_program_state);
      g_slice_free (CoglPipelineFragendARBfpPrivate, priv);
      set_arbfp_priv (pipeline, NULL);
    }
}

const CoglPipelineFragend _cogl_pipeline_arbfp_fragend =
{
  _cogl_pipeline_fragend_arbfp_start,
  _cogl_pipeline_fragend_arbfp_add_layer,
  _cogl_pipeline_fragend_arbfp_passthrough,
  _cogl_pipeline_fragend_arbfp_end,
  _cogl_pipeline_fragend_arbfp_pipeline_pre_change_notify,
  NULL,
  _cogl_pipeline_fragend_arbfp_layer_pre_change_notify,
  _cogl_pipeline_fragend_arbfp_free_priv
};

#endif /* COGL_PIPELINE_FRAGEND_ARBFP */

