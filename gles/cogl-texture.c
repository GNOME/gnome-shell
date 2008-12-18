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

#if HAVE_COGL_GLES2
#define glVertexPointer cogl_wrap_glVertexPointer
#define glTexCoordPointer cogl_wrap_glTexCoordPointer
#define glColorPointer cogl_wrap_glColorPointer
#define glDrawArrays cogl_wrap_glDrawArrays
#define glTexParameteri cogl_wrap_glTexParameteri
#define glClientActiveTexture cogl_wrap_glClientActiveTexture
#define glActiveTexture cogl_wrap_glActiveTexture
#define glEnable cogl_wrap_glEnable
#define glEnableClientState cogl_wrap_glEnableClientState
#define glDisable cogl_wrap_glDisable
#define glDisableClientState cogl_wrap_glDisableClientState
#endif

/*
#define COGL_DEBUG 1

#define GE(x) \
{ \
  glGetError(); x; \
  GLuint err = glGetError(); \
  if (err != 0) \
    printf("err: 0x%x\n", err); \
} */

struct _CoglSpanIter
{
  gint              index;
  GArray           *array;
  CoglTexSliceSpan *span;
  CoglFixed      pos;
  CoglFixed      next_pos;
  CoglFixed      origin;
  CoglFixed      cover_start;
  CoglFixed      cover_end;
  CoglFixed      intersect_start;
  CoglFixed      intersect_end;
  CoglFixed      intersect_start_local;
  CoglFixed      intersect_end_local;
  gboolean          intersects;
};

static void _cogl_texture_free (CoglTexture *tex);

COGL_HANDLE_DEFINE (Texture, texture, texture_handles);

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
    COGL_FIXED_FROM_INT (iter->span->size - iter->span->waste);
  
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
		       CoglFixed   origin,
		       CoglFixed   cover_start,
		       CoglFixed   cover_end)
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
                             CoglColor   *back_color,
                             GLint       *viewport)
{
  gint               bpp;
  CoglFixed       rx1, ry1;
  CoglFixed       rx2, ry2;
  CoglFixed       tx1, ty1;
  CoglFixed       tx2, ty2;
  int                bw,  bh;
  CoglBitmap         rect_bmp;
  CoglHandle         handle;
  
  handle = _cogl_texture_handle_from_pointer (tex);
  bpp = _cogl_get_format_bpp (COGL_PIXEL_FORMAT_RGBA_8888);
  
  /* If whole image fits into the viewport and target buffer
     has got no special rowstride, we can do it in one pass */
  if (tex->bitmap.width < viewport[2] - viewport[0] &&
      tex->bitmap.height < viewport[3] - viewport[1] &&
      tex->bitmap.rowstride == bpp * tex->bitmap.width)
    {
      /* Clear buffer with transparent black, draw with white
         for direct copy to framebuffer */
      cogl_paint_init (back_color);
      
      /* Draw the texture image */
      cogl_texture_rectangle (handle,
                              0, 0,
                              COGL_FIXED_FROM_INT (tex->bitmap.width),
                              COGL_FIXED_FROM_INT (tex->bitmap.height),
                              0, 0, COGL_FIXED_1, COGL_FIXED_1);
      
      /* Read into target bitmap */
      prep_for_gl_pixels_download (tex->bitmap.rowstride);
      GE( glReadPixels (viewport[0], viewport[1],
                        tex->bitmap.width,
                        tex->bitmap.height,
                        GL_RGBA, GL_UNSIGNED_BYTE,
                        target_bmp->data) );
    }
  else
    {
      ry1 = 0; ry2 = 0;
      ty1 = 0; ty2 = 0;
      
#define CFIX COGL_FIXED_FROM_INT
      
      /* Walk Y axis until whole bitmap height consumed */
      for (bh = tex->bitmap.height; bh > 0; bh -= viewport[3])
        {
          /* Rectangle Y coords */
          ry1 = ry2;
          ry2 += (bh < viewport[3]) ? bh : viewport[3];
          
          /* Normalized texture Y coords */
          ty1 = ty2;
          ty2 = COGL_FIXED_DIV (CFIX (ry2), CFIX (tex->bitmap.height));
          
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
              tx2 = COGL_FIXED_DIV (CFIX (rx2), CFIX (tex->bitmap.width));
              
              /* Clear buffer with transparent black, draw with white
		 for direct copy to framebuffer */
              cogl_paint_init (back_color);
              
              /* Draw a portion of texture */
              cogl_texture_rectangle (handle,
                                      0, 0,
                                      CFIX (rx2 - rx1),
                                      CFIX (ry2 - ry1),
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
      
#undef CFIX
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
  CoglColor  cwhite;
  CoglBitmap alpha_bmp;
  COGLenum   old_src_factor;
  COGLenum   old_dst_factor;

  _COGL_GET_CONTEXT (ctx, FALSE);

  cogl_color_set_from_4ub (&cwhite, 0xff, 0xff, 0xff, 0xff);

  bpp = _cogl_get_format_bpp (COGL_PIXEL_FORMAT_RGBA_8888);
  
  /* Viewport needs to have some size and be inside the window for this */
  GE( cogl_wrap_glGetIntegerv (GL_VIEWPORT, viewport) );
  
  if (viewport[0] <  0 || viewport[1] <  0 ||
      viewport[2] <= 0 || viewport[3] <= 0)
    return FALSE;
  
  /* Setup orthographic projection into current viewport
     (0,0 in bottom-left corner to draw the texture
     upside-down so we match the way glReadPixels works) */
  
  GE( cogl_wrap_glMatrixMode (GL_PROJECTION) );
  GE( cogl_wrap_glPushMatrix () );
  GE( cogl_wrap_glLoadIdentity () );
  
  GE( cogl_wrap_glOrthox (0, COGL_FIXED_FROM_INT (viewport[2]),
			  0, COGL_FIXED_FROM_INT (viewport[3]),
			  COGL_FIXED_FROM_INT (0),
			  COGL_FIXED_FROM_INT (100)) );
  
  GE( cogl_wrap_glMatrixMode (GL_MODELVIEW) );
  GE( cogl_wrap_glPushMatrix () );
  GE( cogl_wrap_glLoadIdentity () );
  
  /* Draw to all channels */
  cogl_draw_buffer (COGL_WINDOW_BUFFER | COGL_MASK_BUFFER, 0);
  
  /* Store old blending factors */
  old_src_factor = ctx->blend_src_factor;
  old_dst_factor = ctx->blend_dst_factor;
  
  /* Direct copy operation */
  cogl_set_source_color (&cwhite);
  cogl_blend_func (CGL_ONE, CGL_ZERO);
  _cogl_texture_draw_and_read (tex, target_bmp,
                               &cwhite, viewport);
  
  /* Check whether texture has alpha and framebuffer not */
  /* FIXME: For some reason even if ALPHA_BITS is 8, the framebuffer
     still doesn't seem to have an alpha buffer. This might be just
     a PowerVR issue.
  GLint r_bits, g_bits, b_bits, a_bits;
  GE( cogl_wrap_glGetIntegerv (GL_ALPHA_BITS, &a_bits) );
  GE( cogl_wrap_glGetIntegerv (GL_RED_BITS, &r_bits) );
  GE( cogl_wrap_glGetIntegerv (GL_GREEN_BITS, &g_bits) );
  GE( cogl_wrap_glGetIntegerv (GL_BLUE_BITS, &b_bits) );
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
      cogl_blend_func (CGL_ZERO, CGL_SRC_ALPHA);
      _cogl_texture_draw_and_read (tex, &alpha_bmp,
                                   &cwhite, viewport);
      
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
  cogl_wrap_glMatrixMode (GL_PROJECTION);
  cogl_wrap_glPopMatrix ();
  cogl_wrap_glMatrixMode (GL_MODELVIEW);
  cogl_wrap_glPopMatrix ();
  
  cogl_draw_buffer (COGL_WINDOW_BUFFER, 0);
  cogl_blend_func (old_src_factor, old_dst_factor);
  
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
			      0, COGL_FIXED_FROM_INT (dst_y),
			      COGL_FIXED_FROM_INT (dst_y + height));
       
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
				  0, COGL_FIXED_FROM_INT (dst_x),
				  COGL_FIXED_FROM_INT (dst_x + width));
	   
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
	  inter_w = COGL_FIXED_TO_INT (x_iter.intersect_end -
			               x_iter.intersect_start);
	  inter_h = COGL_FIXED_TO_INT (y_iter.intersect_end -
				       y_iter.intersect_start);
	  
	  /* Localize intersection top-left corner to slice*/
	  local_x = COGL_FIXED_TO_INT (x_iter.intersect_start -
				       x_iter.pos);
	  local_y = COGL_FIXED_TO_INT (y_iter.intersect_start -
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
                  + (src_y + COGL_FIXED_TO_INT (y_iter.intersect_start)
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
                  + (src_x + COGL_FIXED_TO_INT (x_iter.intersect_start)
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
  
  
  /* Hardware repeated tiling if supported, else tile in software*/
  if (cogl_features_available (COGL_FEATURE_TEXTURE_NPOT)
      && n_slices == 1)
    tex->wrap_mode = GL_REPEAT;
  else
    tex->wrap_mode = GL_CLAMP_TO_EDGE;
  
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
cogl_texture_new_with_size (guint           width,
			    guint           height,
			    gint            max_waste,
                            gboolean        auto_mipmap,
			    CoglPixelFormat internal_format)
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
  tex->auto_mipmap = auto_mipmap;
  
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
cogl_texture_new_from_data (guint              width,
			    guint              height,
			    gint               max_waste,
                            gboolean           auto_mipmap,
			    CoglPixelFormat    format,
			    CoglPixelFormat    internal_format,
			    guint              rowstride,
			    const guchar      *data)
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
  tex->auto_mipmap = auto_mipmap;

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
cogl_texture_new_from_file (const gchar     *filename,
			    gint             max_waste,
                            gboolean         auto_mipmap,
			    CoglPixelFormat  internal_format,
			    GError         **error)
{
  CoglBitmap   bmp;
  CoglTexture *tex;
  
  g_return_val_if_fail (error == NULL || *error == NULL, COGL_INVALID_HANDLE);

  /* Try loading with imaging backend */
  if (!_cogl_bitmap_from_file (&bmp, filename, error))
    {
      /* Try fallback */
      if (!_cogl_bitmap_fallback_from_file (&bmp, filename))
	return COGL_INVALID_HANDLE;
      else if (error && *error)
	{
	  g_error_free (*error);
	  *error = NULL;
	}
    }
  
  /* Create new texture and fill with loaded data */
  tex = (CoglTexture*) g_malloc ( sizeof (CoglTexture));
  
  tex->ref_count = 1;
  COGL_HANDLE_DEBUG_NEW (texture, tex);
  
  tex->is_foreign = FALSE;
  tex->auto_mipmap = auto_mipmap;

  tex->bitmap = bmp;
  tex->bitmap_owner = TRUE;
  
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
  
  /* Force appropriate wrap parameter */
  if (cogl_features_available (COGL_FEATURE_TEXTURE_NPOT) &&
      gl_target == GL_TEXTURE_2D)
    {
      /* Hardware repeated tiling */
      tex->wrap_mode = GL_REPEAT;
      GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_S, GL_REPEAT) );
      GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_T, GL_REPEAT) );
    }
  else
    {
      /* Any tiling will be done in software */
      tex->wrap_mode = GL_CLAMP_TO_EDGE;
      GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
      GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );
    }
  
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

static void
_cogl_texture_quad_sw (CoglTexture *tex,
		       CoglFixed x1,
		       CoglFixed y1,
		       CoglFixed x2,
		       CoglFixed y2,
		       CoglFixed tx1,
		       CoglFixed ty1,
		       CoglFixed tx2,
		       CoglFixed ty2)
{
  CoglSpanIter    iter_x    ,  iter_y;
  CoglFixed       tw        ,  th;
  CoglFixed       tqx       ,  tqy;
  CoglFixed       first_tx  ,  first_ty;
  CoglFixed       first_qx  ,  first_qy;
  CoglFixed       slice_tx1 ,  slice_ty1;
  CoglFixed       slice_tx2 ,  slice_ty2;
  CoglFixed       slice_qx1 ,  slice_qy1;
  CoglFixed       slice_qx2 ,  slice_qy2;
  GLfloat         tex_coords[8];
  GLfloat         quad_coords[8];
  GLuint          gl_handle;
  gulong          enable_flags = (COGL_ENABLE_TEXTURE_2D
                                  | COGL_ENABLE_VERTEX_ARRAY
                                  | COGL_ENABLE_TEXCOORD_ARRAY);
  
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

#if COGL_DEBUG
  printf("=== Drawing Tex Quad (Software Tiling Mode) ===\n");
#endif
  
  
  /* Prepare GL state */
  if (ctx->color_alpha < 255
      || tex->bitmap.format & COGL_A_BIT)
    {
      enable_flags |= COGL_ENABLE_BLEND;
    }

  if (ctx->enable_backface_culling)
    enable_flags |= COGL_ENABLE_BACKFACE_CULLING;

  cogl_enable (enable_flags);

  /* If the texture coordinates are backwards then swap both the
     geometry and texture coordinates so that the texture will be
     flipped but we can still use the same algorithm to iterate the
     slices */
  if (tx2 < tx1)
    {
      CoglFixed temp = x1;
      x1 = x2;
      x2 = temp;
      temp = tx1;
      tx1 = tx2;
      tx2 = temp;
    }
  if (ty2 < ty1)
    {
      CoglFixed temp = y1;
      y1 = y2;
      y2 = temp;
      temp = ty1;
      ty1 = ty2;
      ty2 = temp;
    }
  
  GE( glTexCoordPointer (2, GL_FLOAT, 0, tex_coords) );
  GE( glVertexPointer   (2, GL_FLOAT, 0, quad_coords) );
  
  /* Scale ratio from texture to quad widths */
  tw = COGL_FIXED_FROM_INT (tex->bitmap.width);
  th = COGL_FIXED_FROM_INT (tex->bitmap.height);
  
  tqx = COGL_FIXED_DIV (x2 - x1, COGL_FIXED_MUL (tw, (tx2 - tx1)));
  tqy = COGL_FIXED_DIV (y2 - y1, COGL_FIXED_MUL (th, (ty2 - ty1)));

  /* Integral texture coordinate for first tile */
  first_tx = COGL_FIXED_FROM_INT (COGL_FIXED_FLOOR (tx1));
  first_ty = COGL_FIXED_FROM_INT (COGL_FIXED_FLOOR (ty1));
  
  /* Denormalize texture coordinates */
  first_tx = COGL_FIXED_MUL (first_tx, tw);
  first_ty = COGL_FIXED_MUL (first_ty, th);
  tx1 = COGL_FIXED_MUL (tx1, tw);
  ty1 = COGL_FIXED_MUL (ty1, th);
  tx2 = COGL_FIXED_MUL (tx2, tw);
  ty2 = COGL_FIXED_MUL (ty2, th);

  /* Quad coordinate of the first tile */
  first_qx = x1 - COGL_FIXED_MUL (tx1 - first_tx, tqx);
  first_qy = y1 - COGL_FIXED_MUL (ty1 - first_ty, tqy);
  
  
  /* Iterate until whole quad height covered */
  for (_cogl_span_iter_begin (&iter_y, tex->slice_y_spans,
			      first_ty, ty1, ty2) ;
       !_cogl_span_iter_end  (&iter_y) ;
       _cogl_span_iter_next  (&iter_y) )
    { 
      /* Discard slices out of quad early */
      if (!iter_y.intersects) continue;
      
      /* Span-quad intersection in quad coordinates */
      slice_qy1 = first_qy +
	COGL_FIXED_MUL (iter_y.intersect_start - first_ty, tqy);
      
      slice_qy2 = first_qy +
	COGL_FIXED_MUL (iter_y.intersect_end - first_ty, tqy);
      
      /* Localize slice texture coordinates */
      slice_ty1 = iter_y.intersect_start - iter_y.pos;
      slice_ty2 = iter_y.intersect_end - iter_y.pos;
      
      /* Normalize texture coordinates to current slice
         (rectangle texture targets take denormalized) */
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
	  slice_qx1 = first_qx +
	    COGL_FIXED_MUL (iter_x.intersect_start - first_tx, tqx);
	  
	  slice_qx2 = first_qx +
	    COGL_FIXED_MUL (iter_x.intersect_end - first_tx, tqx);
	  
	  /* Localize slice texture coordinates */
	  slice_tx1 = iter_x.intersect_start - iter_x.pos;
	  slice_tx2 = iter_x.intersect_end - iter_x.pos;
	  
	  /* Normalize texture coordinates to current slice
             (rectangle texture targets take denormalized) */
          slice_tx1 /= iter_x.span->size;
          slice_tx2 /= iter_x.span->size;
	  
#if COGL_DEBUG
	  printf("~~~~~ slice (%d,%d)\n", iter_x.index, iter_y.index);
	  printf("qx1: %f\n", COGL_FIXED_TO_FLOAT (slice_qx1));
	  printf("qy1: %f\n", COGL_FIXED_TO_FLOAT (slice_qy1));
	  printf("qx2: %f\n", COGL_FIXED_TO_FLOAT (slice_qx2));
	  printf("qy2: %f\n", COGL_FIXED_TO_FLOAT (slice_qy2));
	  printf("tx1: %f\n", COGL_FIXED_TO_FLOAT (slice_tx1));
	  printf("ty1: %f\n", COGL_FIXED_TO_FLOAT (slice_ty1));
	  printf("tx2: %f\n", COGL_FIXED_TO_FLOAT (slice_tx2));
	  printf("ty2: %f\n", COGL_FIXED_TO_FLOAT (slice_ty2));
#endif
	  
	  /* Pick and bind opengl texture object */
	  gl_handle = g_array_index (tex->slice_gl_handles, GLuint,
				     iter_y.index * iter_x.array->len +
				     iter_x.index);
	  
	  GE( cogl_gles2_wrapper_bind_texture (tex->gl_target, gl_handle,
					       tex->gl_intformat) );

#define CFX_F COGL_FIXED_TO_FLOAT
	  
	  /* Draw textured quad */
          tex_coords[0] = CFX_F(slice_tx1); tex_coords[1] = CFX_F(slice_ty2);
          tex_coords[2] = CFX_F(slice_tx2); tex_coords[3] = CFX_F(slice_ty2);
          tex_coords[4] = CFX_F(slice_tx1); tex_coords[5] = CFX_F(slice_ty1);
          tex_coords[6] = CFX_F(slice_tx2); tex_coords[7] = CFX_F(slice_ty1);

          quad_coords[0] = CFX_F(slice_qx1); quad_coords[1] = CFX_F(slice_qy2);
          quad_coords[2] = CFX_F(slice_qx2); quad_coords[3] = CFX_F(slice_qy2);
          quad_coords[4] = CFX_F(slice_qx1); quad_coords[5] = CFX_F(slice_qy1);
          quad_coords[6] = CFX_F(slice_qx2); quad_coords[7] = CFX_F(slice_qy1);

	  GE (glDrawArrays (GL_TRIANGLE_STRIP, 0, 4) );

#undef CFX_F
	}
    }
}

static void
_cogl_texture_quad_hw (CoglTexture *tex,
		       CoglFixed x1,
		       CoglFixed y1,
		       CoglFixed x2,
		       CoglFixed y2,
		       CoglFixed tx1,
		       CoglFixed ty1,
		       CoglFixed tx2,
		       CoglFixed ty2)
{
  GLfloat           tex_coords[8];
  GLfloat           quad_coords[8];
  GLuint            gl_handle;
  CoglTexSliceSpan *x_span;
  CoglTexSliceSpan *y_span;
  gulong            enable_flags = (COGL_ENABLE_TEXTURE_2D
                                   | COGL_ENABLE_VERTEX_ARRAY
                                   | COGL_ENABLE_TEXCOORD_ARRAY);

#if COGL_DEBUG
  printf("=== Drawing Tex Quad (Hardware Tiling Mode) ===\n");
#endif
  
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  
  /* Prepare GL state */
  if (ctx->color_alpha < 255
      || tex->bitmap.format & COGL_A_BIT)
    {
      enable_flags |= COGL_ENABLE_BLEND;
    }
  
  if (ctx->enable_backface_culling)
    enable_flags |= COGL_ENABLE_BACKFACE_CULLING;

  cogl_enable (enable_flags);
  
  GE( glTexCoordPointer (2, GL_FLOAT, 0, tex_coords) );
  GE( glVertexPointer   (2, GL_FLOAT, 0, quad_coords) );
  
  /* Pick and bind opengl texture object */
  gl_handle = g_array_index (tex->slice_gl_handles, GLuint, 0);
  GE( cogl_gles2_wrapper_bind_texture (tex->gl_target, gl_handle,
				       tex->gl_intformat) );
  
  /* Don't include the waste in the texture coordinates */
  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, 0);
  y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, 0);

  /* Don't include the waste in the texture coordinates */
  tx1 = tx1 * (x_span->size - x_span->waste) / x_span->size;
  tx2 = tx2 * (x_span->size - x_span->waste) / x_span->size;
  ty1 = ty1 * (y_span->size - y_span->waste) / y_span->size;
  ty2 = ty2 * (y_span->size - y_span->waste) / y_span->size;

#define CFX_F(x) COGL_FIXED_TO_FLOAT(x)
  
  /* Draw textured quad */
  tex_coords[0] = CFX_F(tx1); tex_coords[1] = CFX_F(ty2);
  tex_coords[2] = CFX_F(tx2); tex_coords[3] = CFX_F(ty2);
  tex_coords[4] = CFX_F(tx1); tex_coords[5] = CFX_F(ty1);
  tex_coords[6] = CFX_F(tx2); tex_coords[7] = CFX_F(ty1);

  quad_coords[0] = CFX_F(x1); quad_coords[1] = CFX_F(y2);
  quad_coords[2] = CFX_F(x2); quad_coords[3] = CFX_F(y2);
  quad_coords[4] = CFX_F(x1); quad_coords[5] = CFX_F(y1);
  quad_coords[6] = CFX_F(x2); quad_coords[7] = CFX_F(y1);

  GE (glDrawArrays (GL_TRIANGLE_STRIP, 0, 4) );

#undef CFX_F
}

void
cogl_texture_rectangle (CoglHandle   handle,
			CoglFixed x1,
			CoglFixed y1,
			CoglFixed x2,
			CoglFixed y2,
			CoglFixed tx1,
			CoglFixed ty1,
			CoglFixed tx2,
			CoglFixed ty2)
{
  CoglTexture       *tex;
  
  /* Check if valid texture */
  if (!cogl_is_texture (handle))
    return;

  cogl_clip_ensure ();
  
  tex = _cogl_texture_pointer_from_handle (handle);
  
  /* Make sure we got stuff to draw */
  if (tex->slice_gl_handles == NULL)
    return;
  
  if (tex->slice_gl_handles->len == 0)
    return;
  
  if (tx1 == tx2 || ty1 == ty2)
    return;
  
  /* Pick tiling mode according to hw support */
  if (cogl_features_available (COGL_FEATURE_TEXTURE_NPOT)
      && tex->slice_gl_handles->len == 1)
    {
      _cogl_texture_quad_hw (tex, x1,y1, x2,y2, tx1,ty1, tx2,ty2);
    }
  else
    {
      if (tex->slice_gl_handles->len == 1
          && tx1 >= -COGL_FIXED_1
          && tx2 <= COGL_FIXED_1
          && ty1 >= -COGL_FIXED_1
          && ty2 <= COGL_FIXED_1)
	{
	  _cogl_texture_quad_hw (tex, x1,y1, x2,y2, tx1,ty1, tx2,ty2);
	}
      else
	{
	  _cogl_texture_quad_sw (tex, x1,y1, x2,y2, tx1,ty1, tx2,ty2);
	}
    }
}

void
cogl_texture_polygon (CoglHandle         handle,
		      guint              n_vertices,
		      CoglTextureVertex *vertices,
		      gboolean           use_color)
{
  CoglTexture      *tex;
  int               i;
  GLuint            gl_handle;
  CoglTexSliceSpan *y_span, *x_span;
  gulong            enable_flags;
  CoglTextureGLVertex *p;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Check if valid texture */
  if (!cogl_is_texture (handle))
    return;

  cogl_clip_ensure ();

  tex = _cogl_texture_pointer_from_handle (handle);

  /* GL ES has no GL_CLAMP_TO_BORDER wrap mode so the method used to
     render sliced textures in the GL backend will not work. Therefore
     cogl_texture_polygon is only supported if the texture is not
     sliced */
  if (tex->slice_gl_handles->len != 1)
    {
      static gboolean shown_warning = FALSE;

      if (!shown_warning)
	{
	  g_warning ("cogl_texture_polygon does not work for "
		     "sliced textures on GL ES");
	  shown_warning = TRUE;
	}
      return;
    }

  /* Make sure there is enough space in the global texture vertex
     array. This is used so we can render the polygon with a single
     call to OpenGL but still support any number of vertices */
  if (ctx->texture_vertices_size < n_vertices)
    {
      guint nsize = ctx->texture_vertices_size;
      
      if (nsize == 0)
	nsize = 1;
      do
	nsize *= 2;
      while (nsize < n_vertices);
      
      ctx->texture_vertices_size = nsize;

      ctx->texture_vertices = g_realloc (ctx->texture_vertices,
					 nsize
					 * sizeof (CoglTextureGLVertex));
    }
  
  /* Prepare GL state */
  enable_flags = (COGL_ENABLE_TEXTURE_2D
		  | COGL_ENABLE_VERTEX_ARRAY
		  | COGL_ENABLE_TEXCOORD_ARRAY);

  if ((tex->bitmap.format & COGL_A_BIT))
    enable_flags |= COGL_ENABLE_BLEND;
  else if (use_color)
    {
      for (i = 0; i < n_vertices; i++)
	if (cogl_color_get_alpha_byte(&vertices[i].color) < 255)
	  {
	    enable_flags |= COGL_ENABLE_BLEND;
	    break;
	  }
    }
  else if (ctx->color_alpha < 255)
    enable_flags |= COGL_ENABLE_BLEND;

  if (ctx->enable_backface_culling)
    enable_flags |= COGL_ENABLE_BACKFACE_CULLING;

  if (use_color)
    {
      enable_flags |= COGL_ENABLE_COLOR_ARRAY;
      GE( glColorPointer (4, GL_UNSIGNED_BYTE, sizeof (CoglTextureGLVertex),
			  ctx->texture_vertices[0].c) );
    }

  GE( glVertexPointer (3, GL_FLOAT, sizeof (CoglTextureGLVertex),
		       ctx->texture_vertices[0].v) );
  GE( glTexCoordPointer (2, GL_FLOAT, sizeof (CoglTextureGLVertex),
			 ctx->texture_vertices[0].t) );

  cogl_enable (enable_flags);
  
  gl_handle = g_array_index (tex->slice_gl_handles, GLuint, 0);
  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, 0);
  y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, 0);

  /* Convert the vertices into an array of GLfloats ready to pass to
     OpenGL */
  for (i = 0, p = ctx->texture_vertices; i < n_vertices; i++, p++)
    {
#define CFX_F COGL_FIXED_TO_FLOAT

      p->v[0] = CFX_F(vertices[i].x);
      p->v[1] = CFX_F(vertices[i].y);
      p->v[2] = CFX_F(vertices[i].z);
      p->t[0] = CFX_F(vertices[i].tx
		      * (x_span->size - x_span->waste) / x_span->size);
      p->t[1] = CFX_F(vertices[i].ty
		      * (y_span->size - y_span->waste) / y_span->size);
      p->c[0] = cogl_color_get_red_byte(&vertices[i].color);
      p->c[1] = cogl_color_get_green_byte(&vertices[i].color);
      p->c[2] = cogl_color_get_blue_byte(&vertices[i].color);
      p->c[3] = cogl_color_get_alpha_byte(&vertices[i].color);

#undef CFX_F
    }

  GE( cogl_gles2_wrapper_bind_texture (tex->gl_target, gl_handle,
				       tex->gl_intformat) );

  GE( glDrawArrays (GL_TRIANGLE_FAN, 0, n_vertices) );

  /* Set the last color so that the cache of the alpha value will work
     properly */
  if (use_color && n_vertices > 0)
    cogl_set_source_color (&vertices[n_vertices - 1].color);
}

void
cogl_material_rectangle (CoglFixed   x1,
			 CoglFixed   y1,
			 CoglFixed   x2,
			 CoglFixed   y2,
			 CoglFixed  *user_tex_coords)
{
  CoglHandle	 material;
  const GList	*layers;
  int		 n_layers;
  const GList	*tmp;
  CoglHandle	*valid_layers = NULL;
  int		 n_valid_layers = 0;
  gboolean	 handle_slicing = FALSE;
  int		 i;
  GLfloat	*tex_coords_buff;
  GLfloat	 quad_coords[8];
  gulong	 enable_flags = 0;
  GLfloat	 values[4];

  /* FIXME - currently cogl deals with enabling texturing via enable flags,
   * but that can't scale to n texture units. Currently we have to be carefull
   * how we leave the environment so we don't break things. See the cleanup
   * notes at the end of this function */

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  material = ctx->source_material;

  layers = cogl_material_get_layers (material);
  n_layers = g_list_length ((GList *)layers);
  valid_layers = alloca (sizeof (CoglHandle) * n_layers);

  for (tmp = layers; tmp != NULL; tmp = tmp->next)
    {
      CoglHandle layer = tmp->data;
      CoglHandle texture = cogl_material_layer_get_texture (layer);
      
      if (cogl_material_layer_get_type (layer)
	  != COGL_MATERIAL_LAYER_TYPE_TEXTURE)
	continue;

      /* FIXME: support sliced textures. For now if the first layer is
       * sliced then all other layers are ignored, or if the first layer
       * is not sliced, we ignore sliced textures in other layers. */
      if (cogl_texture_is_sliced (texture))
	{
	  if (n_valid_layers == 0)
	    {
	      valid_layers[n_valid_layers++] = layer;
	      handle_slicing = TRUE;
	      break;
	    }
	  continue;
	}
      valid_layers[n_valid_layers++] = tmp->data;

      if (n_valid_layers >= CGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS)
	break;
    }
  
  /* NB: It could be that no valid texture layers were found, but
   * we will still submit a non-textured rectangle in that case. */
  if (n_valid_layers)
    tex_coords_buff = alloca (sizeof(GLfloat) * 8 * n_valid_layers);

  for (i = 0; i < n_valid_layers; i++)
    {
      CoglHandle layer = valid_layers[i];
      CoglHandle texture_handle = cogl_material_layer_get_texture (layer);
      CoglTexture *texture = _cogl_texture_pointer_from_handle (texture_handle);
      CoglFixed *in_tex_coords = &user_tex_coords[i * 4];
      GLfloat *out_tex_coords = &tex_coords_buff[i * 8];
      GLuint gl_tex_handle;
      GLenum gl_target;

#define CFX_F COGL_FIXED_TO_FLOAT
      /* IN LAYOUT: [ tx1:0, ty1:1, tx2:2, ty2:3 ]  */
      out_tex_coords[0] = CFX_F (in_tex_coords[0]); /* tx1 */
      out_tex_coords[1] = CFX_F (in_tex_coords[1]); /* ty1 */
      out_tex_coords[2] = CFX_F (in_tex_coords[2]); /* tx2 */
      out_tex_coords[3] = CFX_F (in_tex_coords[1]); /* ty1 */
      out_tex_coords[4] = CFX_F (in_tex_coords[0]); /* tx1 */
      out_tex_coords[5] = CFX_F (in_tex_coords[3]); /* ty2 */
      out_tex_coords[6] = CFX_F (in_tex_coords[2]); /* tx2 */
      out_tex_coords[7] = CFX_F (in_tex_coords[3]); /* ty2 */
#undef CFX_F

      /* TODO - support sliced textures */
      cogl_texture_get_gl_texture (texture, &gl_tex_handle, &gl_target);

      GE (glActiveTexture (GL_TEXTURE0 + i));
      cogl_material_layer_flush_gl_sampler_state (layer);
      GE( cogl_gles2_wrapper_bind_texture (gl_target,
					   gl_tex_handle,
					   texure->gl_intformat));
      /* GE (glEnable (GL_TEXTURE_2D)); */

      GE (glClientActiveTexture (GL_TEXTURE0 + i));
      GE (glTexCoordPointer (2, GL_FLOAT, 0, out_tex_coords));
      /* GE (glEnableClientState (GL_TEXTURE_COORD_ARRAY)); */

      /* FIXME - cogl only knows about one texture unit a.t.m
       * (Also see cleanup note below) */
      if (i == 0)
	enable_flags |= COGL_ENABLE_TEXTURE_2D | COGL_ENABLE_TEXCOORD_ARRAY;
      else
	{
	  GE (glEnable (GL_TEXTURE_2D));
	  GE (glEnableClientState (GL_TEXTURE_COORD_ARRAY));
	}
    }

#define CFX_F COGL_FIXED_TO_FLOAT
  quad_coords[0] = CFX_F (x1);
  quad_coords[1] = CFX_F (y1);
  quad_coords[2] = CFX_F (x2);
  quad_coords[3] = CFX_F (y1);
  quad_coords[4] = CFX_F (x1);
  quad_coords[5] = CFX_F (y2);
  quad_coords[6] = CFX_F (x2);
  quad_coords[7] = CFX_F (y2);
#undef CFX_F

  enable_flags |= COGL_ENABLE_VERTEX_ARRAY;
  GE( glVertexPointer (2, GL_FLOAT, 0, quad_coords));

  /* Setup the remaining GL state according to this material... */
  cogl_material_flush_gl_material_state (material);
  cogl_material_flush_gl_alpha_func (material);
  cogl_material_flush_gl_blend_func (material);
  /* FIXME: This api is a bit yukky, ideally it will be removed if we
   * re-work the cogl_enable mechanism */
  enable_flags |= cogl_material_get_cogl_enable_flags (material);

  /* FIXME - cogl only knows about one texture unit so assumes that unit 0
   * is always active...*/
  GE (glActiveTexture (GL_TEXTURE0));
  GE (glClientActiveTexture (GL_TEXTURE0));
  cogl_enable (enable_flags);
  glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

  /* FIXME - cogl doesn't currently have a way of caching the
   * enable states for more than one texture unit so for now,
   * we just disable anything relating to additional units once
   * we are done with them. */
  for (i = 1; i < n_valid_layers; i++)
    {
      GE (glActiveTexture (GL_TEXTURE0 + i));
      GE (glClientActiveTexture (GL_TEXTURE0 + i));

      GE (glDisable (GL_TEXTURE_2D));
      GE (glDisableClientState (GL_TEXTURE_COORD_ARRAY));
    }
  
  /* FIXME - CoglMaterials aren't yet used pervasively throughout
   * the cogl API, so we currently need to cleanup material state
   * that will confuse other parts of the API.
   * Other places to tweak, include the primitives API and lite
   * GL wrappers like cogl_rectangle */
  values[0] = 0.2; values[1] = 0.2; values[2] = 0.2; values[3] = 1.0;
  GE (glMaterialfv (GL_FRONT_AND_BACK, GL_AMBIENT, values));
  values[0] = 0.8; values[1] = 0.8; values[2] = 0.8; values[3] = 1.0;
  GE (glMaterialfv (GL_FRONT_AND_BACK, GL_DIFFUSE, values));
  values[0] = 0; values[1] = 0; values[2] = 0; values[3] = 1.0;
  GE (glMaterialfv (GL_FRONT_AND_BACK, GL_SPECULAR, values));
  values[0] = 0; values[1] = 0; values[2] = 0; values[3] = 1.0;
  GE (glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, values));
  values[0] = 0;
  GE (glMaterialfv (GL_FRONT_AND_BACK, GL_SHININESS, values));
}
