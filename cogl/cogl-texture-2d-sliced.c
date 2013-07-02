/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009,2010 Intel Corporation.
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
 *  Matthew Allum  <mallum@openedhand.com>
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-debug.h"
#include "cogl-private.h"
#include "cogl-util.h"
#include "cogl-bitmap.h"
#include "cogl-bitmap-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-gl.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-2d-sliced-private.h"
#include "cogl-texture-gl-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-spans.h"
#include "cogl-journal-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-primitive-texture.h"
#include "cogl-error-private.h"
#include "cogl-texture-gl-private.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

static void _cogl_texture_2d_sliced_free (CoglTexture2DSliced *tex_2ds);

COGL_TEXTURE_DEFINE (Texture2DSliced, texture_2d_sliced);

static const CoglTextureVtable cogl_texture_2d_sliced_vtable;

typedef struct _ForeachData
{
  CoglMetaTextureCallback callback;
  void *user_data;
  float x_normalize_factor;
  float y_normalize_factor;
} ForeachData;

static void
re_normalize_sub_texture_coords_cb (CoglTexture *sub_texture,
                                    const float *sub_texture_coords,
                                    const float *meta_coords,
                                    void *user_data)
{
  ForeachData *data = user_data;
  /* The coordinates passed to the span iterating code were
   * un-normalized so we need to renormalize them before passing them
   * on */
  float re_normalized_coords[4] =
    {
      meta_coords[0] * data->x_normalize_factor,
      meta_coords[1] * data->y_normalize_factor,
      meta_coords[2] * data->x_normalize_factor,
      meta_coords[3] * data->y_normalize_factor
    };

  data->callback (sub_texture, sub_texture_coords, re_normalized_coords,
                  data->user_data);
}

static void
_cogl_texture_2d_sliced_foreach_sub_texture_in_region (
                                       CoglTexture *tex,
                                       float virtual_tx_1,
                                       float virtual_ty_1,
                                       float virtual_tx_2,
                                       float virtual_ty_2,
                                       CoglMetaTextureCallback callback,
                                       void *user_data)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglSpan *x_spans = (CoglSpan *)tex_2ds->slice_x_spans->data;
  CoglSpan *y_spans = (CoglSpan *)tex_2ds->slice_y_spans->data;
  CoglTexture **textures = (CoglTexture **)tex_2ds->slice_textures->data;
  float un_normalized_coords[4];
  ForeachData data;

  /* NB: its convenient for us to store non-normalized coordinates in
   * our CoglSpans but that means we need to un-normalize the incoming
   * virtual coordinates and make sure we re-normalize the coordinates
   * before calling the given callback.
   */

  data.callback = callback;
  data.user_data = user_data;
  data.x_normalize_factor = 1.0f / tex->width;
  data.y_normalize_factor = 1.0f / tex->height;

  un_normalized_coords[0] = virtual_tx_1 * tex->width;
  un_normalized_coords[1] = virtual_ty_1 * tex->height;
  un_normalized_coords[2] = virtual_tx_2 * tex->width;
  un_normalized_coords[3] = virtual_ty_2 * tex->height;

  /* Note that the normalize factors passed here are the reciprocal of
   * the factors calculated above because the span iterating code
   * normalizes by dividing by the factor instead of multiplying */
  _cogl_texture_spans_foreach_in_region (x_spans,
                                         tex_2ds->slice_x_spans->len,
                                         y_spans,
                                         tex_2ds->slice_y_spans->len,
                                         textures,
                                         un_normalized_coords,
                                         tex->width,
                                         tex->height,
                                         COGL_PIPELINE_WRAP_MODE_REPEAT,
                                         COGL_PIPELINE_WRAP_MODE_REPEAT,
                                         re_normalize_sub_texture_coords_cb,
                                         &data);
}

static uint8_t *
_cogl_texture_2d_sliced_allocate_waste_buffer (CoglTexture2DSliced *tex_2ds,
                                               CoglPixelFormat format)
{
  CoglSpan *last_x_span;
  CoglSpan *last_y_span;
  uint8_t *waste_buf = NULL;

  /* If the texture has any waste then allocate a buffer big enough to
     fill the gaps */
  last_x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan,
                                tex_2ds->slice_x_spans->len - 1);
  last_y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan,
                                tex_2ds->slice_y_spans->len - 1);
  if (last_x_span->waste > 0 || last_y_span->waste > 0)
    {
      int bpp = _cogl_pixel_format_get_bytes_per_pixel (format);
      CoglSpan  *first_x_span
        = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, 0);
      CoglSpan  *first_y_span
        = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, 0);
      unsigned int right_size = first_y_span->size * last_x_span->waste;
      unsigned int bottom_size = first_x_span->size * last_y_span->waste;

      waste_buf = g_malloc (MAX (right_size, bottom_size) * bpp);
    }

  return waste_buf;
}

static CoglBool
_cogl_texture_2d_sliced_set_waste (CoglTexture2DSliced *tex_2ds,
                                   CoglBitmap *source_bmp,
                                   CoglTexture2D *slice_tex,
                                   uint8_t *waste_buf,
                                   CoglSpan *x_span,
                                   CoglSpan *y_span,
                                   CoglSpanIter *x_iter,
                                   CoglSpanIter *y_iter,
                                   int src_x,
                                   int src_y,
                                   int dst_x,
                                   int dst_y,
                                   CoglError **error)
{
  CoglBool need_x, need_y;
  CoglContext *ctx = COGL_TEXTURE (tex_2ds)->context;

  /* If the x_span is sliced and the upload touches the
     rightmost pixels then fill the waste with copies of the
     pixels */
  need_x = x_span->waste > 0 &&
    x_iter->intersect_end - x_iter->pos >= x_span->size - x_span->waste;

  /* same for the bottom-most pixels */
  need_y = y_span->waste > 0 &&
    y_iter->intersect_end - y_iter->pos >= y_span->size - y_span->waste;

  if (need_x || need_y)
    {
      int bmp_rowstride = cogl_bitmap_get_rowstride (source_bmp);
      CoglPixelFormat source_format = cogl_bitmap_get_format (source_bmp);
      int bpp = _cogl_pixel_format_get_bytes_per_pixel (source_format);
      uint8_t *bmp_data;
      const uint8_t *src;
      uint8_t *dst;
      unsigned int wy, wx;
      CoglBitmap *waste_bmp;

      bmp_data = _cogl_bitmap_map (source_bmp, COGL_BUFFER_ACCESS_READ, 0, error);
      if (bmp_data == NULL)
        return FALSE;

      if (need_x)
        {
          src = (bmp_data + ((src_y + (int) y_iter->intersect_start - dst_y) *
                             bmp_rowstride) +
                 (src_x + (int)x_span->start + (int)x_span->size -
                  (int)x_span->waste - dst_x - 1) * bpp);

          dst = waste_buf;

          for (wy = 0;
               wy < y_iter->intersect_end - y_iter->intersect_start;
               wy++)
            {
              for (wx = 0; wx < x_span->waste; wx++)
                {
                  memcpy (dst, src, bpp);
                  dst += bpp;
                }
              src += bmp_rowstride;
            }

          waste_bmp = cogl_bitmap_new_for_data (ctx,
                                                x_span->waste,
                                                y_iter->intersect_end -
                                                y_iter->intersect_start,
                                                source_format,
                                                x_span->waste * bpp,
                                                waste_buf);

          if (!_cogl_texture_set_region_from_bitmap (COGL_TEXTURE (slice_tex),
                                                     0, /* src_x */
                                                     0, /* src_y */
                                                     x_span->waste, /* width */
                                                     /* height */
                                                     y_iter->intersect_end -
                                                     y_iter->intersect_start,
                                                     waste_bmp,
                                                     /* dst_x */
                                                     x_span->size - x_span->waste,
                                                     y_iter->intersect_start -
                                                     y_span->start, /* dst_y */
                                                     0, /* level */
                                                     error))
            {
              cogl_object_unref (waste_bmp);
              _cogl_bitmap_unmap (source_bmp);
              return FALSE;
            }

          cogl_object_unref (waste_bmp);
        }

      if (need_y)
        {
          unsigned int copy_width, intersect_width;

          src = (bmp_data + ((src_x + (int) x_iter->intersect_start - dst_x) *
                             bpp) +
                 (src_y + (int)y_span->start + (int)y_span->size -
                  (int)y_span->waste - dst_y - 1) * bmp_rowstride);

          dst = waste_buf;

          if (x_iter->intersect_end - x_iter->pos
              >= x_span->size - x_span->waste)
            copy_width = x_span->size + x_iter->pos - x_iter->intersect_start;
          else
            copy_width = x_iter->intersect_end - x_iter->intersect_start;

          intersect_width = x_iter->intersect_end - x_iter->intersect_start;

          for (wy = 0; wy < y_span->waste; wy++)
            {
              memcpy (dst, src, intersect_width * bpp);
              dst += intersect_width * bpp;

              for (wx = intersect_width; wx < copy_width; wx++)
                {
                  memcpy (dst, dst - bpp, bpp);
                  dst += bpp;
                }
            }

          waste_bmp = cogl_bitmap_new_for_data (ctx,
                                                copy_width,
                                                y_span->waste,
                                                source_format,
                                                copy_width * bpp,
                                                waste_buf);

          if (!_cogl_texture_set_region_from_bitmap (COGL_TEXTURE (slice_tex),
                                                     0, /* src_x */
                                                     0, /* src_y */
                                                     copy_width, /* width */
                                                     y_span->waste, /* height */
                                                     waste_bmp,
                                                     /* dst_x */
                                                     x_iter->intersect_start -
                                                     x_iter->pos,
                                                     /* dst_y */
                                                     y_span->size - y_span->waste,
                                                     0, /* level */
                                                     error))
            {
              cogl_object_unref (waste_bmp);
              _cogl_bitmap_unmap (source_bmp);
              return FALSE;
            }

          cogl_object_unref (waste_bmp);
        }

      _cogl_bitmap_unmap (source_bmp);
    }

  return TRUE;
}

static CoglBool
_cogl_texture_2d_sliced_upload_bitmap (CoglTexture2DSliced *tex_2ds,
                                       CoglBitmap *bmp,
                                       CoglError **error)
{
  CoglSpan *x_span;
  CoglSpan *y_span;
  CoglTexture2D *slice_tex;
  int x, y;
  uint8_t *waste_buf;
  CoglPixelFormat bmp_format;

  bmp_format = cogl_bitmap_get_format (bmp);

  waste_buf = _cogl_texture_2d_sliced_allocate_waste_buffer (tex_2ds,
                                                             bmp_format);

  /* Iterate vertical slices */
  for (y = 0; y < tex_2ds->slice_y_spans->len; ++y)
    {
      y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, y);

      /* Iterate horizontal slices */
      for (x = 0; x < tex_2ds->slice_x_spans->len; ++x)
        {
          int slice_num = y * tex_2ds->slice_x_spans->len + x;
          CoglSpanIter x_iter, y_iter;

          x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, x);

          /* Pick the gl texture object handle */
          slice_tex = g_array_index (tex_2ds->slice_textures,
                                     CoglTexture2D *, slice_num);

          if (!_cogl_texture_set_region_from_bitmap (COGL_TEXTURE (slice_tex),
                                                     x_span->start, /* src x */
                                                     y_span->start, /* src y */
                                                     x_span->size -
                                                     x_span->waste, /* width */
                                                     y_span->size -
                                                     y_span->waste, /* height */
                                                     bmp,
                                                     0, /* dst x */
                                                     0, /* dst y */
                                                     0, /* level */
                                                     error))
            {
              if (waste_buf)
                g_free (waste_buf);
              return FALSE;
            }

          /* Set up a fake iterator that covers the whole slice */
          x_iter.intersect_start = x_span->start;
          x_iter.intersect_end = (x_span->start +
                                  x_span->size -
                                  x_span->waste);
          x_iter.pos = x_span->start;

          y_iter.intersect_start = y_span->start;
          y_iter.intersect_end = (y_span->start +
                                  y_span->size -
                                  y_span->waste);
          y_iter.pos = y_span->start;

          if (!_cogl_texture_2d_sliced_set_waste (tex_2ds,
                                                  bmp,
                                                  slice_tex,
                                                  waste_buf,
                                                  x_span, y_span,
                                                  &x_iter, &y_iter,
                                                  0, /* src_x */
                                                  0, /* src_y */
                                                  0, /* dst_x */
                                                  0,
                                                  error)) /* dst_y */
            {
              if (waste_buf)
                g_free (waste_buf);
              return FALSE;
            }
        }
    }

  if (waste_buf)
    g_free (waste_buf);

  return TRUE;
}

static CoglBool
_cogl_texture_2d_sliced_upload_subregion (CoglTexture2DSliced *tex_2ds,
                                          int src_x,
                                          int src_y,
                                          int dst_x,
                                          int dst_y,
                                          int width,
                                          int height,
                                          CoglBitmap *source_bmp,
                                          CoglError **error)
{
  CoglTexture *tex = COGL_TEXTURE (tex_2ds);
  CoglSpan *x_span;
  CoglSpan *y_span;
  CoglSpanIter x_iter;
  CoglSpanIter y_iter;
  CoglTexture2D *slice_tex;
  int source_x = 0, source_y = 0;
  int inter_w = 0, inter_h = 0;
  int local_x = 0, local_y = 0;
  uint8_t *waste_buf;
  CoglPixelFormat source_format;

  source_format = cogl_bitmap_get_format (source_bmp);

  waste_buf =
    _cogl_texture_2d_sliced_allocate_waste_buffer (tex_2ds, source_format);

  /* Iterate vertical spans */
  for (source_y = src_y,
       _cogl_span_iter_begin (&y_iter,
                              (CoglSpan *)tex_2ds->slice_y_spans->data,
                              tex_2ds->slice_y_spans->len,
                              tex->height,
                              dst_y,
                              dst_y + height,
                              COGL_PIPELINE_WRAP_MODE_REPEAT);

       !_cogl_span_iter_end (&y_iter);

       _cogl_span_iter_next (&y_iter),
       source_y += inter_h )
    {
      y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan,
                               y_iter.index);

      /* Iterate horizontal spans */
      for (source_x = src_x,
           _cogl_span_iter_begin (&x_iter,
                                  (CoglSpan *)tex_2ds->slice_x_spans->data,
                                  tex_2ds->slice_x_spans->len,
                                  tex->width,
                                  dst_x,
                                  dst_x + width,
                                  COGL_PIPELINE_WRAP_MODE_REPEAT);

           !_cogl_span_iter_end (&x_iter);

           _cogl_span_iter_next (&x_iter),
           source_x += inter_w )
        {
          int slice_num;

          x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan,
                                   x_iter.index);

          /* Pick intersection width and height */
          inter_w =  (x_iter.intersect_end - x_iter.intersect_start);
          inter_h =  (y_iter.intersect_end - y_iter.intersect_start);

          /* Localize intersection top-left corner to slice*/
          local_x =  (x_iter.intersect_start - x_iter.pos);
          local_y =  (y_iter.intersect_start - y_iter.pos);

          slice_num = y_iter.index * tex_2ds->slice_x_spans->len + x_iter.index;

          /* Pick slice texture */
          slice_tex = g_array_index (tex_2ds->slice_textures,
                                     CoglTexture2D *, slice_num);

          if (!_cogl_texture_set_region_from_bitmap (COGL_TEXTURE (slice_tex),
                                                     source_x,
                                                     source_y,
                                                     inter_w, /* width */
                                                     inter_h, /* height */
                                                     source_bmp,
                                                     local_x, /* dst x */
                                                     local_y, /* dst y */
                                                     0, /* level */
                                                     error))
            {
              if (waste_buf)
                g_free (waste_buf);
              return FALSE;
            }

          if (!_cogl_texture_2d_sliced_set_waste (tex_2ds,
                                                  source_bmp,
                                                  slice_tex,
                                                  waste_buf,
                                                  x_span, y_span,
                                                  &x_iter, &y_iter,
                                                  src_x, src_y,
                                                  dst_x, dst_y,
                                                  error))
            {
              if (waste_buf)
                g_free (waste_buf);
              return FALSE;
            }
        }
    }

  if (waste_buf)
    g_free (waste_buf);

  return TRUE;
}

static int
_cogl_rect_slices_for_size (int     size_to_fill,
                            int     max_span_size,
                            int     max_waste,
                            GArray *out_spans)
{
  int       n_spans = 0;
  CoglSpan  span;

  /* Init first slice span */
  span.start = 0;
  span.size = max_span_size;
  span.waste = 0;

  /* Repeat until whole area covered */
  while (size_to_fill >= span.size)
    {
      /* Add another slice span of same size */
      if (out_spans)
        g_array_append_val (out_spans, span);
      span.start   += span.size;
      size_to_fill -= span.size;
      n_spans++;
    }

  /* Add one last smaller slice span */
  if (size_to_fill > 0)
    {
      span.size = size_to_fill;
      if (out_spans)
        g_array_append_val (out_spans, span);
      n_spans++;
    }

  return n_spans;
}

static int
_cogl_pot_slices_for_size (int    size_to_fill,
                           int    max_span_size,
                           int    max_waste,
                           GArray *out_spans)
{
  int      n_spans = 0;
  CoglSpan span;

  /* Init first slice span */
  span.start = 0;
  span.size = max_span_size;
  span.waste = 0;

  /* Fix invalid max_waste */
  if (max_waste < 0)
    max_waste = 0;

  while (TRUE)
    {
      /* Is the whole area covered? */
      if (size_to_fill > span.size)
        {
          /* Not yet - add a span of this size */
          if (out_spans)
            g_array_append_val (out_spans, span);

          span.start   += span.size;
          size_to_fill -= span.size;
          n_spans++;
        }
      else if (span.size - size_to_fill <= max_waste)
        {
          /* Yes and waste is small enough */
          /* Pick the next power of two up from size_to_fill. This can
             sometimes be less than the span.size that would be chosen
             otherwise */
          span.size = _cogl_util_next_p2 (size_to_fill);
          span.waste = span.size - size_to_fill;
          if (out_spans)
            g_array_append_val (out_spans, span);

          return ++n_spans;
        }
      else
        {
          /* Yes but waste is too large */
          while (span.size - size_to_fill > max_waste)
            {
              span.size /= 2;
              g_assert (span.size > 0);
            }
        }
    }

  /* Can't get here */
  return 0;
}

static void
_cogl_texture_2d_sliced_gl_flush_legacy_texobj_wrap_modes (CoglTexture *tex,
                                                           GLenum wrap_mode_s,
                                                           GLenum wrap_mode_t,
                                                           GLenum wrap_mode_p)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  int i;

  /* Pass the set wrap mode on to all of the child textures */
  for (i = 0; i < tex_2ds->slice_textures->len; i++)
    {
      CoglTexture2D *slice_tex = g_array_index (tex_2ds->slice_textures,
                                                CoglTexture2D *,
                                                i);

      _cogl_texture_gl_flush_legacy_texobj_wrap_modes (COGL_TEXTURE (slice_tex),
                                                       wrap_mode_s,
                                                       wrap_mode_t,
                                                       wrap_mode_p);
    }
}

static void
free_spans (CoglTexture2DSliced *tex_2ds)
{
  if (tex_2ds->slice_x_spans != NULL)
    {
      g_array_free (tex_2ds->slice_x_spans, TRUE);
      tex_2ds->slice_x_spans = NULL;
    }

  if (tex_2ds->slice_y_spans != NULL)
    {
      g_array_free (tex_2ds->slice_y_spans, TRUE);
      tex_2ds->slice_y_spans = NULL;
    }
}

static CoglBool
setup_spans (CoglContext *ctx,
             CoglTexture2DSliced *tex_2ds,
             int width,
             int height,
             int max_waste,
             CoglPixelFormat internal_format,
             CoglError **error)
{
  int max_width;
  int max_height;
  int n_x_slices;
  int n_y_slices;

  int   (*slices_for_size) (int, int, int, GArray*);

  /* Initialize size of largest slice according to supported features */
  if (cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_NPOT))
    {
      max_width = width;
      max_height = height;
      slices_for_size = _cogl_rect_slices_for_size;
    }
  else
    {
      max_width = _cogl_util_next_p2 (width);
      max_height = _cogl_util_next_p2 (height);
      slices_for_size = _cogl_pot_slices_for_size;
    }

  /* Negative number means no slicing forced by the user */
  if (max_waste <= -1)
    {
      CoglSpan span;

      /* Check if size supported else bail out */
      if (!ctx->driver_vtable->texture_2d_can_create (ctx,
                                                      max_width,
                                                      max_height,
                                                      internal_format))
        {
          _cogl_set_error (error,
                           COGL_TEXTURE_ERROR,
                           COGL_TEXTURE_ERROR_SIZE,
                           "Sliced texture size of %d x %d not possible "
                           "with max waste set to -1",
                           width,
                           height);
          return FALSE;
        }

      n_x_slices = 1;
      n_y_slices = 1;

      /* Init span arrays */
      tex_2ds->slice_x_spans = g_array_sized_new (FALSE, FALSE,
                                                  sizeof (CoglSpan),
                                                  1);

      tex_2ds->slice_y_spans = g_array_sized_new (FALSE, FALSE,
                                                  sizeof (CoglSpan),
                                                  1);

      /* Add a single span for width and height */
      span.start = 0;
      span.size = max_width;
      span.waste = max_width - width;
      g_array_append_val (tex_2ds->slice_x_spans, span);

      span.size = max_height;
      span.waste = max_height - height;
      g_array_append_val (tex_2ds->slice_y_spans, span);
    }
  else
    {
      /* Decrease the size of largest slice until supported by GL */
      while (!ctx->driver_vtable->texture_2d_can_create (ctx,
                                                         max_width,
                                                         max_height,
                                                         internal_format))
        {
          /* Alternate between width and height */
          if (max_width > max_height)
            max_width /= 2;
          else
            max_height /= 2;

          if (max_width == 0 || max_height == 0)
            {
              /* Maybe it would be ok to just g_warn_if_reached() for this
               * codepath */
              _cogl_set_error (error,
                               COGL_TEXTURE_ERROR,
                               COGL_TEXTURE_ERROR_SIZE,
                               "No suitable slice geometry found");
              free_spans (tex_2ds);
              return FALSE;
            }
        }

      /* Determine the slices required to cover the bitmap area */
      n_x_slices = slices_for_size (width,
                                    max_width, max_waste,
                                    NULL);

      n_y_slices = slices_for_size (height,
                                    max_height, max_waste,
                                    NULL);

      /* Init span arrays with reserved size */
      tex_2ds->slice_x_spans = g_array_sized_new (FALSE, FALSE,
                                                  sizeof (CoglSpan),
                                                  n_x_slices);

      tex_2ds->slice_y_spans = g_array_sized_new (FALSE, FALSE,
                                                  sizeof (CoglSpan),
                                                  n_y_slices);

      /* Fill span arrays with info */
      slices_for_size (width,
                       max_width, max_waste,
                       tex_2ds->slice_x_spans);

      slices_for_size (height,
                       max_height, max_waste,
                       tex_2ds->slice_y_spans);
    }

  return TRUE;
}

static void
free_slices (CoglTexture2DSliced *tex_2ds)
{
  if (tex_2ds->slice_textures != NULL)
    {
      int i;

      for (i = 0; i < tex_2ds->slice_textures->len; i++)
        {
          CoglTexture2D *slice_tex =
            g_array_index (tex_2ds->slice_textures, CoglTexture2D *, i);
          cogl_object_unref (slice_tex);
        }

      g_array_free (tex_2ds->slice_textures, TRUE);
    }

  free_spans (tex_2ds);
}

static CoglBool
allocate_slices (CoglTexture2DSliced *tex_2ds,
                 int width,
                 int height,
                 int max_waste,
                 CoglPixelFormat internal_format,
                 CoglError **error)
{
  CoglTexture *tex = COGL_TEXTURE (tex_2ds);
  CoglContext *ctx = tex->context;
  int n_x_slices;
  int n_y_slices;
  int n_slices;
  int x, y;
  CoglSpan *x_span;
  CoglSpan *y_span;

  tex_2ds->internal_format = internal_format;

  if (!setup_spans (ctx, tex_2ds,
                    width,
                    height,
                    max_waste,
                    internal_format,
                    error))
    {
      return FALSE;
    }

  n_x_slices = tex_2ds->slice_x_spans->len;
  n_y_slices = tex_2ds->slice_y_spans->len;
  n_slices = n_x_slices * n_y_slices;

  tex_2ds->slice_textures = g_array_sized_new (FALSE, FALSE,
                                               sizeof (CoglTexture2D *),
                                               n_slices);

  /* Allocate each slice */
  for (y = 0; y < n_y_slices; ++y)
    {
      y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, y);

      for (x = 0; x < n_x_slices; ++x)
        {
          CoglTexture *slice;

          x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, x);

          COGL_NOTE (SLICING, "CREATE SLICE (%d,%d)\tsize (%d,%d)",
                     x, y,
                     (int)(x_span->size - x_span->waste),
                     (int)(y_span->size - y_span->waste));

          slice = COGL_TEXTURE (
            cogl_texture_2d_new_with_size (ctx,
                                           x_span->size, y_span->size));

          _cogl_texture_copy_internal_format (tex, slice);

          g_array_append_val (tex_2ds->slice_textures, slice);
          if (!cogl_texture_allocate (slice, error))
            {
              free_slices (tex_2ds);
              return FALSE;
            }
        }
    }

  return TRUE;
}

static void
_cogl_texture_2d_sliced_free (CoglTexture2DSliced *tex_2ds)
{
  free_slices (tex_2ds);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (tex_2ds));
}

static CoglTexture2DSliced *
_cogl_texture_2d_sliced_create_base (CoglContext *ctx,
                                     int width,
                                     int height,
                                     int max_waste,
                                     CoglPixelFormat internal_format,
                                     CoglTextureLoader *loader)
{
  CoglTexture2DSliced *tex_2ds = g_new0 (CoglTexture2DSliced, 1);

  _cogl_texture_init (COGL_TEXTURE (tex_2ds), ctx, width, height,
                      internal_format, loader,
                      &cogl_texture_2d_sliced_vtable);

  tex_2ds->max_waste = max_waste;

  return _cogl_texture_2d_sliced_object_new (tex_2ds);
}

CoglTexture2DSliced *
cogl_texture_2d_sliced_new_with_size (CoglContext *ctx,
                                      int width,
                                      int height,
                                      int max_waste)
{
  CoglTextureLoader *loader = _cogl_texture_create_loader ();
  loader->src_type = COGL_TEXTURE_SOURCE_TYPE_SIZED;
  loader->src.sized.width = width;
  loader->src.sized.height = height;

  return _cogl_texture_2d_sliced_create_base (ctx,
                                              width,
                                              height,
                                              max_waste,
                                              COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                              loader);
}

CoglTexture2DSliced *
_cogl_texture_2d_sliced_new_from_bitmap (CoglBitmap *bmp,
                                         int max_waste,
                                         CoglBool can_convert_in_place)
{
  CoglTextureLoader *loader;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_bitmap (bmp), NULL);

  loader = _cogl_texture_create_loader ();
  loader->src_type = COGL_TEXTURE_SOURCE_TYPE_BITMAP;
  loader->src.bitmap.bitmap = cogl_object_ref (bmp);
  loader->src.bitmap.can_convert_in_place = can_convert_in_place;

  return _cogl_texture_2d_sliced_create_base (_cogl_bitmap_get_context (bmp),
                                              cogl_bitmap_get_width (bmp),
                                              cogl_bitmap_get_height (bmp),
                                              max_waste,
                                              cogl_bitmap_get_format (bmp),
                                              loader);
}

CoglTexture2DSliced *
cogl_texture_2d_sliced_new_from_bitmap (CoglBitmap *bmp,
                                        int max_waste)
{
  return _cogl_texture_2d_sliced_new_from_bitmap (bmp,
                                                  max_waste,
                                                  FALSE);
}

CoglTexture2DSliced *
_cogl_texture_2d_sliced_new_from_foreign (CoglContext *ctx,
                                          unsigned int gl_handle,
                                          unsigned int gl_target,
                                          int width,
                                          int height,
                                          int x_pot_waste,
                                          int y_pot_waste,
                                          CoglPixelFormat format)
{
  CoglTextureLoader *loader;

  /* NOTE: width, height and internal format are not queriable
   * in GLES, hence such a function prototype.
   */

  /* This should only be called when the texture target is 2D. If a
     rectangle texture is used then _cogl_texture_new_from_foreign
     will create a cogl_texture_rectangle instead */
  _COGL_RETURN_VAL_IF_FAIL (gl_target == GL_TEXTURE_2D, NULL);

  /* Assert it is a valid GL texture object */
  _COGL_RETURN_VAL_IF_FAIL (ctx->glIsTexture (gl_handle), FALSE);

  /* Validate width and height */
  _COGL_RETURN_VAL_IF_FAIL (width > 0 && height > 0, NULL);

  /* Validate pot waste */
  _COGL_RETURN_VAL_IF_FAIL (x_pot_waste >= 0 && x_pot_waste < width &&
                            y_pot_waste >= 0 && y_pot_waste < height,
                            NULL);

  loader = _cogl_texture_create_loader ();
  loader->src_type = COGL_TEXTURE_SOURCE_TYPE_GL_FOREIGN;
  loader->src.gl_foreign.gl_handle = gl_handle;
  loader->src.gl_foreign.width = width + x_pot_waste;
  loader->src.gl_foreign.height = height + y_pot_waste;
  loader->src.gl_foreign.format = format;

  return _cogl_texture_2d_sliced_create_base (ctx,
                                              width,
                                              height,
                                              0, /* max waste */
                                              format, loader);
}

CoglTexture2DSliced *
cogl_texture_2d_sliced_new_from_data (CoglContext *ctx,
                                      int width,
                                      int height,
                                      int max_waste,
                                      CoglPixelFormat format,
                                      int rowstride,
                                      const uint8_t *data,
                                      CoglError **error)
{
  CoglBitmap *bmp;
  CoglTexture2DSliced *tex_2ds;

  _COGL_RETURN_VAL_IF_FAIL (format != COGL_PIXEL_FORMAT_ANY, NULL);
  _COGL_RETURN_VAL_IF_FAIL (data != NULL, NULL);

  /* Rowstride from width if not given */
  if (rowstride == 0)
    rowstride = width * _cogl_pixel_format_get_bytes_per_pixel (format);

  /* Wrap the data into a bitmap */
  bmp = cogl_bitmap_new_for_data (ctx,
                                  width, height,
                                  format,
                                  rowstride,
                                  (uint8_t *) data);

  tex_2ds = cogl_texture_2d_sliced_new_from_bitmap (bmp, max_waste);

  cogl_object_unref (bmp);

  if (tex_2ds &&
      !cogl_texture_allocate (COGL_TEXTURE (tex_2ds), error))
    {
      cogl_object_unref (tex_2ds);
      return NULL;
    }

  return tex_2ds;
}

CoglTexture2DSliced *
cogl_texture_2d_sliced_new_from_file (CoglContext *ctx,
                                      const char *filename,
                                      int max_waste,
                                      CoglError **error)
{
  CoglBitmap *bmp;
  CoglTexture2DSliced *tex_2ds = NULL;

  _COGL_RETURN_VAL_IF_FAIL (error == NULL || *error == NULL, NULL);

  bmp = _cogl_bitmap_from_file (ctx, filename, error);
  if (bmp == NULL)
    return NULL;

  tex_2ds = _cogl_texture_2d_sliced_new_from_bitmap (bmp,
                                           max_waste,
                                           TRUE); /* can convert in-place */

  cogl_object_unref (bmp);

  return tex_2ds;
}

static CoglBool
allocate_with_size (CoglTexture2DSliced *tex_2ds,
                    CoglTextureLoader *loader,
                    CoglError **error)
{
  CoglTexture *tex = COGL_TEXTURE (tex_2ds);
  CoglPixelFormat internal_format =
    _cogl_texture_determine_internal_format (tex, COGL_PIXEL_FORMAT_ANY);

  if (allocate_slices (tex_2ds,
                       loader->src.sized.width,
                       loader->src.sized.height,
                       tex_2ds->max_waste,
                       internal_format,
                       error))
    {
      _cogl_texture_set_allocated (COGL_TEXTURE (tex_2ds),
                                   internal_format,
                                   loader->src.sized.width,
                                   loader->src.sized.height);
      return TRUE;
    }
  else
    return FALSE;
}

static CoglBool
allocate_from_bitmap (CoglTexture2DSliced *tex_2ds,
                      CoglTextureLoader *loader,
                      CoglError **error)
{
  CoglTexture *tex = COGL_TEXTURE (tex_2ds);
  CoglBitmap *bmp = loader->src.bitmap.bitmap;
  int width = cogl_bitmap_get_width (bmp);
  int height = cogl_bitmap_get_height (bmp);
  CoglBool can_convert_in_place = loader->src.bitmap.can_convert_in_place;
  CoglPixelFormat internal_format;
  CoglBitmap *upload_bmp;

  _COGL_RETURN_VAL_IF_FAIL (tex_2ds->slice_textures == NULL, FALSE);

  internal_format =
    _cogl_texture_determine_internal_format (tex,
                                             cogl_bitmap_get_format (bmp));

  upload_bmp = _cogl_bitmap_convert_for_upload (bmp,
                                                internal_format,
                                                can_convert_in_place,
                                                error);
  if (upload_bmp == NULL)
    return FALSE;

  if (!allocate_slices (tex_2ds,
                        width,
                        height,
                        tex_2ds->max_waste,
                        internal_format,
                        error))
    {
      cogl_object_unref (upload_bmp);
      return FALSE;
    }

  if (!_cogl_texture_2d_sliced_upload_bitmap (tex_2ds,
                                              upload_bmp,
                                              error))
    {
      free_slices (tex_2ds);
      cogl_object_unref (upload_bmp);
      return FALSE;
    }

  cogl_object_unref (upload_bmp);

  _cogl_texture_set_allocated (tex, internal_format, width, height);

  return TRUE;
}

static CoglBool
allocate_from_gl_foreign (CoglTexture2DSliced *tex_2ds,
                          CoglTextureLoader *loader,
                          CoglError **error)
{
  CoglTexture *tex = COGL_TEXTURE (tex_2ds);
  CoglContext *ctx = tex->context;
  CoglPixelFormat format = loader->src.gl_foreign.format;
  int gl_width = loader->src.gl_foreign.width;
  int gl_height = loader->src.gl_foreign.height;
  int x_pot_waste = gl_width - tex->width;
  int y_pot_waste = gl_height - tex->height;
  CoglSpan x_span;
  CoglSpan y_span;
  CoglTexture2D *tex_2d =
    cogl_texture_2d_gl_new_from_foreign (ctx,
                                         loader->src.gl_foreign.gl_handle,
                                         gl_width,
                                         gl_height,
                                         format);

  if (!cogl_texture_allocate (COGL_TEXTURE (tex_2d), error))
    {
      cogl_object_unref (tex_2d);
      return FALSE;
    }

  /* The texture 2d backend may use a different pixel format if it
     queries the actual texture so we'll refetch the format it
     actually used */
  format = _cogl_texture_get_format (tex);

  tex_2ds->internal_format = format;

  /* Create slice arrays */
  tex_2ds->slice_x_spans =
    g_array_sized_new (FALSE, FALSE, sizeof (CoglSpan), 1);

  tex_2ds->slice_y_spans =
    g_array_sized_new (FALSE, FALSE, sizeof (CoglSpan), 1);

  tex_2ds->slice_textures =
    g_array_sized_new (FALSE, FALSE, sizeof (CoglTexture2D *), 1);

  /* Store info for a single slice */
  x_span.start = 0;
  x_span.size = gl_width;
  x_span.waste = x_pot_waste;
  g_array_append_val (tex_2ds->slice_x_spans, x_span);

  y_span.start = 0;
  y_span.size = gl_height;
  y_span.waste = y_pot_waste;
  g_array_append_val (tex_2ds->slice_y_spans, y_span);

  g_array_append_val (tex_2ds->slice_textures, tex_2d);

  _cogl_texture_set_allocated (tex,
                               format,
                               tex->width,
                               tex->height);

  return TRUE;
}

static CoglBool
_cogl_texture_2d_sliced_allocate (CoglTexture *tex,
                                  CoglError **error)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglTextureLoader *loader = tex->loader;

  _COGL_RETURN_VAL_IF_FAIL (loader, FALSE);

  switch (loader->src_type)
    {
    case COGL_TEXTURE_SOURCE_TYPE_SIZED:
      return allocate_with_size (tex_2ds, loader, error);
    case COGL_TEXTURE_SOURCE_TYPE_BITMAP:
      return allocate_from_bitmap (tex_2ds, loader, error);
    case COGL_TEXTURE_SOURCE_TYPE_GL_FOREIGN:
      return allocate_from_gl_foreign (tex_2ds, loader, error);
    default:
      break;
    }

  g_return_val_if_reached (FALSE);
}

static CoglBool
_cogl_texture_2d_sliced_is_foreign (CoglTexture *tex)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglTexture2D *slice_tex;

  /* Make sure slices were created */
  if (tex_2ds->slice_textures == NULL)
    return FALSE;

  /* Pass the call on to the first slice */
  slice_tex = g_array_index (tex_2ds->slice_textures, CoglTexture2D *, 0);
  return _cogl_texture_is_foreign (COGL_TEXTURE (slice_tex));
}

static int
_cogl_texture_2d_sliced_get_max_waste (CoglTexture *tex)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);

  return tex_2ds->max_waste;
}

static CoglBool
_cogl_texture_2d_sliced_is_sliced (CoglTexture *tex)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);

  /* It's only after allocating a sliced texture that we will know
   * whether it really needed to be sliced... */
  if (!tex->allocated)
    cogl_texture_allocate (tex, NULL);

  if (tex_2ds->slice_x_spans->len != 1 ||
      tex_2ds->slice_y_spans->len != 1)
    return TRUE;
  else
    return FALSE;
}

static CoglBool
_cogl_texture_2d_sliced_can_hardware_repeat (CoglTexture *tex)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglTexture2D *slice_tex;
  CoglSpan *x_span;
  CoglSpan *y_span;

  /* If there's more than one texture then we can't hardware repeat */
  if (tex_2ds->slice_textures->len != 1)
    return FALSE;

  /* If there's any waste then we can't hardware repeat */
  x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, 0);
  y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, 0);
  if (x_span->waste > 0 || y_span->waste > 0)
    return FALSE;

  /* Otherwise pass the query on to the single slice texture */
  slice_tex = g_array_index (tex_2ds->slice_textures, CoglTexture2D *, 0);
  return _cogl_texture_can_hardware_repeat (COGL_TEXTURE (slice_tex));
}

static void
_cogl_texture_2d_sliced_transform_coords_to_gl (CoglTexture *tex,
                                                float *s,
                                                float *t)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglSpan *x_span;
  CoglSpan *y_span;
  CoglTexture2D *slice_tex;

  g_assert (!_cogl_texture_2d_sliced_is_sliced (tex));

  /* Don't include the waste in the texture coordinates */
  x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, 0);
  y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, 0);

  *s *= tex->width / (float)x_span->size;
  *t *= tex->height / (float)y_span->size;

  /* Let the child texture further transform the coords */
  slice_tex = g_array_index (tex_2ds->slice_textures, CoglTexture2D *, 0);
  _cogl_texture_transform_coords_to_gl (COGL_TEXTURE (slice_tex), s, t);
}

static CoglTransformResult
_cogl_texture_2d_sliced_transform_quad_coords_to_gl (CoglTexture *tex,
                                                     float *coords)
{
  CoglBool need_repeat = FALSE;
  int i;

  /* This is a bit lazy - in the case where the quad lies entirely
   * within a single slice we could avoid the fallback. But that
   * could likely lead to visual inconsistency if the fallback involves
   * dropping layers, so this might be the right thing to do anyways.
   */
  if (_cogl_texture_2d_sliced_is_sliced (tex))
    return COGL_TRANSFORM_SOFTWARE_REPEAT;

  for (i = 0; i < 4; i++)
    if (coords[i] < 0.0f || coords[i] > 1.0f)
      need_repeat = TRUE;

  if (need_repeat && !_cogl_texture_2d_sliced_can_hardware_repeat (tex))
    return COGL_TRANSFORM_SOFTWARE_REPEAT;

  _cogl_texture_2d_sliced_transform_coords_to_gl (tex, coords + 0, coords + 1);
  _cogl_texture_2d_sliced_transform_coords_to_gl (tex, coords + 2, coords + 3);

  return (need_repeat
          ? COGL_TRANSFORM_HARDWARE_REPEAT : COGL_TRANSFORM_NO_REPEAT);
}

static CoglBool
_cogl_texture_2d_sliced_get_gl_texture (CoglTexture *tex,
                                        GLuint *out_gl_handle,
                                        GLenum *out_gl_target)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglTexture2D *slice_tex;

  if (tex_2ds->slice_textures == NULL)
    return FALSE;

  if (tex_2ds->slice_textures->len < 1)
    return FALSE;

  slice_tex = g_array_index (tex_2ds->slice_textures, CoglTexture2D *, 0);

  return cogl_texture_get_gl_texture (COGL_TEXTURE (slice_tex),
                                      out_gl_handle, out_gl_target);
}

static void
_cogl_texture_2d_sliced_gl_flush_legacy_texobj_filters (CoglTexture *tex,
                                                        GLenum min_filter,
                                                        GLenum mag_filter)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglTexture2D *slice_tex;
  int i;

  _COGL_RETURN_IF_FAIL (tex_2ds->slice_textures != NULL);

  /* Apply new filters to every slice. The slice texture itself should
     cache the value and avoid resubmitting the same filter value to
     GL */
  for (i = 0; i < tex_2ds->slice_textures->len; i++)
    {
      slice_tex = g_array_index (tex_2ds->slice_textures, CoglTexture2D *, i);
      _cogl_texture_gl_flush_legacy_texobj_filters (COGL_TEXTURE (slice_tex),
                                                    min_filter, mag_filter);
    }
}

static void
_cogl_texture_2d_sliced_pre_paint (CoglTexture *tex,
                                   CoglTexturePrePaintFlags flags)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  int i;

  _COGL_RETURN_IF_FAIL (tex_2ds->slice_textures != NULL);

  /* Pass the pre-paint on to every slice */
  for (i = 0; i < tex_2ds->slice_textures->len; i++)
    {
      CoglTexture2D *slice_tex = g_array_index (tex_2ds->slice_textures,
                                                CoglTexture2D *, i);
      _cogl_texture_pre_paint (COGL_TEXTURE (slice_tex), flags);
    }
}

static void
_cogl_texture_2d_sliced_ensure_non_quad_rendering (CoglTexture *tex)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  int i;

  _COGL_RETURN_IF_FAIL (tex_2ds->slice_textures != NULL);

  /* Pass the call on to every slice */
  for (i = 0; i < tex_2ds->slice_textures->len; i++)
    {
      CoglTexture2D *slice_tex = g_array_index (tex_2ds->slice_textures,
                                                CoglTexture2D *, i);
      _cogl_texture_ensure_non_quad_rendering (COGL_TEXTURE (slice_tex));
    }
}

static CoglBool
_cogl_texture_2d_sliced_set_region (CoglTexture *tex,
                                    int src_x,
                                    int src_y,
                                    int dst_x,
                                    int dst_y,
                                    int dst_width,
                                    int dst_height,
                                    int level,
                                    CoglBitmap *bmp,
                                    CoglError **error)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglBitmap *upload_bmp;
  CoglBool status;

  upload_bmp = _cogl_bitmap_convert_for_upload (bmp,
                                                _cogl_texture_get_format (tex),
                                                FALSE, /* can't convert in
                                                          place */
                                                error);
  if (!upload_bmp)
    return FALSE;

  status = _cogl_texture_2d_sliced_upload_subregion (tex_2ds,
                                                     src_x, src_y,
                                                     dst_x, dst_y,
                                                     dst_width, dst_height,
                                                     upload_bmp,
                                                     error);
  cogl_object_unref (upload_bmp);

  return status;
}

static CoglPixelFormat
_cogl_texture_2d_sliced_get_format (CoglTexture *tex)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);

  return tex_2ds->internal_format;
}

static GLenum
_cogl_texture_2d_sliced_get_gl_format (CoglTexture *tex)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglTexture2D *slice_tex;

  /* Assert that we've allocated our slices at this point */
  cogl_texture_allocate (tex, NULL); /* (abort on error) */

  /* Pass the call on to the first slice */
  slice_tex = g_array_index (tex_2ds->slice_textures, CoglTexture2D *, 0);
  return _cogl_texture_gl_get_format (COGL_TEXTURE (slice_tex));
}

static CoglTextureType
_cogl_texture_2d_sliced_get_type (CoglTexture *tex)
{
  return COGL_TEXTURE_TYPE_2D;
}

static const CoglTextureVtable
cogl_texture_2d_sliced_vtable =
  {
    FALSE, /* not primitive */
    _cogl_texture_2d_sliced_allocate,
    _cogl_texture_2d_sliced_set_region,
    NULL, /* get_data */
    _cogl_texture_2d_sliced_foreach_sub_texture_in_region,
    _cogl_texture_2d_sliced_get_max_waste,
    _cogl_texture_2d_sliced_is_sliced,
    _cogl_texture_2d_sliced_can_hardware_repeat,
    _cogl_texture_2d_sliced_transform_coords_to_gl,
    _cogl_texture_2d_sliced_transform_quad_coords_to_gl,
    _cogl_texture_2d_sliced_get_gl_texture,
    _cogl_texture_2d_sliced_gl_flush_legacy_texobj_filters,
    _cogl_texture_2d_sliced_pre_paint,
    _cogl_texture_2d_sliced_ensure_non_quad_rendering,
    _cogl_texture_2d_sliced_gl_flush_legacy_texobj_wrap_modes,
    _cogl_texture_2d_sliced_get_format,
    _cogl_texture_2d_sliced_get_gl_format,
    _cogl_texture_2d_sliced_get_type,
    _cogl_texture_2d_sliced_is_foreign,
    NULL /* set_auto_mipmap */
  };
