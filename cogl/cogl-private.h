/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
