/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2012 Intel Corporation.
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
 */

#ifndef _COGL_TEXTURE_GL_PRIVATE_H_
#define _COGL_TEXTURE_GL_PRIVATE_H_

#include "cogl-context.h"

void
_cogl_texture_gl_prep_alignment_for_pixels_upload (CoglContext *ctx,
                                                   int pixels_rowstride);

void
_cogl_texture_gl_prep_alignment_for_pixels_download (CoglContext *ctx,
                                                     int bpp,
                                                     int width,
                                                     int rowstride);

void
_cogl_texture_gl_flush_legacy_texobj_wrap_modes (CoglTexture *texture,
                                                 unsigned int wrap_mode_s,
                                                 unsigned int wrap_mode_t,
                                                 unsigned int wrap_mode_p);

void
_cogl_texture_gl_flush_legacy_texobj_filters (CoglTexture *texture,
                                              unsigned int min_filter,
                                              unsigned int mag_filter);

void
_cogl_texture_gl_maybe_update_max_level (CoglTexture *texture,
                                         int max_level);

void
_cogl_texture_gl_generate_mipmaps (CoglTexture *texture);

GLenum
_cogl_texture_gl_get_format (CoglTexture *texture);

#endif /* _COGL_TEXTURE_GL_PRIVATE_H_ */
