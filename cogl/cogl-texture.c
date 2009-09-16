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
#include "cogl-texture-driver.h"
#include "cogl-material.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-primitives.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

static void _cogl_texture_free (CoglTexture *tex);

COGL_HANDLE_DEFINE (Texture, texture);

static void
_cogl_texture_bitmap_free (CoglTexture *tex)
{
  if (tex->bitmap.data != NULL && tex->bitmap_owner)
    g_free (tex->bitmap.data);

  tex->bitmap.data = NULL;
  tex->bitmap_owner = FALSE;
}

static void
_cogl_texture_bitmap_swap (CoglTexture     *tex,
			   CoglBitmap      *new_bitmap)
{
  if (tex->bitmap.data != NULL && tex->bitmap_owner)
    g_free (tex->bitmap.data);

  tex->bitmap = *new_bitmap;
  tex->bitmap_owner = TRUE;
}

static void
_cogl_span_iter_update (CoglSpanIter *iter)
{
  /* Pick current span */
  iter->span = &g_array_index (iter->array,
			       CoglTexSliceSpan,
			       iter->index);

  /* Offset next position by span size */
  iter->next_pos = iter->pos +
    (float)(iter->span->size - iter->span->waste);

  /* Check if span intersects the area to cover */
  if (iter->next_pos <= iter->cover_start ||
      iter->pos >= iter->cover_end)
    {
      /* Intersection undefined */
      iter->intersects = FALSE;
      return;
    }

  iter->intersects = TRUE;

  /* Clip start position to coverage area */
  if (iter->pos < iter->cover_start)
    iter->intersect_start = iter->cover_start;
  else
    iter->intersect_start = iter->pos;

  /* Clip end position to coverage area */
  if (iter->next_pos > iter->cover_end)
    iter->intersect_end = iter->cover_end;
  else
    iter->intersect_end = iter->next_pos;
}

void
_cogl_span_iter_begin (CoglSpanIter         *iter,
                       GArray               *spans,
                       float                 normalize_factor,
                       float                 cover_start,
                       float                 cover_end)
{
  float cover_start_normalized;

  iter->index = 0;
  iter->span = NULL;

  iter->array = spans;

  /* We always iterate in a positive direction from the origin. If
   * iter->flipped == TRUE that means whoever is using this API should
   * interpreted the current span as extending in the opposite direction. I.e.
   * it extends to the left if iterating the X axis, or up if the Y axis. */
  if (cover_start > cover_end)
    {
      float tmp = cover_start;
      cover_start = cover_end;
      cover_end = tmp;
      iter->flipped = TRUE;
    }
  else
    iter->flipped = FALSE;

  /* The texture spans cover the normalized texture coordinate space ranging
   * from [0,1] but to help support repeating of sliced textures we allow
   * iteration of any range so we need to relate the start of the range to the
   * nearest point equivalent to 0.
   */
  cover_start_normalized = cover_start / normalize_factor;
  iter->origin = floorf (cover_start_normalized) * normalize_factor;

  iter->cover_start = cover_start;
  iter->cover_end = cover_end;
  iter->pos = iter->origin;

  /* Update intersection */
  _cogl_span_iter_update (iter);
}

void
_cogl_span_iter_next (CoglSpanIter *iter)
{
  /* Move current position */
  iter->pos = iter->next_pos;

  /* Pick next slice (wrap when last reached) */
  iter->index = (iter->index + 1) % iter->array->len;

  /* Update intersection */
  _cogl_span_iter_update (iter);
}

gboolean
_cogl_span_iter_end (CoglSpanIter *iter)
{
  /* End reached when whole area covered */
  return iter->pos >= iter->cover_end;
}

/* Some CoglTextures, notably sliced textures or atlas textures when repeating
 * is used, will need to divide the coordinate space into multiple GL textures
 * (or rather; in the case of atlases duplicate a single texture in multiple
 * positions to handle repeating)
 *
 * This function helps you implement primitives using such textures by
 * invoking a callback once for each sub texture that intersects a given
 * region specified in texture coordinates.
 */
/* To differentiate between texture coordinates of a specific, real, slice
 * texture and the texture coordinates of the composite, sliced texture, the
 * coordinates of the sliced texture are called "virtual" coordinates and the
 * coordinates of slices are called "slice" coordinates. */
/* This function lets you iterate all the slices that lie within the given
 * virtual coordinates of the parent sliced texture. */
/* Note: no guarantee is given about the order in which the slices will be
 * visited */
void
_cogl_texture_foreach_sub_texture_in_region (CoglHandle handle,
                                             float virtual_tx_1,
                                             float virtual_ty_1,
                                             float virtual_tx_2,
                                             float virtual_ty_2,
                                             CoglTextureSliceCallback callback,
                                             void *user_data)
{
  CoglTexture *tex = _cogl_texture_pointer_from_handle (handle);
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
                              tex->slice_y_spans,
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
                                  tex->slice_x_spans,
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
	  gl_handle = g_array_index (tex->slice_gl_handles, GLuint,
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

void
_cogl_texture_prep_gl_alignment_for_pixels_upload (int pixels_rowstride)
{
  if (!(pixels_rowstride & 0x7))
    GE( glPixelStorei (GL_UNPACK_ALIGNMENT, 8) );
  else if (!(pixels_rowstride & 0x3))
    GE( glPixelStorei (GL_UNPACK_ALIGNMENT, 4) );
  else if (!(pixels_rowstride & 0x1))
    GE( glPixelStorei (GL_UNPACK_ALIGNMENT, 2) );
  else
    GE( glPixelStorei (GL_UNPACK_ALIGNMENT, 1) );
}

void
_cogl_texture_prep_gl_alignment_for_pixels_download (int pixels_rowstride)
{
  if (!(pixels_rowstride & 0x7))
    GE( glPixelStorei (GL_PACK_ALIGNMENT, 8) );
  else if (!(pixels_rowstride & 0x3))
    GE( glPixelStorei (GL_PACK_ALIGNMENT, 4) );
  else if (!(pixels_rowstride & 0x1))
    GE( glPixelStorei (GL_PACK_ALIGNMENT, 2) );
  else
    GE( glPixelStorei (GL_PACK_ALIGNMENT, 1) );
}

static guchar *
_cogl_texture_allocate_waste_buffer (CoglTexture *tex)
{
  CoglTexSliceSpan *last_x_span;
  CoglTexSliceSpan *last_y_span;
  guchar           *waste_buf = NULL;

  /* If the texture has any waste then allocate a buffer big enough to
     fill the gaps */
  last_x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan,
                                tex->slice_x_spans->len - 1);
  last_y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan,
                                tex->slice_y_spans->len - 1);
  if (last_x_span->waste > 0 || last_y_span->waste > 0)
    {
      gint bpp = _cogl_get_format_bpp (tex->bitmap.format);
      CoglTexSliceSpan  *first_x_span
        = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, 0);
      CoglTexSliceSpan  *first_y_span
        = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, 0);
      guint right_size = first_y_span->size * last_x_span->waste;
      guint bottom_size = first_x_span->size * last_y_span->waste;

      waste_buf = g_malloc (MAX (right_size, bottom_size) * bpp);
    }

  return waste_buf;
}

static gboolean
_cogl_texture_upload_to_gl (CoglTexture *tex)
{
  CoglTexSliceSpan  *x_span;
  CoglTexSliceSpan  *y_span;
  GLuint             gl_handle;
  gint               bpp;
  gint               x,y;
  guchar            *waste_buf;

  bpp = _cogl_get_format_bpp (tex->bitmap.format);

  waste_buf = _cogl_texture_allocate_waste_buffer (tex);

  /* Iterate vertical slices */
  for (y = 0; y < tex->slice_y_spans->len; ++y)
    {
      y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, y);

      /* Iterate horizontal slices */
      for (x = 0; x < tex->slice_x_spans->len; ++x)
	{
          gint slice_num = y * tex->slice_x_spans->len + x;

	  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, x);

	  /* Pick the gl texture object handle */
	  gl_handle = g_array_index (tex->slice_gl_handles, GLuint, slice_num);

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
          if (tex->first_pixels)
            {
              memcpy (tex->first_pixels[slice_num].data,
                      tex->bitmap.data + x_span->start * bpp
                      + y_span->start * tex->bitmap.rowstride,
                      bpp);
              tex->first_pixels[slice_num].gl_format = tex->gl_format;
              tex->first_pixels[slice_num].gl_type = tex->gl_type;
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
_cogl_texture_upload_subregion_to_gl (CoglTexture *tex,
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
  CoglTexSliceSpan *x_span;
  CoglTexSliceSpan *y_span;
  gint              bpp;
  CoglSpanIter      x_iter;
  CoglSpanIter      y_iter;
  GLuint            gl_handle;
  gint              source_x = 0, source_y = 0;
  gint              inter_w = 0, inter_h = 0;
  gint              local_x = 0, local_y = 0;
  guchar           *waste_buf;

  bpp = _cogl_get_format_bpp (source_bmp->format);

  waste_buf = _cogl_texture_allocate_waste_buffer (tex);

  /* Iterate vertical spans */
  for (source_y = src_y,
       _cogl_span_iter_begin (&y_iter, tex->slice_y_spans,
			      tex->bitmap.height, (float)(dst_y),
			      (float)(dst_y + height));

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

      y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan,
                               y_iter.index);

      /* Iterate horizontal spans */
      for (source_x = src_x,
	   _cogl_span_iter_begin (&x_iter, tex->slice_x_spans,
				  tex->bitmap.width, (float)(dst_x),
				  (float)(dst_x + width));

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

          x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan,
                                   x_iter.index);

	  /* Pick intersection width and height */
	  inter_w =  (x_iter.intersect_end - x_iter.intersect_start);
	  inter_h =  (y_iter.intersect_end - y_iter.intersect_start);

	  /* Localize intersection top-left corner to slice*/
	  local_x =  (x_iter.intersect_start - x_iter.pos);
	  local_y =  (y_iter.intersect_start - y_iter.pos);

          slice_num = y_iter.index * tex->slice_x_spans->len + x_iter.index;

	  /* Pick slice GL handle */
	  gl_handle = g_array_index (tex->slice_gl_handles, GLuint, slice_num);

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
          if (tex->first_pixels && local_x == 0 && local_y == 0)
            {
              memcpy (tex->first_pixels[slice_num].data,
                      source_bmp->data + source_x * bpp
                      + source_y * source_bmp->rowstride,
                      bpp);
              tex->first_pixels[slice_num].gl_format = source_gl_format;
              tex->first_pixels[slice_num].gl_type = source_gl_type;
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
_cogl_rect_slices_for_size (gint    size_to_fill,
			    gint    max_span_size,
			    gint    max_waste,
			    GArray *out_spans)
{
  gint             n_spans = 0;
  CoglTexSliceSpan span;

  /* Init first slice span */
  span.start = 0;
  span.size = max_span_size;
  span.waste = 0;

  /* Repeat until whole area covered */
  while (size_to_fill >= span.size)
    {
      /* Add another slice span of same size */
      if (out_spans) g_array_append_val (out_spans, span);
      span.start   += span.size;
      size_to_fill -= span.size;
      n_spans++;
    }

  /* Add one last smaller slice span */
  if (size_to_fill > 0)
    {
      span.size = size_to_fill;
      if (out_spans) g_array_append_val (out_spans, span);
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
  CoglTexSliceSpan span;

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
_cogl_texture_set_wrap_mode_parameter (CoglHandle handle,
                                       GLenum wrap_mode)
{
  CoglTexture *tex = _cogl_texture_pointer_from_handle (handle);

  /* Only set the wrap mode if it's different from the current
     value to avoid too many GL calls */
  if (tex->wrap_mode != wrap_mode)
    {
      int i;

      /* Any queued texture rectangles may be depending on the previous
       * wrap mode... */
      _cogl_journal_flush ();

      for (i = 0; i < tex->slice_gl_handles->len; i++)
        {
          GLuint texnum = g_array_index (tex->slice_gl_handles, GLuint, i);

          GE( glBindTexture (tex->gl_target, texnum) );
          GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_S, wrap_mode) );
          GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_T, wrap_mode) );
        }

      tex->wrap_mode = wrap_mode;
    }
}

static gboolean
_cogl_texture_slices_create (CoglTexture *tex)
{
  gint              bpp;
  gint              max_width;
  gint              max_height;
  GLuint           *gl_handles;
  gint              n_x_slices;
  gint              n_y_slices;
  gint              n_slices;
  gint              x, y;
  CoglTexSliceSpan *x_span;
  CoglTexSliceSpan *y_span;
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
  if (tex->max_waste <= -1)
    {
      CoglTexSliceSpan span;

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
      tex->slice_x_spans = g_array_sized_new (FALSE, FALSE,
					      sizeof (CoglTexSliceSpan),
					      1);

      tex->slice_y_spans = g_array_sized_new (FALSE, FALSE,
					      sizeof (CoglTexSliceSpan),
					      1);

      /* Add a single span for width and height */
      span.start = 0;
      span.size = max_width;
      span.waste = max_width - tex->bitmap.width;
      g_array_append_val (tex->slice_x_spans, span);

      span.size = max_height;
      span.waste = max_height - tex->bitmap.height;
      g_array_append_val (tex->slice_y_spans, span);
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
				    max_width, tex->max_waste,
				    NULL);

      n_y_slices = slices_for_size (tex->bitmap.height,
				    max_height, tex->max_waste,
				    NULL);

      /* Init span arrays with reserved size */
      tex->slice_x_spans = g_array_sized_new (FALSE, FALSE,
					      sizeof (CoglTexSliceSpan),
					      n_x_slices);

      tex->slice_y_spans = g_array_sized_new (FALSE, FALSE,
					      sizeof (CoglTexSliceSpan),
					      n_y_slices);

      /* Fill span arrays with info */
      slices_for_size (tex->bitmap.width,
		       max_width, tex->max_waste,
		       tex->slice_x_spans);

      slices_for_size (tex->bitmap.height,
		       max_height, tex->max_waste,
		       tex->slice_y_spans);
    }

  /* Init and resize GL handle array */
  n_slices = n_x_slices * n_y_slices;

  tex->slice_gl_handles = g_array_sized_new (FALSE, FALSE,
					     sizeof (GLuint),
					     n_slices);

  g_array_set_size (tex->slice_gl_handles, n_slices);

  /* Allocate some space to store a copy of the first pixel of each
     slice. This is only needed if glGenerateMipmap (which is part of
     the FBO extension) is not available */
  if (cogl_features_available (COGL_FEATURE_OFFSCREEN))
    tex->first_pixels = NULL;
  else
    tex->first_pixels = g_new (CoglTexturePixel, n_slices);

  /* Wrap mode not yet set */
  tex->wrap_mode = GL_FALSE;

  /* Generate a "working set" of GL texture objects
   * (some implementations might supported faster
   *  re-binding between textures inside a set) */
  gl_handles = (GLuint*) tex->slice_gl_handles->data;

  GE( glGenTextures (n_slices, gl_handles) );


  /* Init each GL texture object */
  for (y = 0; y < n_y_slices; ++y)
    {
      y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, y);

      for (x = 0; x < n_x_slices; ++x)
	{
	  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, x);

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
_cogl_texture_slices_free (CoglTexture *tex)
{
  if (tex->slice_x_spans != NULL)
    g_array_free (tex->slice_x_spans, TRUE);

  if (tex->slice_y_spans != NULL)
    g_array_free (tex->slice_y_spans, TRUE);

  if (tex->slice_gl_handles != NULL)
    {
      if (tex->is_foreign == FALSE)
	{
	  GE( glDeleteTextures (tex->slice_gl_handles->len,
				(GLuint*) tex->slice_gl_handles->data) );
	}

      g_array_free (tex->slice_gl_handles, TRUE);
    }

  if (tex->first_pixels != NULL)
    g_free (tex->first_pixels);
}

gboolean
_cogl_texture_span_has_waste (CoglTexture *tex,
                              gint x_span_index,
                              gint y_span_index)
{
  CoglTexSliceSpan *x_span;
  CoglTexSliceSpan *y_span;

  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, x_span_index);
  y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, y_span_index);

  return (x_span->waste || y_span->waste) ? TRUE : FALSE;
}

static gboolean
_cogl_texture_bitmap_prepare (CoglTexture     *tex,
			      CoglPixelFormat  internal_format)
{
  CoglBitmap        new_bitmap;
  CoglPixelFormat   new_data_format;
  gboolean          success;

  /* Was there any internal conversion requested?
   * By default Cogl will use a premultiplied internal format. Later we will
   * add control over this. */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    {
      if ((tex->bitmap.format & COGL_A_BIT) &&
          tex->bitmap.format != COGL_PIXEL_FORMAT_A_8)
        internal_format = tex->bitmap.format | COGL_PREMULT_BIT;
      else
        internal_format = tex->bitmap.format;
    }

  /* Find closest format accepted by GL */
  new_data_format = _cogl_pixel_format_to_gl (internal_format,
					      &tex->gl_intformat,
					      &tex->gl_format,
					      &tex->gl_type);

  /* Convert to internal format */
  if (new_data_format != tex->bitmap.format)
    {
      success = _cogl_bitmap_convert_and_premult (&tex->bitmap,
						  &new_bitmap,
						  new_data_format);

      if (!success)
	return FALSE;

      /* Update texture with new data */
      _cogl_texture_bitmap_swap (tex, &new_bitmap);
    }

  return TRUE;
}

static void
_cogl_texture_free (CoglTexture *tex)
{
  /* Frees texture resources but its handle is not
     released! Do that separately before this! */
  _cogl_texture_bitmap_free (tex);
  _cogl_texture_slices_free (tex);
  g_free (tex);
}

CoglHandle
cogl_texture_new_with_size (guint            width,
			    guint            height,
                            CoglTextureFlags flags,
			    CoglPixelFormat  internal_format)
{
  CoglTexture *tex;
  gint         bpp;
  gint         rowstride;

  /* Since no data, we need some internal format */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    return COGL_INVALID_HANDLE;

  /* Rowstride from width */
  bpp = _cogl_get_format_bpp (internal_format);
  rowstride = width * bpp;

  /* Init texture with empty bitmap */
  tex = (CoglTexture*) g_malloc (sizeof (CoglTexture));

  tex->is_foreign = FALSE;
  tex->auto_mipmap = (flags & COGL_TEXTURE_NO_AUTO_MIPMAP) == 0;
  tex->mipmaps_dirty = TRUE;
  tex->first_pixels = NULL;

  tex->bitmap.width = width;
  tex->bitmap.height = height;
  tex->bitmap.format = internal_format;
  tex->bitmap.rowstride = rowstride;
  tex->bitmap.data = NULL;
  tex->bitmap_owner = FALSE;

  tex->slice_x_spans = NULL;
  tex->slice_y_spans = NULL;
  tex->slice_gl_handles = NULL;

  if (flags & COGL_TEXTURE_NO_SLICING)
    tex->max_waste = -1;
  else
    tex->max_waste = COGL_TEXTURE_MAX_WASTE;

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
  if (!_cogl_texture_slices_create (tex))
    {
      _cogl_texture_free (tex);
      return COGL_INVALID_HANDLE;
    }

  return _cogl_texture_handle_new (tex);
}

CoglHandle
cogl_texture_new_from_data (guint             width,
			    guint             height,
                            CoglTextureFlags  flags,
			    CoglPixelFormat   format,
			    CoglPixelFormat   internal_format,
			    guint             rowstride,
			    const guchar     *data)
{
  CoglTexture *tex;
  gint         bpp;

  if (format == COGL_PIXEL_FORMAT_ANY)
    return COGL_INVALID_HANDLE;

  if (data == NULL)
    return COGL_INVALID_HANDLE;

  /* Rowstride from width if not given */
  bpp = _cogl_get_format_bpp (format);
  if (rowstride == 0) rowstride = width * bpp;

  /* Create new texture and fill with given data */
  tex = (CoglTexture*) g_malloc (sizeof (CoglTexture));

  tex->is_foreign = FALSE;
  tex->auto_mipmap = (flags & COGL_TEXTURE_NO_AUTO_MIPMAP) == 0;
  tex->mipmaps_dirty = TRUE;
  tex->first_pixels = NULL;

  tex->bitmap.width = width;
  tex->bitmap.height = height;
  tex->bitmap.data = (guchar*)data;
  tex->bitmap.format = format;
  tex->bitmap.rowstride = rowstride;
  tex->bitmap_owner = FALSE;

  tex->slice_x_spans = NULL;
  tex->slice_y_spans = NULL;
  tex->slice_gl_handles = NULL;

  if (flags & COGL_TEXTURE_NO_SLICING)
    tex->max_waste = -1;
  else
    tex->max_waste = COGL_TEXTURE_MAX_WASTE;

  /* Unknown filter */
  tex->min_filter = GL_FALSE;
  tex->mag_filter = GL_FALSE;

  /* FIXME: If upload fails we should set some kind of
   * error flag but still return texture handle (this
   * is to keep the behavior equal to _new_from_file;
   * see below) */

  if (!_cogl_texture_bitmap_prepare (tex, internal_format))
    {
      _cogl_texture_free (tex);
      return COGL_INVALID_HANDLE;
    }

  if (!_cogl_texture_slices_create (tex))
    {
      _cogl_texture_free (tex);
      return COGL_INVALID_HANDLE;
    }

  if (!_cogl_texture_upload_to_gl (tex))
    {
      _cogl_texture_free (tex);
      return COGL_INVALID_HANDLE;
    }

  _cogl_texture_bitmap_free (tex);

  return _cogl_texture_handle_new (tex);
}

CoglHandle
cogl_texture_new_from_bitmap (CoglHandle       bmp_handle,
                              CoglTextureFlags flags,
                              CoglPixelFormat  internal_format)
{
  CoglTexture *tex;
  CoglBitmap *bmp = (CoglBitmap *)bmp_handle;

  g_return_val_if_fail (bmp_handle != COGL_INVALID_HANDLE, COGL_INVALID_HANDLE);

  /* Create new texture and fill with loaded data */
  tex = (CoglTexture*) g_malloc ( sizeof (CoglTexture));

  tex->is_foreign = FALSE;
  tex->auto_mipmap = (flags & COGL_TEXTURE_NO_AUTO_MIPMAP) == 0;
  tex->mipmaps_dirty = TRUE;
  tex->first_pixels = NULL;

  tex->bitmap = *bmp;
  tex->bitmap_owner = FALSE;

  tex->slice_x_spans = NULL;
  tex->slice_y_spans = NULL;
  tex->slice_gl_handles = NULL;

  if (flags & COGL_TEXTURE_NO_SLICING)
    tex->max_waste = -1;
  else
    tex->max_waste = COGL_TEXTURE_MAX_WASTE;

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
      _cogl_texture_free (tex);
      return COGL_INVALID_HANDLE;
    }

  if (!_cogl_texture_slices_create (tex))
    {
      _cogl_texture_free (tex);
      return COGL_INVALID_HANDLE;
    }

  if (!_cogl_texture_upload_to_gl (tex))
    {
      _cogl_texture_free (tex);
      return COGL_INVALID_HANDLE;
    }

  _cogl_texture_bitmap_free (tex);

  return _cogl_texture_handle_new (tex);
}

CoglHandle
cogl_texture_new_from_file (const gchar       *filename,
                            CoglTextureFlags   flags,
                            CoglPixelFormat    internal_format,
                            GError           **error)
{
  CoglHandle bmp;
  CoglHandle handle;

  g_return_val_if_fail (error == NULL || *error == NULL, COGL_INVALID_HANDLE);

  bmp = cogl_bitmap_new_from_file (filename, error);
  if (bmp == COGL_INVALID_HANDLE)
    return COGL_INVALID_HANDLE;

  handle = cogl_texture_new_from_bitmap (bmp, flags, internal_format);
  cogl_handle_unref (bmp);

  return handle;
}

CoglHandle
cogl_texture_new_from_foreign (GLuint           gl_handle,
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

  GLenum           gl_error = 0;
  GLboolean        gl_istexture;
  GLint            gl_compressed = GL_FALSE;
  GLint            gl_int_format = 0;
  GLint            gl_width = 0;
  GLint            gl_height = 0;
  GLint            gl_gen_mipmap;
  guint            bpp;
  CoglTexture     *tex;
  CoglTexSliceSpan x_span;
  CoglTexSliceSpan y_span;

  if (!_cogl_texture_driver_allows_foreign_gl_target (gl_target))
    return COGL_INVALID_HANDLE;

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
  tex = (CoglTexture *) g_malloc (sizeof (CoglTexture));

  /* Setup bitmap info */
  tex->is_foreign = TRUE;
  tex->auto_mipmap = (gl_gen_mipmap == GL_TRUE) ? TRUE : FALSE;
  tex->mipmaps_dirty = TRUE;
  tex->first_pixels = NULL;

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
  tex->max_waste = 0;

  /* Wrap mode not yet set */
  tex->wrap_mode = GL_FALSE;

  /* Create slice arrays */
  tex->slice_x_spans =
    g_array_sized_new (FALSE, FALSE,
		       sizeof (CoglTexSliceSpan), 1);

  tex->slice_y_spans =
    g_array_sized_new (FALSE, FALSE,
		       sizeof (CoglTexSliceSpan), 1);

  tex->slice_gl_handles =
    g_array_sized_new (FALSE, FALSE,
		       sizeof (GLuint), 1);

  /* Store info for a single slice */
  x_span.start = 0;
  x_span.size = gl_width;
  x_span.waste = x_pot_waste;
  g_array_append_val (tex->slice_x_spans, x_span);

  y_span.start = 0;
  y_span.size = gl_height;
  y_span.waste = y_pot_waste;
  g_array_append_val (tex->slice_y_spans, y_span);

  g_array_append_val (tex->slice_gl_handles, gl_handle);

  tex->first_pixels = NULL;

  return _cogl_texture_handle_new (tex);
}

guint
cogl_texture_get_width (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = _cogl_texture_pointer_from_handle (handle);

  return tex->bitmap.width;
}

guint
cogl_texture_get_height (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = _cogl_texture_pointer_from_handle (handle);

  return tex->bitmap.height;
}

CoglPixelFormat
cogl_texture_get_format (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return COGL_PIXEL_FORMAT_ANY;

  tex = _cogl_texture_pointer_from_handle (handle);

  return tex->bitmap.format;
}

guint
cogl_texture_get_rowstride (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = _cogl_texture_pointer_from_handle (handle);

  return tex->bitmap.rowstride;
}

gint
cogl_texture_get_max_waste (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = _cogl_texture_pointer_from_handle (handle);

  return tex->max_waste;
}

gboolean
cogl_texture_is_sliced (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return FALSE;

  tex = _cogl_texture_pointer_from_handle (handle);

  if (tex->slice_gl_handles == NULL)
    return FALSE;

  if (tex->slice_gl_handles->len <= 1)
    return FALSE;

  return TRUE;
}

gboolean
_cogl_texture_can_hardware_repeat (CoglHandle handle)
{
  CoglTexture *tex = _cogl_texture_pointer_from_handle (handle);
  CoglTexSliceSpan *x_span;
  CoglTexSliceSpan *y_span;

#if HAVE_COGL_GL
  if (tex->gl_target == GL_TEXTURE_RECTANGLE_ARB)
    return FALSE;
#endif

  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, 0);
  y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, 0);

  return (x_span->waste || y_span->waste) ? FALSE : TRUE;
}

void
_cogl_texture_transform_coords_to_gl (CoglHandle handle,
                                      float *s,
                                      float *t)
{
  CoglTexture *tex = _cogl_texture_pointer_from_handle (handle);
  CoglTexSliceSpan *x_span;
  CoglTexSliceSpan *y_span;

  g_assert (!cogl_texture_is_sliced (tex));

  /* Don't include the waste in the texture coordinates */
  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, 0);
  y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, 0);

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

GLenum
_cogl_texture_get_internal_gl_format (CoglHandle handle)
{
  CoglTexture *tex = _cogl_texture_pointer_from_handle (handle);

  return tex->gl_intformat;
}

gboolean
cogl_texture_get_gl_texture (CoglHandle handle,
			     GLuint *out_gl_handle,
			     GLenum *out_gl_target)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return FALSE;

  tex = _cogl_texture_pointer_from_handle (handle);

  if (tex->slice_gl_handles == NULL)
    return FALSE;

  if (tex->slice_gl_handles->len < 1)
    return FALSE;

  if (out_gl_handle != NULL)
    *out_gl_handle = g_array_index (tex->slice_gl_handles, GLuint, 0);

  if (out_gl_target != NULL)
    *out_gl_target = tex->gl_target;

  return TRUE;
}

void
_cogl_texture_set_filters (CoglHandle handle,
                           GLenum min_filter,
                           GLenum mag_filter)
{
  CoglTexture *tex;
  GLuint       gl_handle;
  int          i;

  if (!cogl_is_texture (handle))
    return;

  tex = _cogl_texture_pointer_from_handle (handle);

  /* Make sure slices were created */
  if (tex->slice_gl_handles == NULL)
    return;

  if (min_filter == tex->min_filter
      && mag_filter == tex->mag_filter)
    return;

  /* Store new values */
  tex->min_filter = min_filter;
  tex->mag_filter = mag_filter;

  /* Apply new filters to every slice */
  for (i=0; i<tex->slice_gl_handles->len; ++i)
    {
      gl_handle = g_array_index (tex->slice_gl_handles, GLuint, i);
      GE( glBindTexture (tex->gl_target, gl_handle) );
      GE( glTexParameteri (tex->gl_target, GL_TEXTURE_MAG_FILTER,
			   tex->mag_filter) );
      GE( glTexParameteri (tex->gl_target, GL_TEXTURE_MIN_FILTER,
			   tex->min_filter) );
    }
}

void
_cogl_texture_ensure_mipmaps (CoglHandle handle)
{
  CoglTexture *tex;
  int          i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!cogl_is_texture (handle))
    return;

  tex = _cogl_texture_pointer_from_handle (handle);

  /* Only update if the mipmaps are dirty */
  if (!tex->auto_mipmap || !tex->mipmaps_dirty)
    return;

  /* Make sure slices were created */
  if (tex->slice_gl_handles == NULL)
    return;

  /* Regenerate the mipmaps on every slice */
  for (i = 0; i < tex->slice_gl_handles->len; i++)
    {
      GLuint gl_handle = g_array_index (tex->slice_gl_handles, GLuint, i);
      GE( glBindTexture (tex->gl_target, gl_handle) );

      /* glGenerateMipmap is defined in the FBO extension */
      if (cogl_features_available (COGL_FEATURE_OFFSCREEN))
        _cogl_texture_driver_gl_generate_mipmaps (tex->gl_target);
      else if (tex->first_pixels)
        {
          CoglTexturePixel *pixel = tex->first_pixels + i;
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
cogl_texture_set_region (CoglHandle       handle,
			 gint             src_x,
			 gint             src_y,
			 gint             dst_x,
			 gint             dst_y,
			 guint            dst_width,
			 guint            dst_height,
			 gint             width,
			 gint             height,
			 CoglPixelFormat  format,
			 guint            rowstride,
			 const guchar    *data)
{
  CoglTexture     *tex;
  gint             bpp;
  CoglBitmap       source_bmp;
  CoglBitmap       temp_bmp;
  gboolean         source_bmp_owner = FALSE;
  CoglPixelFormat  closest_format;
  GLenum           closest_gl_format;
  GLenum           closest_gl_type;
  gboolean         success;

  /* Check if valid texture handle */
  if (!cogl_is_texture (handle))
    return FALSE;

  tex = _cogl_texture_pointer_from_handle (handle);

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
  _cogl_texture_upload_subregion_to_gl (tex,
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

gint
cogl_texture_get_data (CoglHandle       handle,
		       CoglPixelFormat  format,
		       guint            rowstride,
		       guchar          *data)
{
  CoglTexture      *tex;
  gint              bpp;
  gint              byte_size;
  CoglPixelFormat   closest_format;
  gint              closest_bpp;
  GLenum            closest_gl_format;
  GLenum            closest_gl_type;
  CoglBitmap        target_bmp;
  CoglBitmap        new_bmp;
  gboolean          success;
  guchar           *src;
  guchar           *dst;
  gint              y;

  /* Check if valid texture handle */
  if (!cogl_is_texture (handle))
    return 0;

  tex = _cogl_texture_pointer_from_handle (handle);

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
  _cogl_texture_driver_download_from_gl (tex, &target_bmp,
                                         closest_gl_format,
                                         closest_gl_type);

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

