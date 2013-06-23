/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009,2010,2011,2012 Intel Corporation.
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
 *
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-private.h"
#include "cogl-texture-2d-nop-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-error-private.h"

void
_cogl_texture_2d_nop_free (CoglTexture2D *tex_2d)
{
}

CoglBool
_cogl_texture_2d_nop_can_create (CoglContext *ctx,
                                 int width,
                                 int height,
                                 CoglPixelFormat internal_format)
{
  return TRUE;
}

void
_cogl_texture_2d_nop_init (CoglTexture2D *tex_2d)
{
}

CoglBool
_cogl_texture_2d_nop_allocate (CoglTexture *tex,
                               CoglError **error)
{
  return TRUE;
}

void
_cogl_texture_2d_nop_flush_legacy_texobj_filters (CoglTexture *tex,
                                                  GLenum min_filter,
                                                  GLenum mag_filter)
{
}

void
_cogl_texture_2d_nop_flush_legacy_texobj_wrap_modes (CoglTexture *tex,
                                                     GLenum wrap_mode_s,
                                                     GLenum wrap_mode_t,
                                                     GLenum wrap_mode_p)
{
}

void
_cogl_texture_2d_nop_copy_from_framebuffer (CoglTexture2D *tex_2d,
                                            int src_x,
                                            int src_y,
                                            int width,
                                            int height,
                                            CoglFramebuffer *src_fb,
                                            int dst_x,
                                            int dst_y,
                                            int level)
{
}

unsigned int
_cogl_texture_2d_nop_get_gl_handle (CoglTexture2D *tex_2d)
{
  return 0;
}

void
_cogl_texture_2d_nop_generate_mipmap (CoglTexture2D *tex_2d)
{
}

CoglBool
_cogl_texture_2d_nop_copy_from_bitmap (CoglTexture2D *tex_2d,
                                       int src_x,
                                       int src_y,
                                       int width,
                                       int height,
                                       CoglBitmap *bitmap,
                                       int dst_x,
                                       int dst_y,
                                       int level,
                                       CoglError **error)
{
  return TRUE;
}

void
_cogl_texture_2d_nop_get_data (CoglTexture2D *tex_2d,
                               CoglPixelFormat format,
                               size_t rowstride,
                               uint8_t *data)
{
}
