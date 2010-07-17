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

typedef struct _CoglMaterialBackendARBfpPrivate
{
  CoglMaterial *authority_cache;
  unsigned long authority_cache_age;

  GString *source;
  GLuint gl_program;
  gboolean *sampled;
  int next_constant_id;
} CoglMaterialBackendARBfpPrivate;

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
    COGL_MATERIAL_LAYER_STATE_COMBINE_CONSTANT |
    COGL_MATERIAL_LAYER_STATE_UNIT |
    COGL_MATERIAL_LAYER_STATE_TEXTURE;

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
find_arbfp_authority (CoglMaterial *material)
{
  CoglMaterial *authority0;
  CoglMaterial *authority1;
  int n_layers;
  CoglMaterialLayer **authority0_layers;
  CoglMaterialLayer **authority1_layers;

  /* XXX: we'll need to update this when we add fog support to the
   * arbfp codegen */

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
      _cogl_material_foreach_layer (authority0,
                                    add_layer_to_array_cb,
                                    &state);

      authority1_layers =
        g_alloca (sizeof (CoglMaterialLayer *) * n_layers);
      state.i = 0;
      state.layers = authority1_layers;
      _cogl_material_foreach_layer (authority1,
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

static void
invalidate_arbfp_authority_cache (CoglMaterial *material)
{
  if (material->backend_priv_set_mask  & COGL_MATERIAL_BACKEND_ARBFP_MASK)
    {
      CoglMaterialBackendARBfpPrivate *priv =
        material->backend_privs[COGL_MATERIAL_BACKEND_ARBFP];
      priv->authority_cache = NULL;
      priv->authority_cache_age = 0;
    }
}

static gboolean
_cogl_material_backend_arbfp_start (CoglMaterial *material,
                                    int n_layers,
                                    unsigned long materials_difference)
{
  CoglMaterial *authority;
  CoglMaterialBackendARBfpPrivate *priv;
  CoglMaterialBackendARBfpPrivate *authority_priv;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!_cogl_features_available_private (COGL_FEATURE_PRIVATE_ARB_FP))
    return FALSE;

  /* TODO: support fog */
  if (ctx->legacy_fog_state.enabled)
    return FALSE;

  /* Note: we allocate ARBfp private state for both the given material
   * and the authority. (The oldest ancestor whos state will result in
   * the same program being generated) The former will simply cache a
   * pointer to the authority and the later will track the arbfp
   * program that we will generate.
   */

  if (!(material->backend_priv_set_mask & COGL_MATERIAL_BACKEND_ARBFP_MASK))
    {
      material->backend_privs[COGL_MATERIAL_BACKEND_ARBFP] =
        g_slice_new0 (CoglMaterialBackendARBfpPrivate);
      material->backend_priv_set_mask |= COGL_MATERIAL_BACKEND_ARBFP_MASK;
    }
  priv = material->backend_privs[COGL_MATERIAL_BACKEND_ARBFP];

  /* XXX: We are making assumptions that we don't yet support
   * modification of ancestors to optimize the sharing of state in the
   * material graph. When we start to support this then the arbfp
   * backend will somehow need to be notified of graph changes that
   * may invalidate authority_cache pointers.
   */

  if (priv->authority_cache &&
      priv->authority_cache_age != _cogl_material_get_age (material))
    invalidate_arbfp_authority_cache (material);

  if (!priv->authority_cache)
    {
      priv->authority_cache = find_arbfp_authority (material);
      priv->authority_cache_age = _cogl_material_get_age (material);
    }

  authority = priv->authority_cache;
  if (!(authority->backend_priv_set_mask & COGL_MATERIAL_BACKEND_ARBFP_MASK))
    {
      authority->backend_privs[COGL_MATERIAL_BACKEND_ARBFP] =
        g_slice_new0 (CoglMaterialBackendARBfpPrivate);
      authority->backend_priv_set_mask |= COGL_MATERIAL_BACKEND_ARBFP_MASK;
    }
  authority_priv = authority->backend_privs[COGL_MATERIAL_BACKEND_ARBFP];

  if (authority_priv->gl_program == 0)
    {
      /* We reuse a single grow-only GString for ARBfp code-gen */
      g_string_set_size (ctx->arbfp_source_buffer, 0);
      authority_priv->source = ctx->arbfp_source_buffer;
      g_string_append (authority_priv->source,
                       "!!ARBfp1.0\n"
                       "TEMP output;\n"
                       "TEMP tmp0, tmp1, tmp2, tmp3, tmp4;\n"
                       "PARAM half = {.5, .5, .5, .5};\n"
                       "PARAM one = {1, 1, 1, 1};\n"
                       "PARAM two = {2, 2, 2, 2};\n"
                       "PARAM minus_one = {-1, -1, -1, -1};\n");
      authority_priv->sampled = g_new0 (gboolean, n_layers);
    }

  return TRUE;
}

static CoglMaterial *
get_arbfp_authority (CoglMaterial *material)
{
  CoglMaterialBackendARBfpPrivate *priv =
    material->backend_privs[COGL_MATERIAL_BACKEND_ARBFP];

  g_return_val_if_fail (priv != NULL, NULL);

  return priv->authority_cache;
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
setup_texture_source (CoglMaterialBackendARBfpPrivate *priv,
                      int unit_index,
                      GLenum gl_target)
{
  if (!priv->sampled[unit_index])
    {
      g_string_append_printf (priv->source,
                              "TEMP texel%d;\n"
                              "TEX texel%d,fragment.texcoord[%d],"
                              "texture[%d],%s;\n",
                              unit_index,
                              unit_index,
                              unit_index,
                              unit_index,
                              gl_target_to_arbfp_string (gl_target));
      priv->sampled[unit_index] = TRUE;
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
      g_string_append_printf (source, "constant%d%s",
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
  CoglMaterial *arbfp_authority = get_arbfp_authority (material);
  CoglMaterialBackendARBfpPrivate *priv =
    arbfp_authority->backend_privs[COGL_MATERIAL_BACKEND_ARBFP];
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
      setup_texture_source (priv, arg->texture_unit, gl_target);
      break;
    case GL_CONSTANT:
      {
        unsigned long state = COGL_MATERIAL_LAYER_STATE_COMBINE_CONSTANT;
        CoglMaterialLayer *authority =
          _cogl_material_layer_get_authority (layer, state);
        CoglMaterialLayerBigState *big_state = authority->big_state;
        char buf[4][G_ASCII_DTOSTR_BUF_SIZE];
        int i;

        arg->type = COGL_MATERIAL_BACKEND_ARBFP_ARG_TYPE_CONSTANT;
        arg->name = "constant%d";
        arg->constant_id = priv->next_constant_id++;

        for (i = 0; i < 4; i++)
          g_ascii_dtostr (buf[i], G_ASCII_DTOSTR_BUF_SIZE,
                          big_state->texture_combine_constant[i]);

        g_string_append_printf (priv->source,
                                "PARAM constant%d = "
                                "  {%s, %s, %s, %s};\n",
                                arg->constant_id,
                                buf[0],
                                buf[1],
                                buf[2],
                                buf[3]);
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
      setup_texture_source (priv, arg->texture_unit, gl_target);
    }

  arg->swizzle = "";

  switch (op)
    {
    case GL_SRC_COLOR:
      break;
    case GL_ONE_MINUS_SRC_COLOR:
      g_string_append_printf (priv->source,
                              "SUB tmp%d, one, ",
                              arg_index);
      append_arg (priv->source, arg);
      g_string_append_printf (priv->source, ";\n");
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
      g_string_append_printf (priv->source,
                              "SUB tmp%d, one, ",
                              arg_index);
      append_arg (priv->source, arg);
      /* avoid a swizzle if we know RGB are going to be masked
       * in the end anyway */
      if (mask != COGL_BLEND_STRING_CHANNEL_MASK_ALPHA)
        g_string_append_printf (priv->source, ".a;\n");
      else
        g_string_append_printf (priv->source, ";\n");
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
  CoglMaterial *arbfp_authority = get_arbfp_authority (material);
  CoglMaterialBackendARBfpPrivate *priv =
    arbfp_authority->backend_privs[COGL_MATERIAL_BACKEND_ARBFP];
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
      g_string_append_printf (priv->source, "ADD_SAT output%s, ",
                              mask_name);
      break;
    case GL_MODULATE:
      /* Note: no need to saturate since we can assume operands
       * have values in the range [0,1] */
      g_string_append_printf (priv->source, "MUL output%s, ",
                              mask_name);
      break;
    case GL_REPLACE:
      /* Note: no need to saturate since we can assume operand
       * has a value in the range [0,1] */
      g_string_append_printf (priv->source, "MOV output%s, ",
                              mask_name);
      break;
    case GL_SUBTRACT:
      g_string_append_printf (priv->source, "SUB_SAT output%s, ",
                              mask_name);
      break;
    case GL_ADD_SIGNED:
      g_string_append_printf (priv->source, "ADD tmp3%s, ",
                              mask_name);
      append_arg (priv->source, &args[0]);
      g_string_append (priv->source, ", ");
      append_arg (priv->source, &args[1]);
      g_string_append (priv->source, ";\n");
      g_string_append_printf (priv->source, "SUB_SAT output%s, tmp3, half",
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

        g_string_append (priv->source, "MAD tmp3, two, ");
        append_arg (priv->source, &args[0]);
        g_string_append (priv->source, ", minus_one;\n");

        if (!backend_arbfp_args_equal (&args[0], &args[1]))
          {
            g_string_append (priv->source, "MAD tmp4, two, ");
            append_arg (priv->source, &args[1]);
            g_string_append (priv->source, ", minus_one;\n");
          }
        else
          tmp4 = "tmp3";

        g_string_append_printf (priv->source,
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
      g_string_append_printf (priv->source, "LRP output%s, ",
                              mask_name);
      append_arg (priv->source, &args[2]);
      g_string_append (priv->source, ", ");
      append_arg (priv->source, &args[0]);
      g_string_append (priv->source, ", ");
      append_arg (priv->source, &args[1]);
      n_args = 0;
      break;
    default:
      g_error ("Unknown texture combine function %d", function);
      g_string_append_printf (priv->source, "MUL_SAT output%s, ",
                              mask_name);
      n_args = 2;
      break;
    }

  if (n_args > 0)
    append_arg (priv->source, &args[0]);
  if (n_args > 1)
    {
      g_string_append (priv->source, ", ");
      append_arg (priv->source, &args[1]);
    }
  g_string_append (priv->source, ";\n");
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
  CoglMaterial *arbfp_authority = get_arbfp_authority (material);
  CoglMaterialBackendARBfpPrivate *priv =
    arbfp_authority->backend_privs[COGL_MATERIAL_BACKEND_ARBFP];
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

  if (!priv->source)
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
  CoglMaterial *arbfp_authority = get_arbfp_authority (material);
  CoglMaterialBackendARBfpPrivate *priv =
    arbfp_authority->backend_privs[COGL_MATERIAL_BACKEND_ARBFP];

  if (!priv->source)
    return TRUE;

  g_string_append (priv->source, "MOV output, fragment.color.primary;\n");
  return TRUE;
}

static gboolean
_cogl_material_backend_arbfp_end (CoglMaterial *material,
                                  unsigned long materials_difference)
{
  CoglMaterial *arbfp_authority = get_arbfp_authority (material);
  CoglMaterialBackendARBfpPrivate *priv =
    arbfp_authority->backend_privs[COGL_MATERIAL_BACKEND_ARBFP];

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (priv->source)
    {
      GLenum gl_error;
      COGL_STATIC_COUNTER (backend_arbfp_compile_counter,
                           "arbfp compile counter",
                           "Increments each time a new ARBfp "
                           "program is compiled",
                           0 /* no application private data */);

      COGL_COUNTER_INC (_cogl_uprof_context, backend_arbfp_compile_counter);

      g_string_append (priv->source, "MOV result.color,output;\n");
      g_string_append (priv->source, "END\n");

      if (G_UNLIKELY (cogl_debug_flags & COGL_DEBUG_SHOW_SOURCE))
        g_message ("material program:\n%s", priv->source->str);

      GE (glGenPrograms (1, &priv->gl_program));

      GE (glBindProgram (GL_FRAGMENT_PROGRAM_ARB, priv->gl_program));

      while ((gl_error = glGetError ()) != GL_NO_ERROR)
        ;
      glProgramString (GL_FRAGMENT_PROGRAM_ARB,
                       GL_PROGRAM_FORMAT_ASCII_ARB,
                       priv->source->len,
                       priv->source->str);
      if (glGetError () != GL_NO_ERROR)
        {
          g_warning ("\n%s\n%s",
                     priv->source->str,
                     glGetString (GL_PROGRAM_ERROR_STRING_ARB));
        }

      priv->source = NULL;

      g_free (priv->sampled);
      priv->sampled = NULL;
    }
  else
    GE (glBindProgram (GL_FRAGMENT_PROGRAM_ARB, priv->gl_program));

  _cogl_use_program (COGL_INVALID_HANDLE, COGL_MATERIAL_PROGRAM_TYPE_ARBFP);

  return TRUE;
}

static void
_cogl_material_backend_arbfp_material_pre_change_notify (
                                                   CoglMaterial *material,
                                                   CoglMaterialState change,
                                                   const CoglColor *new_color)
{
  CoglMaterialBackendARBfpPrivate *priv =
    material->backend_privs[COGL_MATERIAL_BACKEND_ARBFP];
  static const unsigned long fragment_op_changes =
    COGL_MATERIAL_STATE_LAYERS;
    /* TODO: COGL_MATERIAL_STATE_FOG */

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (material->backend_priv_set_mask & COGL_MATERIAL_BACKEND_ARBFP_MASK &&
      priv->gl_program &&
      change & fragment_op_changes)
    {
      GE (glDeletePrograms (1, &priv->gl_program));
      priv->gl_program = 0;
    }
}

static gboolean
invalidate_arbfp_authority_cache_cb (CoglMaterialNode *node,
                                     void *user_data)
{
  CoglMaterial *material = COGL_MATERIAL (node);
  invalidate_arbfp_authority_cache (material);
  return TRUE;
}

static void
_cogl_material_backend_arbfp_material_set_parent_notify (
                                                CoglMaterial *material)
{
  /* Any arbfp authority cache associated with this material or
   * any of its descendants will now be invalid. */
  invalidate_arbfp_authority_cache (material);

  _cogl_material_node_foreach_child (COGL_MATERIAL_NODE (material),
                                     invalidate_arbfp_authority_cache_cb,
                                     NULL);
}

static void
_cogl_material_backend_arbfp_layer_pre_change_notify (
                                                CoglMaterialLayer *layer,
                                                CoglMaterialLayerState changes)
{
  /* TODO: we could be saving snippets of texture combine code along
   * with each layer and then when a layer changes we would just free
   * the snippet. */
  return;
}

static void
_cogl_material_backend_arbfp_free_priv (CoglMaterial *material)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (material->backend_priv_set_mask & COGL_MATERIAL_BACKEND_ARBFP_MASK)
    {
      CoglMaterialBackendARBfpPrivate *priv =
        material->backend_privs[COGL_MATERIAL_BACKEND_ARBFP];

      glDeletePrograms (1, &priv->gl_program);
      if (priv->sampled)
        g_free (priv->sampled);
      g_slice_free (CoglMaterialBackendARBfpPrivate, priv);
      material->backend_priv_set_mask &= ~COGL_MATERIAL_BACKEND_ARBFP_MASK;
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
  _cogl_material_backend_arbfp_material_set_parent_notify,
  _cogl_material_backend_arbfp_layer_pre_change_notify,
  _cogl_material_backend_arbfp_free_priv,
  NULL
};

#endif /* COGL_MATERIAL_BACKEND_ARBFP */

