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
#include "cogl-bitmap-private.h"

#include <string.h>

static void _cogl_bitmap_free (CoglBitmap *bmp);

COGL_HANDLE_DEFINE (Bitmap, bitmap);

static void
_cogl_bitmap_free (CoglBitmap *bmp)
{
  g_free (bmp->data);
  g_free (bmp);
}

gint
_cogl_get_format_bpp (CoglPixelFormat format)
{
  gint bpp_lut[] = {
    0, /* invalid  */
    1, /* A_8      */
    3, /* 888      */
    4, /* 8888     */
    2, /* 565      */
    2, /* 4444     */
    2, /* 5551     */
    2, /* YUV      */
    1  /* G_8      */
  };

  return bpp_lut [format & COGL_UNORDERED_MASK];
}

gboolean
_cogl_bitmap_convert_and_premult (const CoglBitmap *bmp,
				  CoglBitmap       *dst_bmp,
				  CoglPixelFormat   dst_format)
{
  CoglBitmap  tmp_bmp = *bmp;
  CoglBitmap  new_bmp = *bmp;
  gboolean    new_bmp_owner = FALSE;

  /* Is base format different (not considering premult status)? */
  if ((bmp->format & COGL_UNPREMULT_MASK) !=
      (dst_format & COGL_UNPREMULT_MASK))
    {
      /* Try converting using imaging library */
      if (!_cogl_bitmap_convert (&new_bmp, &tmp_bmp, dst_format))
	{
	  /* ... or try fallback */
	  if (!_cogl_bitmap_fallback_convert (&new_bmp, &tmp_bmp, dst_format))
	    return FALSE;
	}

      /* Update bitmap with new data */
      new_bmp = tmp_bmp;
      new_bmp_owner = TRUE;
    }

  /* Do we need to unpremultiply */
  if ((bmp->format & COGL_PREMULT_BIT) > 0 &&
      (dst_format & COGL_PREMULT_BIT) == 0)
    {
      /* Try unpremultiplying using imaging library */
      if (!_cogl_bitmap_unpremult (&new_bmp, &tmp_bmp))
	{
	  /* ... or try fallback */
	  if (!_cogl_bitmap_fallback_unpremult (&new_bmp, &tmp_bmp))
	    {
	      if (new_bmp_owner)
		g_free (new_bmp.data);

	      return FALSE;
	    }
	}

      /* Update bitmap with new data */
      if (new_bmp_owner)
	g_free (new_bmp.data);

      new_bmp = tmp_bmp;
      new_bmp_owner = TRUE;
    }

  /* Do we need to premultiply */
  if ((bmp->format & COGL_PREMULT_BIT) == 0 &&
      (dst_format & COGL_PREMULT_BIT) > 0)
    {
      /* Try premultiplying using imaging library */
      if (!_cogl_bitmap_premult (&new_bmp, &tmp_bmp))
	{
	  /* ... or try fallback */
	  if (!_cogl_bitmap_fallback_premult (&new_bmp, &tmp_bmp))
	    {
	      if (new_bmp_owner)
		g_free (new_bmp.data);

	      return FALSE;
	    }
	}

      /* Update bitmap with new data */
      if (new_bmp_owner)
	g_free (new_bmp.data);

      new_bmp = tmp_bmp;
      new_bmp_owner = TRUE;
    }

  /* Output new bitmap info */
  *dst_bmp = new_bmp;

  return TRUE;
}

void
_cogl_bitmap_copy_subregion (CoglBitmap *src,
			     CoglBitmap *dst,
			     gint        src_x,
			     gint        src_y,
			     gint        dst_x,
			     gint        dst_y,
			     gint        width,
			     gint        height)
{
  guchar *srcdata;
  guchar *dstdata;
  gint    bpp;
  gint    line;

  /* Intended only for fast copies when format is equal! */
  g_assert (src->format == dst->format);
  bpp = _cogl_get_format_bpp (src->format);

  srcdata = src->data + src_y * src->rowstride + src_x * bpp;
  dstdata = dst->data + dst_y * dst->rowstride + dst_x * bpp;

  for (line=0; line<height; ++line)
    {
      memcpy (dstdata, srcdata, width * bpp);
      srcdata += src->rowstride;
      dstdata += dst->rowstride;
    }
}

gboolean
cogl_bitmap_get_size_from_file (const gchar *filename,
                                gint        *width,
                                gint        *height)
{
  return _cogl_bitmap_get_size_from_file (filename, width, height);
}

CoglHandle
cogl_bitmap_new_from_file (const gchar    *filename,
                           GError        **error)
{
  CoglBitmap   bmp;
  CoglBitmap  *ret;

  g_return_val_if_fail (error == NULL || *error == NULL, COGL_INVALID_HANDLE);

  /* Try loading with imaging backend */
  if (!_cogl_bitmap_from_file (&bmp, filename, error))
    {
      /* Try fallback */
      if (!_cogl_bitmap_fallback_from_file (&bmp, filename))
	return NULL;
      else if (error && *error)
	{
	  g_error_free (*error);
	  *error = NULL;
	}
    }

  ret = g_memdup (&bmp, sizeof (CoglBitmap));
  return _cogl_bitmap_handle_new (ret);
}

