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
#include "cogl-texture-2d-private.h"
#include "cogl-texture-2d-sliced-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-spans.h"
#include "cogl-journal-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-primitive-texture.h"

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
  float re_normalized_coords[4] =
    {
      sub_texture_coords[0] * data->x_normalize_factor,
      sub_texture_coords[1] * data->y_normalize_factor,
      sub_texture_coords[2] * data->x_normalize_factor,
      sub_texture_coords[3] * data->y_normalize_factor
    };

  data->callback (sub_texture, re_normalized_coords, meta_coords,
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
  data.x_normalize_factor = 1.0f / tex_2ds->width;
  data.y_normalize_factor = 1.0f / tex_2ds->height;

  un_normalized_coords[0] = virtual_tx_1 * data.x_normalize_factor;
  un_normalized_coords[1] = virtual_ty_1 * data.y_normalize_factor;
  un_normalized_coords[2] = virtual_tx_2 * data.x_normalize_factor;
  un_normalized_coords[3] = virtual_ty_2 * data.y_normalize_factor;

  _cogl_texture_spans_foreach_in_region (x_spans,
                                         tex_2ds->slice_x_spans->len,
                                         y_spans,
                                         tex_2ds->slice_y_spans->len,
                                         textures,
                                         un_normalized_coords,
                                         1, /* x_normalize_factor */
                                         1, /* y_normalize_factor */
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

static void
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
                                   int dst_y)
{
  CoglBool need_x, need_y;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

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

      bmp_data = _cogl_bitmap_map (source_bmp, COGL_BUFFER_ACCESS_READ, 0);

      if (bmp_data == NULL)
        return;

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

          cogl_texture_set_region_from_bitmap (COGL_TEXTURE (slice_tex),
                                               0, /* src_x */
                                               0, /* src_y */
                                               /* dst_x */
                                               x_span->size - x_span->waste,
                                               y_iter->intersect_start -
                                               y_span->start, /* dst_y */
                                               x_span->waste, /* dst_width */
                                               /* dst_height */
                                               y_iter->intersect_end -
                                               y_iter->intersect_start,
                                               waste_bmp);

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

          cogl_texture_set_region_from_bitmap (COGL_TEXTURE (slice_tex),
                                               0, /* src_x */
                                               0, /* src_y */
                                               /* dst_x */
                                               x_iter->intersect_start -
                                               x_iter->pos,
                                               /* dst_y */
                                               y_span->size - y_span->waste,
                                               copy_width, /* dst_width */
                                               y_span->waste, /* dst_height */
                                               waste_bmp);

          cogl_object_unref (waste_bmp);
        }

      _cogl_bitmap_unmap (source_bmp);
    }
}

static CoglBool
_cogl_texture_2d_sliced_upload_to_gl (CoglTexture2DSliced *tex_2ds,
                                      CoglBitmap          *bmp)
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

          cogl_texture_set_region_from_bitmap (COGL_TEXTURE (slice_tex),
                                               x_span->start, /* src x */
                                               y_span->start, /* src y */
                                               0, /* dst x */
                                               0, /* dst y */
                                               x_span->size -
                                               x_span->waste, /* width */
                                               y_span->size -
                                               y_span->waste, /* height */
                                               bmp);

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

          _cogl_texture_2d_sliced_set_waste (tex_2ds,
                                             bmp,
                                             slice_tex,
                                             waste_buf,
                                             x_span, y_span,
                                             &x_iter, &y_iter,
                                             0, /* src_x */
                                             0, /* src_y */
                                             0, /* dst_x */
                                             0); /* dst_y */
        }
    }

  if (waste_buf)
    g_free (waste_buf);

  return TRUE;
}

static CoglBool
_cogl_texture_2d_sliced_upload_subregion_to_gl (CoglTexture2DSliced *tex_2ds,
                                                int          src_x,
                                                int          src_y,
                                                int          dst_x,
                                                int          dst_y,
                                                int          width,
                                                int          height,
                                                CoglBitmap  *source_bmp,
                                                GLuint       source_gl_format,
                                                GLuint       source_gl_type)
{
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
                              tex_2ds->height,
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
                                  tex_2ds->width,
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

          cogl_texture_set_region_from_bitmap (COGL_TEXTURE (slice_tex),
                                               source_x,
                                               source_y,
                                               local_x, /* dst x */
                                               local_y, /* dst y */
                                               inter_w, /* width */
                                               inter_h, /* height */
                                               source_bmp);

          _cogl_texture_2d_sliced_set_waste (tex_2ds,
                                             source_bmp,
                                             slice_tex,
                                             waste_buf,
                                             x_span, y_span,
                                             &x_iter, &y_iter,
                                             src_x, src_y,
                                             dst_x, dst_y);
        }
    }

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
_cogl_texture_2d_sliced_set_wrap_mode_parameters (CoglTexture *tex,
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

      _cogl_texture_set_wrap_mode_parameters (COGL_TEXTURE (slice_tex),
                                              wrap_mode_s,
                                              wrap_mode_t,
                                              wrap_mode_p);
    }
}

static CoglBool
_cogl_texture_2d_sliced_slices_create (CoglContext *ctx,
                                       CoglTexture2DSliced *tex_2ds,
                                       int width, int height,
                                       CoglPixelFormat format)
{
  int max_width;
  int max_height;
  CoglTexture2D **slice_textures;
  int n_x_slices;
  int n_y_slices;
  int n_slices;
  int x, y;
  CoglSpan *x_span;
  CoglSpan *y_span;
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;

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

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          format,
                                          &gl_intformat,
                                          &gl_format,
                                          &gl_type);

  /* Negative number means no slicing forced by the user */
  if (tex_2ds->max_waste <= -1)
    {
      CoglSpan span;

      /* Check if size supported else bail out */
      if (!ctx->texture_driver->size_supported (ctx,
                                                GL_TEXTURE_2D,
                                                gl_intformat,
                                                gl_format,
                                                gl_type,
                                                max_width,
                                                max_height))
        {
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
      while (!ctx->texture_driver->size_supported (ctx,
                                                   GL_TEXTURE_2D,
                                                   gl_intformat,
                                                   gl_format,
                                                   gl_type,
                                                   max_width,
                                                   max_height))
        {
          /* Alternate between width and height */
          if (max_width > max_height)
            max_width /= 2;
          else
            max_height /= 2;

          if (max_width == 0 || max_height == 0)
            return FALSE;
        }

      /* Determine the slices required to cover the bitmap area */
      n_x_slices = slices_for_size (width,
                                    max_width, tex_2ds->max_waste,
                                    NULL);

      n_y_slices = slices_for_size (height,
                                    max_height, tex_2ds->max_waste,
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
                       max_width, tex_2ds->max_waste,
                       tex_2ds->slice_x_spans);

      slices_for_size (height,
                       max_height, tex_2ds->max_waste,
                       tex_2ds->slice_y_spans);
    }

  /* Init and resize GL handle array */
  n_slices = n_x_slices * n_y_slices;

  tex_2ds->slice_textures = g_array_sized_new (FALSE, FALSE,
                                               sizeof (CoglTexture2D *),
                                               n_slices);

  g_array_set_size (tex_2ds->slice_textures, n_slices);

  slice_textures = (CoglTexture2D **) tex_2ds->slice_textures->data;

  /* Init each GL texture object */
  for (y = 0; y < n_y_slices; ++y)
    {
      y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, y);

      for (x = 0; x < n_x_slices; ++x)
        {
          GError *error = NULL;
          x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, x);

          COGL_NOTE (SLICING, "CREATE SLICE (%d,%d)\tsize (%d,%d)",
                     x, y,
                     (int)(x_span->size - x_span->waste),
                     (int)(y_span->size - y_span->waste));

          slice_textures[y * n_x_slices + x] =
            cogl_texture_2d_new_with_size (ctx, x_span->size, y_span->size,
                                           format, &error);
          if (!slice_textures[y * n_x_slices + x])
            {
              g_array_set_size (tex_2ds->slice_textures, y * n_x_slices + x);
              g_error_free (error);
              return FALSE;
            }
        }
    }

  return TRUE;
}

static void
_cogl_texture_2d_sliced_slices_free (CoglTexture2DSliced *tex_2ds)
{
  if (tex_2ds->slice_x_spans != NULL)
    g_array_free (tex_2ds->slice_x_spans, TRUE);

  if (tex_2ds->slice_y_spans != NULL)
    g_array_free (tex_2ds->slice_y_spans, TRUE);

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
}

static void
_cogl_texture_2d_sliced_free (CoglTexture2DSliced *tex_2ds)
{
  _cogl_texture_2d_sliced_slices_free (tex_2ds);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (tex_2ds));
}

static CoglBool
_cogl_texture_2d_sliced_init_base (CoglContext *ctx,
                                   CoglTexture2DSliced *tex_2ds,
                                   int width,
                                   int height,
                                   CoglPixelFormat internal_format)
{
  CoglTexture *tex = COGL_TEXTURE (tex_2ds);

  _cogl_texture_init (tex, &cogl_texture_2d_sliced_vtable);

  tex_2ds->slice_x_spans = NULL;
  tex_2ds->slice_y_spans = NULL;
  tex_2ds->slice_textures = NULL;

  /* Create slices for the given format and size */
  if (!_cogl_texture_2d_sliced_slices_create (ctx,
                                              tex_2ds,
                                              width,
                                              height,
                                              internal_format))
    return FALSE;

  tex_2ds->width = width;
  tex_2ds->height = height;

  return TRUE;
}

CoglTexture2DSliced *
cogl_texture_2d_sliced_new_with_size (CoglContext *ctx,
                                      unsigned int width,
                                      unsigned int height,
                                      int max_waste,
                                      CoglPixelFormat internal_format,
                                      GError **error)
{
  CoglTexture2DSliced   *tex_2ds;

  /* Since no data, we need some internal format */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    internal_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;

  /* Init texture with empty bitmap */
  tex_2ds = g_new (CoglTexture2DSliced, 1);

  tex_2ds->max_waste = max_waste;

  if (!_cogl_texture_2d_sliced_init_base (ctx,
                                          tex_2ds,
                                          width, height,
                                          internal_format))
    {
      _cogl_texture_2d_sliced_free (tex_2ds);
      g_set_error (error,
                   COGL_ERROR,
                   COGL_ERROR_NO_MEMORY,
                   "Not enough memory to allocate texture slices");
      return NULL;
    }

  return _cogl_texture_2d_sliced_object_new (tex_2ds);
}

CoglTexture2DSliced *
_cogl_texture_2d_sliced_new_from_bitmap (CoglBitmap      *bmp,
                                         CoglTextureFlags flags,
                                         CoglPixelFormat  internal_format)
{
  CoglTexture2DSliced *tex_2ds;
  CoglBitmap          *dst_bmp;
  GLenum               gl_intformat;
  GLenum               gl_format;
  GLenum               gl_type;
  int                  width, height;
  CoglContext         *ctx;
  int                  i;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_bitmap (bmp), NULL);

  ctx = _cogl_bitmap_get_context (bmp);

  width = cogl_bitmap_get_width (bmp);
  height = cogl_bitmap_get_height (bmp);

  /* Create new texture and fill with loaded data */
  tex_2ds = g_new0 (CoglTexture2DSliced, 1);

  if (flags & COGL_TEXTURE_NO_SLICING)
    tex_2ds->max_waste = -1;
  else
    tex_2ds->max_waste = COGL_TEXTURE_MAX_WASTE;

  dst_bmp = _cogl_texture_prepare_for_upload (bmp,
                                              internal_format,
                                              &internal_format,
                                              &gl_intformat,
                                              &gl_format,
                                              &gl_type);
  if (dst_bmp == NULL)
    {
      _cogl_texture_2d_sliced_free (tex_2ds);
      return NULL;
    }

  if (!_cogl_texture_2d_sliced_init_base (ctx,
                                          tex_2ds,
                                          width, height,
                                          internal_format))
    goto error;

  if (!_cogl_texture_2d_sliced_upload_to_gl (tex_2ds,
                                             dst_bmp))
    goto error;

  cogl_object_unref (dst_bmp);

  if ((flags & COGL_TEXTURE_NO_AUTO_MIPMAP))
    for (i = 0; i < tex_2ds->slice_textures->len; i++)
      {
        CoglPrimitiveTexture *slice_tex;

        slice_tex = g_array_index (tex_2ds->slice_textures,
                                   CoglPrimitiveTexture *,
                                   i);

        cogl_primitive_texture_set_auto_mipmap (slice_tex, FALSE);
      }

  return _cogl_texture_2d_sliced_object_new (tex_2ds);

 error:
  cogl_object_unref (dst_bmp);
  _cogl_texture_2d_sliced_free (tex_2ds);
  return NULL;
}

CoglTexture2DSliced *
_cogl_texture_2d_sliced_new_from_foreign (GLuint           gl_handle,
                                          GLenum           gl_target,
                                          GLuint           width,
                                          GLuint           height,
                                          GLuint           x_pot_waste,
                                          GLuint           y_pot_waste,
                                          CoglPixelFormat  format)
{
  /* NOTE: width, height and internal format are not queriable
   * in GLES, hence such a function prototype.
   */

  GLint                gl_width = 0;
  GLint                gl_height = 0;
  CoglTexture2DSliced *tex_2ds;
  CoglTexture         *tex;
  CoglSpan             x_span;
  CoglSpan             y_span;
  CoglTexture2D       *tex_2d;

  _COGL_GET_CONTEXT (ctx, NULL);

  /* This should only be called when the texture target is 2D. If a
     rectangle texture is used then _cogl_texture_new_from_foreign
     will create a cogl_texture_rectangle instead */
  g_assert (gl_target == GL_TEXTURE_2D);

  gl_width = width + x_pot_waste;
  gl_height = height + y_pot_waste;

  /* Validate pot waste */
  if (x_pot_waste < 0 || x_pot_waste >= width ||
      y_pot_waste < 0 || y_pot_waste >= height)
    return NULL;

  tex_2d = cogl_texture_2d_new_from_foreign (ctx,
                                             gl_target,
                                             gl_width,
                                             gl_height,
                                             format,
                                             NULL);

  if (!tex_2d)
    return NULL;

  /* The texture 2d backend may use a different pixel format if it
     queries the actual texture so we'll refetch the format it
     actually used */
  format = cogl_texture_get_format (COGL_TEXTURE (tex_2d));

  /* Create new texture */
  tex_2ds = g_new0 (CoglTexture2DSliced, 1);

  tex = COGL_TEXTURE (tex_2ds);
  tex->vtable = &cogl_texture_2d_sliced_vtable;

  tex_2ds->width = gl_width - x_pot_waste;
  tex_2ds->height = gl_height - y_pot_waste;
  tex_2ds->max_waste = 0;

  /* Create slice arrays */
  tex_2ds->slice_x_spans =
    g_array_sized_new (FALSE, FALSE,
                       sizeof (CoglSpan), 1);

  tex_2ds->slice_y_spans =
    g_array_sized_new (FALSE, FALSE,
                       sizeof (CoglSpan), 1);

  tex_2ds->slice_textures =
    g_array_sized_new (FALSE, FALSE,
                       sizeof (CoglTexture2D *), 1);

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

  return _cogl_texture_2d_sliced_object_new (tex_2ds);
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

  if (tex_2ds->slice_textures == NULL)
    return FALSE;

  if (tex_2ds->slice_textures->len <= 1)
    return FALSE;

  return TRUE;
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

  *s *= tex_2ds->width / (float)x_span->size;
  *t *= tex_2ds->height / (float)y_span->size;

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
_cogl_texture_2d_sliced_set_filters (CoglTexture *tex,
                                     GLenum min_filter,
                                     GLenum mag_filter)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglTexture2D       *slice_tex;
  int                  i;

  /* Make sure slices were created */
  if (tex_2ds->slice_textures == NULL)
    return;

  /* Apply new filters to every slice. The slice texture itself should
     cache the value and avoid resubmitting the same filter value to
     GL */
  for (i = 0; i < tex_2ds->slice_textures->len; i++)
    {
      slice_tex = g_array_index (tex_2ds->slice_textures, CoglTexture2D *, i);
      _cogl_texture_set_filters (COGL_TEXTURE (slice_tex),
                                 min_filter, mag_filter);
    }
}

static void
_cogl_texture_2d_sliced_pre_paint (CoglTexture *tex,
                                   CoglTexturePrePaintFlags flags)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  int                  i;

  /* Make sure slices were created */
  if (tex_2ds->slice_textures == NULL)
    return;

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

  /* Make sure slices were created */
  if (tex_2ds->slice_textures == NULL)
    return;

  /* Pass the call on to every slice */
  for (i = 0; i < tex_2ds->slice_textures->len; i++)
    {
      CoglTexture2D *slice_tex = g_array_index (tex_2ds->slice_textures,
                                                CoglTexture2D *, i);
      _cogl_texture_ensure_non_quad_rendering (COGL_TEXTURE (slice_tex));
    }
}

static CoglBool
_cogl_texture_2d_sliced_set_region (CoglTexture    *tex,
                                    int             src_x,
                                    int             src_y,
                                    int             dst_x,
                                    int             dst_y,
                                    unsigned int    dst_width,
                                    unsigned int    dst_height,
                                    CoglBitmap     *bmp)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  GLenum               gl_format;
  GLenum               gl_type;

  _COGL_GET_CONTEXT (ctx, FALSE);

  bmp = _cogl_texture_prepare_for_upload (bmp,
                                          cogl_texture_get_format (tex),
                                          NULL,
                                          NULL,
                                          &gl_format,
                                          &gl_type);

  /* Send data to GL */
  _cogl_texture_2d_sliced_upload_subregion_to_gl (tex_2ds,
                                                  src_x, src_y,
                                                  dst_x, dst_y,
                                                  dst_width, dst_height,
                                                  bmp,
                                                  gl_format,
                                                  gl_type);

  cogl_object_unref (bmp);

  return TRUE;
}

static CoglPixelFormat
_cogl_texture_2d_sliced_get_format (CoglTexture *tex)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglTexture2D *slice_tex;

  /* Make sure slices were created */
  if (tex_2ds->slice_textures == NULL)
    return 0;

  /* Pass the call on to the first slice */
  slice_tex = g_array_index (tex_2ds->slice_textures, CoglTexture2D *, 0);
  return cogl_texture_get_format (COGL_TEXTURE (slice_tex));
}

static GLenum
_cogl_texture_2d_sliced_get_gl_format (CoglTexture *tex)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
  CoglTexture2D *slice_tex;

  /* Make sure slices were created */
  if (tex_2ds->slice_textures == NULL)
    return 0;

  /* Pass the call on to the first slice */
  slice_tex = g_array_index (tex_2ds->slice_textures, CoglTexture2D *, 0);
  return _cogl_texture_get_gl_format (COGL_TEXTURE (slice_tex));
}

static int
_cogl_texture_2d_sliced_get_width (CoglTexture *tex)
{
  return COGL_TEXTURE_2D_SLICED (tex)->width;
}

static int
_cogl_texture_2d_sliced_get_height (CoglTexture *tex)
{
  return COGL_TEXTURE_2D_SLICED (tex)->height;
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
    _cogl_texture_2d_sliced_set_region,
    NULL, /* get_data */
    _cogl_texture_2d_sliced_foreach_sub_texture_in_region,
    _cogl_texture_2d_sliced_get_max_waste,
    _cogl_texture_2d_sliced_is_sliced,
    _cogl_texture_2d_sliced_can_hardware_repeat,
    _cogl_texture_2d_sliced_transform_coords_to_gl,
    _cogl_texture_2d_sliced_transform_quad_coords_to_gl,
    _cogl_texture_2d_sliced_get_gl_texture,
    _cogl_texture_2d_sliced_set_filters,
    _cogl_texture_2d_sliced_pre_paint,
    _cogl_texture_2d_sliced_ensure_non_quad_rendering,
    _cogl_texture_2d_sliced_set_wrap_mode_parameters,
    _cogl_texture_2d_sliced_get_format,
    _cogl_texture_2d_sliced_get_gl_format,
    _cogl_texture_2d_sliced_get_width,
    _cogl_texture_2d_sliced_get_height,
    _cogl_texture_2d_sliced_get_type,
    _cogl_texture_2d_sliced_is_foreign,
    NULL /* set_auto_mipmap */
  };
