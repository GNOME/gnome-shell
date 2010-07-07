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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-bitmap-private.h"

#include <string.h>

struct _CoglBitmap
{
  CoglHandleObject         _parent;
  CoglPixelFormat          format;
  int                      width;
  int                      height;
  int                      rowstride;

  guint8                  *data;
  CoglBitmapDestroyNotify  destroy_fn;
  void                    *destroy_fn_data;

  gboolean                 mapped;

  /* If this is non-null then 'data' is ignored and instead it is
     fetched from this shared bitmap. */
  CoglBitmap              *shared_bmp;
};

static void _cogl_bitmap_free (CoglBitmap *bmp);

COGL_OBJECT_DEFINE (Bitmap, bitmap);

static void
_cogl_bitmap_free (CoglBitmap *bmp)
{
  g_assert (!bmp->mapped);

  if (bmp->destroy_fn)
    bmp->destroy_fn (bmp->data, bmp->destroy_fn_data);

  if (bmp->shared_bmp)
    cogl_object_unref (bmp->shared_bmp);

  g_slice_free (CoglBitmap, bmp);
}

int
_cogl_get_format_bpp (CoglPixelFormat format)
{
  int bpp_lut[] = {
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
_cogl_bitmap_convert_premult_status (CoglBitmap      *bmp,
                                     CoglPixelFormat  dst_format)
{
  /* Do we need to unpremultiply? */
  if ((bmp->format & COGL_PREMULT_BIT) > 0 &&
      (dst_format & COGL_PREMULT_BIT) == 0)
    /* Try unpremultiplying using imaging library */
    return (_cogl_bitmap_unpremult (bmp)
            /* ... or try fallback */
            || _cogl_bitmap_fallback_unpremult (bmp));

  /* Do we need to premultiply? */
  if ((bmp->format & COGL_PREMULT_BIT) == 0 &&
      (dst_format & COGL_PREMULT_BIT) > 0)
    /* Try premultiplying using imaging library */
    return (_cogl_bitmap_premult (bmp)
            /* ... or try fallback */
            || _cogl_bitmap_fallback_premult (bmp));

  return TRUE;
}

CoglBitmap *
_cogl_bitmap_convert_format_and_premult (CoglBitmap *bmp,
                                         CoglPixelFormat   dst_format)
{
  CoglPixelFormat src_format = _cogl_bitmap_get_format (bmp);
  CoglBitmap *dst_bmp;

  /* Is base format different (not considering premult status)? */
  if ((src_format & COGL_UNPREMULT_MASK) !=
      (dst_format & COGL_UNPREMULT_MASK))
    {
      /* Try converting using imaging library */
      if ((dst_bmp = _cogl_bitmap_convert (bmp, dst_format)) == NULL)
        {
          /* ... or try fallback */
          if ((dst_bmp = _cogl_bitmap_fallback_convert (bmp,
                                                        dst_format)) == NULL)
            return NULL;
        }
    }
  else
    {
      int rowstride = _cogl_bitmap_get_rowstride (bmp);
      int height = _cogl_bitmap_get_height (bmp);
      guint8 *data;

      /* Copy the bitmap so that we can premultiply in-place */

      if ((data = _cogl_bitmap_map (bmp, COGL_BUFFER_ACCESS_READ, 0)) == NULL)
        return NULL;

      dst_bmp = _cogl_bitmap_new_from_data (g_memdup (data, height * rowstride),
                                            src_format,
                                            _cogl_bitmap_get_width (bmp),
                                            height,
                                            rowstride,
                                            (CoglBitmapDestroyNotify) g_free,
                                            NULL);

      _cogl_bitmap_unmap (bmp);
    }

  src_format = _cogl_bitmap_get_format (dst_bmp);

  /* We only need to do a premult conversion if both formats have an
     alpha channel. If we're converting from RGB to RGBA then the
     alpha will have been filled with 255 so the premult won't do
     anything or if we are converting from RGBA to RGB we're losing
     information so either converting or not will be wrong for
     transparent pixels */
  if ((src_format & COGL_A_BIT) == COGL_A_BIT &&
      (dst_format & COGL_A_BIT) == COGL_A_BIT &&
      !_cogl_bitmap_convert_premult_status (dst_bmp, dst_format))
    {
      cogl_object_unref (dst_bmp);
      return NULL;
    }

  return dst_bmp;
}

void
_cogl_bitmap_copy_subregion (CoglBitmap *src,
			     CoglBitmap *dst,
			     int         src_x,
			     int         src_y,
			     int         dst_x,
			     int         dst_y,
			     int         width,
			     int         height)
{
  guint8 *srcdata;
  guint8 *dstdata;
  int    bpp;
  int    line;

  /* Intended only for fast copies when format is equal! */
  g_assert (src->format == dst->format);
  bpp = _cogl_get_format_bpp (src->format);

  if ((srcdata = _cogl_bitmap_map (src, COGL_BUFFER_ACCESS_READ, 0)))
    {
      if ((dstdata = _cogl_bitmap_map (dst, COGL_BUFFER_ACCESS_WRITE, 0)))
        {
          srcdata += src_y * src->rowstride + src_x * bpp;
          dstdata += dst_y * dst->rowstride + dst_x * bpp;

          for (line=0; line<height; ++line)
            {
              memcpy (dstdata, srcdata, width * bpp);
              srcdata += src->rowstride;
              dstdata += dst->rowstride;
            }

          _cogl_bitmap_unmap (dst);
        }

      _cogl_bitmap_unmap (src);
    }
}

gboolean
cogl_bitmap_get_size_from_file (const char *filename,
                                int        *width,
                                int        *height)
{
  return _cogl_bitmap_get_size_from_file (filename, width, height);
}

CoglBitmap *
_cogl_bitmap_new_from_data (guint8                  *data,
                            CoglPixelFormat          format,
                            int                      width,
                            int                      height,
                            int                      rowstride,
                            CoglBitmapDestroyNotify  destroy_fn,
                            void                    *destroy_fn_data)
{
  CoglBitmap *bmp = g_slice_new (CoglBitmap);

  bmp->format = format;
  bmp->width = width;
  bmp->height = height;
  bmp->rowstride = rowstride;
  bmp->data = data;
  bmp->destroy_fn = destroy_fn;
  bmp->destroy_fn_data = destroy_fn_data;
  bmp->mapped = FALSE;
  bmp->shared_bmp = NULL;

  return _cogl_bitmap_object_new (bmp);
}

CoglBitmap *
_cogl_bitmap_new_shared (CoglBitmap              *shared_bmp,
                         CoglPixelFormat          format,
                         int                      width,
                         int                      height,
                         int                      rowstride)
{
  CoglBitmap *bmp = _cogl_bitmap_new_from_data (NULL, /* data */
                                                format,
                                                width,
                                                height,
                                                rowstride,
                                                NULL, /* destroy_fn */
                                                NULL /* destroy_fn_data */);

  bmp->shared_bmp = cogl_object_ref (shared_bmp);

  return bmp;
}

CoglBitmap *
cogl_bitmap_new_from_file (const char  *filename,
                           GError     **error)
{
  CoglBitmap *bmp;

  g_return_val_if_fail (error == NULL || *error == NULL, COGL_INVALID_HANDLE);

  if ((bmp = _cogl_bitmap_from_file (filename, error)) == NULL)
    {
      /* Try fallback */
      if ((bmp = _cogl_bitmap_fallback_from_file (filename))
          && error && *error)
        {
          g_error_free (*error);
          *error = NULL;
        }
    }

  return bmp;
}

CoglPixelFormat
_cogl_bitmap_get_format (CoglBitmap *bitmap)
{
  return bitmap->format;
}

void
_cogl_bitmap_set_format (CoglBitmap *bitmap,
                         CoglPixelFormat format)
{
  bitmap->format = format;
}

int
_cogl_bitmap_get_width (CoglBitmap *bitmap)
{
  return bitmap->width;
}

GQuark
cogl_bitmap_error_quark (void)
{
  return g_quark_from_static_string ("cogl-bitmap-error-quark");
}

int
_cogl_bitmap_get_height (CoglBitmap *bitmap)
{
  return bitmap->height;
}

int
_cogl_bitmap_get_rowstride (CoglBitmap *bitmap)
{
  return bitmap->rowstride;
}

guint8 *
_cogl_bitmap_map (CoglBitmap *bitmap,
                  CoglBufferAccess access,
                  CoglBufferMapHint hints)
{
  /* Divert to another bitmap if this data is shared */
  if (bitmap->shared_bmp)
    return _cogl_bitmap_map (bitmap->shared_bmp, access, hints);

  g_assert (!bitmap->mapped);
  bitmap->mapped = TRUE;

  /* Currently the bitmap is always in regular memory so we can just
     directly return the pointer */
  return bitmap->data;
}

void
_cogl_bitmap_unmap (CoglBitmap *bitmap)
{
  /* Divert to another bitmap if this data is shared */
  if (bitmap->shared_bmp)
    return _cogl_bitmap_unmap (bitmap->shared_bmp);

  g_assert (bitmap->mapped);
  bitmap->mapped = FALSE;

  /* Currently the bitmap is always in regular memory so we don't need
     to do anything */
}
