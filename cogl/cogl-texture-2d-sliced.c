/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *  Matthew Allum  <mallum@openedhand.com>
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-bitmap.h"
#include "cogl-bitmap-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-sliced-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-spans.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

static void _cogl_texture_2d_sliced_free (CoglTexture2DSliced *tex_2ds);

COGL_HANDLE_DEFINE (Texture2DSliced, texture_2d_sliced);

/* To differentiate between texture coordinates of a specific, real, slice
 * texture and the texture coordinates of the composite, sliced texture, the
 * coordinates of the sliced texture are called "virtual" coordinates and the
 * coordinates of slices are called "slice" coordinates. */
/* This function lets you iterate all the slices that lie within the given
 * virtual coordinates of the parent sliced texture. */
/* Note: no guarantee is given about the order in which the slices will be
 * visited */
void
_cogl_texture_2d_sliced_foreach_sub_texture_in_region (
                                       CoglTexture2DSliced *tex_2ds,
                                       float virtual_tx_1,
                                       float virtual_ty_1,
                                       float virtual_tx_2,
                                       float virtual_ty_2,
                                       CoglTextureSliceCallback callback,
                                       void *user_data)
{
  CoglTexture *tex = COGL_TEXTURE (tex_2ds);
  float width = tex->bitmap.width;
  float height = tex->bitmap.height;
  CoglSpanIter iter_x;
  CoglSpanIter iter_y;

  g_assert (tex->gl_target == GL_TEXTURE_2D);

  /* Slice spans are stored in denormalized coordinates, and this is what
   * the _cogl_span_iter_* funcs expect to be given, so we scale the given
   * virtual coordinates by the texture size to denormalize.
   */
  /* XXX: I wonder if it's worth changing how we store spans so we can avoid
   * the need to denormalize here */
  virtual_tx_1 *= width;
  virtual_ty_1 *= height;
  virtual_tx_2 *= width;
  virtual_ty_2 *= height;

  /* Iterate the y axis of the virtual rectangle */
  for (_cogl_span_iter_begin (&iter_y,
                              tex_2ds->slice_y_spans,
                              height,
                              virtual_ty_1,
                              virtual_ty_2);
       !_cogl_span_iter_end (&iter_y);
       _cogl_span_iter_next (&iter_y))
    {
      float y_intersect_start = iter_y.intersect_start;
      float y_intersect_end = iter_y.intersect_end;
      float slice_ty1;
      float slice_ty2;

      /* Discard slices out of rectangle early */
      if (!iter_y.intersects)
        continue;

      if (iter_y.flipped)
        {
          y_intersect_start = iter_y.intersect_end;
          y_intersect_end = iter_y.intersect_start;
        }

      /* Localize slice texture coordinates */
      slice_ty1 = y_intersect_start - iter_y.pos;
      slice_ty2 = y_intersect_end - iter_y.pos;

      /* Normalize slice texture coordinates */
      slice_ty1 /= iter_y.span->size;
      slice_ty2 /= iter_y.span->size;

      /* Iterate the x axis of the virtual rectangle */
      for (_cogl_span_iter_begin (&iter_x,
                                  tex_2ds->slice_x_spans,
                                  width,
                                  virtual_tx_1,
                                  virtual_tx_2);
	   !_cogl_span_iter_end (&iter_x);
	   _cogl_span_iter_next (&iter_x))
        {
          float slice_coords[4];
          float virtual_coords[4];
          float x_intersect_start = iter_x.intersect_start;
          float x_intersect_end = iter_x.intersect_end;
          float slice_tx1;
          float slice_tx2;
          GLuint gl_handle;

	  /* Discard slices out of rectangle early */
	  if (!iter_x.intersects)
            continue;

          if (iter_x.flipped)
            {
              x_intersect_start = iter_x.intersect_end;
              x_intersect_end = iter_x.intersect_start;
            }

	  /* Localize slice texture coordinates */
          slice_tx1 = x_intersect_start - iter_x.pos;
          slice_tx2 = x_intersect_end - iter_x.pos;

          /* Normalize slice texture coordinates */
          slice_tx1 /= iter_x.span->size;
          slice_tx2 /= iter_x.span->size;

	  /* Pluck out opengl texture object for this slice */
	  gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint,
				     iter_y.index * iter_x.array->len +
				     iter_x.index);

          slice_coords[0] = slice_tx1;
          slice_coords[1] = slice_ty1;
          slice_coords[2] = slice_tx2;
          slice_coords[3] = slice_ty2;

          virtual_coords[0] = x_intersect_start / width;
          virtual_coords[1] = y_intersect_start / height;
          virtual_coords[2] = x_intersect_end / width;
          virtual_coords[3] = y_intersect_end / height;

          callback (tex,
                    gl_handle,
                    tex->gl_target,
                    slice_coords,
                    virtual_coords,
                    user_data);
	}
    }
}

static guchar *
_cogl_texture_2d_sliced_allocate_waste_buffer (CoglTexture2DSliced *tex_2ds)
{
  CoglSpan *last_x_span;
  CoglSpan *last_y_span;
  guchar           *waste_buf = NULL;
  CoglTexture      *tex = COGL_TEXTURE (tex_2ds);

  /* If the texture has any waste then allocate a buffer big enough to
     fill the gaps */
  last_x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan,
                                tex_2ds->slice_x_spans->len - 1);
  last_y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan,
                                tex_2ds->slice_y_spans->len - 1);
  if (last_x_span->waste > 0 || last_y_span->waste > 0)
    {
      gint bpp = _cogl_get_format_bpp (tex->bitmap.format);
      CoglSpan  *first_x_span
        = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, 0);
      CoglSpan  *first_y_span
        = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, 0);
      guint right_size = first_y_span->size * last_x_span->waste;
      guint bottom_size = first_x_span->size * last_y_span->waste;

      waste_buf = g_malloc (MAX (right_size, bottom_size) * bpp);
    }

  return waste_buf;
}

static gboolean
_cogl_texture_2d_sliced_upload_to_gl (CoglTexture2DSliced *tex_2ds)
{
  CoglSpan  *x_span;
  CoglSpan  *y_span;
  GLuint             gl_handle;
  gint               bpp;
  gint               x,y;
  guchar            *waste_buf;
  CoglTexture       *tex = COGL_TEXTURE (tex_2ds);

  bpp = _cogl_get_format_bpp (tex->bitmap.format);

  waste_buf = _cogl_texture_2d_sliced_allocate_waste_buffer (tex_2ds);

  /* Iterate vertical slices */
  for (y = 0; y < tex_2ds->slice_y_spans->len; ++y)
    {
      y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, y);

      /* Iterate horizontal slices */
      for (x = 0; x < tex_2ds->slice_x_spans->len; ++x)
        {
          gint slice_num = y * tex_2ds->slice_x_spans->len + x;

          x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, x);

          /* Pick the gl texture object handle */
          gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint, slice_num);

          _cogl_texture_driver_upload_subregion_to_gl (
                                     tex,
                                     x_span->start, /* src x */
                                     y_span->start, /* src y */
                                     0, /* dst x */
                                     0, /* dst y */
                                     x_span->size - x_span->waste, /* width */
                                     y_span->size - y_span->waste, /* height */
                                     &tex->bitmap,
                                     tex->gl_format,
                                     tex->gl_type,
                                     gl_handle);

          /* Keep a copy of the first pixel if needed */
          if (tex_2ds->first_pixels)
            {
              memcpy (tex_2ds->first_pixels[slice_num].data,
                      tex->bitmap.data + x_span->start * bpp
                      + y_span->start * tex->bitmap.rowstride,
                      bpp);
              tex_2ds->first_pixels[slice_num].gl_format = tex->gl_format;
              tex_2ds->first_pixels[slice_num].gl_type = tex->gl_type;
            }

          /* Fill the waste with a copies of the rightmost pixels */
          if (x_span->waste > 0)
            {
              const guchar *src = tex->bitmap.data
                + y_span->start * tex->bitmap.rowstride
                + (x_span->start + x_span->size - x_span->waste - 1) * bpp;
              guchar *dst = waste_buf;
              guint wx, wy;

              for (wy = 0; wy < y_span->size - y_span->waste; wy++)
                {
                  for (wx = 0; wx < x_span->waste; wx++)
                    {
                      memcpy (dst, src, bpp);
                      dst += bpp;
                    }
                  src += tex->bitmap.rowstride;
                }

              _cogl_texture_driver_prep_gl_for_pixels_upload (
                                                        x_span->waste * bpp,
                                                        bpp);

              GE( glTexSubImage2D (tex->gl_target, 0,
                                   x_span->size - x_span->waste,
                                   0,
                                   x_span->waste,
                                   y_span->size - y_span->waste,
                                   tex->gl_format, tex->gl_type,
                                   waste_buf) );
            }

          if (y_span->waste > 0)
            {
              const guchar *src = tex->bitmap.data
                + ((y_span->start + y_span->size - y_span->waste - 1)
                   * tex->bitmap.rowstride)
                + x_span->start * bpp;
              guchar *dst = waste_buf;
              guint wy, wx;

              for (wy = 0; wy < y_span->waste; wy++)
                {
                  memcpy (dst, src, (x_span->size - x_span->waste) * bpp);
                  dst += (x_span->size - x_span->waste) * bpp;

                  for (wx = 0; wx < x_span->waste; wx++)
                    {
                      memcpy (dst, dst - bpp, bpp);
                      dst += bpp;
                    }
                }

              _cogl_texture_driver_prep_gl_for_pixels_upload (
                                                        x_span->size * bpp,
                                                        bpp);

              GE( glTexSubImage2D (tex->gl_target, 0,
                                   0,
                                   y_span->size - y_span->waste,
                                   x_span->size,
                                   y_span->waste,
                                   tex->gl_format, tex->gl_type,
                                   waste_buf) );
            }
        }
    }

  if (waste_buf)
    g_free (waste_buf);

  tex->mipmaps_dirty = TRUE;

  return TRUE;
}

static gboolean
_cogl_texture_2d_sliced_upload_subregion_to_gl (CoglTexture2DSliced *tex_2ds,
                                                gint         src_x,
                                                gint         src_y,
                                                gint         dst_x,
                                                gint         dst_y,
                                                gint         width,
                                                gint         height,
                                                CoglBitmap  *source_bmp,
                                                GLuint       source_gl_format,
                                                GLuint       source_gl_type)
{
  CoglTexture      *tex = COGL_TEXTURE (tex_2ds);
  CoglSpan *x_span;
  CoglSpan *y_span;
  gint              bpp;
  CoglSpanIter      x_iter;
  CoglSpanIter      y_iter;
  GLuint            gl_handle;
  gint              source_x = 0, source_y = 0;
  gint              inter_w = 0, inter_h = 0;
  gint              local_x = 0, local_y = 0;
  guchar           *waste_buf;

  bpp = _cogl_get_format_bpp (source_bmp->format);

  waste_buf = _cogl_texture_2d_sliced_allocate_waste_buffer (tex_2ds);

  /* Iterate vertical spans */
  for (source_y = src_y,
       _cogl_span_iter_begin (&y_iter,
                              tex_2ds->slice_y_spans,
                              tex->bitmap.height,
                              dst_y,
                              dst_y + height);

       !_cogl_span_iter_end (&y_iter);

       _cogl_span_iter_next (&y_iter),
       source_y += inter_h )
    {
      /* Discard slices out of the subregion early */
      if (!y_iter.intersects)
        {
          inter_h = 0;
          continue;
        }

      y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan,
                               y_iter.index);

      /* Iterate horizontal spans */
      for (source_x = src_x,
           _cogl_span_iter_begin (&x_iter,
                                  tex_2ds->slice_x_spans,
                                  tex->bitmap.width,
                                  dst_x,
                                  dst_x + width);

           !_cogl_span_iter_end (&x_iter);

           _cogl_span_iter_next (&x_iter),
           source_x += inter_w )
        {
          gint slice_num;

          /* Discard slices out of the subregion early */
          if (!x_iter.intersects)
            {
              inter_w = 0;
              continue;
            }

          x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan,
                                   x_iter.index);

          /* Pick intersection width and height */
          inter_w =  (x_iter.intersect_end - x_iter.intersect_start);
          inter_h =  (y_iter.intersect_end - y_iter.intersect_start);

          /* Localize intersection top-left corner to slice*/
          local_x =  (x_iter.intersect_start - x_iter.pos);
          local_y =  (y_iter.intersect_start - y_iter.pos);

          slice_num = y_iter.index * tex_2ds->slice_x_spans->len + x_iter.index;

          /* Pick slice GL handle */
          gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint, slice_num);

          _cogl_texture_driver_upload_subregion_to_gl (tex,
                                                       source_x,
                                                       source_y,
                                                       local_x, /* dst x */
                                                       local_y, /* dst x */
                                                       inter_w, /* width */
                                                       inter_h, /* height */
                                                       source_bmp,
                                                       source_gl_format,
                                                       source_gl_type,
                                                       gl_handle);

          /* Keep a copy of the first pixel if needed */
          if (tex_2ds->first_pixels && local_x == 0 && local_y == 0)
            {
              memcpy (tex_2ds->first_pixels[slice_num].data,
                      source_bmp->data + source_x * bpp
                      + source_y * source_bmp->rowstride,
                      bpp);
              tex_2ds->first_pixels[slice_num].gl_format = source_gl_format;
              tex_2ds->first_pixels[slice_num].gl_type = source_gl_type;
            }

          /* If the x_span is sliced and the upload touches the
             rightmost pixels then fill the waste with copies of the
             pixels */
          if (x_span->waste > 0
              && local_x < x_span->size - x_span->waste
              && local_x + inter_w >= x_span->size - x_span->waste)
            {
              const guchar *src;
              guchar *dst;
              guint wx, wy;

              src = source_bmp->data
                + (src_y +  ((int)y_iter.intersect_start)
                   - dst_y)
                * source_bmp->rowstride
                + (src_x + x_span->start + x_span->size - x_span->waste
                   - dst_x - 1)
                * bpp;

              dst = waste_buf;

              for (wy = 0; wy < inter_h; wy++)
                {
                  for (wx = 0; wx < x_span->waste; wx++)
                    {
                      memcpy (dst, src, bpp);
                      dst += bpp;
                    }
                  src += source_bmp->rowstride;
                }

              _cogl_texture_driver_prep_gl_for_pixels_upload (
                                                        x_span->waste * bpp,
                                                        bpp);

              GE( glTexSubImage2D (tex->gl_target, 0,
                                   x_span->size - x_span->waste,
                                   local_y,
                                   x_span->waste,
                                   inter_h,
                                   source_gl_format,
                                   source_gl_type,
                                   waste_buf) );
            }

          /* same for the bottom-most pixels */
          if (y_span->waste > 0
              && local_y < y_span->size - y_span->waste
              && local_y + inter_h >= y_span->size - y_span->waste)
            {
              const guchar *src;
              guchar *dst;
              guint wy, wx;
              guint copy_width;

              src = source_bmp->data
                + (src_x +  ((int)x_iter.intersect_start)
                   - dst_x)
                * bpp
                + (src_y + y_span->start + y_span->size - y_span->waste
                   - dst_y - 1)
                * source_bmp->rowstride;

              dst = waste_buf;

              if (local_x + inter_w >= x_span->size - x_span->waste)
                copy_width = x_span->size - local_x;
              else
                copy_width = inter_w;

              for (wy = 0; wy < y_span->waste; wy++)
                {
                  memcpy (dst, src, inter_w * bpp);
                  dst += inter_w * bpp;

                  for (wx = inter_w; wx < copy_width; wx++)
                    {
                      memcpy (dst, dst - bpp, bpp);
                      dst += bpp;
                    }
                }

              _cogl_texture_driver_prep_gl_for_pixels_upload (
                                                          copy_width * bpp,
                                                          bpp);

              GE( glTexSubImage2D (tex->gl_target, 0,
                                   local_x,
                                   y_span->size - y_span->waste,
                                   copy_width,
                                   y_span->waste,
                                   source_gl_format,
                                   source_gl_type,
                                   waste_buf) );
            }
        }
    }

  if (waste_buf)
    g_free (waste_buf);

  tex->mipmaps_dirty = TRUE;

  return TRUE;
}

static gint
_cogl_rect_slices_for_size (int     size_to_fill,
                            int     max_span_size,
                            int     max_waste,
                            GArray *out_spans)
{
  int               n_spans = 0;
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

static gint
_cogl_pot_slices_for_size (gint    size_to_fill,
                           gint    max_span_size,
                           gint    max_waste,
                           GArray *out_spans)
{
  gint             n_spans = 0;
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

void
_cogl_texture_2d_sliced_set_wrap_mode_parameter (CoglTexture2DSliced *tex_2ds,
                                                 GLenum wrap_mode)
{
  CoglTexture *tex = COGL_TEXTURE (tex_2ds);

  /* Only set the wrap mode if it's different from the current
     value to avoid too many GL calls */
  if (tex->wrap_mode != wrap_mode)
    {
      int i;

      /* Any queued texture rectangles may be depending on the previous
       * wrap mode... */
      _cogl_journal_flush ();

      for (i = 0; i < tex_2ds->slice_gl_handles->len; i++)
        {
          GLuint texnum = g_array_index (tex_2ds->slice_gl_handles, GLuint, i);

          GE( glBindTexture (tex->gl_target, texnum) );
          GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_S, wrap_mode) );
          GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_T, wrap_mode) );
        }

      tex->wrap_mode = wrap_mode;
    }
}

static gboolean
_cogl_texture_2d_sliced_slices_create (CoglTexture2DSliced *tex_2ds)
{
  CoglTexture      *tex = COGL_TEXTURE (tex_2ds);
  gint              bpp;
  gint              max_width;
  gint              max_height;
  GLuint           *gl_handles;
  gint              n_x_slices;
  gint              n_y_slices;
  gint              n_slices;
  gint              x, y;
  CoglSpan *x_span;
  CoglSpan *y_span;
  const GLfloat     transparent_color[4] = { 0x00, 0x00, 0x00, 0x00 };

  gint   (*slices_for_size) (gint, gint, gint, GArray*);

  bpp = _cogl_get_format_bpp (tex->bitmap.format);

  /* Initialize size of largest slice according to supported features */
  if (cogl_features_available (COGL_FEATURE_TEXTURE_NPOT))
    {
      max_width = tex->bitmap.width;
      max_height = tex->bitmap.height;
      tex->gl_target  = GL_TEXTURE_2D;
      slices_for_size = _cogl_rect_slices_for_size;
    }
  else
    {
      max_width = cogl_util_next_p2 (tex->bitmap.width);
      max_height = cogl_util_next_p2 (tex->bitmap.height);
      tex->gl_target = GL_TEXTURE_2D;
      slices_for_size = _cogl_pot_slices_for_size;
    }

  /* Negative number means no slicing forced by the user */
  if (tex_2ds->max_waste <= -1)
    {
      CoglSpan span;

      /* Check if size supported else bail out */
      if (!_cogl_texture_driver_size_supported (tex->gl_target,
                                                tex->gl_format,
                                                tex->gl_type,
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
      span.waste = max_width - tex->bitmap.width;
      g_array_append_val (tex_2ds->slice_x_spans, span);

      span.size = max_height;
      span.waste = max_height - tex->bitmap.height;
      g_array_append_val (tex_2ds->slice_y_spans, span);
    }
  else
    {
      /* Decrease the size of largest slice until supported by GL */
      while (!_cogl_texture_driver_size_supported (tex->gl_target,
                                                   tex->gl_format,
                                                   tex->gl_type,
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
      n_x_slices = slices_for_size (tex->bitmap.width,
                                    max_width, tex_2ds->max_waste,
                                    NULL);

      n_y_slices = slices_for_size (tex->bitmap.height,
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
      slices_for_size (tex->bitmap.width,
                       max_width, tex_2ds->max_waste,
                       tex_2ds->slice_x_spans);

      slices_for_size (tex->bitmap.height,
                       max_height, tex_2ds->max_waste,
                       tex_2ds->slice_y_spans);
    }

  /* Init and resize GL handle array */
  n_slices = n_x_slices * n_y_slices;

  tex_2ds->slice_gl_handles = g_array_sized_new (FALSE, FALSE,
                                                 sizeof (GLuint),
                                                 n_slices);

  g_array_set_size (tex_2ds->slice_gl_handles, n_slices);

  /* Allocate some space to store a copy of the first pixel of each
     slice. This is only needed if glGenerateMipmap (which is part of
     the FBO extension) is not available */
  if (cogl_features_available (COGL_FEATURE_OFFSCREEN))
    tex_2ds->first_pixels = NULL;
  else
    tex_2ds->first_pixels = g_new (CoglTexturePixel, n_slices);

  /* Wrap mode not yet set */
  tex->wrap_mode = GL_FALSE;

  /* Generate a "working set" of GL texture objects
   * (some implementations might supported faster
   *  re-binding between textures inside a set) */
  gl_handles = (GLuint*) tex_2ds->slice_gl_handles->data;

  GE( glGenTextures (n_slices, gl_handles) );


  /* Init each GL texture object */
  for (y = 0; y < n_y_slices; ++y)
    {
      y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, y);

      for (x = 0; x < n_x_slices; ++x)
        {
          x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, x);

          COGL_NOTE (TEXTURE, "CREATE SLICE (%d,%d)\tsize (%d,%d)",
                     x, y,
                     x_span->size - x_span->waste,
                     y_span->size - y_span->waste);

          /* Setup texture parameters */
          GE( _cogl_texture_driver_bind (tex->gl_target,
                                         gl_handles[y * n_x_slices + x],
                                         tex->gl_intformat) );

          _cogl_texture_driver_try_setting_gl_border_color (tex->gl_target,
                                                            transparent_color);

          /* Pass NULL data to init size and internal format */
          GE( glTexImage2D (tex->gl_target, 0, tex->gl_intformat,
                            x_span->size, y_span->size, 0,
                            tex->gl_format, tex->gl_type, 0) );
        }
    }

  return TRUE;
}

static void
_cogl_texture_2d_sliced_slices_free (CoglTexture2DSliced *tex_2ds)
{
  CoglTexture *tex = COGL_TEXTURE (tex_2ds);

  if (tex_2ds->slice_x_spans != NULL)
    g_array_free (tex_2ds->slice_x_spans, TRUE);

  if (tex_2ds->slice_y_spans != NULL)
    g_array_free (tex_2ds->slice_y_spans, TRUE);

  if (tex_2ds->slice_gl_handles != NULL)
    {
      if (tex->is_foreign == FALSE)
        {
          GE( glDeleteTextures (tex_2ds->slice_gl_handles->len,
                                (GLuint*) tex_2ds->slice_gl_handles->data) );
        }

      g_array_free (tex_2ds->slice_gl_handles, TRUE);
    }

  if (tex_2ds->first_pixels != NULL)
    g_free (tex_2ds->first_pixels);
}

static void
_cogl_texture_2d_sliced_free (CoglTexture2DSliced *tex_2ds)
{
  /* Chain up to parent */
  _cogl_texture_free (COGL_TEXTURE (tex_2ds));

  _cogl_texture_2d_sliced_slices_free (tex_2ds);
  g_free (tex_2ds);
}

CoglHandle
_cogl_texture_2d_sliced_new_with_size (unsigned int     width,
                                       unsigned int     height,
                                       CoglTextureFlags flags,
                                       CoglPixelFormat  internal_format)
{
  CoglTexture2DSliced *tex_2ds;
  CoglTexture         *tex;
  gint                 bpp;
  gint                 rowstride;

  /* Since no data, we need some internal format */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    internal_format = COGL_PIXEL_FORMAT_RGBA_8888_PRE;

  /* Rowstride from width */
  bpp = _cogl_get_format_bpp (internal_format);
  rowstride = width * bpp;

  /* Init texture with empty bitmap */
  tex_2ds = g_new (CoglTexture2DSliced, 1);

  tex = COGL_TEXTURE (tex_2ds);
  tex->type = COGL_TEXTURE_TYPE_2D_SLICED;

  tex->is_foreign = FALSE;
  tex->auto_mipmap = (flags & COGL_TEXTURE_NO_AUTO_MIPMAP) == 0;
  tex->mipmaps_dirty = TRUE;
  tex_2ds->first_pixels = NULL;

  tex->bitmap.width = width;
  tex->bitmap.height = height;
  tex->bitmap.format = internal_format;
  tex->bitmap.rowstride = rowstride;
  tex->bitmap.data = NULL;
  tex->bitmap_owner = FALSE;

  tex_2ds->slice_x_spans = NULL;
  tex_2ds->slice_y_spans = NULL;
  tex_2ds->slice_gl_handles = NULL;

  if (flags & COGL_TEXTURE_NO_SLICING)
    tex_2ds->max_waste = -1;
  else
    tex_2ds->max_waste = COGL_TEXTURE_MAX_WASTE;

  /* Unknown filter */
  tex->min_filter = GL_FALSE;
  tex->mag_filter = GL_FALSE;

  /* Find closest GL format match */
  tex->bitmap.format =
    _cogl_pixel_format_to_gl (internal_format,
                              &tex->gl_intformat,
                              &tex->gl_format,
                              &tex->gl_type);

  /* Create slices for the given format and size */
  if (!_cogl_texture_2d_sliced_slices_create (tex_2ds))
    {
      _cogl_texture_2d_sliced_free (tex_2ds);
      return COGL_INVALID_HANDLE;
    }

  return _cogl_texture_2d_sliced_handle_new (tex_2ds);
}

CoglHandle
_cogl_texture_2d_sliced_new_from_data (unsigned int     width,
                                       unsigned int     height,
                                       CoglTextureFlags flags,
                                       CoglPixelFormat  format,
                                       CoglPixelFormat  internal_format,
                                       unsigned int     rowstride,
                                       const guint8    *data)
{
  CoglTexture2DSliced *tex_2ds;
  CoglTexture         *tex;
  int                  bpp;

  if (format == COGL_PIXEL_FORMAT_ANY)
    return COGL_INVALID_HANDLE;

  if (data == NULL)
    return COGL_INVALID_HANDLE;

  /* Rowstride from width if not given */
  bpp = _cogl_get_format_bpp (format);
  if (rowstride == 0) rowstride = width * bpp;

  /* Create new texture and fill with given data */
  tex_2ds = g_new0 (CoglTexture2DSliced, 1);

  tex = COGL_TEXTURE (tex_2ds);
  tex->type = COGL_TEXTURE_TYPE_2D_SLICED;

  tex->is_foreign = FALSE;
  tex->auto_mipmap = (flags & COGL_TEXTURE_NO_AUTO_MIPMAP) == 0;
  tex->mipmaps_dirty = TRUE;
  tex_2ds->first_pixels = NULL;

  tex->bitmap.width = width;
  tex->bitmap.height = height;
  tex->bitmap.data = (guchar*)data;
  tex->bitmap.format = format;
  tex->bitmap.rowstride = rowstride;
  tex->bitmap_owner = FALSE;

  tex_2ds->slice_x_spans = NULL;
  tex_2ds->slice_y_spans = NULL;
  tex_2ds->slice_gl_handles = NULL;

  if (flags & COGL_TEXTURE_NO_SLICING)
    tex_2ds->max_waste = -1;
  else
    tex_2ds->max_waste = COGL_TEXTURE_MAX_WASTE;

  /* Unknown filter */
  tex->min_filter = GL_FALSE;
  tex->mag_filter = GL_FALSE;

  /* FIXME: If upload fails we should set some kind of
   * error flag but still return texture handle (this
   * is to keep the behavior equal to _new_from_file;
   * see below) */

  if (!_cogl_texture_bitmap_prepare (tex, internal_format))
    {
      _cogl_texture_2d_sliced_free (tex_2ds);
      return COGL_INVALID_HANDLE;
    }

  if (!_cogl_texture_2d_sliced_slices_create (tex_2ds))
    {
      _cogl_texture_2d_sliced_free (tex_2ds);
      return COGL_INVALID_HANDLE;
    }

  if (!_cogl_texture_2d_sliced_upload_to_gl (tex_2ds))
    {
      _cogl_texture_2d_sliced_free (tex_2ds);
      return COGL_INVALID_HANDLE;
    }

  _cogl_texture_bitmap_free (COGL_TEXTURE (tex_2ds));

  return _cogl_texture_2d_sliced_handle_new (tex_2ds);
}

CoglHandle
_cogl_texture_2d_sliced_new_from_bitmap (CoglHandle       bmp_handle,
                                         CoglTextureFlags flags,
                                         CoglPixelFormat  internal_format)
{
  CoglTexture2DSliced *tex_2ds;
  CoglTexture         *tex;
  CoglBitmap          *bmp = (CoglBitmap *)bmp_handle;

  g_return_val_if_fail (bmp_handle != COGL_INVALID_HANDLE, COGL_INVALID_HANDLE);

  /* Create new texture and fill with loaded data */
  tex_2ds = g_new0 (CoglTexture2DSliced, 1);

  tex = COGL_TEXTURE (tex_2ds);
  tex->type = COGL_TEXTURE_TYPE_2D_SLICED;

  tex->is_foreign = FALSE;
  tex->auto_mipmap = (flags & COGL_TEXTURE_NO_AUTO_MIPMAP) == 0;
  tex->mipmaps_dirty = TRUE;
  tex_2ds->first_pixels = NULL;

  tex->bitmap = *bmp;
  tex->bitmap_owner = FALSE;

  tex_2ds->slice_x_spans = NULL;
  tex_2ds->slice_y_spans = NULL;
  tex_2ds->slice_gl_handles = NULL;

  if (flags & COGL_TEXTURE_NO_SLICING)
    tex_2ds->max_waste = -1;
  else
    tex_2ds->max_waste = COGL_TEXTURE_MAX_WASTE;

  /* Unknown filter */
  tex->min_filter = GL_FALSE;
  tex->mag_filter = GL_FALSE;

  /* FIXME: If upload fails we should set some kind of
   * error flag but still return texture handle if the
   * user decides to destroy another texture and upload
   * this one instead (reloading from file is not needed
   * in that case). As a rule then, everytime a valid
   * CoglHandle is returned, it should also be destroyed
   * with cogl_handle_unref at some point! */

  if (!_cogl_texture_bitmap_prepare (tex, internal_format))
    {
      _cogl_texture_2d_sliced_free (tex_2ds);
      return COGL_INVALID_HANDLE;
    }

  if (!_cogl_texture_2d_sliced_slices_create (tex_2ds))
    {
      _cogl_texture_2d_sliced_free (tex_2ds);
      return COGL_INVALID_HANDLE;
    }

  if (!_cogl_texture_2d_sliced_upload_to_gl (tex_2ds))
    {
      _cogl_texture_2d_sliced_free (tex_2ds);
      return COGL_INVALID_HANDLE;
    }

  _cogl_texture_bitmap_free (tex);

  return _cogl_texture_2d_sliced_handle_new (tex_2ds);
}

CoglHandle
_cogl_texture_2d_sliced_new_from_file (
                                const char       *filename,
                                CoglTextureFlags  flags,
                                CoglPixelFormat   internal_format,
                                GError          **error)
{
  CoglHandle bmp;
  CoglHandle handle;

  g_return_val_if_fail (error == NULL || *error == NULL, COGL_INVALID_HANDLE);

  bmp = cogl_bitmap_new_from_file (filename, error);
  if (bmp == COGL_INVALID_HANDLE)
    return COGL_INVALID_HANDLE;

  handle =
    _cogl_texture_2d_sliced_new_from_bitmap (bmp, flags, internal_format);
  cogl_handle_unref (bmp);

  return handle;
}

CoglHandle
_cogl_texture_2d_sliced_new_from_foreign (GLuint           gl_handle,
                                          GLenum           gl_target,
                                          GLuint           width,
                                          GLuint           height,
                                          GLuint           x_pot_waste,
                                          GLuint           y_pot_waste,
                                          CoglPixelFormat  format)
{
  /* NOTE: width, height and internal format are not queriable
     in GLES, hence such a function prototype. However, for
     OpenGL they are still queried from the texture for improved
     robustness and for completeness in case one day GLES gains
     support for them.
     */

  GLenum               gl_error = 0;
  GLboolean            gl_istexture;
  GLint                gl_compressed = GL_FALSE;
  GLint                gl_int_format = 0;
  GLint                gl_width = 0;
  GLint                gl_height = 0;
  GLint                gl_gen_mipmap;
  guint                bpp;
  CoglTexture2DSliced *tex_2ds;
  CoglTexture         *tex;
  CoglSpan     x_span;
  CoglSpan     y_span;

  if (!_cogl_texture_driver_allows_foreign_gl_target (gl_target))
    return COGL_INVALID_HANDLE;

#if HAVE_COGL_GL
  /* It shouldn't be necissary to have waste in this case since
   * the texture isn't limited to power of two sizes. */
  if (gl_target == GL_TEXTURE_RECTANGLE_ARB &&
      (x_pot_waste != 0 || y_pot_waste != 0))
    {
      g_warning ("You can't create a foreign GL_TEXTURE_RECTANGLE cogl "
                 "texture with waste\n");
      return COGL_INVALID_HANDLE;
    }
#endif

  /* Make sure it is a valid GL texture object */
  gl_istexture = glIsTexture (gl_handle);
  if (gl_istexture == GL_FALSE)
    return COGL_INVALID_HANDLE;

  /* Make sure binding succeeds */
  gl_error = glGetError ();
  glBindTexture (gl_target, gl_handle);
  if (glGetError () != GL_NO_ERROR)
    return COGL_INVALID_HANDLE;

  /* Obtain texture parameters
     (only level 0 we are interested in) */

#if HAVE_COGL_GL
  GE( glGetTexLevelParameteriv (gl_target, 0,
                                GL_TEXTURE_COMPRESSED,
                                &gl_compressed) );

  GE( glGetTexLevelParameteriv (gl_target, 0,
                                GL_TEXTURE_INTERNAL_FORMAT,
                                &gl_int_format) );


  GE( glGetTexLevelParameteriv (gl_target, 0,
                                GL_TEXTURE_WIDTH,
                                &gl_width) );

  GE( glGetTexLevelParameteriv (gl_target, 0,
                                GL_TEXTURE_HEIGHT,
                                &gl_height) );
#else
  gl_width = width + x_pot_waste;
  gl_height = height + y_pot_waste;
#endif

  GE( glGetTexParameteriv (gl_target,
                           GL_GENERATE_MIPMAP,
                           &gl_gen_mipmap) );

  /* Validate width and height */
  if (gl_width <= 0 || gl_height <= 0)
    return COGL_INVALID_HANDLE;

  /* Validate pot waste */
  if (x_pot_waste < 0 || x_pot_waste >= gl_width ||
      y_pot_waste < 0 || y_pot_waste >= gl_height)
    return COGL_INVALID_HANDLE;

  /* Compressed texture images not supported */
  if (gl_compressed == GL_TRUE)
    return COGL_INVALID_HANDLE;

  /* Try and match to a cogl format */
  if (!_cogl_pixel_format_from_gl_internal (gl_int_format, &format))
    return COGL_INVALID_HANDLE;

  /* Create new texture */
  tex_2ds = g_new0 (CoglTexture2DSliced, 1);

  tex = COGL_TEXTURE (tex_2ds);
  tex->type = COGL_TEXTURE_TYPE_2D_SLICED;

  /* Setup bitmap info */
  tex->is_foreign = TRUE;
  tex->auto_mipmap = (gl_gen_mipmap == GL_TRUE) ? TRUE : FALSE;
  tex->mipmaps_dirty = TRUE;
  tex_2ds->first_pixels = NULL;

  bpp = _cogl_get_format_bpp (format);
  tex->bitmap.format = format;
  tex->bitmap.width = gl_width - x_pot_waste;
  tex->bitmap.height = gl_height - y_pot_waste;
  tex->bitmap.rowstride = tex->bitmap.width * bpp;
  tex->bitmap_owner = FALSE;

  tex->gl_target = gl_target;
  tex->gl_intformat = gl_int_format;
  tex->gl_format = gl_int_format;
  tex->gl_type = GL_UNSIGNED_BYTE;

  /* Unknown filter */
  tex->min_filter = GL_FALSE;
  tex->mag_filter = GL_FALSE;
  tex_2ds->max_waste = 0;

  /* Wrap mode not yet set */
  tex->wrap_mode = GL_FALSE;

  /* Create slice arrays */
  tex_2ds->slice_x_spans =
    g_array_sized_new (FALSE, FALSE,
                       sizeof (CoglSpan), 1);

  tex_2ds->slice_y_spans =
    g_array_sized_new (FALSE, FALSE,
                       sizeof (CoglSpan), 1);

  tex_2ds->slice_gl_handles =
    g_array_sized_new (FALSE, FALSE,
                       sizeof (GLuint), 1);

  /* Store info for a single slice */
  x_span.start = 0;
  x_span.size = gl_width;
  x_span.waste = x_pot_waste;
  g_array_append_val (tex_2ds->slice_x_spans, x_span);

  y_span.start = 0;
  y_span.size = gl_height;
  y_span.waste = y_pot_waste;
  g_array_append_val (tex_2ds->slice_y_spans, y_span);

  g_array_append_val (tex_2ds->slice_gl_handles, gl_handle);

  tex_2ds->first_pixels = NULL;

  return _cogl_texture_2d_sliced_handle_new (tex_2ds);
}

gint
_cogl_texture_2d_sliced_get_max_waste (CoglHandle handle)
{
  CoglTexture2DSliced *tex_2ds;

  tex_2ds = _cogl_texture_2d_sliced_pointer_from_handle (handle);

  return tex_2ds->max_waste;
}

gboolean
_cogl_texture_2d_sliced_is_sliced (CoglHandle handle)
{
  CoglTexture2DSliced *tex_2ds;

  tex_2ds = _cogl_texture_2d_sliced_pointer_from_handle (handle);

  if (tex_2ds->slice_gl_handles == NULL)
    return FALSE;

  if (tex_2ds->slice_gl_handles->len <= 1)
    return FALSE;

  return TRUE;
}

gboolean
_cogl_texture_2d_sliced_can_hardware_repeat (CoglHandle handle)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (handle);
  CoglSpan *x_span;
  CoglSpan *y_span;

  x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, 0);
  y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, 0);

  return (x_span->waste || y_span->waste) ? FALSE : TRUE;
}

void
_cogl_texture_2d_sliced_transform_coords_to_gl (CoglTexture2DSliced *tex_2ds,
                                                float *s,
                                                float *t)
{
  CoglTexture *tex = COGL_TEXTURE (tex_2ds);
  CoglSpan *x_span;
  CoglSpan *y_span;

  g_assert (!_cogl_texture_2d_sliced_is_sliced (tex_2ds));

  /* Don't include the waste in the texture coordinates */
  x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, 0);
  y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, 0);

  *s *= tex->bitmap.width / (float)x_span->size;
  *t *= tex->bitmap.height / (float)y_span->size;

#if HAVE_COGL_GL
  /* Denormalize texture coordinates for rectangle textures */
  if (tex->gl_target == GL_TEXTURE_RECTANGLE_ARB)
    {
      *s *= x_span->size;
      *t *= y_span->size;
    }
#endif
}

gboolean
_cogl_texture_2d_sliced_get_gl_texture (CoglHandle handle,
                                        GLuint *out_gl_handle,
                                        GLenum *out_gl_target)
{
  CoglTexture2DSliced *tex_2ds;
  CoglTexture         *tex;

  if (!cogl_is_texture (handle))
    return FALSE;

  tex_2ds = _cogl_texture_2d_sliced_pointer_from_handle (handle);
  tex = COGL_TEXTURE (tex_2ds);

  if (tex_2ds->slice_gl_handles == NULL)
    return FALSE;

  if (tex_2ds->slice_gl_handles->len < 1)
    return FALSE;

  if (out_gl_handle != NULL)
    *out_gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint, 0);

  if (out_gl_target != NULL)
    *out_gl_target = tex->gl_target;

  return TRUE;
}

void
_cogl_texture_2d_sliced_set_filters (CoglHandle handle,
                                     GLenum min_filter,
                                     GLenum mag_filter)
{
  CoglTexture2DSliced *tex_2ds;
  CoglTexture         *tex;
  GLuint               gl_handle;
  int                  i;

  if (!cogl_is_texture (handle))
    return;

  tex_2ds = _cogl_texture_2d_sliced_pointer_from_handle (handle);
  tex = COGL_TEXTURE (tex_2ds);

  /* Make sure slices were created */
  if (tex_2ds->slice_gl_handles == NULL)
    return;

  if (min_filter == tex->min_filter
      && mag_filter == tex->mag_filter)
    return;

  /* Store new values */
  tex->min_filter = min_filter;
  tex->mag_filter = mag_filter;

  /* Apply new filters to every slice */
  for (i=0; i<tex_2ds->slice_gl_handles->len; ++i)
    {
      gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint, i);
      GE( glBindTexture (tex->gl_target, gl_handle) );
      GE( glTexParameteri (tex->gl_target, GL_TEXTURE_MAG_FILTER,
                           tex->mag_filter) );
      GE( glTexParameteri (tex->gl_target, GL_TEXTURE_MIN_FILTER,
                           tex->min_filter) );
    }
}

void
_cogl_texture_2d_sliced_ensure_mipmaps (CoglHandle handle)
{
  CoglTexture2DSliced *tex_2ds;
  CoglTexture         *tex;
  int                  i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_texture (handle))
    return;

  tex_2ds = _cogl_texture_2d_sliced_pointer_from_handle (handle);
  tex = COGL_TEXTURE (tex_2ds);

  /* Only update if the mipmaps are dirty */
  if (!tex->auto_mipmap || !tex->mipmaps_dirty)
    return;

  /* Make sure slices were created */
  if (tex_2ds->slice_gl_handles == NULL)
    return;

  /* Regenerate the mipmaps on every slice */
  for (i = 0; i < tex_2ds->slice_gl_handles->len; i++)
    {
      GLuint gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint, i);
      GE( glBindTexture (tex->gl_target, gl_handle) );

      /* glGenerateMipmap is defined in the FBO extension */
      if (cogl_features_available (COGL_FEATURE_OFFSCREEN))
        _cogl_texture_driver_gl_generate_mipmaps (tex->gl_target);
      else if (tex_2ds->first_pixels)
        {
          CoglTexturePixel *pixel = tex_2ds->first_pixels + i;
          /* Temporarily enable automatic mipmap generation and
             re-upload the first pixel to cause a regeneration */
          GE( glTexParameteri (tex->gl_target, GL_GENERATE_MIPMAP, GL_TRUE) );
          GE( glTexSubImage2D (tex->gl_target, 0, 0, 0, 1, 1,
                               pixel->gl_format, pixel->gl_type,
                               pixel->data) );
          GE( glTexParameteri (tex->gl_target, GL_GENERATE_MIPMAP, GL_FALSE) );
        }
    }

  tex->mipmaps_dirty = FALSE;
}

gboolean
_cogl_texture_2d_sliced_set_region (CoglHandle      handle,
                                    int             src_x,
                                    int             src_y,
                                    int             dst_x,
                                    int             dst_y,
                                    unsigned int    dst_width,
                                    unsigned int    dst_height,
                                    int             width,
                                    int             height,
                                    CoglPixelFormat format,
                                    unsigned int    rowstride,
                                    const guint8   *data)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (handle);
  CoglTexture         *tex = COGL_TEXTURE (handle);
  gint                 bpp;
  CoglBitmap           source_bmp;
  CoglBitmap           temp_bmp;
  gboolean             source_bmp_owner = FALSE;
  CoglPixelFormat      closest_format;
  GLenum               closest_gl_format;
  GLenum               closest_gl_type;
  gboolean             success;

  /* Check for valid format */
  if (format == COGL_PIXEL_FORMAT_ANY)
    return FALSE;

  /* Shortcut out early if the image is empty */
  if (width == 0 || height == 0)
    return TRUE;

  /* Init source bitmap */
  source_bmp.width = width;
  source_bmp.height = height;
  source_bmp.format = format;
  source_bmp.data = (guchar*)data;

  /* Rowstride from width if none specified */
  bpp = _cogl_get_format_bpp (format);
  source_bmp.rowstride = (rowstride == 0) ? width * bpp : rowstride;

  /* Find closest format to internal that's supported by GL */
  closest_format = _cogl_pixel_format_to_gl (tex->bitmap.format,
                                             NULL, /* don't need */
                                             &closest_gl_format,
                                             &closest_gl_type);

  /* If no direct match, convert */
  if (closest_format != format)
    {
      /* Convert to required format */
      success = _cogl_bitmap_convert_and_premult (&source_bmp,
                                                  &temp_bmp,
                                                  closest_format);

      /* Swap bitmaps if succeeded */
      if (!success) return FALSE;
      source_bmp = temp_bmp;
      source_bmp_owner = TRUE;
    }

  /* Send data to GL */
  _cogl_texture_2d_sliced_upload_subregion_to_gl (tex_2ds,
                                                  src_x, src_y,
                                                  dst_x, dst_y,
                                                  dst_width, dst_height,
                                                  &source_bmp,
                                                  closest_gl_format,
                                                  closest_gl_type);

  /* Free data if owner */
  if (source_bmp_owner)
    g_free (source_bmp.data);

  return TRUE;
}

gboolean
_cogl_texture_2d_sliced_download_from_gl (
                                      CoglTexture2DSliced *tex_2ds,
                                      CoglBitmap          *target_bmp,
                                      GLuint               target_gl_format,
                                      GLuint               target_gl_type)
{
  CoglTexture       *tex = COGL_TEXTURE (tex_2ds);
  CoglSpan  *x_span;
  CoglSpan  *y_span;
  GLuint             gl_handle;
  gint               bpp;
  gint               x,y;
  CoglBitmap         slice_bmp;

  bpp = _cogl_get_format_bpp (target_bmp->format);

  /* Iterate vertical slices */
  for (y = 0; y < tex_2ds->slice_y_spans->len; ++y)
    {
      y_span = &g_array_index (tex_2ds->slice_y_spans, CoglSpan, y);

      /* Iterate horizontal slices */
      for (x = 0; x < tex_2ds->slice_x_spans->len; ++x)
	{
	  /*if (x != 0 || y != 1) continue;*/
	  x_span = &g_array_index (tex_2ds->slice_x_spans, CoglSpan, x);

	  /* Pick the gl texture object handle */
	  gl_handle = g_array_index (tex_2ds->slice_gl_handles, GLuint,
				     y * tex_2ds->slice_x_spans->len + x);

	  /* If there's any waste we need to copy manually
             (no glGetTexSubImage) */

	  if (y_span->waste != 0 || x_span->waste != 0)
	    {
	      /* Setup temp bitmap for slice subregion */
	      slice_bmp.format = target_bmp->format;
	      slice_bmp.width  = x_span->size;
	      slice_bmp.height = y_span->size;
	      slice_bmp.rowstride = bpp * slice_bmp.width;
	      slice_bmp.data = (guchar*) g_malloc (slice_bmp.rowstride *
						   slice_bmp.height);

	      /* Setup gl alignment to 0,0 top-left corner */
	      _cogl_texture_driver_prep_gl_for_pixels_download (
                                                          slice_bmp.rowstride,
                                                          bpp);

	      /* Download slice image data into temp bmp */
	      GE( glBindTexture (tex->gl_target, gl_handle) );

              if (!_cogl_texture_driver_gl_get_tex_image (tex->gl_target,
                                                          target_gl_format,
                                                          target_gl_type,
                                                          slice_bmp.data))
                {
                  /* Free temp bitmap */
                  g_free (slice_bmp.data);
                  return FALSE;
                }

	      /* Copy portion of slice from temp to target bmp */
	      _cogl_bitmap_copy_subregion (&slice_bmp,
					   target_bmp,
					   0, 0,
					   x_span->start,
					   y_span->start,
					   x_span->size - x_span->waste,
					   y_span->size - y_span->waste);
	      /* Free temp bitmap */
	      g_free (slice_bmp.data);
	    }
	  else
	    {
	      GLvoid *dst = target_bmp->data
			    + x_span->start * bpp
			    + y_span->start * target_bmp->rowstride;

	      _cogl_texture_driver_prep_gl_for_pixels_download (
                                                        target_bmp->rowstride,
                                                        bpp);

	      /* Download slice image data */
	      GE( glBindTexture (tex->gl_target, gl_handle) );

              if (!_cogl_texture_driver_gl_get_tex_image (tex->gl_target,
                                                          target_gl_format,
                                                          target_gl_type,
                                                          dst))
                {
                  return FALSE;
                }
	    }
	}
    }

  return TRUE;
}

int
_cogl_texture_2d_sliced_get_data (CoglHandle       handle,
                                  CoglPixelFormat  format,
                                  unsigned int     rowstride,
                                  guint8          *data)
{
  CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (handle);
  CoglTexture         *tex = COGL_TEXTURE (handle);
  gint                 bpp;
  gint                 byte_size;
  CoglPixelFormat      closest_format;
  gint                 closest_bpp;
  GLenum               closest_gl_format;
  GLenum               closest_gl_type;
  CoglBitmap           target_bmp;
  CoglBitmap           new_bmp;
  gboolean             success;
  guchar              *src;
  guchar              *dst;
  gint                 y;

  /* Default to internal format if none specified */
  if (format == COGL_PIXEL_FORMAT_ANY)
    format = tex->bitmap.format;

  /* Rowstride from texture width if none specified */
  bpp = _cogl_get_format_bpp (format);
  if (rowstride == 0) rowstride = tex->bitmap.width * bpp;

  /* Return byte size if only that requested */
  byte_size =  tex->bitmap.height * rowstride;
  if (data == NULL) return byte_size;

  closest_format =
    _cogl_texture_driver_find_best_gl_get_data_format (format,
                                                       &closest_gl_format,
                                                       &closest_gl_type);
  closest_bpp = _cogl_get_format_bpp (closest_format);

  /* Is the requested format supported? */
  if (closest_format == format)
    {
      /* Target user data directly */
      target_bmp = tex->bitmap;
      target_bmp.format = format;
      target_bmp.rowstride = rowstride;
      target_bmp.data = data;
    }
  else
    {
      /* Target intermediate buffer */
      target_bmp = tex->bitmap;
      target_bmp.format = closest_format;
      target_bmp.rowstride = target_bmp.width * closest_bpp;
      target_bmp.data = (guchar*) g_malloc (target_bmp.height
                                            * target_bmp.rowstride);
    }

  /* Retrieve data from slices */
  if (!_cogl_texture_2d_sliced_download_from_gl (tex_2ds, &target_bmp,
                                                 closest_gl_format,
                                                 closest_gl_type))
    {
      /* XXX: In some cases _cogl_texture_2d_sliced_download_from_gl may
       * fail to read back the texture data; such as for GLES which doesn't
       * support glGetTexImage, so here we fallback to drawing the texture
       * and reading the pixels from the framebuffer. */
      _cogl_texture_draw_and_read (tex, &target_bmp,
                                   closest_gl_format,
                                   closest_gl_type);
    }

  /* Was intermediate used? */
  if (closest_format != format)
    {
      /* Convert to requested format */
      success = _cogl_bitmap_convert_and_premult (&target_bmp,
                                                  &new_bmp,
                                                  format);

      /* Free intermediate data and return if failed */
      g_free (target_bmp.data);
      if (!success) return 0;

      /* Copy to user buffer */
      for (y = 0; y < new_bmp.height; ++y)
        {
          src = new_bmp.data + y * new_bmp.rowstride;
          dst = data + y * rowstride;
          memcpy (dst, src, new_bmp.width);
        }

      /* Free converted data */
      g_free (new_bmp.data);
    }

  return byte_size;
}

