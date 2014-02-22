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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-private.h"
#include "cogl-context-private.h"
#include "cogl-feature-private.h"
#include "cogl-renderer-private.h"
#include "cogl-error-private.h"
#include "cogl-framebuffer-nop-private.h"
#include "cogl-texture-2d-nop-private.h"
#include "cogl-attribute-nop-private.h"
#include "cogl-clip-stack-nop-private.h"

static CoglBool
_cogl_driver_update_features (CoglContext *ctx,
                              CoglError **error)
{
  /* _cogl_gpu_info_init (ctx, &ctx->gpu); */

  memset (ctx->private_features, 0, sizeof (ctx->private_features));
  ctx->feature_flags = 0;

  return TRUE;
}

const CoglDriverVtable
_cogl_driver_nop =
  {
    NULL, /* pixel_format_from_gl_internal */
    NULL, /* pixel_format_to_gl */
    _cogl_driver_update_features,
    _cogl_offscreen_nop_allocate,
    _cogl_offscreen_nop_free,
    _cogl_framebuffer_nop_flush_state,
    _cogl_framebuffer_nop_clear,
    _cogl_framebuffer_nop_query_bits,
    _cogl_framebuffer_nop_finish,
    _cogl_framebuffer_nop_discard_buffers,
    _cogl_framebuffer_nop_draw_attributes,
    _cogl_framebuffer_nop_draw_indexed_attributes,
    _cogl_framebuffer_nop_read_pixels_into_bitmap,
    _cogl_texture_2d_nop_free,
    _cogl_texture_2d_nop_can_create,
    _cogl_texture_2d_nop_init,
    _cogl_texture_2d_nop_allocate,
    _cogl_texture_2d_nop_copy_from_framebuffer,
    _cogl_texture_2d_nop_get_gl_handle,
    _cogl_texture_2d_nop_generate_mipmap,
    _cogl_texture_2d_nop_copy_from_bitmap,
    NULL, /* texture_2d_get_data */
    _cogl_nop_flush_attributes_state,
    _cogl_clip_stack_nop_flush,
  };
