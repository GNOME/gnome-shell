/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef COGL_BLEND_STRING_H
#define COGL_BLEND_STRING_H

#include <stdlib.h>
#include <glib.h>

typedef enum _CoglBlendStringContext
{
  COGL_BLEND_STRING_CONTEXT_BLENDING,
  COGL_BLEND_STRING_CONTEXT_TEXTURE_COMBINE
} CoglBlendStringContext;

/* NB: debug stringify code will get upset if these
 * are re-ordered */
typedef enum _CoglBlendStringChannelMask
{
  COGL_BLEND_STRING_CHANNEL_MASK_RGB,
  COGL_BLEND_STRING_CHANNEL_MASK_ALPHA,
  COGL_BLEND_STRING_CHANNEL_MASK_RGBA
} CoglBlendStringChannelMask;

typedef enum _CoglBlendStringColorSourceType
{
  /* blending */
  COGL_BLEND_STRING_COLOR_SOURCE_SRC_COLOR,
  COGL_BLEND_STRING_COLOR_SOURCE_DST_COLOR,

  /* shared */
  COGL_BLEND_STRING_COLOR_SOURCE_CONSTANT,

  /* texture combining */
  COGL_BLEND_STRING_COLOR_SOURCE_TEXTURE,
  COGL_BLEND_STRING_COLOR_SOURCE_TEXTURE_N,
  COGL_BLEND_STRING_COLOR_SOURCE_PRIMARY,
  COGL_BLEND_STRING_COLOR_SOURCE_PREVIOUS
} CoglBlendStringColorSourceType;

typedef struct _CoglBlendStringColorSourceInfo
{
  CoglBlendStringColorSourceType type;
  const char *name;
  size_t name_len;
} CoglBlendStringColorSourceInfo;

typedef struct _CoglBlendStringColorSource
{
  gboolean is_zero;
  const CoglBlendStringColorSourceInfo *info;
  int texture; /* for the TEXTURE_N color source */
  gboolean one_minus;
  CoglBlendStringChannelMask mask;
} CoglBlendStringColorSource;

typedef struct _CoglBlendStringFactor
{
  gboolean is_one;
  gboolean is_src_alpha_saturate;
  gboolean is_color;
  CoglBlendStringColorSource source;
} CoglBlendStringFactor;

typedef struct _CoglBlendStringArgument
{
  CoglBlendStringColorSource source;
  CoglBlendStringFactor factor;
} CoglBlendStringArgument;

typedef enum _CoglBlendStringFunctionType
{
  /* shared */
  COGL_BLEND_STRING_FUNCTION_AUTO_COMPOSITE,
  COGL_BLEND_STRING_FUNCTION_ADD,

  /* texture combine only */
  COGL_BLEND_STRING_FUNCTION_REPLACE,
  COGL_BLEND_STRING_FUNCTION_MODULATE,
  COGL_BLEND_STRING_FUNCTION_ADD_SIGNED,
  COGL_BLEND_STRING_FUNCTION_INTERPOLATE,
  COGL_BLEND_STRING_FUNCTION_SUBTRACT,
  COGL_BLEND_STRING_FUNCTION_DOT3_RGB,
  COGL_BLEND_STRING_FUNCTION_DOT3_RGBA
} CoglBlendStringFunctionType;

typedef struct _CoglBlendStringFunctionInfo
{
  enum _CoglBlendStringFunctionType type;
  const char *name;
  size_t name_len;
  int argc;
} CoglBlendStringFunctionInfo;

typedef struct _CoglBlendStringStatement
{
  CoglBlendStringChannelMask mask;
  const CoglBlendStringFunctionInfo *function;
  CoglBlendStringArgument args[3];
} CoglBlendStringStatement;


gboolean
_cogl_blend_string_compile (const char *string,
                            CoglBlendStringContext context,
                            CoglBlendStringStatement *statements,
                            GError **error);

void
_cogl_blend_string_split_rgba_statement (CoglBlendStringStatement *statement,
                                         CoglBlendStringStatement *rgb,
                                         CoglBlendStringStatement *a);

#endif /* COGL_BLEND_STRING_H */

