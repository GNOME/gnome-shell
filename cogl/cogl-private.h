/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010,2013 Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_PRIVATE_H__
#define __COGL_PRIVATE_H__

#include <cogl/cogl-pipeline.h>

#include "cogl-context.h"


COGL_BEGIN_DECLS

typedef enum
{
  COGL_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE = 1L<<0,
  COGL_PRIVATE_FEATURE_MESA_PACK_INVERT = 1L<<1,
  COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT = 1L<<2,
  COGL_PRIVATE_FEATURE_FOUR_CLIP_PLANES = 1L<<3,
  COGL_PRIVATE_FEATURE_PBOS = 1L<<4,
  COGL_PRIVATE_FEATURE_VBOS = 1L<<5,
  COGL_PRIVATE_FEATURE_EXT_PACKED_DEPTH_STENCIL = 1L<<6,
  COGL_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL = 1L<<7,
  COGL_PRIVATE_FEATURE_TEXTURE_FORMAT_BGRA8888 = 1L<<8,
  COGL_PRIVATE_FEATURE_UNPACK_SUBIMAGE = 1L<<9,
  COGL_PRIVATE_FEATURE_SAMPLER_OBJECTS = 1L<<10,
  COGL_PRIVATE_FEATURE_READ_PIXELS_ANY_FORMAT = 1L<<11,
  COGL_PRIVATE_FEATURE_ALPHA_TEST = 1L<<12,
  COGL_PRIVATE_FEATURE_FORMAT_CONVERSION = 1L<<13,
  COGL_PRIVATE_FEATURE_QUADS = 1L<<14,
  COGL_PRIVATE_FEATURE_BLEND_CONSTANT = 1L<<15,
  COGL_PRIVATE_FEATURE_QUERY_FRAMEBUFFER_BITS = 1L<<16,
  COGL_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM = 1L<<17,
  COGL_PRIVATE_FEATURE_QUERY_TEXTURE_PARAMETERS = 1L<<18,
  COGL_PRIVATE_FEATURE_ALPHA_TEXTURES = 1L<<19,
  COGL_PRIVATE_FEATURE_TEXTURE_SWIZZLE = 1L<<20,
  COGL_PRIVATE_FEATURE_TEXTURE_MAX_LEVEL = 1L<<21,
  COGL_PRIVATE_FEATURE_ARBFP = 1L<<22,
  COGL_PRIVATE_FEATURE_OES_EGL_SYNC = 1L<<23,
  /* If this is set then the winsys is responsible for queueing dirty
   * events. Otherwise a dirty event will be queued when the onscreen
   * is first allocated or when it is shown or resized */
  COGL_PRIVATE_FEATURE_DIRTY_EVENTS = 1L<<24,
  COGL_PRIVATE_FEATURE_ENABLE_PROGRAM_POINT_SIZE = 1L<<25,
  /* These features let us avoid conditioning code based on the exact
   * driver being used and instead check for broad opengl feature
   * sets that can be shared by several GL apis */
  COGL_PRIVATE_FEATURE_ANY_GL = 1L<<26,
  COGL_PRIVATE_FEATURE_GL_FIXED = 1L<<27,
  COGL_PRIVATE_FEATURE_GL_PROGRAMMABLE = 1L<<28,
  COGL_PRIVATE_FEATURE_GL_EMBEDDED = 1L<<29
} CoglPrivateFeatureFlags;

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
