/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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
