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

#include "cogl-util.h"
#include "cogl-debug.h"
#include "cogl-private.h"
#include "cogl-bitmap-private.h"
#include "cogl-buffer-private.h"
#include "cogl-pixel-buffer.h"
#include "cogl-context-private.h"

#include <string.h>

struct _CoglBitmap
{
  CoglHandleObject         _parent;

  /* Pointer back to the context that this bitmap was created with */
  CoglContext             *context;

  CoglPixelFormat          format;
  int                      width;
  int                      height;
  int                      rowstride;

  guint8                  *data;

  gboolean                 mapped;
  gboolean                 bound;

  /* If this is non-null then 'data' is ignored and instead it is
     fetched from this shared bitmap. */
  CoglBitmap              *shared_bmp;

  /* If this is non-null then 'data' is treated as an offset into the
     buffer and map will divert to mapping the buffer */
  CoglBuffer              *buffer;
};

static void _cogl_bitmap_free (CoglBitmap *bmp);

COGL_OBJECT_DEFINE (Bitmap, bitmap);

static void
_cogl_bitmap_free (CoglBitmap *bmp)
{
  g_assert (!bmp->mapped);
  g_assert (!bmp->bound);

  if (bmp->shared_bmp)
    cogl_object_unref (bmp->shared_bmp);

  if (bmp->buffer)
    cogl_object_unref (bmp->buffer);

  if (bmp->context)
    cogl_object_unref (bmp->context);

  g_slice_free (CoglBitmap, bmp);
}

gboolean
_cogl_bitmap_convert_premult_status (CoglBitmap      *bmp,
                                     CoglPixelFormat  dst_format)
{
  /* Do we need to unpremultiply? */
  if ((bmp->format & COGL_PREMULT_BIT) > 0 &&
      (dst_format & COGL_PREMULT_BIT) == 0 &&
      COGL_PIXEL_FORMAT_CAN_HAVE_PREMULT (dst_format))
    return _cogl_bitmap_unpremult (bmp);

  /* Do we need to premultiply? */
  if ((bmp->format & COGL_PREMULT_BIT) == 0 &&
      COGL_PIXEL_FORMAT_CAN_HAVE_PREMULT (bmp->format) &&
      (dst_format & COGL_PREMULT_BIT) > 0)
    /* Try premultiplying using imaging library */
    return _cogl_bitmap_premult (bmp);

  return TRUE;
}

CoglBitmap *
_cogl_bitmap_copy (CoglBitmap *src_bmp)
{
  CoglBitmap *dst_bmp;
  CoglPixelFormat src_format = cogl_bitmap_get_format (src_bmp);
  int width = cogl_bitmap_get_width (src_bmp);
  int height = cogl_bitmap_get_height (src_bmp);

  dst_bmp =
    _cogl_bitmap_new_with_malloc_buffer (src_bmp->context,
                                         width, height,
                                         src_format);

  _cogl_bitmap_copy_subregion (src_bmp,
                               dst_bmp,
                               0, 0, /* src_x/y */
                               0, 0, /* dst_x/y */
                               width, height);

  return dst_bmp;
}

gboolean
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
  gboolean succeeded = FALSE;

  /* Intended only for fast copies when format is equal! */
  g_assert ((src->format & ~COGL_PREMULT_BIT) ==
            (dst->format & ~COGL_PREMULT_BIT));

  bpp = _cogl_pixel_format_get_bytes_per_pixel (src->format);

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

          succeeded = TRUE;

          _cogl_bitmap_unmap (dst);
        }

      _cogl_bitmap_unmap (src);
    }

  return succeeded;
}

gboolean
cogl_bitmap_get_size_from_file (const char *filename,
                                int        *width,
                                int        *height)
{
  return _cogl_bitmap_get_size_from_file (filename, width, height);
}

CoglBitmap *
cogl_bitmap_new_for_data (CoglContext *context,
                          int width,
                          int height,
                          CoglPixelFormat format,
                          int rowstride,
                          guint8 *data)
{
  CoglBitmap *bmp;

  g_return_val_if_fail (cogl_is_context (context), NULL);

  bmp = g_slice_new (CoglBitmap);
  bmp->context = cogl_object_ref (context);
  bmp->format = format;
  bmp->width = width;
  bmp->height = height;
  bmp->rowstride = rowstride;
  bmp->data = data;
  bmp->mapped = FALSE;
  bmp->bound = FALSE;
  bmp->shared_bmp = NULL;
  bmp->buffer = NULL;

  return _cogl_bitmap_object_new (bmp);
}

CoglBitmap *
_cogl_bitmap_new_with_malloc_buffer (CoglContext *context,
                                     unsigned int width,
                                     unsigned int height,
                                     CoglPixelFormat format)
{
  static CoglUserDataKey bitmap_free_key;
  int bpp = _cogl_pixel_format_get_bytes_per_pixel (format);
  int rowstride = ((width * bpp) + 3) & ~3;
  guint8 *data = g_malloc (rowstride * height);
  CoglBitmap *bitmap;

  bitmap = cogl_bitmap_new_for_data (context,
                                     width, height,
                                     format,
                                     rowstride,
                                     data);
  cogl_object_set_user_data (COGL_OBJECT (bitmap),
                             &bitmap_free_key,
                             data,
                             g_free);

  return bitmap;
}

CoglBitmap *
_cogl_bitmap_new_shared (CoglBitmap              *shared_bmp,
                         CoglPixelFormat          format,
                         int                      width,
                         int                      height,
                         int                      rowstride)
{
  CoglBitmap *bmp;

  bmp = cogl_bitmap_new_for_data (shared_bmp->context,
                                  width, height,
                                  format,
                                  rowstride,
                                  NULL /* data */);

  bmp->shared_bmp = cogl_object_ref (shared_bmp);

  return bmp;
}

CoglBitmap *
cogl_bitmap_new_from_file (const char  *filename,
                           GError     **error)
{
  _COGL_RETURN_VAL_IF_FAIL (error == NULL || *error == NULL, COGL_INVALID_HANDLE);

  return _cogl_bitmap_from_file (filename, error);
}

CoglBitmap *
cogl_bitmap_new_from_buffer (CoglBuffer *buffer,
                             CoglPixelFormat format,
                             int width,
                             int height,
                             int rowstride,
                             int offset)
{
  CoglBitmap *bmp;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_buffer (buffer), NULL);

  bmp = cogl_bitmap_new_for_data (buffer->context,
                                  width, height,
                                  format,
                                  rowstride,
                                  NULL /* data */);

  bmp->buffer = cogl_object_ref (buffer);
  bmp->data = GINT_TO_POINTER (offset);

  return bmp;
}

CoglBitmap *
cogl_bitmap_new_with_size (CoglContext *context,
                           unsigned int width,
                           unsigned int height,
                           CoglPixelFormat format)
{
  CoglPixelBuffer *pixel_buffer;
  CoglBitmap *bitmap;
  unsigned int rowstride;

  /* creating a buffer to store "any" format does not make sense */
  if (G_UNLIKELY (format == COGL_PIXEL_FORMAT_ANY))
    return NULL;

  /* for now we fallback to cogl_pixel_buffer_new, later, we could ask
   * libdrm a tiled buffer for instance */
  rowstride = width * _cogl_pixel_format_get_bytes_per_pixel (format);

  pixel_buffer = cogl_pixel_buffer_new (context, height * rowstride, NULL);

  if (G_UNLIKELY (pixel_buffer == NULL))
    return NULL;

  bitmap = cogl_bitmap_new_from_buffer (COGL_BUFFER (pixel_buffer),
                                        format,
                                        width, height,
                                        rowstride,
                                        0 /* offset */);

  cogl_object_unref (pixel_buffer);

  return bitmap;
}

CoglPixelFormat
cogl_bitmap_get_format (CoglBitmap *bitmap)
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
cogl_bitmap_get_width (CoglBitmap *bitmap)
{
  return bitmap->width;
}

int
cogl_bitmap_get_height (CoglBitmap *bitmap)
{
  return bitmap->height;
}

int
cogl_bitmap_get_rowstride (CoglBitmap *bitmap)
{
  return bitmap->rowstride;
}

CoglPixelBuffer *
cogl_bitmap_get_buffer (CoglBitmap *bitmap)
{
  while (bitmap->shared_bmp)
    bitmap = bitmap->shared_bmp;

  return COGL_PIXEL_BUFFER (bitmap->buffer);
}

GQuark
cogl_bitmap_error_quark (void)
{
  return g_quark_from_static_string ("cogl-bitmap-error-quark");
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

  if (bitmap->buffer)
    {
      guint8 *data = cogl_buffer_map (bitmap->buffer,
                                      access,
                                      hints);

      COGL_NOTE (BITMAP, "A pixel array is being mapped from a bitmap. This "
                 "usually means that some conversion on the pixel array is "
                 "needed so a sub-optimal format is being used.");

      if (data)
        {
          bitmap->mapped = TRUE;

          return data + GPOINTER_TO_INT (bitmap->data);
        }
      else
        return NULL;
    }
  else
    {
      bitmap->mapped = TRUE;

      return bitmap->data;
    }
}

void
_cogl_bitmap_unmap (CoglBitmap *bitmap)
{
  /* Divert to another bitmap if this data is shared */
  if (bitmap->shared_bmp)
    {
      _cogl_bitmap_unmap (bitmap->shared_bmp);
      return;
    }

  g_assert (bitmap->mapped);
  bitmap->mapped = FALSE;

  if (bitmap->buffer)
    cogl_buffer_unmap (bitmap->buffer);
}

guint8 *
_cogl_bitmap_bind (CoglBitmap *bitmap,
                   CoglBufferAccess access,
                   CoglBufferMapHint hints)
{
  guint8 *ptr;

  /* Divert to another bitmap if this data is shared */
  if (bitmap->shared_bmp)
    return _cogl_bitmap_bind (bitmap->shared_bmp, access, hints);

  g_assert (!bitmap->bound);

  /* If the bitmap wasn't created from a buffer then the
     implementation of bind is the same as map */
  if (bitmap->buffer == NULL)
    {
      guint8 *data = _cogl_bitmap_map (bitmap, access, hints);
      if (data)
        bitmap->bound = TRUE;
      return data;
    }

  bitmap->bound = TRUE;

  if (access == COGL_BUFFER_ACCESS_READ)
    ptr = _cogl_buffer_bind (bitmap->buffer,
                             COGL_BUFFER_BIND_TARGET_PIXEL_UNPACK);
  else if (access == COGL_BUFFER_ACCESS_WRITE)
    ptr = _cogl_buffer_bind (bitmap->buffer,
                             COGL_BUFFER_BIND_TARGET_PIXEL_PACK);
  else
    g_assert_not_reached ();

  /* The data pointer actually stores the offset */
  return GPOINTER_TO_INT (bitmap->data) + ptr;
}

void
_cogl_bitmap_unbind (CoglBitmap *bitmap)
{
  /* Divert to another bitmap if this data is shared */
  if (bitmap->shared_bmp)
    {
      _cogl_bitmap_unbind (bitmap->shared_bmp);
      return;
    }

  g_assert (bitmap->bound);
  bitmap->bound = FALSE;

  /* If the bitmap wasn't created from a pixel array then the
     implementation of unbind is the same as unmap */
  if (bitmap->buffer)
    _cogl_buffer_unbind (bitmap->buffer);
  else
    _cogl_bitmap_unmap (bitmap);
}

CoglContext *
_cogl_bitmap_get_context (CoglBitmap *bitmap)
{
  return bitmap->context;
}
