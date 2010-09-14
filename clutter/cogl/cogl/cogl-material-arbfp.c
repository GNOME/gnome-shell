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
#include "cogl-material-private.h"

#ifdef COGL_MATERIAL_BACKEND_ARBFP

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context.h"
#include "cogl-handle.h"

#include "cogl-texture-private.h"
#include "cogl-blend-string.h"
#include "cogl-journal-private.h"
#include "cogl-color-private.h"
#include "cogl-profile.h"
#ifndef HAVE_COGL_GLES
#include "cogl-program.h"
#endif

#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>

/*
 * GL/GLES compatability defines for material thingies:
 */

#ifdef HAVE_COGL_GLES2
#include "../gles/cogl-gles2-wrapper.h"
#endif

#ifdef HAVE_COGL_GL
#define glProgramString ctx->drv.pf_glProgramString
#define glBindProgram ctx->drv.pf_glBindProgram
#define glDeletePrograms ctx->drv.pf_glDeletePrograms
#define glGenPrograms ctx->drv.pf_glGenPrograms
#define glProgramLocalParameter4fv ctx->drv.pf_glProgramLocalParameter4fv
#define glUseProgram ctx->drv.pf_glUseProgram
#endif

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

  CoglHandle user_program;
  GString *source;
  GLuint gl_program;
  UnitState *unit_state;
  int next_constant_id;

  /* We need to track the last material that an ARBfp program was used
   * with so know if we need to update any program.local parameters. */
  CoglMaterial *last_used_for_material;
} ArbfpProgramState;

typedef struct _CoglMaterialBackendARBfpPrivate
{
  ArbfpProgramState *arbfp_program_state;
} CoglMaterialBackendARBfpPrivate;

const CoglMaterialBackend _cogl_material_arbfp_backend;


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
          GE (glDeletePrograms (1, &state->gl_program));
          state->gl_program = 0;
        }

      g_free (state->unit_state);

      g_slice_free (ArbfpProgramState, state);
    }
}

static int
_cogl_material_backend_arbfp_get_max_texture_units (void)
{
  return _cogl_get_max_texture_image_units ();
}

typedef struct
{
  int i;
  CoglMaterialLayer **layers;
} AddLayersToArrayState;

static gboolean
add_layer_to_array_cb (CoglMaterialLayer *layer,
                       void *user_data)
{
  AddLayersToArrayState *state = user_data;
  state->layers[state->i++] = layer;
  return TRUE;
}

static gboolean
layers_arbfp_would_differ (CoglMaterialLayer **material0_layers,
                           CoglMaterialLayer **material1_layers,
                           int n_layers)
{
  int i;
  /* The layer state that affects arbfp codegen... */
  unsigned long arbfp_codegen_modifiers =
    COGL_MATERIAL_LAYER_STATE_COMBINE |
    COGL_MATERIAL_LAYER_STATE_UNIT;

  for (i = 0; i < n_layers; i++)
    {
      CoglMaterialLayer *layer0 = material0_layers[i];
      CoglMaterialLayer *layer1 = material1_layers[i];
      unsigned long layer_differences;

      if (layer0 == layer1)
        continue;

      layer_differences =
        _cogl_material_layer_compare_differences (layer0, layer1);

      if (layer_differences & arbfp_codegen_modifiers)
        {
          /* When it comes to texture differences the only thing that
           * affects the arbfp is the target enum... */
          if (layer_differences == COGL_MATERIAL_LAYER_STATE_TEXTURE)
            {
              CoglHandle tex0 = _cogl_material_layer_get_texture (layer0);
              CoglHandle tex1 = _cogl_material_layer_get_texture (layer1);
              GLenum gl_target0;
              GLenum gl_target1;

              cogl_texture_get_gl_texture (tex0, NULL, &gl_target0);
              cogl_texture_get_gl_texture (tex1, NULL, &gl_target1);
              if (gl_target0 == gl_target1)
                continue;
            }
          return TRUE;
        }
    }

  return FALSE;
}

/* This tries to find the oldest ancestor whos state would generate
 * the same arbfp program as the current material. This is a simple
 * mechanism for reducing the number of arbfp programs we have to
 * generate.
 */
static CoglMaterial *
find_arbfp_authority (CoglMaterial *material, CoglHandle user_program)
{
  CoglMaterial *authority0;
  CoglMaterial *authority1;
  int n_layers;
  CoglMaterialLayer **authority0_layers;
  CoglMaterialLayer **authority1_layers;

  /* XXX: we'll need to update this when we add fog support to the
   * arbfp codegen */

  if (user_program != COGL_INVALID_HANDLE)
    return material;

  /* Find the first material that modifies state that affects the
   * arbfp codegen... */
  authority0 = _cogl_material_get_authority (material,
                                             COGL_MATERIAL_STATE_LAYERS);

  /* Find the next ancestor after that, that also modifies state
   * affecting arbfp codegen... */
  if (_cogl_material_get_parent (authority0))
    {
      authority1 =
        _cogl_material_get_authority (_cogl_material_get_parent (authority0),
                                      COGL_MATERIAL_STATE_LAYERS);
    }
  else
    return authority0;

  n_layers = authority0->n_layers;

  for (;;)
    {
      AddLayersToArrayState state;

      if (authority0->n_layers != authority1->n_layers)
        return authority0;

      authority0_layers =
        g_alloca (sizeof (CoglMaterialLayer *) * n_layers);
      state.i = 0;
      state.layers = authority0_layers;
      _cogl_material_foreach_layer_internal (authority0,
                                             add_layer_to_array_cb,
                                             &state);

      authority1_layers =
        g_alloca (sizeof (CoglMaterialLayer *) * n_layers);
      state.i = 0;
      state.layers = authority1_layers;
      _cogl_material_foreach_layer_internal (authority1,
                                             add_layer_to_array_cb,
                                             &state);

      if (layers_arbfp_would_differ (authority0_layers, authority1_layers,
                                     n_layers))
        return authority0;

      /* Find the next ancestor after that, that also modifies state
       * affecting arbfp codegen... */

      if (!_cogl_material_get_parent (authority1))
        break;

      authority0 = authority1;
      authority1 =
        _cogl_material_get_authority (_cogl_material_get_parent (authority1),
                                      COGL_MATERIAL_STATE_LAYERS);
      if (authority1 == authority0)
        break;
    }

  return authority1;
}

static CoglMaterialBackendARBfpPrivate *
get_arbfp_priv (CoglMaterial *material)
{
  if (!(material->backend_priv_set_mask & COGL_MATERIAL_BACKEND_ARBFP_MASK))
    return NULL;

  return material->backend_privs[COGL_MATERIAL_BACKEND_ARBFP];
}

static void
set_arbfp_priv (CoglMaterial *material, CoglMaterialBackendARBfpPrivate *priv)
{
  if (priv)
    {
      material->backend_privs[COGL_MATERIAL_BACKEND_ARBFP] = priv;
      material->backend_priv_set_mask |= COGL_MATERIAL_BACKEND_ARBFP_MASK;
    }
  else
    material->backend_priv_set_mask &= ~COGL_MATERIAL_BACKEND_ARBFP_MASK;
}

static ArbfpProgramState *
get_arbfp_program_state (CoglMaterial *material)
{
  CoglMaterialBackendARBfpPrivate *priv = get_arbfp_priv (material);
  if (!priv)
    return NULL;
  return priv->arbfp_program_state;
}

static gboolean
_cogl_material_backend_arbfp_start (CoglMaterial *material,
                                    int n_layers,
                                    unsigned long materials_difference)
{
  CoglMaterialBackendARBfpPrivate *priv;
  CoglMaterial *authority;
  CoglMaterialBackendARBfpPrivate *authority_priv;
  CoglHandle user_program;

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* First validate that we can handle the current state using ARBfp
   */

  if (!cogl_features_available (COGL_FEATURE_SHADERS_ARBFP))
    return FALSE;

  /* TODO: support fog */
  if (ctx->legacy_fog_state.enabled)
    return FALSE;

  user_program = cogl_material_get_user_program (material);
  if (user_program != COGL_INVALID_HANDLE &&
      _cogl_program_get_language (user_program) != COGL_SHADER_LANGUAGE_ARBFP)
    return FALSE;

  /* Now lookup our ARBfp backend private state (allocating if
   * necessary) */
  priv = get_arbfp_priv (material);
  if (!priv)
    {
      priv = g_slice_new0 (CoglMaterialBackendARBfpPrivate);
      set_arbfp_priv (material, priv);
    }

  /* If we have a valid arbfp_program_state pointer then we are all
   * set and don't need to generate a new program. */
  if (priv->arbfp_program_state)
    return TRUE;

  /* If we don't have an associated arbfp program yet then find the
   * arbfp-authority (the oldest ancestor whose state will result in
   * the same program being generated as for this material).
   *
   * We always make sure to associate new programs with the
   * arbfp-authority to maximize the chance that other materials can
   * share it.
   */
  authority = find_arbfp_authority (material, user_program);
  authority_priv = get_arbfp_priv (authority);
  if (!authority_priv)
    {
      authority_priv = g_slice_new0 (CoglMaterialBackendARBfpPrivate);
      set_arbfp_priv (authority, authority_priv);
    }

  /* If we don't have an existing program associated with the
   * arbfp-authority then start generating code for a new program...
   */
  if (!authority_priv->arbfp_program_state)
    {
      ArbfpProgramState *arbfp_program_state =
        arbfp_program_state_new (n_layers);
      authority_priv->arbfp_program_state = arbfp_program_state;

      arbfp_program_state->user_program = user_program;
      if (user_program == COGL_INVALID_HANDLE)
        {
          int i;

          /* We reuse a single grow-only GString for ARBfp code-gen */
          g_string_set_size (ctx->arbfp_source_buffer, 0);
          arbfp_program_state->source = ctx->arbfp_source_buffer;
          g_string_append (arbfp_program_state->source,
                           "!!ARBfp1.0\n"
                           "TEMP output;\n"
                           "TEMP tmp0, tmp1, tmp2, tmp3, tmp4;\n"
                           "PARAM half = {.5, .5, .5, .5};\n"
                           "PARAM one = {1, 1, 1, 1};\n"
                           "PARAM two = {2, 2, 2, 2};\n"
                           "PARAM minus_one = {-1, -1, -1, -1};\n");

          for (i = 0; i < n_layers; i++)
            {
              arbfp_program_state->unit_state[i].sampled = FALSE;
              arbfp_program_state->unit_state[i].dirty_combine_constant = FALSE;
            }
          arbfp_program_state->next_constant_id = 0;
        }
    }

  /* Finally, if the material isn't actually its own arbfp-authority
   * then steal a reference to the program state associated with the
   * arbfp-authority... */
  if (authority != material)
    priv->arbfp_program_state =
      arbfp_program_state_ref (authority_priv->arbfp_program_state);

  return TRUE;
}

/* Determines if we need to handle the RGB and A texture combining
 * separately or is the same function used for both channel masks and
 * with the same arguments...
 */
static gboolean
need_texture_combine_separate (CoglMaterialLayer *combine_authority)
{
  CoglMaterialLayerBigState *big_state = combine_authority->big_state;
  int n_args;
  int i;

  if (big_state->texture_combine_rgb_func !=
      big_state->texture_combine_alpha_func)
    return TRUE;

  n_args = _cogl_get_n_args_for_combine_func (big_state->texture_combine_rgb_func);

  for (i = 0; i < n_args; i++)
    {
      if (big_state->texture_combine_rgb_src[i] !=
          big_state->texture_combine_alpha_src[i])
        return TRUE;

      /*
       * We can allow some variation of the source operands without
       * needing a separation...
       *
       * "A = REPLACE (CONSTANT[A])" + either of the following...
       * "RGB = REPLACE (CONSTANT[RGB])"
       * "RGB = REPLACE (CONSTANT[A])"
       *
       * can be combined as:
       * "RGBA = REPLACE (CONSTANT)" or
       * "RGBA = REPLACE (CONSTANT[A])" or
       *
       * And "A = REPLACE (1-CONSTANT[A])" + either of the following...
       * "RGB = REPLACE (1-CONSTANT)" or
       * "RGB = REPLACE (1-CONSTANT[A])"
       *
       * can be combined as:
       * "RGBA = REPLACE (1-CONSTANT)" or
       * "RGBA = REPLACE (1-CONSTANT[A])"
       */
      switch (big_state->texture_combine_alpha_op[i])
        {
        case GL_SRC_ALPHA:
          switch (big_state->texture_combine_rgb_op[i])
            {
            case GL_SRC_COLOR:
            case GL_SRC_ALPHA:
              break;
            default:
              return FALSE;
            }
          break;
        case GL_ONE_MINUS_SRC_ALPHA:
          switch (big_state->texture_combine_rgb_op[i])
            {
            case GL_ONE_MINUS_SRC_COLOR:
            case GL_ONE_MINUS_SRC_ALPHA:
              break;
            default:
              return FALSE;
            }
          break;
        default:
          return FALSE;	/* impossible */
        }
    }

   return FALSE;
}

static const char *
gl_target_to_arbfp_string (GLenum gl_target)
{
#ifndef HAVE_COGL_GLES2
  if (gl_target == GL_TEXTURE_1D)
    return "1D";
  else
#endif
    if (gl_target == GL_TEXTURE_2D)
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

typedef enum _CoglMaterialBackendARBfpArgType
{
  COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_SIMPLE,
  COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_CONSTANT,
  COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_TEXTURE
} CoglMaterialBackendARBfpArgType;

typedef struct _CoglMaterialBackendARBfpArg
{
  const char *name;

  CoglMaterialBackendARBfpArgType type;

  /* for type = TEXTURE */
  int texture_unit;
  GLenum texture_target;

  /* for type = CONSTANT */
  int constant_id;

  const char *swizzle;

} CoglMaterialBackendARBfpArg;

static void
append_arg (GString *source, const CoglMaterialBackendARBfpArg *arg)
{
  switch (arg->type)
    {
    case COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_TEXTURE:
      g_string_append_printf (source, "texel%d%s",
                              arg->texture_unit, arg->swizzle);
      break;
    case COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_CONSTANT:
      g_string_append_printf (source, "program.local[%d]%s",
                              arg->constant_id, arg->swizzle);
      break;
    case COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_SIMPLE:
      g_string_append_printf (source, "%s%s",
                              arg->name, arg->swizzle);
      break;
    }
}

/* Note: we are trying to avoid duplicating strings during codegen
 * which is why we have the slightly awkward
 * CoglMaterialBackendARBfpArg mechanism. */
static void
setup_arg (CoglMaterial *material,
           CoglMaterialLayer *layer,
           CoglBlendStringChannelMask mask,
           int arg_index,
           GLint src,
           GLint op,
           CoglMaterialBackendARBfpArg *arg)
{
  ArbfpProgramState *arbfp_program_state = get_arbfp_program_state (material);
  static const char *tmp_name[3] = { "tmp0", "tmp1", "tmp2" };
  GLenum gl_target;
  CoglHandle texture;

  switch (src)
    {
    case GL_TEXTURE:
      arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_TEXTURE;
      arg->name = "texel%d";
      arg->texture_unit = _cogl_material_layer_get_unit_index (layer);
      texture = _cogl_material_layer_get_texture (layer);
      cogl_texture_get_gl_texture (texture, NULL, &gl_target);
      setup_texture_source (arbfp_program_state, arg->texture_unit, gl_target);
      break;
    case GL_CONSTANT:
      {
        int unit_index = _cogl_material_layer_get_unit_index (layer);
        UnitState *unit_state = &arbfp_program_state->unit_state[unit_index];

        unit_state->constant_id = arbfp_program_state->next_constant_id++;
        unit_state->dirty_combine_constant = TRUE;

        arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_CONSTANT;
        arg->name = "program.local[%d]";
        arg->constant_id = unit_state->constant_id;
        break;
      }
    case GL_PRIMARY_COLOR:
      arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_SIMPLE;
      arg->name = "fragment.color.primary";
      break;
    case GL_PREVIOUS:
      arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_SIMPLE;
      if (_cogl_material_layer_get_unit_index (layer) == 0)
        arg->name = "fragment.color.primary";
      else
        arg->name = "output";
      break;
    default: /* GL_TEXTURE0..N */
      arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_TEXTURE;
      arg->name = "texture[%d]";
      arg->texture_unit = src - GL_TEXTURE0;
      texture = _cogl_material_layer_get_texture (layer);
      cogl_texture_get_gl_texture (texture, NULL, &gl_target);
      setup_texture_source (arbfp_program_state, arg->texture_unit, gl_target);
    }

  arg->swizzle = "";

  switch (op)
    {
    case GL_SRC_COLOR:
      break;
    case GL_ONE_MINUS_SRC_COLOR:
      g_string_append_printf (arbfp_program_state->source,
                              "SUB tmp%d, one, ",
                              arg_index);
      append_arg (arbfp_program_state->source, arg);
      g_string_append_printf (arbfp_program_state->source, ";\n");
      arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_SIMPLE;
      arg->name = tmp_name[arg_index];
      arg->swizzle = "";
      break;
    case GL_SRC_ALPHA:
      /* avoid a swizzle if we know RGB are going to be masked
       * in the end anyway */
      if (mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        arg->swizzle = ".a";
      break;
    case GL_ONE_MINUS_SRC_ALPHA:
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
      arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_SIMPLE;
      arg->name = tmp_name[arg_index];
      break;
    default:
      g_error ("Unknown texture combine operator %d", op);
      break;
    }
}

static gboolean
backend_arbfp_args_equal (CoglMaterialBackendARBfpArg *arg0,
                          CoglMaterialBackendARBfpArg *arg1)
{
  if (arg0->type != arg1->type)
    return FALSE;

  if (arg0->name != arg1->name &&
      strcmp (arg0->name, arg1->name) != 0)
    return FALSE;

  if (arg0->type == COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_TEXTURE &&
      arg0->texture_unit != arg1->texture_unit)
    return FALSE;
  /* Note we don't have to check the target; a texture unit can only
   * have one target enabled at a time. */

  if (arg0->type == COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_CONSTANT &&
      arg0->constant_id != arg0->constant_id)
    return FALSE;

  if (arg0->swizzle != arg1->swizzle &&
      strcmp (arg0->swizzle, arg1->swizzle) != 0)
    return FALSE;

  return TRUE;
}

static void
append_function (CoglMaterial *material,
                 CoglBlendStringChannelMask mask,
                 GLint function,
                 CoglMaterialBackendARBfpArg *args,
                 int n_args)
{
  ArbfpProgramState *arbfp_program_state = get_arbfp_program_state (material);
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
    case GL_ADD:
      g_string_append_printf (arbfp_program_state->source,
                              "ADD_SAT output%s, ",
                              mask_name);
      break;
    case GL_MODULATE:
      /* Note: no need to saturate since we can assume operands
       * have values in the range [0,1] */
      g_string_append_printf (arbfp_program_state->source, "MUL output%s, ",
                              mask_name);
      break;
    case GL_REPLACE:
      /* Note: no need to saturate since we can assume operand
       * has a value in the range [0,1] */
      g_string_append_printf (arbfp_program_state->source, "MOV output%s, ",
                              mask_name);
      break;
    case GL_SUBTRACT:
      g_string_append_printf (arbfp_program_state->source,
                              "SUB_SAT output%s, ",
                              mask_name);
      break;
    case GL_ADD_SIGNED:
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
    case GL_DOT3_RGB:
    /* These functions are the same except that GL_DOT3_RGB never
     * updates the alpha channel.
     *
     * NB: GL_DOT3_RGBA is a bit special because it effectively forces
     * an RGBA mask and we end up ignoring any separate alpha channel
     * function.
     */
    case GL_DOT3_RGBA:
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

        if (!backend_arbfp_args_equal (&args[0], &args[1]))
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
    case GL_INTERPOLATE:
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
append_masked_combine (CoglMaterial *arbfp_authority,
                       CoglMaterialLayer *layer,
                       CoglBlendStringChannelMask mask,
                       GLint function,
                       GLint *src,
                       GLint *op)
{
  int i;
  int n_args;
  CoglMaterialBackendARBfpArg args[3];

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
_cogl_material_backend_arbfp_add_layer (CoglMaterial *material,
                                        CoglMaterialLayer *layer,
                                        unsigned long layers_difference)
{
  ArbfpProgramState *arbfp_program_state = get_arbfp_program_state (material);
  CoglMaterialLayer *combine_authority =
    _cogl_material_layer_get_authority (layer,
                                        COGL_MATERIAL_LAYER_STATE_COMBINE);
  CoglMaterialLayerBigState *big_state = combine_authority->big_state;

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

  if (!need_texture_combine_separate (combine_authority))
    {
      append_masked_combine (material,
                             layer,
                             COGL_BLEND_STRING_CHANNEL_MASK_RGBA,
                             big_state->texture_combine_rgb_func,
                             big_state->texture_combine_rgb_src,
                             big_state->texture_combine_rgb_op);
    }
  else if (big_state->texture_combine_rgb_func == GL_DOT3_RGBA)
    {
      /* GL_DOT3_RGBA Is a bit weird as a GL_COMBINE_RGB function
       * since if you use it, it overrides your ALPHA function...
       */
      append_masked_combine (material,
                             layer,
                             COGL_BLEND_STRING_CHANNEL_MASK_RGBA,
                             big_state->texture_combine_rgb_func,
                             big_state->texture_combine_rgb_src,
                             big_state->texture_combine_rgb_op);
    }
  else
    {
      append_masked_combine (material,
                             layer,
                             COGL_BLEND_STRING_CHANNEL_MASK_RGB,
                             big_state->texture_combine_rgb_func,
                             big_state->texture_combine_rgb_src,
                             big_state->texture_combine_rgb_op);
      append_masked_combine (material,
                             layer,
                             COGL_BLEND_STRING_CHANNEL_MASK_ALPHA,
                             big_state->texture_combine_alpha_func,
                             big_state->texture_combine_alpha_src,
                             big_state->texture_combine_alpha_op);
    }

  return TRUE;
}

gboolean
_cogl_material_backend_arbfp_passthrough (CoglMaterial *material)
{
  ArbfpProgramState *arbfp_program_state = get_arbfp_program_state (material);

  if (!arbfp_program_state->source)
    return TRUE;

  g_string_append (arbfp_program_state->source,
                   "MOV output, fragment.color.primary;\n");
  return TRUE;
}

typedef struct _UpdateConstantsState
{
  int unit;
  ArbfpProgramState *arbfp_program_state;
} UpdateConstantsState;

static gboolean
update_constants_cb (CoglMaterial *material,
                     int layer_index,
                     void *user_data)
{
  UpdateConstantsState *state = user_data;
  ArbfpProgramState *arbfp_program_state = state->arbfp_program_state;
  UnitState *unit_state = &arbfp_program_state->unit_state[state->unit++];

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (unit_state->dirty_combine_constant)
    {
      float constant[4];
      _cogl_material_get_layer_combine_constant (material,
                                                 layer_index,
                                                 constant);
      GE (glProgramLocalParameter4fv (GL_FRAGMENT_PROGRAM_ARB,
                                      unit_state->constant_id,
                                      constant));
      unit_state->dirty_combine_constant = FALSE;
    }
  return TRUE;
}

static gboolean
_cogl_material_backend_arbfp_end (CoglMaterial *material,
                                  unsigned long materials_difference)
{
  ArbfpProgramState *arbfp_program_state = get_arbfp_program_state (material);
  GLuint gl_program;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (arbfp_program_state->source)
    {
      GLenum gl_error;
      COGL_STATIC_COUNTER (backend_arbfp_compile_counter,
                           "arbfp compile counter",
                           "Increments each time a new ARBfp "
                           "program is compiled",
                           0 /* no application private data */);

      COGL_COUNTER_INC (_cogl_uprof_context, backend_arbfp_compile_counter);

      g_string_append (arbfp_program_state->source,
                       "MOV result.color,output;\n");
      g_string_append (arbfp_program_state->source, "END\n");

      if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_SHOW_SOURCE))
        g_message ("material program:\n%s", arbfp_program_state->source->str);

      GE (glGenPrograms (1, &arbfp_program_state->gl_program));

      GE (glBindProgram (GL_FRAGMENT_PROGRAM_ARB,
                         arbfp_program_state->gl_program));

      while ((gl_error = glGetError ()) != GL_NO_ERROR)
        ;
      glProgramString (GL_FRAGMENT_PROGRAM_ARB,
                       GL_PROGRAM_FORMAT_ASCII_ARB,
                       arbfp_program_state->source->len,
                       arbfp_program_state->source->str);
      if (glGetError () != GL_NO_ERROR)
        {
          g_warning ("\n%s\n%s",
                     arbfp_program_state->source->str,
                     glGetString (GL_PROGRAM_ERROR_STRING_ARB));
        }

      arbfp_program_state->source = NULL;
    }

  if (arbfp_program_state->user_program != COGL_INVALID_HANDLE)
    {
      CoglProgram *program = (CoglProgram *)arbfp_program_state->user_program;
      gl_program = program->gl_handle;
    }
  else
    gl_program = arbfp_program_state->gl_program;

  GE (glBindProgram (GL_FRAGMENT_PROGRAM_ARB, gl_program));
  _cogl_use_program (COGL_INVALID_HANDLE, COGL_MATERIAL_PROGRAM_TYPE_ARBFP);

  if (arbfp_program_state->user_program == COGL_INVALID_HANDLE)
    {
      UpdateConstantsState state;
      state.unit = 0;
      state.arbfp_program_state = arbfp_program_state;
      cogl_material_foreach_layer (material,
                                   update_constants_cb,
                                   &state);
    }

  return TRUE;
}

static void
dirty_arbfp_program_state (CoglMaterial *material)
{
  CoglMaterialBackendARBfpPrivate *priv;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  priv = get_arbfp_priv (material);
  if (!priv)
    return;

  if (priv->arbfp_program_state)
    {
      arbfp_program_state_unref (priv->arbfp_program_state);
      priv->arbfp_program_state = NULL;
    }
}

static void
_cogl_material_backend_arbfp_material_pre_change_notify (
                                                   CoglMaterial *material,
                                                   CoglMaterialState change,
                                                   const CoglColor *new_color)
{
  static const unsigned long fragment_op_changes =
    COGL_MATERIAL_STATE_LAYERS |
    COGL_MATERIAL_STATE_USER_SHADER;
    /* TODO: COGL_MATERIAL_STATE_FOG */

  if (!(change & fragment_op_changes))
    return;

  dirty_arbfp_program_state (material);
}

/* NB: layers are considered immutable once they have any dependants
 * so although multiple materials can end up depending on a single
 * static layer, we can guarantee that if a layer is being *changed*
 * then it can only have one material depending on it.
 *
 * XXX: Don't forget this is *pre* change, we can't read the new value
 * yet!
 */
static void
_cogl_material_backend_arbfp_layer_pre_change_notify (
                                                CoglMaterial *owner,
                                                CoglMaterialLayer *layer,
                                                CoglMaterialLayerState change)
{
  CoglMaterialBackendARBfpPrivate *priv;
  static const unsigned long not_fragment_op_changes =
    COGL_MATERIAL_LAYER_STATE_COMBINE_CONSTANT |
    COGL_MATERIAL_LAYER_STATE_TEXTURE;

  priv = get_arbfp_priv (owner);
  if (!priv)
    return;

  if (!(change & not_fragment_op_changes))
    {
      dirty_arbfp_program_state (owner);
      return;
    }

  if (change & COGL_MATERIAL_LAYER_STATE_COMBINE_CONSTANT)
    {
      ArbfpProgramState *arbfp_program_state =
        get_arbfp_program_state (owner);
      int unit_index = _cogl_material_layer_get_unit_index (layer);
      arbfp_program_state->unit_state[unit_index].dirty_combine_constant = TRUE;
    }

  /* TODO: we could be saving snippets of texture combine code along
   * with each layer and then when a layer changes we would just free
   * the snippet. */
  return;
}

static void
_cogl_material_backend_arbfp_free_priv (CoglMaterial *material)
{
  CoglMaterialBackendARBfpPrivate *priv = get_arbfp_priv (material);
  if (priv)
    {
      if (priv->arbfp_program_state)
        arbfp_program_state_unref (priv->arbfp_program_state);
      g_slice_free (CoglMaterialBackendARBfpPrivate, priv);
      set_arbfp_priv (material, NULL);
    }
}

const CoglMaterialBackend _cogl_material_arbfp_backend =
{
  _cogl_material_backend_arbfp_get_max_texture_units,
  _cogl_material_backend_arbfp_start,
  _cogl_material_backend_arbfp_add_layer,
  _cogl_material_backend_arbfp_passthrough,
  _cogl_material_backend_arbfp_end,
  _cogl_material_backend_arbfp_material_pre_change_notify,
  NULL,
  _cogl_material_backend_arbfp_layer_pre_change_notify,
  _cogl_material_backend_arbfp_free_priv,
  NULL
};

#endif /* COGL_MATERIAL_BACKEND_ARBFP */

