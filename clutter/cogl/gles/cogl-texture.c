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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-bitmap-private.h"
#include "cogl-texture-private.h"
#include "cogl-material.h"
#include "cogl-context.h"
#include "cogl-handle.h"

#include "cogl-gles2-wrapper.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

extern void _cogl_journal_flush (void);

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
_cogl_span_iter_begin (CoglSpanIter  *iter,
		       GArray        *array,
		       float   origin,
		       float   cover_start,
		       float   cover_end)
{
  /* Copy info */
  iter->index = 0;
  iter->array = array;
  iter->span = NULL;
  iter->origin = origin;
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

static void
prep_for_gl_pixels_upload (gint	       pixels_rowstride,
			   gint	       pixels_src_x,
			   gint	       pixels_src_y,
			   gint	       pixels_bpp)
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

static void
prep_for_gl_pixels_download (gint pixels_rowstride)
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
  CoglBitmap         slice_bmp;

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

	  /* FIXME: might optimize by not copying to intermediate slice
	     bitmap when source rowstride = bpp * width and the texture
	     image is not sliced */

	  /* Setup temp bitmap for slice subregion */
	  slice_bmp.format = tex->bitmap.format;
	  slice_bmp.width  = x_span->size - x_span->waste;
	  slice_bmp.height = y_span->size - y_span->waste;
	  slice_bmp.rowstride = bpp * slice_bmp.width;
	  slice_bmp.data = (guchar*) g_malloc (slice_bmp.rowstride *
					       slice_bmp.height);

	  /* Setup gl alignment to match rowstride and top-left corner */
	  prep_for_gl_pixels_upload (tex->bitmap.rowstride,
				     0,
				     0,
				     bpp);

	  /* Copy subregion data */
	  _cogl_bitmap_copy_subregion (&tex->bitmap,
				       &slice_bmp,
				       x_span->start,
				       y_span->start,
				       0, 0,
				       slice_bmp.width,
				       slice_bmp.height);

          /* Keep a copy of the first pixel if needed */
          if (tex->first_pixels)
            {
              memcpy (tex->first_pixels[slice_num].data,
                      slice_bmp.data,
                      bpp);
              tex->first_pixels[slice_num].gl_format = tex->gl_format;
              tex->first_pixels[slice_num].gl_type = tex->gl_type;
            }

	  /* Upload new image data */
	  GE( cogl_gles2_wrapper_bind_texture (tex->gl_target, gl_handle,
					       tex->gl_intformat) );

	  GE( glTexSubImage2D (tex->gl_target, 0,
			       0,
			       0,
			       slice_bmp.width,
			       slice_bmp.height,
			       tex->gl_format, tex->gl_type,
			       slice_bmp.data) );

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

              prep_for_gl_pixels_upload (x_span->waste * bpp,
					 0, /* src x */
					 0, /* src y */
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

              prep_for_gl_pixels_upload (x_span->size * bpp,
					 0, /* src x */
					 0, /* src y */
					 bpp);

              GE( glTexSubImage2D (tex->gl_target, 0,
                                   0,
				   y_span->size - y_span->waste,
                                   x_span->size,
                                   y_span->waste,
                                   tex->gl_format, tex->gl_type,
                                   waste_buf) );
            }

	  /* Free temp bitmap */
	  g_free (slice_bmp.data);
	}
    }

  if (waste_buf)
    g_free (waste_buf);

  tex->mipmaps_dirty = TRUE;

  return TRUE;
}

static void
_cogl_texture_draw_and_read (CoglTexture *tex,
                             CoglBitmap  *target_bmp,
                             GLint       *viewport)
{
  gint        bpp;
  float       rx1, ry1;
  float       rx2, ry2;
  float       tx1, ty1;
  float       tx2, ty2;
  int         bw,  bh;
  CoglBitmap  rect_bmp;
  CoglHandle  handle;

  handle = (CoglHandle) tex;
  bpp = _cogl_get_format_bpp (COGL_PIXEL_FORMAT_RGBA_8888);

  ry1 = 0; ry2 = 0;
  ty1 = 0; ty2 = 0;

  /* Walk Y axis until whole bitmap height consumed */
  for (bh = tex->bitmap.height; bh > 0; bh -= viewport[3])
    {
      /* Rectangle Y coords */
      ry1 = ry2;
      ry2 += (bh < viewport[3]) ? bh : viewport[3];

      /* Normalized texture Y coords */
      ty1 = ty2;
      ty2 = (ry2 / (float)tex->bitmap.height);

      rx1 = 0; rx2 = 0;
      tx1 = 0; tx2 = 0;

      /* Walk X axis until whole bitmap width consumed */
      for (bw = tex->bitmap.width; bw > 0; bw-=viewport[2])
        {
          /* Rectangle X coords */
          rx1 = rx2;
          rx2 += (bw < viewport[2]) ? bw : viewport[2];

          /* Normalized texture X coords */
          tx1 = tx2;
          tx2 = (rx2 / (float)tex->bitmap.width);

          /* Draw a portion of texture */
          cogl_rectangle_with_texture_coords (0, 0,
                                              rx2 - rx1,
                                              ry2 - ry1,
                                              tx1, ty1,
                                              tx2, ty2);

          /* Read into a temporary bitmap */
          rect_bmp.format = COGL_PIXEL_FORMAT_RGBA_8888;
          rect_bmp.width = rx2 - rx1;
          rect_bmp.height = ry2 - ry1;
          rect_bmp.rowstride = bpp * rect_bmp.width;
          rect_bmp.data = (guchar*) g_malloc (rect_bmp.rowstride *
                                              rect_bmp.height);

          prep_for_gl_pixels_download (rect_bmp.rowstride);
          GE( glReadPixels (viewport[0], viewport[1],
                            rect_bmp.width,
                            rect_bmp.height,
                            GL_RGBA, GL_UNSIGNED_BYTE,
                            rect_bmp.data) );

          /* Copy to target bitmap */
          _cogl_bitmap_copy_subregion (&rect_bmp,
                                       target_bmp,
                                       0,0,
                                       rx1,ry1,
                                       rect_bmp.width,
                                       rect_bmp.height);

          /* Free temp bitmap */
          g_free (rect_bmp.data);
        }
    }
}

static gboolean
_cogl_texture_download_from_gl (CoglTexture *tex,
				CoglBitmap  *target_bmp,
				GLuint       target_gl_format,
				GLuint       target_gl_type)
{
  gint       bpp;
  GLint      viewport[4];
  CoglBitmap alpha_bmp;
  CoglHandle prev_source;

  _COGL_GET_CONTEXT (ctx, FALSE);


  bpp = _cogl_get_format_bpp (COGL_PIXEL_FORMAT_RGBA_8888);

  /* Viewport needs to have some size and be inside the window for this */
  GE( glGetIntegerv (GL_VIEWPORT, viewport) );

  if (viewport[0] <  0 || viewport[1] <  0 ||
      viewport[2] <= 0 || viewport[3] <= 0)
    return FALSE;

  /* Setup orthographic projection into current viewport
     (0,0 in bottom-left corner to draw the texture
     upside-down so we match the way glReadPixels works) */

  _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
  _cogl_current_matrix_push ();
  _cogl_current_matrix_identity ();

  _cogl_current_matrix_ortho (0, (float)(viewport[2]),
                              0, (float)(viewport[3]),
                              (float)(0),
                              (float)(100));

  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
  _cogl_current_matrix_push ();
  _cogl_current_matrix_identity ();

  /* Direct copy operation */

  if (ctx->texture_download_material == COGL_INVALID_HANDLE)
    {
      ctx->texture_download_material = cogl_material_new ();
      cogl_material_set_blend (ctx->texture_download_material,
                               "RGBA = ADD (SRC_COLOR, 0)",
                               NULL);
    }

  prev_source = cogl_handle_ref (ctx->source_material);
  cogl_set_source (ctx->texture_download_material);

  cogl_material_set_layer (ctx->texture_download_material, 0, tex);

  cogl_material_set_layer_combine (ctx->texture_download_material,
                                   0, /* layer */
                                   "RGBA = REPLACE (TEXTURE)",
                                   NULL);

  _cogl_texture_draw_and_read (tex, target_bmp, viewport);

  /* Check whether texture has alpha and framebuffer not */
  /* FIXME: For some reason even if ALPHA_BITS is 8, the framebuffer
     still doesn't seem to have an alpha buffer. This might be just
     a PowerVR issue.
  GLint r_bits, g_bits, b_bits, a_bits;
  GE( glGetIntegerv (GL_ALPHA_BITS, &a_bits) );
  GE( glGetIntegerv (GL_RED_BITS, &r_bits) );
  GE( glGetIntegerv (GL_GREEN_BITS, &g_bits) );
  GE( glGetIntegerv (GL_BLUE_BITS, &b_bits) );
  printf ("R bits: %d\n", r_bits);
  printf ("G bits: %d\n", g_bits);
  printf ("B bits: %d\n", b_bits);
  printf ("A bits: %d\n", a_bits); */
  if ((tex->bitmap.format & COGL_A_BIT)/* && a_bits == 0*/)
    {
      guchar *srcdata;
      guchar *dstdata;
      guchar *srcpixel;
      guchar *dstpixel;
      gint    x,y;

      /* Create temp bitmap for alpha values */
      alpha_bmp.format = COGL_PIXEL_FORMAT_RGBA_8888;
      alpha_bmp.width = target_bmp->width;
      alpha_bmp.height = target_bmp->height;
      alpha_bmp.rowstride = bpp * alpha_bmp.width;
      alpha_bmp.data = (guchar*) g_malloc (alpha_bmp.rowstride *
                                           alpha_bmp.height);

      /* Draw alpha values into RGB channels */
      cogl_material_set_layer_combine (ctx->texture_download_material,
                                       0, /* layer */
                                       "RGBA = REPLACE (TEXTURE[A])",
                                       NULL);

      _cogl_texture_draw_and_read (tex, &alpha_bmp, viewport);

      /* Copy temp R to target A */
      srcdata = alpha_bmp.data;
      dstdata = target_bmp->data;

      for (y=0; y<target_bmp->height; ++y)
        {
          for (x=0; x<target_bmp->width; ++x)
            {
              srcpixel = srcdata + x*bpp;
              dstpixel = dstdata + x*bpp;
              dstpixel[3] = srcpixel[0];
            }
          srcdata += alpha_bmp.rowstride;
          dstdata += target_bmp->rowstride;
        }

      g_free (alpha_bmp.data);
    }

  /* Restore old state */
  _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
  _cogl_current_matrix_pop ();
  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
  _cogl_current_matrix_pop ();

  /* restore the original material */
  cogl_set_source (prev_source);
  cogl_handle_unref (prev_source);

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
  CoglBitmap        slice_bmp;

  bpp = _cogl_get_format_bpp (source_bmp->format);

  waste_buf = _cogl_texture_allocate_waste_buffer (tex);

  /* Iterate vertical spans */
  for (source_y = src_y,
       _cogl_span_iter_begin (&y_iter, tex->slice_y_spans,
			      0, (float)(dst_y),
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
				  0, (float)(dst_x),
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
	  inter_w =  (x_iter.intersect_end -
			               x_iter.intersect_start);
	  inter_h =  (y_iter.intersect_end -
				       y_iter.intersect_start);

	  /* Localize intersection top-left corner to slice*/
	  local_x =  (x_iter.intersect_start -
				       x_iter.pos);
	  local_y =  (y_iter.intersect_start -
				       y_iter.pos);

          slice_num = y_iter.index * tex->slice_x_spans->len + x_iter.index;

	  /* Pick slice GL handle */
	  gl_handle = g_array_index (tex->slice_gl_handles, GLuint, slice_num);

	/* FIXME: might optimize by not copying to intermediate slice
	   bitmap when source rowstride = bpp * width and the texture
	   image is not sliced */

	  /* Setup temp bitmap for slice subregion */
	  slice_bmp.format = tex->bitmap.format;
	  slice_bmp.width  = inter_w;
	  slice_bmp.height = inter_h;
	  slice_bmp.rowstride = bpp * slice_bmp.width;
	  slice_bmp.data = (guchar*) g_malloc (slice_bmp.rowstride *
					     slice_bmp.height);

	  /* Setup gl alignment to match rowstride and top-left corner */
	  prep_for_gl_pixels_upload (slice_bmp.rowstride,
				     0, /* src x */
				     0, /* src y */
				     bpp);

	  /* Copy subregion data */
	  _cogl_bitmap_copy_subregion (source_bmp,
				       &slice_bmp,
				       source_x,
				       source_y,
				       0, 0,
				       slice_bmp.width,
				       slice_bmp.height);

          /* Keep a copy of the first pixel if needed */
          if (tex->first_pixels && local_x == 0 && local_y == 0)
            {
              memcpy (tex->first_pixels[slice_num].data,
                      slice_bmp.data, bpp);
              tex->first_pixels[slice_num].gl_format = source_gl_format;
              tex->first_pixels[slice_num].gl_type = source_gl_type;
            }

	  /* Upload new image data */
	  GE( cogl_gles2_wrapper_bind_texture (tex->gl_target, gl_handle,
					       tex->gl_intformat) );

	  GE( glTexSubImage2D (tex->gl_target, 0,
			       local_x, local_y,
			       inter_w, inter_h,
			       source_gl_format,
			       source_gl_type,
			       slice_bmp.data) );

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

              prep_for_gl_pixels_upload (x_span->waste * bpp,
					 0, /* src x */
					 0, /* src y */
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

              prep_for_gl_pixels_upload (copy_width * bpp,
					 0, /* src x */
					 0, /* src y */
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

	  /* Free temp bitmap */
	  g_free (slice_bmp.data);
	}
    }

  if (waste_buf)
    g_free (waste_buf);

  tex->mipmaps_dirty = TRUE;

  return TRUE;
}

static gint
_cogl_rect_slices_for_size (gint     size_to_fill,
			    gint     max_span_size,
			    gint     max_waste,
			    GArray  *out_spans)
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
_cogl_pot_slices_for_size (gint     size_to_fill,
			   gint     max_span_size,
			   gint     max_waste,
			   GArray  *out_spans)
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

static gboolean
_cogl_texture_size_supported (GLenum gl_target,
			      GLenum gl_format,
			      GLenum gl_type,
			      int    width,
			      int    height)
{
  return TRUE;
}

void
_cogl_texture_set_wrap_mode_parameter (CoglTexture *tex,
                                       GLenum wrap_mode)
{
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
      if (!_cogl_texture_size_supported (tex->gl_target,
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
      while (!_cogl_texture_size_supported (tex->gl_target,
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
     slice. This is only needed to glGenerateMipmap (which is part of
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

#if COGL_DEBUG
	  printf ("CREATE SLICE (%d,%d)\n", x,y);
	  printf ("size: (%d x %d)\n",
	    x_span->size - x_span->waste,
	    y_span->size - y_span->waste);
#endif
	  /* Setup texture parameters */
	  GE( cogl_gles2_wrapper_bind_texture (tex->gl_target,
					       gl_handles[y * n_x_slices + x],
					       tex->gl_intformat) );
	  GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_S,
			       tex->wrap_mode) );
	  GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_T,
			       tex->wrap_mode) );

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
_cogl_pixel_format_from_gl_internal (GLenum            gl_int_format,
				     CoglPixelFormat  *out_format)
{
  return TRUE;
}

static CoglPixelFormat
_cogl_pixel_format_to_gl (CoglPixelFormat  format,
			  GLenum          *out_glintformat,
			  GLenum          *out_glformat,
			  GLenum          *out_gltype)
{
  CoglPixelFormat required_format;
  GLenum          glintformat = 0;
  GLenum          glformat = 0;
  GLenum          gltype = 0;

  /* If PREMULT_BIT isn't specified, that means that we premultiply
   * textures with alpha before uploading to GL; once we are in GL land,
   * everything is premultiplied.
   *
   * Everything else accepted (FIXME: check YUV support)
   */
  if ((format & COGL_A_BIT) != 0 &&
      format != COGL_PIXEL_FORMAT_A_8)
    required_format = format | COGL_PREMULT_BIT;
  else
    required_format = format;

  /* Find GL equivalents */
  switch (format & COGL_UNPREMULT_MASK)
    {
    case COGL_PIXEL_FORMAT_A_8:
      glintformat = GL_ALPHA;
      glformat = GL_ALPHA;
      gltype = GL_UNSIGNED_BYTE;
      break;
    case COGL_PIXEL_FORMAT_G_8:
      glintformat = GL_LUMINANCE;
      glformat = GL_LUMINANCE;
      gltype = GL_UNSIGNED_BYTE;
      break;

      /* Just one 24-bit ordering supported */
    case COGL_PIXEL_FORMAT_RGB_888:
    case COGL_PIXEL_FORMAT_BGR_888:
      glintformat = GL_RGB;
      glformat = GL_RGB;
      gltype = GL_UNSIGNED_BYTE;
      required_format = COGL_PIXEL_FORMAT_RGB_888;
      break;

      /* Just one 32-bit ordering supported */
    case COGL_PIXEL_FORMAT_RGBA_8888:
    case COGL_PIXEL_FORMAT_BGRA_8888:
    case COGL_PIXEL_FORMAT_ARGB_8888:
    case COGL_PIXEL_FORMAT_ABGR_8888:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_BYTE;
      required_format = COGL_PIXEL_FORMAT_RGBA_8888;
      required_format |= (format & COGL_PREMULT_BIT);
      break;

      /* The following three types of channel ordering
       * are always defined using system word byte
       * ordering (even according to GLES spec) */
    case COGL_PIXEL_FORMAT_RGB_565:
      glintformat = GL_RGB;
      glformat = GL_RGB;
      gltype = GL_UNSIGNED_SHORT_5_6_5;
      break;
    case COGL_PIXEL_FORMAT_RGBA_4444:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_SHORT_4_4_4_4;
      break;
    case COGL_PIXEL_FORMAT_RGBA_5551:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_SHORT_5_5_5_1;
      break;

      /* FIXME: check extensions for YUV support */
    default:
      break;
    }

  if (out_glintformat != NULL)
    *out_glintformat = glintformat;
  if (out_glformat != NULL)
    *out_glformat = glformat;
  if (out_gltype != NULL)
    *out_gltype = gltype;

  return required_format;
}

static gboolean
_cogl_texture_bitmap_prepare (CoglTexture     *tex,
			      CoglPixelFormat  internal_format)
{
  CoglBitmap        new_bitmap;
  CoglPixelFormat   new_data_format;
  gboolean          success;

  /* Was there any internal conversion requested? */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    internal_format = tex->bitmap.format;

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

  /* Create new texture and fill with loaded data */
  tex = (CoglTexture*) g_malloc ( sizeof (CoglTexture));

  tex->is_foreign = FALSE;
  tex->auto_mipmap = (flags & COGL_TEXTURE_NO_AUTO_MIPMAP) == 0;
  tex->mipmaps_dirty = TRUE;
  tex->first_pixels = NULL;

  tex->bitmap = *bmp;
  tex->bitmap_owner = TRUE;
  bmp->data = NULL;

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
  CoglHandle   bmp;
  CoglHandle   handle;

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

  /* Allow 2-dimensional textures only */
  if (gl_target != GL_TEXTURE_2D)
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
  if (!_cogl_pixel_format_from_gl_internal (gl_int_format,
					    &format))
    {
      return COGL_INVALID_HANDLE;
    }

  /* Create new texture */
  tex = (CoglTexture*) g_malloc ( sizeof (CoglTexture));

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
        GE( cogl_wrap_glGenerateMipmap (tex->gl_target) );
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

  /* Find closest format that's supported by GL
     (Can't use _cogl_pixel_format_to_gl since available formats
      when reading pixels on GLES are severely limited) */
  closest_format = COGL_PIXEL_FORMAT_RGBA_8888;
  closest_gl_format = GL_RGBA;
  closest_gl_type = GL_UNSIGNED_BYTE;
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
  _cogl_texture_download_from_gl (tex, &target_bmp,
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

