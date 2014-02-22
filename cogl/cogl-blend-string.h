/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2009 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
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
  CoglBool is_zero;
  const CoglBlendStringColorSourceInfo *info;
  int texture; /* for the TEXTURE_N color source */
  CoglBool one_minus;
  CoglBlendStringChannelMask mask;
} CoglBlendStringColorSource;

typedef struct _CoglBlendStringFactor
{
  CoglBool is_one;
  CoglBool is_src_alpha_saturate;
  CoglBool is_color;
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


CoglBool
_cogl_blend_string_compile (const char *string,
                            CoglBlendStringContext context,
                            CoglBlendStringStatement *statements,
                            CoglError **error);

void
_cogl_blend_string_split_rgba_statement (CoglBlendStringStatement *statement,
                                         CoglBlendStringStatement *rgb,
                                         CoglBlendStringStatement *a);

#endif /* COGL_BLEND_STRING_H */

