/*
 * Clutter COGL
 *
 * A basic GL/GLES Abstraction/Utility Layer
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2007 OpenedHand
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
#include "cogl-bitmap.h"
#include "cogl-texture-private.h"
#include "cogl-material.h"
#include "cogl-context.h"
#include "cogl-handle.h"

#include "cogl-gles2-wrapper.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

/*
#define COGL_DEBUG 1

#define GE(x) \
{ \
  glGetError(); x; \
  GLuint err = glGetError(); \
  if (err != 0) \
    printf("err: 0x%x\n", err); \
} */

#ifdef HAVE_COGL_GL

#define glDrawRangeElements ctx->pf_glDrawRangeElements

#else

/* GLES doesn't have glDrawRangeElements, so we simply pretend it does
 * but that it makes no use of the start, end constraints: */
#define glDrawRangeElements(mode, start, end, count, type, indices) \
  glDrawElements (mode, count, type, indices)

#endif

static void _cogl_journal_flush (void);

static void _cogl_texture_free (CoglTexture *tex);

COGL_HANDLE_DEFINE (Texture, texture, texture_handles);

struct _CoglSpanIter
{
  gint              index;
  GArray           *array;
  CoglTexSliceSpan *span;
  float      pos;
  float      next_pos;
  float      origin;
  float      cover_start;
  float      cover_end;
  float      intersect_start;
  float      intersect_end;
  float      intersect_start_local;
  float      intersect_end_local;
  gboolean          intersects;
};

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

static void
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

static gboolean
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
	  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, x);

	  /* Pick the gl texture object handle */
	  gl_handle = g_array_index (tex->slice_gl_handles, GLuint,
				     y * tex->slice_x_spans->len + x);

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

	  if (tex->auto_mipmap)
	    cogl_wrap_glGenerateMipmap (tex->gl_target);

	  /* Free temp bitmap */
	  g_free (slice_bmp.data);
	}
    }

  if (waste_buf)
    g_free (waste_buf);

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

  GE( glMatrixMode (GL_PROJECTION) );
  GE( glPushMatrix () );
  GE( glLoadIdentity () );

  GE( glOrthof (0, (float)(viewport[2]),
			  0, (float)(viewport[3]),
			  (float)(0),
			  (float)(100)) );

  GE( glMatrixMode (GL_MODELVIEW) );
  GE( glPushMatrix () );
  GE( glLoadIdentity () );

  /* Draw to all channels */
  cogl_draw_buffer (COGL_WINDOW_BUFFER | COGL_MASK_BUFFER, 0);

  /* Direct copy operation */

  if (ctx->texture_download_material == COGL_INVALID_HANDLE)
    {
      ctx->texture_download_material = cogl_material_new ();
      cogl_material_set_layer_combine_function (
                                    ctx->texture_download_material,
                                    0, /* layer */
                                    COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB,
                                    COGL_MATERIAL_LAYER_COMBINE_FUNC_REPLACE);
      cogl_material_set_layer_combine_arg_src (
                                    ctx->texture_download_material,
                                    0, /* layer */
                                    0, /* arg */
                                    COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB,
                                    COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE);
      cogl_material_set_blend_factors (ctx->texture_download_material,
                                       COGL_MATERIAL_BLEND_FACTOR_ONE,
                                       COGL_MATERIAL_BLEND_FACTOR_ZERO);
    }

  cogl_material_set_layer (ctx->texture_download_material, 0, tex);

  cogl_material_set_layer_combine_arg_op (
                                  ctx->texture_download_material,
                                  0, /* layer */
                                  0, /* arg */
                                  COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB,
                                  COGL_MATERIAL_LAYER_COMBINE_OP_SRC_COLOR);
  cogl_material_flush_gl_state (ctx->texture_download_material, NULL);
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
      cogl_material_set_layer_combine_arg_op (
                                  ctx->texture_download_material,
                                  0, /* layer */
                                  0, /* arg */
                                  COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB,
                                  COGL_MATERIAL_LAYER_COMBINE_OP_SRC_ALPHA);
      cogl_material_flush_gl_state (ctx->texture_download_material, NULL);
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
  glMatrixMode (GL_PROJECTION);
  glPopMatrix ();
  glMatrixMode (GL_MODELVIEW);
  glPopMatrix ();

  cogl_draw_buffer (COGL_WINDOW_BUFFER, 0);

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

	  /* Pick slice GL handle */
	  gl_handle = g_array_index (tex->slice_gl_handles, GLuint,
				     y_iter.index * tex->slice_x_spans->len +
				     x_iter.index);

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

	  if (tex->auto_mipmap)
	    cogl_wrap_glGenerateMipmap (tex->gl_target);

	  /* Free temp bitmap */
	  g_free (slice_bmp.data);
	}
    }

  if (waste_buf)
    g_free (waste_buf);

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
  if (max_waste < 0) max_waste = 0;

  while (TRUE)
    {
      /* Is the whole area covered? */
      if (size_to_fill > span.size)
	{
	  /* Not yet - add a span of this size */
	  if (out_spans) g_array_append_val (out_spans, span);
	  span.start   += span.size;
	  size_to_fill -= span.size;
	  n_spans++;
	}
      else if (span.size - size_to_fill <= max_waste)
	{
	  /* Yes and waste is small enough */
	  span.waste = span.size - size_to_fill;
	  if (out_spans) g_array_append_val (out_spans, span);
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

static void
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
	  GE( glTexParameteri (tex->gl_target, GL_TEXTURE_MAG_FILTER,
			       tex->mag_filter) );
	  GE( glTexParameteri (tex->gl_target, GL_TEXTURE_MIN_FILTER,
			       tex->min_filter) );

	  GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_S,
			       tex->wrap_mode) );
	  GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_T,
			       tex->wrap_mode) );

          if (tex->auto_mipmap)
            GE( glTexParameteri (tex->gl_target, GL_GENERATE_MIPMAP,
				 GL_TRUE) );

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

  /* No premultiplied formats accepted  by GL
   * (FIXME: latest hardware?) */

  if (format & COGL_PREMULT_BIT)
    format = (format & COGL_UNPREMULT_MASK);

  /* Everything else accepted
   * (FIXME: check YUV support) */
  required_format = format;

  /* Find GL equivalents */
  switch (format)
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
cogl_texture_new_with_size (guint             width,
			    guint             height,
			    gint              max_waste,
                            CoglTextureFlags  flags,
			    CoglPixelFormat   internal_format)
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

  tex->ref_count = 1;
  COGL_HANDLE_DEBUG_NEW (texture, tex);

  tex->is_foreign = FALSE;
  tex->auto_mipmap = ((flags & COGL_TEXTURE_AUTO_MIPMAP) != 0);

  tex->bitmap.width = width;
  tex->bitmap.height = height;
  tex->bitmap.format = internal_format;
  tex->bitmap.rowstride = rowstride;
  tex->bitmap.data = NULL;
  tex->bitmap_owner = FALSE;

  tex->slice_x_spans = NULL;
  tex->slice_y_spans = NULL;
  tex->slice_gl_handles = NULL;

  tex->max_waste = max_waste;
  tex->min_filter = CGL_NEAREST;
  tex->mag_filter = CGL_NEAREST;

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
			    gint              max_waste,
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

  tex->ref_count = 1;
  COGL_HANDLE_DEBUG_NEW (texture, tex);

  tex->is_foreign = FALSE;
  tex->auto_mipmap = ((flags & COGL_TEXTURE_AUTO_MIPMAP) != 0);

  tex->bitmap.width = width;
  tex->bitmap.height = height;
  tex->bitmap.data = (guchar*)data;
  tex->bitmap.format = format;
  tex->bitmap.rowstride = rowstride;
  tex->bitmap_owner = FALSE;

  tex->slice_x_spans = NULL;
  tex->slice_y_spans = NULL;
  tex->slice_gl_handles = NULL;

  tex->max_waste = max_waste;
  tex->min_filter = CGL_NEAREST;
  tex->mag_filter = CGL_NEAREST;

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
cogl_texture_new_from_bitmap (CoglBitmap       *bmp,
                              gint              max_waste,
                              CoglTextureFlags  flags,
                              CoglPixelFormat   internal_format)
{
  CoglTexture *tex;

  /* Create new texture and fill with loaded data */
  tex = (CoglTexture*) g_malloc ( sizeof (CoglTexture));

  tex->ref_count = 1;
  COGL_HANDLE_DEBUG_NEW (texture, tex);

  tex->is_foreign = FALSE;
  tex->auto_mipmap = ((flags & COGL_TEXTURE_AUTO_MIPMAP) != 0);

  tex->bitmap = *bmp;
  tex->bitmap_owner = TRUE;
  bmp->data = NULL;

  tex->slice_x_spans = NULL;
  tex->slice_y_spans = NULL;
  tex->slice_gl_handles = NULL;

  tex->max_waste = max_waste;
  tex->min_filter = CGL_NEAREST;
  tex->mag_filter = CGL_NEAREST;

  /* FIXME: If upload fails we should set some kind of
   * error flag but still return texture handle if the
   * user decides to destroy another texture and upload
   * this one instead (reloading from file is not needed
   * in that case). As a rule then, everytime a valid
   * CoglHandle is returned, it should also be destroyed
   * with cogl_texture_unref at some point! */

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
                            gint               max_waste,
                            CoglTextureFlags   flags,
                            CoglPixelFormat    internal_format,
                            GError           **error)
{
  CoglBitmap  *bmp;
  CoglHandle   handle;

  g_return_val_if_fail (error == NULL || *error == NULL, COGL_INVALID_HANDLE);

  if (!(bmp = cogl_bitmap_new_from_file (filename, error)))
    return COGL_INVALID_HANDLE;

  handle = cogl_texture_new_from_bitmap (bmp,
                                         max_waste,
                                         flags,
                                         internal_format);
  cogl_bitmap_free (bmp);

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
  GLint            gl_min_filter;
  GLint            gl_mag_filter;
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
			   GL_TEXTURE_MIN_FILTER,
			   &gl_min_filter) );

  GE( glGetTexParameteriv (gl_target,
			   GL_TEXTURE_MAG_FILTER,
			   &gl_mag_filter) );

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

  tex->ref_count = 1;
  COGL_HANDLE_DEBUG_NEW (texture, tex);

  /* Setup bitmap info */
  tex->is_foreign = TRUE;
  tex->auto_mipmap = (gl_gen_mipmap == GL_TRUE) ? TRUE : FALSE;

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

  tex->min_filter = gl_min_filter;
  tex->mag_filter = gl_mag_filter;
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

COGLenum
cogl_texture_get_min_filter (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = _cogl_texture_pointer_from_handle (handle);

  return tex->min_filter;
}

COGLenum
cogl_texture_get_mag_filter (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = _cogl_texture_pointer_from_handle (handle);

  return tex->mag_filter;
}

void
cogl_texture_set_filters (CoglHandle handle,
			  COGLenum   min_filter,
			  COGLenum   mag_filter)
{
  CoglTexture *tex;
  GLuint       gl_handle;
  int          i;

  if (!cogl_is_texture (handle))
    return;

  tex = _cogl_texture_pointer_from_handle (handle);

  /* Store new values */
  tex->min_filter = min_filter;
  tex->mag_filter = mag_filter;

  /* Make sure slices were created */
  if (tex->slice_gl_handles == NULL)
    return;

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


/******************************************************************************
 * XXX: Here ends the code that strictly implements "CoglTextures".
 *
 * The following consists of code for rendering rectangles and polygons. It
 * might be neater to move this code somewhere else. I think everything below
 * here should be implementable without access to CoglTexture internals, but
 * that will at least mean exposing the cogl_span_iter_* funcs.
 */

static void
_cogl_journal_flush_quad_batch (CoglJournalEntry *batch_start,
                                gint              batch_len,
                                GLfloat          *vertex_pointer)
{
  int     needed_indices;
  gsize   stride;
  int     i;
  gulong  enable_flags = 0;
  guint32 disable_mask;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* The indices are always the same sequence regardless of the vertices so we
   * only need to change it if there are more vertices than ever before. */
  needed_indices = batch_len * 6;
  if (needed_indices > ctx->static_indices->len)
    {
      int old_len = ctx->static_indices->len;
      int vert_num = old_len / 6 * 4;
      GLushort *q;

      /* Add two triangles for each quad to the list of
         indices. That makes six new indices but two of the
         vertices in the triangles are shared. */
      g_array_set_size (ctx->static_indices, needed_indices);
      q = &g_array_index (ctx->static_indices, GLushort, old_len);

      for (i = old_len;
           i < ctx->static_indices->len;
           i += 6, vert_num += 4)
        {
          *(q++) = vert_num + 0;
          *(q++) = vert_num + 1;
          *(q++) = vert_num + 3;

          *(q++) = vert_num + 1;
          *(q++) = vert_num + 2;
          *(q++) = vert_num + 3;
        }
    }

  /* XXX NB:
   * Our vertex data is arranged as follows:
   * 4 vertices per quad: 2 GLfloats per position,
   *                      2 GLfloats per tex coord * n_layers
   */
  stride = 2 + 2 * batch_start->n_layers;
  stride *= sizeof (GLfloat);

  disable_mask = (1 << batch_start->n_layers) - 1;
  disable_mask = ~disable_mask;

  cogl_material_flush_gl_state (ctx->source_material,
                                COGL_MATERIAL_FLUSH_FALLBACK_MASK,
                                batch_start->fallback_mask,
                                COGL_MATERIAL_FLUSH_DISABLE_MASK,
                                disable_mask,
                                /* Redundant when dealing with unsliced
                                 * textures but does no harm... */
                                COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE,
                                batch_start->layer0_override_texture,
                                NULL);

  for (i = 0; i < batch_start->n_layers; i++)
    {
      GE (glClientActiveTexture (GL_TEXTURE0 + i));
      GE (glEnableClientState (GL_TEXTURE_COORD_ARRAY));
      GE (glTexCoordPointer (2, GL_FLOAT, stride, vertex_pointer + 2 + 2 * i));
    }
  /* XXX: Without this we get a segfault with the PVR SDK.
   * We should probably be doing this for cogl/gl too though. */
  for (; i < ctx->n_texcoord_arrays_enabled; i++)
    {
      GE (glClientActiveTexture (GL_TEXTURE0 + i));
      GE (glDisableClientState (GL_TEXTURE_COORD_ARRAY));
    }
  ctx->n_texcoord_arrays_enabled = i + 1;

  /* FIXME: This api is a bit yukky, ideally it will be removed if we
   * re-work the cogl_enable mechanism */
  enable_flags |= cogl_material_get_cogl_enable_flags (ctx->source_material);

  if (ctx->enable_backface_culling)
    enable_flags |= COGL_ENABLE_BACKFACE_CULLING;

  enable_flags |= COGL_ENABLE_VERTEX_ARRAY;
  cogl_enable (enable_flags);

  GE (glVertexPointer (2, GL_FLOAT, stride, vertex_pointer));

  GE (glDrawRangeElements (GL_TRIANGLES,
                           0, ctx->static_indices->len - 1,
                           6 * batch_len,
                           GL_UNSIGNED_SHORT,
                           ctx->static_indices->data));
}

static void
_cogl_journal_flush (void)
{
  GLfloat          *current_vertex_pointer;
  GLfloat          *batch_vertex_pointer;
  CoglJournalEntry *batch_start;
  guint             batch_len;
  int               i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->journal->len == 0)
    return;

  /* Current non-variables / constraints:
   *
   * - We don't have to worry about much GL state changing between journal
   *   entries since currently the journal never out lasts a single call to
   *   _cogl_multitexture_multiple_rectangles. So the user doesn't get the
   *   chance to fiddle with anything. (XXX: later this will be extended at
   *   which point we can start logging certain state changes)
   *
   * - Implied from above: all entries will refer to the same material.
   *
   * - Although _cogl_multitexture_multiple_rectangles can cause the wrap mode
   *   of textures to be modified, the journal is flushed if a wrap mode is
   *   changed so we don't currently have to log wrap mode changes.
   *
   * - XXX - others?
   */

  /* TODO: "compile" the journal to find ways of batching draw calls and vertex
   * data.
   *
   * Simple E.g. given current constraints...
   * pass 0 - load all data into a single CoglVertexBuffer
   * pass 1 - batch gl draw calls according to entries that use the same
   *          textures.
   *
   * We will be able to do cooler stuff here when we extend the life of
   * journals beyond _cogl_multitexture_multiple_rectangles.
   */

  batch_vertex_pointer = (GLfloat *)ctx->logged_vertices->data;
  batch_start = (CoglJournalEntry *)ctx->journal->data;
  batch_len = 1;

  current_vertex_pointer = batch_vertex_pointer;

  for (i = 1; i < ctx->journal->len; i++)
    {
      CoglJournalEntry *prev_entry =
        &g_array_index (ctx->journal, CoglJournalEntry, i - 1);
      CoglJournalEntry *current_entry = prev_entry + 1;
      gsize             stride;

      /* Progress the vertex pointer */
      stride = 2 + current_entry->n_layers * 2;
      current_vertex_pointer += stride;

#warning "NB: re-enable batching"
#if 1
      /* batch rectangles using the same textures */
      if (current_entry->material == prev_entry->material &&
          current_entry->n_layers == prev_entry->n_layers &&
          current_entry->fallback_mask == prev_entry->fallback_mask &&
          current_entry->layer0_override_texture
          == prev_entry->layer0_override_texture)
        {
          batch_len++;
          continue;
        }
#endif

      _cogl_journal_flush_quad_batch (batch_start,
                                      batch_len,
                                      batch_vertex_pointer);

      batch_start = current_entry;
      batch_len = 1;
      batch_vertex_pointer = current_vertex_pointer;
    }

  /* The last batch... */
  _cogl_journal_flush_quad_batch (batch_start,
                                  batch_len,
                                  batch_vertex_pointer);


  g_array_set_size (ctx->journal, 0);
  g_array_set_size (ctx->logged_vertices, 0);
}

static void
_cogl_journal_log_quad (float       x1,
                        float       y1,
                        float       x2,
                        float       y2,
                        CoglHandle  material,
                        gint        n_layers,
                        guint32     fallback_mask,
                        GLuint      layer0_override_texture,
                        float      *tex_coords,
                        guint       tex_coords_len)
{
  int               stride;
  int               next_vert;
  GLfloat          *v;
  int               i;
  int               next_entry;
  CoglJournalEntry *entry;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* The vertex data is logged into a seperate array in a layout that can be
   * directly passed to OpenGL
   */

  /* We pack the vertex data as 2 (x,y) GLfloats folowed by 2 (tx,ty) GLfloats
   * for each texture being used, E.g.:
   *  [X, Y, TX0, TY0, TX1, TY1, X, Y, TX0, TY0, X, Y, ...]
   */
  stride = 2 + n_layers * 2;

  next_vert = ctx->logged_vertices->len;
  g_array_set_size (ctx->logged_vertices, next_vert + 4 * stride);
  v = &g_array_index (ctx->logged_vertices, GLfloat, next_vert);

  /* XXX: All the jumping around to fill in this strided buffer doesn't
   * seem ideal. */

  /* XXX: we could defer expanding the vertex data for GL until we come
   * to flushing the journal. */

  v[0] = x1; v[1] = y1;
  v += stride;
  v[0] = x1; v[1] = y2;
  v += stride;
  v[0] = x2; v[1] = y2;
  v += stride;
  v[0] = x2; v[1] = y1;

  for (i = 0; i < n_layers; i++)
    {
      GLfloat *t =
        &g_array_index (ctx->logged_vertices, GLfloat, next_vert + 2 + 2 * i);

      t[0] = tex_coords[0]; t[1] = tex_coords[1];
      t += stride;
      t[0] = tex_coords[0]; t[1] = tex_coords[3];
      t += stride;
      t[0] = tex_coords[2]; t[1] = tex_coords[3];
      t += stride;
      t[0] = tex_coords[2]; t[1] = tex_coords[1];
    }

  next_entry = ctx->journal->len;
  g_array_set_size (ctx->journal, next_entry + 1);
  entry = &g_array_index (ctx->journal, CoglJournalEntry, next_entry);

  entry->material = material;
  entry->n_layers = n_layers;
  entry->fallback_mask = fallback_mask;
  entry->layer0_override_texture = layer0_override_texture;
}

static void
_cogl_texture_sliced_quad (CoglTexture *tex,
                           CoglHandle   material,
		           float        x1,
		           float        y1,
		           float        x2,
		           float        y2,
		           float        tx1,
		           float        ty1,
		           float        tx2,
		           float        ty2)
{
  CoglSpanIter  iter_x    ,  iter_y;
  float         tw        ,  th;
  float         tqx       ,  tqy;
  float         first_tx  ,  first_ty;
  float         first_qx  ,  first_qy;
  float         slice_tx1 ,  slice_ty1;
  float         slice_tx2 ,  slice_ty2;
  float         slice_qx1 ,  slice_qy1;
  float         slice_qx2 ,  slice_qy2;
  GLuint        gl_handle;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

#if COGL_DEBUG
  printf("=== Drawing Tex Quad (Sliced Mode) ===\n");
#endif

  /* We can't use hardware repeat so we need to set clamp to edge
     otherwise it might pull in edge pixels from the other side */
  _cogl_texture_set_wrap_mode_parameter (tex, GL_CLAMP_TO_EDGE);

  /* If the texture coordinates are backwards then swap both the
     geometry and texture coordinates so that the texture will be
     flipped but we can still use the same algorithm to iterate the
     slices */
  if (tx2 < tx1)
    {
      float temp = x1;
      x1 = x2;
      x2 = temp;
      temp = tx1;
      tx1 = tx2;
      tx2 = temp;
    }
  if (ty2 < ty1)
    {
      float temp = y1;
      y1 = y2;
      y2 = temp;
      temp = ty1;
      ty1 = ty2;
      ty2 = temp;
    }

  /* Scale ratio from texture to quad widths */
  tw = (float)(tex->bitmap.width);
  th = (float)(tex->bitmap.height);

  tqx = (x2 - x1) / (tw * (tx2 - tx1));
  tqy = (y2 - y1) / (th * (ty2 - ty1));

  /* Integral texture coordinate for first tile */
  first_tx = (float)(floorf (tx1));
  first_ty = (float)(floorf (ty1));

  /* Denormalize texture coordinates */
  first_tx = (first_tx * tw);
  first_ty = (first_ty * th);
  tx1 = (tx1 * tw);
  ty1 = (ty1 * th);
  tx2 = (tx2 * tw);
  ty2 = (ty2 * th);

  /* Quad coordinate of the first tile */
  first_qx = x1 - (tx1 - first_tx) * tqx;
  first_qy = y1 - (ty1 - first_ty) * tqy;


  /* Iterate until whole quad height covered */
  for (_cogl_span_iter_begin (&iter_y, tex->slice_y_spans,
			      first_ty, ty1, ty2) ;
       !_cogl_span_iter_end  (&iter_y) ;
       _cogl_span_iter_next  (&iter_y) )
    {
      float tex_coords[4];

      /* Discard slices out of quad early */
      if (!iter_y.intersects) continue;

      /* Span-quad intersection in quad coordinates */
      slice_qy1 = first_qy + (iter_y.intersect_start - first_ty) * tqy;

      slice_qy2 = first_qy + (iter_y.intersect_end - first_ty) * tqy;

      /* Localize slice texture coordinates */
      slice_ty1 = iter_y.intersect_start - iter_y.pos;
      slice_ty2 = iter_y.intersect_end - iter_y.pos;

      slice_ty1 /= iter_y.span->size;
      slice_ty2 /= iter_y.span->size;

      /* Iterate until whole quad width covered */
      for (_cogl_span_iter_begin (&iter_x, tex->slice_x_spans,
				  first_tx, tx1, tx2) ;
	   !_cogl_span_iter_end  (&iter_x) ;
	   _cogl_span_iter_next  (&iter_x) )
        {
	  /* Discard slices out of quad early */
	  if (!iter_x.intersects) continue;

	  /* Span-quad intersection in quad coordinates */
	  slice_qx1 = first_qx + (iter_x.intersect_start - first_tx) * tqx;

	  slice_qx2 = first_qx + (iter_x.intersect_end - first_tx) * tqx;

	  /* Localize slice texture coordinates */
	  slice_tx1 = iter_x.intersect_start - iter_x.pos;
	  slice_tx2 = iter_x.intersect_end - iter_x.pos;

	  /* Normalize texture coordinates to current slice
             (rectangle texture targets take denormalized) */
          slice_tx1 /= iter_x.span->size;
          slice_tx2 /= iter_x.span->size;

#if COGL_DEBUG
	  printf("~~~~~ slice (%d,%d)\n", iter_x.index, iter_y.index);
	  printf("qx1: %f\n",  (slice_qx1));
	  printf("qy1: %f\n",  (slice_qy1));
	  printf("qx2: %f\n",  (slice_qx2));
	  printf("qy2: %f\n",  (slice_qy2));
	  printf("tx1: %f\n",  (slice_tx1));
	  printf("ty1: %f\n",  (slice_ty1));
	  printf("tx2: %f\n",  (slice_tx2));
	  printf("ty2: %f\n",  (slice_ty2));
#endif

	  /* Pick and bind opengl texture object */
	  gl_handle = g_array_index (tex->slice_gl_handles, GLuint,
				     iter_y.index * iter_x.array->len +
				     iter_x.index);

          tex_coords[0] = slice_tx1;
          tex_coords[1] = slice_ty1;
          tex_coords[2] = slice_tx2;
          tex_coords[3] = slice_ty2;
          _cogl_journal_log_quad (slice_qx1,
                                  slice_qy1,
                                  slice_qx2,
                                  slice_qy2,
                                  material,
                                  1, /* one layer */
                                  0, /* don't need to use fallbacks */
                                  gl_handle, /* replace the layer0 texture */
                                  tex_coords,
                                  4);
	}
    }
}

static gboolean
_cogl_multitexture_unsliced_quad (float        x1,
                                  float        y1,
                                  float        x2,
                                  float        y2,
                                  CoglHandle   material,
                                  gint         n_layers,
                                  guint32      fallback_mask,
                                  const float *user_tex_coords,
                                  gint         user_tex_coords_len)
{
  float   *final_tex_coords = alloca (sizeof (float) * 4 * n_layers);
  const GList *layers;
  GList       *tmp;
  int          i;

  _COGL_GET_CONTEXT (ctx, FALSE);

  /*
   * Validate the texture coordinates for this rectangle.
   */
  layers = cogl_material_get_layers (material);
  for (tmp = (GList *)layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle         layer = (CoglHandle)tmp->data;
      /* CoglLayerInfo   *layer_info; */
      CoglHandle         tex_handle;
      CoglTexture       *tex;
      const float       *in_tex_coords;
      float             *out_tex_coords;
      CoglTexSliceSpan  *x_span;
      CoglTexSliceSpan  *y_span;

      /* layer_info = &layers[i]; */

      /* FIXME - we shouldn't be checking this stuff if layer_info->gl_texture
       * already == 0 */

      tex_handle = cogl_material_layer_get_texture (layer);
      tex = _cogl_texture_pointer_from_handle (tex_handle);

      in_tex_coords = &user_tex_coords[i * 4];
      out_tex_coords = &final_tex_coords[i * 4];


      /* If the texture has waste or we are using GL_TEXTURE_RECT we
       * can't handle texture repeating so we check that the texture
       * coords lie in the range [0,1].
       *
       * NB: We already know that no texture matrix is being used
       * if the texture has waste since we validated that early on.
       * TODO: check for a texture matrix in the GL_TEXTURE_RECT
       * case.
       */
      if (_cogl_texture_span_has_waste (tex, 0, 0)
          && i < user_tex_coords_len / 4
          && (in_tex_coords[0] < 0 || in_tex_coords[0] > 1.0
              || in_tex_coords[1] < 0 || in_tex_coords[1] > 1.0
              || in_tex_coords[2] < 0 || in_tex_coords[2] > 1.0
              || in_tex_coords[3] < 0 || in_tex_coords[3] > 1.0))
        {
          if (i == 0)
            {
              if (n_layers > 1)
                {
                  g_warning ("Skipping layers 1..n of your material since the "
                             "first layer has waste and you supplied texture "
                             "coordinates outside the range [0,1]. We don't "
                             "currently support any multi-texturing using "
                             "textures with waste when repeating is "
                             "necissary so we are falling back to sliced "
                             "textures assuming layer 0 is the most "
                             "important one keep");
                }
              return FALSE;
            }
          else
            {
              g_warning ("Skipping layer %d of your material "
                         "consisting of a texture with waste since "
                         "you have supplied texture coords outside "
                         "the range [0,1] (unsupported when "
                         "multi-texturing)", i);

              /* NB: marking for fallback will replace the layer with
               * a default transparent texture */
              fallback_mask |= (1 << i);
            }
        }


      /*
       * Setup the texture unit...
       */

      /* NB: The user might not have supplied texture coordinates for all
       * layers... */
      if (i < (user_tex_coords_len / 4))
        {
          GLenum wrap_mode;

          /* If the texture coords are all in the range [0,1] then we want to
             clamp the coords to the edge otherwise it can pull in edge pixels
             from the wrong side when scaled */
          if (in_tex_coords[0] >= 0 && in_tex_coords[0] <= 1.0
              && in_tex_coords[1] >= 0 && in_tex_coords[1] <= 1.0
              && in_tex_coords[2] >= 0 && in_tex_coords[2] <= 1.0
              && in_tex_coords[3] >= 0 && in_tex_coords[3] <= 1.0)
            wrap_mode = GL_CLAMP_TO_EDGE;
          else
            wrap_mode = GL_REPEAT;

          memcpy (out_tex_coords, in_tex_coords, sizeof (GLfloat) * 4);

          _cogl_texture_set_wrap_mode_parameter (tex, wrap_mode);
        }
      else
        {
          out_tex_coords[0] = 0; /* tx1 */
          out_tex_coords[1] = 0; /* ty1 */
          out_tex_coords[2] = 1.0; /* tx2 */
          out_tex_coords[3] = 1.0; /* ty2 */

          _cogl_texture_set_wrap_mode_parameter (tex, GL_CLAMP_TO_EDGE);
        }

      /* Don't include the waste in the texture coordinates */
      x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, 0);
      y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, 0);

      out_tex_coords[0] =
        out_tex_coords[0] * (x_span->size - x_span->waste) / x_span->size;
      out_tex_coords[1] =
        out_tex_coords[1] * (x_span->size - x_span->waste) / x_span->size;
      out_tex_coords[2] =
        out_tex_coords[2] * (y_span->size - y_span->waste) / y_span->size;
      out_tex_coords[3] =
        out_tex_coords[3] * (y_span->size - y_span->waste) / y_span->size;
    }

  _cogl_journal_log_quad (x1,
                          y1,
                          x2,
                          y2,
                          material,
                          n_layers,
                          fallback_mask,
                          0, /* don't replace the layer0 texture */
                          final_tex_coords,
                          n_layers * 4);

  return TRUE;
}

struct _CoglMutiTexturedRect
{
  float        x1;
  float        y1;
  float        x2;
  float        y2;
  const float *tex_coords;
  gint             tex_coords_len;
};

static void
_cogl_rectangles_with_multitexture_coords (
                                        struct _CoglMutiTexturedRect *rects,
                                        gint                          n_rects)
{
  CoglHandle	 material;
  const GList	*layers;
  int		 n_layers;
  const GList	*tmp;
  guint32        fallback_mask = 0;
  gboolean	 all_use_sliced_quad_fallback = FALSE;
  int		 i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_clip_ensure ();

  material = ctx->source_material;

  layers = cogl_material_get_layers (material);
  n_layers = g_list_length ((GList *)layers);

  /*
   * Validate all the layers of the current source material...
   */

  for (tmp = layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle     layer = tmp->data;
      CoglHandle     tex_handle = cogl_material_layer_get_texture (layer);
      CoglTexture   *texture = _cogl_texture_pointer_from_handle (tex_handle);
      gulong         flags;

      if (cogl_material_layer_get_type (layer)
	  != COGL_MATERIAL_LAYER_TYPE_TEXTURE)
	continue;

      /* XXX:
       * For now, if the first layer is sliced then all other layers are
       * ignored since we currently don't support multi-texturing with
       * sliced textures. If the first layer is not sliced then any other
       * layers found to be sliced will be skipped. (with a warning)
       *
       * TODO: Add support for multi-texturing rectangles with sliced
       * textures if no texture matrices are in use.
       */
      if (cogl_texture_is_sliced (tex_handle))
	{
	  if (i == 0)
	    {
              fallback_mask = ~1; /* fallback all except the first layer */
	      all_use_sliced_quad_fallback = TRUE;
              if (tmp->next)
                {
                  g_warning ("Skipping layers 1..n of your material since the "
                             "first layer is sliced. We don't currently "
                             "support any multi-texturing with sliced "
                             "textures but assume layer 0 is the most "
                             "important to keep");
                }
	      break;
	    }
          else
            {
              g_warning ("Skipping layer %d of your material consisting of a "
                         "sliced texture (unsuported for multi texturing)",
                         i);

              /* NB: marking for fallback will replace the layer with
               * a default transparent texture */
              fallback_mask |= (1 << i);
	      continue;
            }
	}

      /* We don't support multi texturing using textures with any waste if the
       * user has supplied a custom texture matrix, since we don't know if
       * the result will end up trying to texture from the waste area. */
      flags = cogl_material_layer_get_flags (layer);
      if (flags & COGL_MATERIAL_LAYER_FLAG_HAS_USER_MATRIX
          && _cogl_texture_span_has_waste (texture, 0, 0))
        {
          static gboolean shown_warning = FALSE;
          if (!shown_warning)
            g_warning ("Skipping layer %d of your material consisting of a "
                       "texture with waste since you have supplied a custom "
                       "texture matrix and the result may try to sample from "
                       "the waste area of your texture.", i);
          shown_warning = TRUE;
          /* NB: marking for fallback will replace the layer with
           * a default transparent texture */
          fallback_mask |= (1 << i);
          continue;
        }
    }

  /*
   * Emit geometry for each of the rectangles...
   */

  for (i = 0; i < n_rects; i++)
    {
      if (all_use_sliced_quad_fallback
          || !_cogl_multitexture_unsliced_quad (rects[i].x1, rects[i].y1,
                                                rects[i].x2, rects[i].y2,
                                                material,
                                                n_layers,
                                                fallback_mask,
                                                rects[i].tex_coords,
                                                rects[i].tex_coords_len))
        {
          const GList *layers;
          CoglHandle   tex_handle;
          CoglTexture *texture;

          layers = cogl_material_get_layers (material);
          tex_handle =
            cogl_material_layer_get_texture ((CoglHandle)layers->data);
          texture = _cogl_texture_pointer_from_handle (tex_handle);
          if (rects[i].tex_coords)
            _cogl_texture_sliced_quad (texture,
                                       material,
                                       rects[i].x1, rects[i].y1,
                                       rects[i].x2, rects[i].y2,
                                       rects[i].tex_coords[0],
                                       rects[i].tex_coords[1],
                                       rects[i].tex_coords[2],
                                       rects[i].tex_coords[3]);
          else
            _cogl_texture_sliced_quad (texture,
                                       material,
                                       rects[i].x1, rects[i].y1,
                                       rects[i].x2, rects[i].y2,
                                       0.0f, 0.0f, 1.0f, 1.0f);
        }
    }

  _cogl_journal_flush ();
}

void
cogl_rectangles_with_texture_coords (const float *verts,
                                     guint            n_rects)
{
  struct _CoglMutiTexturedRect rects[n_rects];
  int i;

  for (i = 0; i < n_rects; i++)
    {
      rects[i].x1 = verts[i * 8];
      rects[i].y1 = verts[i * 8 + 1];
      rects[i].x2 = verts[i * 8 + 2];
      rects[i].y2 = verts[i * 8 + 3];
      /* FIXME: rect should be defined to have a const float *geom;
       * instead, to avoid this copy
       * rect[i].geom = &verts[n_rects * 8]; */
      rects[i].tex_coords = &verts[i * 8 + 4];
      rects[i].tex_coords_len = 4;
    }

  _cogl_rectangles_with_multitexture_coords (rects, n_rects);
}

void
cogl_rectangle_with_texture_coords (float x1,
			            float y1,
			            float x2,
			            float y2,
			            float tx1,
			            float ty1,
			            float tx2,
			            float ty2)
{
  float verts[8];

  verts[0] = x1;
  verts[1] = y1;
  verts[2] = x2;
  verts[3] = y2;
  verts[4] = tx1;
  verts[5] = ty1;
  verts[6] = tx2;
  verts[7] = ty2;

  cogl_rectangles_with_texture_coords (verts, 1);
}

void
cogl_rectangle_with_multitexture_coords (float        x1,
			                 float        y1,
			                 float        x2,
			                 float        y2,
			                 const float *user_tex_coords,
                                         gint         user_tex_coords_len)
{
  struct _CoglMutiTexturedRect rect;

  rect.x1 = x1;
  rect.y1 = y1;
  rect.x2 = x2;
  rect.y2 = y2;
  rect.tex_coords = user_tex_coords;
  rect.tex_coords_len = user_tex_coords_len;

  _cogl_rectangles_with_multitexture_coords (&rect, 1);
}

static void
_cogl_texture_sliced_polygon (CoglTextureVertex *vertices,
                              guint              n_vertices,
                              guint              stride,
                              gboolean           use_color)
{
  const GList         *layers;
  CoglHandle           layer0;
  CoglHandle           tex_handle;
  CoglTexture         *tex;
  CoglTexSliceSpan    *y_span, *x_span;
  int                  x, y, tex_num, i;
  GLuint               gl_handle;
  GLfloat             *v;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* We can assume in this case that we have at least one layer in the
   * material that corresponds to a sliced cogl texture */
  layers = cogl_material_get_layers (ctx->source_material);
  layer0 = (CoglHandle)layers->data;
  tex_handle = cogl_material_layer_get_texture (layer0);
  tex = _cogl_texture_pointer_from_handle (tex_handle);

  v = (GLfloat *)ctx->logged_vertices->data;
  for (i = 0; i < n_vertices; i++)
    {
      GLfloat *c;

      v[0] = vertices[i].x;
      v[1] = vertices[i].y;
      v[2] = vertices[i].z;

      /* NB: [X,Y,Z,TX,TY,R,G,B,A,...] */
      c = v + 5;
      c[0] = cogl_color_get_red_byte (&vertices[i].color);
      c[1] = cogl_color_get_green_byte (&vertices[i].color);
      c[2] = cogl_color_get_blue_byte (&vertices[i].color);
      c[3] = cogl_color_get_alpha_byte (&vertices[i].color);

      v += stride;
    }

  /* Render all of the slices with the full geometry but use a
     transparent border color so that any part of the texture not
     covered by the slice will be ignored */
  tex_num = 0;
  for (y = 0; y < tex->slice_y_spans->len; y++)
    {
      y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, y);

      for (x = 0; x < tex->slice_x_spans->len; x++)
	{
	  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, x);

	  gl_handle = g_array_index (tex->slice_gl_handles, GLuint, tex_num++);

	  /* Convert the vertices into an array of GLfloats ready to pass to
	     OpenGL */
          v = (GLfloat *)ctx->logged_vertices->data;
	  for (i = 0; i < n_vertices; i++)
	    {
              GLfloat *t;
              float    tx, ty;

              tx = ((vertices[i].tx
                     - ((float)(x_span->start)
                        / tex->bitmap.width))
                    * tex->bitmap.width / x_span->size);
              ty = ((vertices[i].ty
                     - ((float)(y_span->start)
                        / tex->bitmap.height))
                    * tex->bitmap.height / y_span->size);

              /* NB: [X,Y,Z,TX,TY,R,G,B,A,...] */
              t = v + 3;
	      t[0] = tx;
	      t[1] = ty;

              v += stride;
	    }

          cogl_material_flush_gl_state (ctx->source_material,
                                        COGL_MATERIAL_FLUSH_DISABLE_MASK,
                                        (guint32)~1, /* disable all except the
                                                        first layer */
                                        COGL_MATERIAL_FLUSH_LAYER0_OVERRIDE,
                                        gl_handle,
                                        NULL);

	  GE( glDrawArrays (GL_TRIANGLE_FAN, 0, n_vertices) );
	}
    }
}


static void
_cogl_multitexture_unsliced_polygon (CoglTextureVertex *vertices,
                                     guint              n_vertices,
                                     guint              n_layers,
                                     guint              stride,
                                     gboolean           use_color,
                                     guint32            fallback_mask)
{
  CoglHandle           material;
  const GList         *layers;
  int                  i;
  GList               *tmp;
  CoglTexSliceSpan    *y_span, *x_span;
  GLfloat             *v;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  v = (GLfloat *)ctx->logged_vertices->data;

  material = ctx->source_material;
  layers = cogl_material_get_layers (material);

  /* Convert the vertices into an array of GLfloats ready to pass to
     OpenGL */
  for (v = (GLfloat *)ctx->logged_vertices->data, i = 0;
       i < n_vertices;
       v += stride, i++)
    {
      GLfloat *c;
      int      j;

      /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
      v[0] = vertices[i].x;
      v[1] = vertices[i].y;
      v[2] = vertices[i].z;

      for (tmp = (GList *)layers, j = 0; tmp != NULL; tmp = tmp->next, j++)
        {
          CoglHandle   layer = (CoglHandle)tmp->data;
          CoglHandle   tex_handle;
          CoglTexture *tex;
          GLfloat     *t;
          float        tx, ty;

          tex_handle = cogl_material_layer_get_texture (layer);
          tex = _cogl_texture_pointer_from_handle (tex_handle);

          y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, 0);
          x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, 0);

          tx = ((vertices[i].tx
                 - ((float)(x_span->start)
                    / tex->bitmap.width))
                * tex->bitmap.width / x_span->size);
          ty = ((vertices[i].ty
                 - ((float)(y_span->start)
                    / tex->bitmap.height))
                * tex->bitmap.height / y_span->size);

          /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
          t = v + 3 + 2 * j;
          t[0] = tx;
          t[1] = ty;
        }

      /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
      c = v + 3 + 2 * n_layers;
      c[0] = cogl_color_get_red_float (&vertices[i].color);
      c[1] = cogl_color_get_green_float (&vertices[i].color);
      c[2] = cogl_color_get_blue_float (&vertices[i].color);
      c[3] = cogl_color_get_alpha_float (&vertices[i].color);
    }

  cogl_material_flush_gl_state (ctx->source_material,
                                COGL_MATERIAL_FLUSH_FALLBACK_MASK,
                                fallback_mask,
                                NULL);

  GE (glDrawArrays (GL_TRIANGLE_FAN, 0, n_vertices));
}

void
cogl_polygon (CoglTextureVertex *vertices,
              guint              n_vertices,
	      gboolean           use_color)
{
  CoglHandle           material;
  const GList         *layers;
  int                  n_layers;
  GList               *tmp;
  gboolean	       use_sliced_polygon_fallback = FALSE;
  guint32              fallback_mask = 0;
  int                  i;
  gulong               enable_flags;
  guint                stride;
  gsize                stride_bytes;
  GLfloat             *v;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  cogl_clip_ensure ();

  material = ctx->source_material;
  layers = cogl_material_get_layers (ctx->source_material);
  n_layers = g_list_length ((GList *)layers);

  for (tmp = (GList *)layers, i = 0; tmp != NULL; tmp = tmp->next, i++)
    {
      CoglHandle   layer = (CoglHandle)tmp->data;
      CoglHandle   tex_handle = cogl_material_layer_get_texture (layer);
      CoglTexture *tex = _cogl_texture_pointer_from_handle (tex_handle);

      if (i == 0 && cogl_texture_is_sliced (tex_handle))
        {
#if defined (HAVE_COGL_GLES) || defined (HAVE_COGL_GLES2)
          {
            static gboolean shown_gles_slicing_warning = FALSE;
            if (!shown_gles_slicing_warning)
              g_warning ("cogl_polygon does not work for sliced textures "
                         "on GL ES");
            shown_gles_slicing_warning = TRUE;
            return;
          }
#endif
          if (n_layers > 1)
            {
              static gboolean shown_slicing_warning = FALSE;
              if (!shown_slicing_warning)
                {
                  g_warning ("Disabling layers 1..n since multi-texturing with "
                             "cogl_polygon isn't supported when using sliced "
                             "textures\n");
                  shown_slicing_warning = TRUE;
                }
            }
          use_sliced_polygon_fallback = TRUE;
          n_layers = 1;

          if (tex->min_filter != GL_NEAREST || tex->mag_filter != GL_NEAREST)
            {
              static gboolean shown_filter_warning = FALSE;
              if (!shown_filter_warning)
                {
                  g_warning ("cogl_texture_polygon does not work for sliced textures "
                             "when the minification and magnification filters are not "
                             "CGL_NEAREST");
                  shown_filter_warning = TRUE;
                }
              return;
            }

#ifdef HAVE_COGL_GL
          /* Temporarily change the wrapping mode on all of the slices to use
           * a transparent border
           * XXX: it's doesn't look like we save/restore this, like the comment
           * implies? */
          _cogl_texture_set_wrap_mode_parameter (tex, GL_CLAMP_TO_BORDER);
#endif
          break;
        }

      if (cogl_texture_is_sliced (tex_handle))
        {
          g_warning ("Disabling layer %d of the current source material, "
                     "because texturing with the vertex buffer API is not "
                     "currently supported using sliced textures, or textures "
                     "with waste\n", i);

          fallback_mask |= (1 << i);
          continue;
        }
    }

  /* Our data is arranged like:
   * [X, Y, Z, TX0, TY0, TX1, TY1..., R, G, B, A,...] */
  stride = 3 + (2 * n_layers) + (use_color ? 4 : 0);
  stride_bytes = stride * sizeof (GLfloat);

  /* Make sure there is enough space in the global vertex
     array. This is used so we can render the polygon with a single
     call to OpenGL but still support any number of vertices */
  g_array_set_size (ctx->logged_vertices, n_vertices * stride);
  v = (GLfloat *)ctx->logged_vertices->data;

  /* Prepare GL state */
  enable_flags = COGL_ENABLE_VERTEX_ARRAY;
  enable_flags |= cogl_material_get_cogl_enable_flags (ctx->source_material);

  if (ctx->enable_backface_culling)
    enable_flags |= COGL_ENABLE_BACKFACE_CULLING;

  if (use_color)
    {
      enable_flags |= COGL_ENABLE_COLOR_ARRAY;
      GE( glColorPointer (4, GL_FLOAT,
                          stride_bytes,
                          /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
                          v + 3 + 2 * n_layers) );
    }

  cogl_enable (enable_flags);

  GE (glVertexPointer (3, GL_FLOAT, stride_bytes, v));

  for (i = 0; i < n_layers; i++)
    {
      GE (glClientActiveTexture (GL_TEXTURE0 + i));
      GE (glTexCoordPointer (2, GL_FLOAT,
                             stride_bytes,
                             /* NB: [X,Y,Z,TX,TY...,R,G,B,A,...] */
                             v + 3 + 2 * i));
    }

  if (use_sliced_polygon_fallback)
    _cogl_texture_sliced_polygon (vertices,
                                  n_vertices,
                                  stride,
                                  use_color);
  else
    _cogl_multitexture_unsliced_polygon (vertices,
                                         n_vertices,
                                         n_layers,
                                         stride,
                                         use_color,
                                         fallback_mask);

  /* Reset the size of the logged vertex array because rendering
     rectangles expects it to start at 0 */
  g_array_set_size (ctx->logged_vertices, 0);
}

