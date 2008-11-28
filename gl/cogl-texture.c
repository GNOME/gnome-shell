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
#include "cogl-context.h"
#include "cogl-handle.h"

#include <string.h>
#include <stdlib.h>

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
  GE( glPixelStorei (GL_UNPACK_ROW_LENGTH, pixels_rowstride / pixels_bpp) );
  
  GE( glPixelStorei (GL_UNPACK_SKIP_PIXELS, pixels_src_x) );
  GE( glPixelStorei (GL_UNPACK_SKIP_ROWS, pixels_src_y) );
  
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
	  
	  /* Setup gl alignment to match rowstride and top-left corner */
	  prep_for_gl_pixels_upload (tex->bitmap.rowstride,
				     x_span->start,
				     y_span->start,
				     bpp);
	  
	  /* Upload new image data */
	  GE( glBindTexture (tex->gl_target, gl_handle) );

	  GE( glTexSubImage2D (tex->gl_target, 0,
			       0,
			       0,
			       x_span->size - x_span->waste,
			       y_span->size - y_span->waste,
			       tex->gl_format, tex->gl_type,
			       tex->bitmap.data) );

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
	}
    }

  if (waste_buf)
    g_free (waste_buf);

  return TRUE;
}

static gboolean
_cogl_texture_download_from_gl (CoglTexture *tex,
				CoglBitmap  *target_bmp,
				GLuint       target_gl_format,
				GLuint       target_gl_type)
{
  CoglTexSliceSpan  *x_span;
  CoglTexSliceSpan  *y_span;
  GLuint             gl_handle;
  gint               bpp;
  gint               x,y;
  CoglBitmap         slice_bmp;
  
  bpp = _cogl_get_format_bpp (target_bmp->format);
  
  /* Iterate vertical slices */
  for (y = 0; y < tex->slice_y_spans->len; ++y)
    {
      y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, y);
      
      /* Iterate horizontal slices */
      for (x = 0; x < tex->slice_x_spans->len; ++x)
	{
	  /*if (x != 0 || y != 1) continue;*/
	  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, x);
	  
	  /* Pick the gl texture object handle */
	  gl_handle = g_array_index (tex->slice_gl_handles, GLuint,
				     y * tex->slice_x_spans->len + x);
	  
	  /* If there's any waste we need to copy manually
             (no glGetTexSubImage) */
	  
	  if (y_span->waste != 0 || x_span->waste != 0)
	    {
	      /* Setup temp bitmap for slice subregion */
	      slice_bmp.format = tex->bitmap.format;
	      slice_bmp.width  = x_span->size;
	      slice_bmp.height = y_span->size;
	      slice_bmp.rowstride = bpp * slice_bmp.width;
	      slice_bmp.data = (guchar*) g_malloc (slice_bmp.rowstride *
						   slice_bmp.height);
	      
	      /* Setup gl alignment to 0,0 top-left corner */
	      prep_for_gl_pixels_download (slice_bmp.rowstride);
	      
	      /* Download slice image data into temp bmp */
	      GE( glBindTexture (tex->gl_target, gl_handle) );
	      
	      GE (glGetTexImage (tex->gl_target,
				 0, /* level */
				 target_gl_format,
				 target_gl_type,
				 slice_bmp.data) );
	      
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

	      prep_for_gl_pixels_download (target_bmp->rowstride);
	      
	      /* Download slice image data */
	      GE( glBindTexture (tex->gl_target, gl_handle) );
	      
	      GE( glGetTexImage (tex->gl_target,
				 0, /* level */
				 target_gl_format,
				 target_gl_type,
				 dst) );
	    }
	}
    }
  
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
	  
	  /* Setup gl alignment to match rowstride and top-left corner */
	  prep_for_gl_pixels_upload (source_bmp->rowstride,
				     source_x,
				     source_y,
				     bpp);
	  
	  /* Upload new image data */
	  GE( glBindTexture (tex->gl_target, gl_handle) );
	  
	  GE( glTexSubImage2D (tex->gl_target, 0,
			       local_x, local_y,
			       inter_w, inter_h,
			       source_gl_format,
			       source_gl_type,
			       source_bmp->data) );

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
  if (gl_target ==  GL_TEXTURE_2D)
    {
      /* Proxy texture allows for a quick check for supported size */
      
      GLint new_width = 0;

      GE( glTexImage2D (GL_PROXY_TEXTURE_2D, 0, GL_RGBA,
			width, height, 0 /* border */,
			gl_format, gl_type, NULL) );

      GE( glGetTexLevelParameteriv (GL_PROXY_TEXTURE_2D, 0,
				    GL_TEXTURE_WIDTH, &new_width) );

      return new_width != 0;
    }
  else
    {
      /* not used */
      return 0;
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
	  GE( glBindTexture (tex->gl_target,
			     gl_handles[y * n_x_slices + x]) );
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

	  /* Use a transparent border color so that we can leave the
	     color buffer alone when using texture co-ordinates
	     outside of the texture */
	  GE( glTexParameterfv (tex->gl_target, GL_TEXTURE_BORDER_COLOR,
				transparent_color) );

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
  /* It doesn't really matter we convert to exact same
     format (some have no cogl match anyway) since format
     is re-matched against cogl when getting or setting
     texture image data.
  */
  
  switch (gl_int_format)
    {
    case GL_ALPHA: case GL_ALPHA4: case GL_ALPHA8:
    case GL_ALPHA12: case GL_ALPHA16:
      
      *out_format = COGL_PIXEL_FORMAT_A_8;
      return TRUE;
      
    case GL_LUMINANCE: case GL_LUMINANCE4: case GL_LUMINANCE8:
    case GL_LUMINANCE12: case GL_LUMINANCE16:
      
      *out_format = COGL_PIXEL_FORMAT_G_8;
      return TRUE;
      
    case GL_RGB: case GL_RGB4: case GL_RGB5: case GL_RGB8:
    case GL_RGB10: case GL_RGB12: case GL_RGB16: case GL_R3_G3_B2:
      
      *out_format = COGL_PIXEL_FORMAT_RGB_888;
      return TRUE;
      
    case GL_RGBA: case GL_RGBA2: case GL_RGBA4: case GL_RGB5_A1:
    case GL_RGBA8: case GL_RGB10_A2: case GL_RGBA12: case GL_RGBA16:
      
      *out_format = COGL_PIXEL_FORMAT_RGBA_8888;
      return TRUE;
    }
  
  return FALSE;
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
      
    case COGL_PIXEL_FORMAT_RGB_888:
      glintformat = GL_RGB;
      glformat = GL_RGB;
      gltype = GL_UNSIGNED_BYTE;
      break;
    case COGL_PIXEL_FORMAT_BGR_888:
      glintformat = GL_RGB;
      glformat = GL_BGR;
      gltype = GL_UNSIGNED_BYTE;
      break;
    case COGL_PIXEL_FORMAT_RGBA_8888:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_BYTE;
      break;
    case COGL_PIXEL_FORMAT_BGRA_8888:
      glintformat = GL_RGBA;
      glformat = GL_BGRA;
      gltype = GL_UNSIGNED_BYTE;
      break;
      
      /* The following two types of channel ordering
       * have no GL equivalent unless defined using
       * system word byte ordering */
    case COGL_PIXEL_FORMAT_ARGB_8888:
      glintformat = GL_RGBA;
      glformat = GL_BGRA;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      gltype = GL_UNSIGNED_INT_8_8_8_8;
#else
      gltype = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif
      break;
      
    case COGL_PIXEL_FORMAT_ABGR_8888:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      gltype = GL_UNSIGNED_INT_8_8_8_8;
#else
      gltype = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif
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
  
  /* Find closest format that's supported by GL */
  closest_format = _cogl_pixel_format_to_gl (format,
					     NULL, /* don't need */
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
_cogl_texture_flush_vertices (void)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (ctx->texture_vertices > 0)
    {
      CoglTextureGLVertex *p
        = (CoglTextureGLVertex *) ctx->texture_vertices->data;

      GE( glVertexPointer (2, GL_FLOAT,
                           sizeof (CoglTextureGLVertex), p->v ) );
      GE( glTexCoordPointer (2, GL_FLOAT,
                             sizeof (CoglTextureGLVertex), p->t ) );

      GE( glBindTexture (ctx->texture_target, ctx->texture_current) );
      GE( glDrawArrays (GL_QUADS, 0, ctx->texture_vertices->len) );

      g_array_set_size (ctx->texture_vertices, 0);
    }
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
  CoglSpanIter         iter_x    ,  iter_y;
  CoglFixed            tw        ,  th;
  CoglFixed            tqx       ,  tqy;
  CoglFixed            first_tx  ,  first_ty;
  CoglFixed            first_qx  ,  first_qy;
  CoglFixed            slice_tx1 ,  slice_ty1;
  CoglFixed            slice_tx2 ,  slice_ty2;
  CoglFixed            slice_qx1 ,  slice_qy1;
  CoglFixed            slice_qx2 ,  slice_qy2;
  GLuint               gl_handle;
  CoglTextureGLVertex *p;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

#if COGL_DEBUG
  printf("=== Drawing Tex Quad (Software Tiling Mode) ===\n");
#endif

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

          /* If we're using a different texture from the one already queued
             then flush the vertices */
          if (ctx->texture_vertices->len > 0
              && gl_handle != ctx->texture_current)
            _cogl_texture_flush_vertices ();
          ctx->texture_target = tex->gl_target;
          ctx->texture_current = gl_handle;

          /* Add the quad to the list of queued vertices */
          g_array_set_size (ctx->texture_vertices,
                            ctx->texture_vertices->len + 4);
          p = &g_array_index (ctx->texture_vertices, CoglTextureGLVertex,
                              ctx->texture_vertices->len - 4);

#define CFX_F(x) COGL_FIXED_TO_FLOAT(x)

          p->v[0] = CFX_F (slice_qx1); p->v[1] = CFX_F (slice_qy2);
          p->t[0] = CFX_F (slice_tx1); p->t[1] = CFX_F (slice_ty2);
          p++;
          p->v[0] = CFX_F (slice_qx2); p->v[1] = CFX_F (slice_qy2);
          p->t[0] = CFX_F (slice_tx2); p->t[1] = CFX_F (slice_ty2);
          p++;
          p->v[0] = CFX_F (slice_qx2); p->v[1] = CFX_F (slice_qy1);
          p->t[0] = CFX_F (slice_tx2); p->t[1] = CFX_F (slice_ty1);
          p++;
          p->v[0] = CFX_F (slice_qx1); p->v[1] = CFX_F (slice_qy1);
          p->t[0] = CFX_F (slice_tx1); p->t[1] = CFX_F (slice_ty1);
          p++;

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
  GLuint               gl_handle;
  CoglTexSliceSpan    *x_span;
  CoglTexSliceSpan    *y_span;
  CoglTextureGLVertex *p;

#if COGL_DEBUG
  printf("=== Drawing Tex Quad (Hardware Tiling Mode) ===\n");
#endif

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Pick and bind opengl texture object */
  gl_handle = g_array_index (tex->slice_gl_handles, GLuint, 0);

  /* If we're using a different texture from the one already queued
     then flush the vertices */
  if (ctx->texture_vertices->len > 0 && gl_handle != ctx->texture_current)
    _cogl_texture_flush_vertices ();
  ctx->texture_target = tex->gl_target;
  ctx->texture_current = gl_handle;

  /* Don't include the waste in the texture coordinates */
  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, 0);
  y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, 0);

  /* Don't include the waste in the texture coordinates */
  tx1 = tx1 * (x_span->size - x_span->waste) / x_span->size;
  tx2 = tx2 * (x_span->size - x_span->waste) / x_span->size;
  ty1 = ty1 * (y_span->size - y_span->waste) / y_span->size;
  ty2 = ty2 * (y_span->size - y_span->waste) / y_span->size;

  /* Add the quad to the list of queued vertices */
  g_array_set_size (ctx->texture_vertices, ctx->texture_vertices->len + 4);
  p = &g_array_index (ctx->texture_vertices, CoglTextureGLVertex,
                      ctx->texture_vertices->len - 4);

#define CFX_F(x) COGL_FIXED_TO_FLOAT(x)

  p->v[0] = CFX_F (x1); p->v[1] = CFX_F (y2);
  p->t[0] = CFX_F (tx1); p->t[1] = CFX_F (ty2);
  p++;
  p->v[0] = CFX_F (x2); p->v[1] = CFX_F (y2);
  p->t[0] = CFX_F (tx2); p->t[1] = CFX_F (ty2);
  p++;
  p->v[0] = CFX_F (x2); p->v[1] = CFX_F (y1);
  p->t[0] = CFX_F (tx2); p->t[1] = CFX_F (ty1);
  p++;
  p->v[0] = CFX_F (x1); p->v[1] = CFX_F (y1);
  p->t[0] = CFX_F (tx1); p->t[1] = CFX_F (ty1);
  p++;

#undef CFX_F
}

void
cogl_texture_multiple_rectangles (CoglHandle       handle,
                                  const CoglFixed *verts,
                                  guint            n_rects)
{
  CoglTexture    *tex;
  gulong          enable_flags = (COGL_ENABLE_TEXTURE_2D
                                  | COGL_ENABLE_VERTEX_ARRAY
                                  | COGL_ENABLE_TEXCOORD_ARRAY);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Check if valid texture */
  if (!cogl_is_texture (handle))
    return;
  
  tex = _cogl_texture_pointer_from_handle (handle);
  
  /* Make sure we got stuff to draw */
  if (tex->slice_gl_handles == NULL)
    return;
  
  if (tex->slice_gl_handles->len == 0)
    return;
  
  /* Prepare GL state */
  if (ctx->color_alpha < 255
      || tex->bitmap.format & COGL_A_BIT)
    enable_flags |= COGL_ENABLE_BLEND;

  if (ctx->enable_backface_culling)
    enable_flags |= COGL_ENABLE_BACKFACE_CULLING;

  cogl_enable (enable_flags);

  g_array_set_size (ctx->texture_vertices, 0);

  while (n_rects-- > 0)
    {
      if (verts[4] != verts[6] && verts[5] != verts[7])
        {
          /* If there is only one GL texture and either the texture is
             NPOT (no waste) or all of the coordinates are in the
             range [0,1] then we can use hardware tiling */
          if (tex->slice_gl_handles->len == 1
              && (cogl_features_available (COGL_FEATURE_TEXTURE_NPOT)
                  || (verts[4] >= 0 && verts[4] <= COGL_FIXED_1
                      && verts[6] >= 0 && verts[6] <= COGL_FIXED_1
                      && verts[5] >= 0 && verts[5] <= COGL_FIXED_1
                      && verts[7] >= 0 && verts[7] <= COGL_FIXED_1)))
            _cogl_texture_quad_hw (tex, verts[0],verts[1], verts[2],verts[3],
                                   verts[4],verts[5], verts[6],verts[7]);
          else
            _cogl_texture_quad_sw (tex, verts[0],verts[1], verts[2],verts[3],
                                   verts[4],verts[5], verts[6],verts[7]);
        }

      verts += 8;
    }

  _cogl_texture_flush_vertices ();
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
  CoglFixed verts[8];

  verts[0] = x1;
  verts[1] = y1;
  verts[2] = x2;
  verts[3] = y2;
  verts[4] = tx1;
  verts[5] = ty1;
  verts[6] = tx2;
  verts[7] = ty2;

  cogl_texture_multiple_rectangles (handle, verts, 1);
}

void
cogl_texture_polygon (CoglHandle         handle,
		      guint              n_vertices,
		      CoglTextureVertex *vertices,
		      gboolean           use_color)
{
  CoglTexture      *tex;
  int               i, x, y, tex_num;
  GLuint            gl_handle;
  CoglTexSliceSpan *y_span, *x_span;
  gulong            enable_flags;
  CoglTextureGLVertex *p;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Check if valid texture */
  if (!cogl_is_texture (handle))
    return;

  tex = _cogl_texture_pointer_from_handle (handle);

  /* The polygon will have artifacts where the slices join if the wrap
     mode is GL_LINEAR because the filtering will pull in pixels from
     the transparent border. To make it clear that the function
     shouldn't be used in these circumstances we just bail out and
     draw nothing */
  if (tex->slice_gl_handles->len != 1
      && (tex->min_filter != GL_NEAREST || tex->mag_filter != GL_NEAREST))
    {
      static gboolean shown_warning = FALSE;

      if (!shown_warning)
	{
	  g_warning ("cogl_texture_polygon does not work for sliced textures "
		     "when the minification and magnification filters are not "
		     "CGL_NEAREST");
	  shown_warning = TRUE;
	}
      return;
    }

  /* Make sure there is enough space in the global texture vertex
     array. This is used so we can render the polygon with a single
     call to OpenGL but still support any number of vertices */
  g_array_set_size (ctx->texture_vertices, n_vertices);
  p = (CoglTextureGLVertex *) ctx->texture_vertices->data;

  /* Prepare GL state */
  enable_flags = (COGL_ENABLE_TEXTURE_2D
		  | COGL_ENABLE_VERTEX_ARRAY
		  | COGL_ENABLE_TEXCOORD_ARRAY
		  | COGL_ENABLE_BLEND);


  if (ctx->enable_backface_culling)
    enable_flags |= COGL_ENABLE_BACKFACE_CULLING;

  if (use_color)
    {
      enable_flags |= COGL_ENABLE_COLOR_ARRAY;
      GE( glColorPointer (4, GL_UNSIGNED_BYTE,
                          sizeof (CoglTextureGLVertex), p->c) );
    }

  GE( glVertexPointer (3, GL_FLOAT, sizeof (CoglTextureGLVertex), p->v ) );
  GE( glTexCoordPointer (2, GL_FLOAT, sizeof (CoglTextureGLVertex), p->t ) );

  cogl_enable (enable_flags);

  /* Temporarily change the wrapping mode on all of the slices to use
     a transparent border */
  for (i = 0; i < tex->slice_gl_handles->len; i++)
    {
      GE( glBindTexture (tex->gl_target,
			 g_array_index (tex->slice_gl_handles, GLuint, i)) );
      GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_S,
			   GL_CLAMP_TO_BORDER) );
      GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_T,
			   GL_CLAMP_TO_BORDER) );
    }

  tex_num = 0;

  /* Render all of the slices with the full geometry but use a
     transparent border color so that any part of the texture not
     covered by the slice will be ignored */
  for (y = 0; y < tex->slice_y_spans->len; y++)
    {
      y_span = &g_array_index (tex->slice_y_spans, CoglTexSliceSpan, y);

      for (x = 0; x < tex->slice_x_spans->len; x++)
	{
	  x_span = &g_array_index (tex->slice_x_spans, CoglTexSliceSpan, x);

	  gl_handle = g_array_index (tex->slice_gl_handles, GLuint, tex_num++);

          p = (CoglTextureGLVertex *) ctx->texture_vertices->data;

	  /* Convert the vertices into an array of GLfloats ready to pass to
	     OpenGL */
	  for (i = 0; i < n_vertices; i++, p++)
	    {
#define CFX_F COGL_FIXED_TO_FLOAT

	      p->v[0] = CFX_F(vertices[i].x);
	      p->v[1] = CFX_F(vertices[i].y);
	      p->v[2] = CFX_F(vertices[i].z);
	      p->t[0] = CFX_F((vertices[i].tx
                               - (COGL_FIXED_FROM_INT (x_span->start)
                                  / tex->bitmap.width))
                              * tex->bitmap.width / x_span->size);
	      p->t[1] = CFX_F((vertices[i].ty
                               - (COGL_FIXED_FROM_INT (y_span->start)
                                  / tex->bitmap.height))
			      * tex->bitmap.height / y_span->size);
	      p->c[0] = cogl_color_get_red_byte(&vertices[i].color);
	      p->c[1] = cogl_color_get_green_byte(&vertices[i].color);
	      p->c[2] = cogl_color_get_blue_byte(&vertices[i].color);
	      p->c[3] = cogl_color_get_alpha_byte(&vertices[i].color);

#undef CFX_F
	    }

	  GE( glBindTexture (tex->gl_target, gl_handle) );

	  GE( glDrawArrays (GL_TRIANGLE_FAN, 0, n_vertices) );
	}
    }
  
  /* Restore the wrapping mode */
  for (i = 0; i < tex->slice_gl_handles->len; i++)
    {
      GE( glBindTexture (tex->gl_target,
			 g_array_index (tex->slice_gl_handles, GLuint, i)) );
      GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_S, tex->wrap_mode) );
      GE( glTexParameteri (tex->gl_target, GL_TEXTURE_WRAP_T, tex->wrap_mode) );
    }
}
