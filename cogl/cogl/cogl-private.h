/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010,2013 Intel Corporation.
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
 */

#ifndef __COGL_PRIVATE_H__
#define __COGL_PRIVATE_H__

#include <cogl/cogl-pipeline.h>

#include "cogl-context.h"
#include "cogl-flags.h"

COGL_BEGIN_DECLS

typedef enum
{
  COGL_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE,
  COGL_PRIVATE_FEATURE_MESA_PACK_INVERT,
  COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT,
  COGL_PRIVATE_FEATURE_FOUR_CLIP_PLANES,
  COGL_PRIVATE_FEATURE_PBOS,
  COGL_PRIVATE_FEATURE_VBOS,
  COGL_PRIVATE_FEATURE_EXT_PACKED_DEPTH_STENCIL,
  COGL_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL,
  COGL_PRIVATE_FEATURE_TEXTURE_FORMAT_BGRA8888,
  COGL_PRIVATE_FEATURE_UNPACK_SUBIMAGE,
  COGL_PRIVATE_FEATURE_SAMPLER_OBJECTS,
  COGL_PRIVATE_FEATURE_READ_PIXELS_ANY_FORMAT,
  COGL_PRIVATE_FEATURE_ALPHA_TEST,
  COGL_PRIVATE_FEATURE_FORMAT_CONVERSION,
  COGL_PRIVATE_FEATURE_QUADS,
  COGL_PRIVATE_FEATURE_BLEND_CONSTANT,
  COGL_PRIVATE_FEATURE_QUERY_FRAMEBUFFER_BITS,
  COGL_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM,
  COGL_PRIVATE_FEATURE_QUERY_TEXTURE_PARAMETERS,
  COGL_PRIVATE_FEATURE_ALPHA_TEXTURES,
  COGL_PRIVATE_FEATURE_TEXTURE_SWIZZLE,
  COGL_PRIVATE_FEATURE_TEXTURE_MAX_LEVEL,
  COGL_PRIVATE_FEATURE_ARBFP,
  COGL_PRIVATE_FEATURE_OES_EGL_SYNC,
  /* If this is set then the winsys is responsible for queueing dirty
   * events. Otherwise a dirty event will be queued when the onscreen
   * is first allocated or when it is shown or resized */
  COGL_PRIVATE_FEATURE_DIRTY_EVENTS,
  COGL_PRIVATE_FEATURE_ENABLE_PROGRAM_POINT_SIZE,
  /* These features let us avoid conditioning code based on the exact
   * driver being used and instead check for broad opengl feature
   * sets that can be shared by several GL apis */
  COGL_PRIVATE_FEATURE_ANY_GL,
  COGL_PRIVATE_FEATURE_GL_FIXED,
  COGL_PRIVATE_FEATURE_GL_PROGRAMMABLE,
  COGL_PRIVATE_FEATURE_GL_EMBEDDED,
  COGL_PRIVATE_FEATURE_GL_WEB,

  COGL_N_PRIVATE_FEATURES
} CoglPrivateFeature;

/* Sometimes when evaluating pipelines, either during comparisons or
 * if calculating a hash value we need to tweak the evaluation
 * semantics */
typedef enum _CoglPipelineEvalFlags
{
  COGL_PIPELINE_EVAL_FLAG_NONE = 0
} CoglPipelineEvalFlags;

void
_cogl_transform_point (const CoglMatrix *matrix_mv,
                       const CoglMatrix *matrix_p,
                       const float *viewport,
                       float *x,
                       float *y);

CoglBool
_cogl_check_extension (const char *name, char * const *ext);

void
_cogl_clear (const CoglColor *color, unsigned long buffers);

void
_cogl_init (void);

void
_cogl_push_source (CoglPipeline *pipeline, CoglBool enable_legacy);

CoglBool
_cogl_get_enable_legacy_state (void);

#define _cogl_has_private_feature(ctx, feature) \
  COGL_FLAGS_GET ((ctx)->private_features, (feature))

/*
 * _cogl_pixel_format_get_bytes_per_pixel:
 * @format: a #CoglPixelFormat
 *
 * Queries how many bytes a pixel of the given @format takes.
 *
 * Return value: The number of bytes taken for a pixel of the given
 *               @format.
 */
int
_cogl_pixel_format_get_bytes_per_pixel (CoglPixelFormat format);

/*
 * _cogl_pixel_format_has_aligned_components:
 * @format: a #CoglPixelFormat
 *
 * Queries whether the ordering of the components for the given
 * @format depend on the endianness of the host CPU or if the
 * components can be accessed using bit shifting and bitmasking by
 * loading a whole pixel into a word.
 *
 * XXX: If we ever consider making something like this public we
 * should really try to think of a better name and come up with
 * much clearer documentation since it really depends on what
 * point of view you consider this from whether a format like
 * COGL_PIXEL_FORMAT_RGBA_8888 is endian dependent. E.g. If you
 * read an RGBA_8888 pixel into a uint32
 * it's endian dependent how you mask out the different channels.
 * But If you already have separate color components and you want
 * to write them to an RGBA_8888 pixel then the bytes can be
 * written sequentially regardless of the endianness.
 *
 * Return value: %TRUE if you need to consider the host CPU
 *               endianness when dealing with the given @format
 *               else %FALSE.
 */
CoglBool
_cogl_pixel_format_is_endian_dependant (CoglPixelFormat format);

/*
 * COGL_PIXEL_FORMAT_CAN_HAVE_PREMULT(format):
 * @format: a #CoglPixelFormat
 *
 * Returns TRUE if the pixel format can take a premult bit. This is
 * currently true for all formats that have an alpha channel except
 * COGL_PIXEL_FORMAT_A_8 (because that doesn't have any other
 * components to multiply by the alpha).
 */
#define COGL_PIXEL_FORMAT_CAN_HAVE_PREMULT(format) \
  (((format) & COGL_A_BIT) && (format) != COGL_PIXEL_FORMAT_A_8)

COGL_END_DECLS

#endif /* __COGL_PRIVATE_H__ */
