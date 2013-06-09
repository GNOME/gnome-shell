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
#include "cogl-buffer-gl-private.h"
#include "cogl-error-private.h"

#include <string.h>

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

  g_slice_free (CoglBitmap, bmp);
}

CoglBool
_cogl_bitmap_convert_premult_status (CoglBitmap *bmp,
                                     CoglPixelFormat dst_format,
                                     CoglError **error)
{
  /* Do we need to unpremultiply? */
  if ((bmp->format & COGL_PREMULT_BIT) > 0 &&
      (dst_format & COGL_PREMULT_BIT) == 0 &&
      COGL_PIXEL_FORMAT_CAN_HAVE_PREMULT (dst_format))
    return _cogl_bitmap_unpremult (bmp, error);

  /* Do we need to premultiply? */
  if ((bmp->format & COGL_PREMULT_BIT) == 0 &&
      COGL_PIXEL_FORMAT_CAN_HAVE_PREMULT (bmp->format) &&
      (dst_format & COGL_PREMULT_BIT) > 0)
    /* Try premultiplying using imaging library */
    return _cogl_bitmap_premult (bmp, error);

  return TRUE;
}

CoglBitmap *
_cogl_bitmap_copy (CoglBitmap *src_bmp,
                   CoglError **error)
{
  CoglBitmap *dst_bmp;
  CoglPixelFormat src_format = cogl_bitmap_get_format (src_bmp);
  int width = cogl_bitmap_get_width (src_bmp);
  int height = cogl_bitmap_get_height (src_bmp);

  dst_bmp =
    _cogl_bitmap_new_with_malloc_buffer (src_bmp->context,
                                         width, height,
                                         src_format,
                                         error);
  if (!dst_bmp)
    return NULL;

  if (!_cogl_bitmap_copy_subregion (src_bmp,
                                    dst_bmp,
                                    0, 0, /* src_x/y */
                                    0, 0, /* dst_x/y */
                                    width, height,
                                    error))
    {
      cogl_object_unref (dst_bmp);
      return NULL;
    }

  return dst_bmp;
}

CoglBool
_cogl_bitmap_copy_subregion (CoglBitmap *src,
			     CoglBitmap *dst,
			     int src_x,
			     int src_y,
			     int dst_x,
			     int dst_y,
			     int width,
			     int height,
                             CoglError **error)
{
  uint8_t *srcdata;
  uint8_t *dstdata;
  int bpp;
  int line;
  CoglBool succeeded = FALSE;

  /* Intended only for fast copies when format is equal! */
  _COGL_RETURN_VAL_IF_FAIL ((src->format & ~COGL_PREMULT_BIT) ==
                            (dst->format & ~COGL_PREMULT_BIT),
                            FALSE);

  bpp = _cogl_pixel_format_get_bytes_per_pixel (src->format);

  if ((srcdata = _cogl_bitmap_map (src, COGL_BUFFER_ACCESS_READ, 0, error)))
    {
      if ((dstdata =
           _cogl_bitmap_map (dst, COGL_BUFFER_ACCESS_WRITE, 0, error)))
        {
          srcdata += src_y * src->rowstride + src_x * bpp;
          dstdata += dst_y * dst->rowstride + dst_x * bpp;

          for (line = 0; line < height; ++line)
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

CoglBool
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
                          uint8_t *data)
{
  CoglBitmap *bmp;

  g_return_val_if_fail (cogl_is_context (context), NULL);

  /* Rowstride from width if not given */
  if (rowstride == 0)
    rowstride = width * _cogl_pixel_format_get_bytes_per_pixel (format);

  bmp = g_slice_new (CoglBitmap);
  bmp->context = context;
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
                                     CoglPixelFormat format,
                                     CoglError **error)
{
  static CoglUserDataKey bitmap_free_key;
  int bpp = _cogl_pixel_format_get_bytes_per_pixel (format);
  int rowstride = ((width * bpp) + 3) & ~3;
  uint8_t *data = g_try_malloc (rowstride * height);
  CoglBitmap *bitmap;

  if (!data)
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_NO_MEMORY,
                       "Failed to allocate memory for bitmap");
      return NULL;
    }

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
cogl_bitmap_new_from_file (const char *filename,
                           CoglError **error)
{
  _COGL_GET_CONTEXT (ctx, NULL);

  _COGL_RETURN_VAL_IF_FAIL (filename != NULL, NULL);
  _COGL_RETURN_VAL_IF_FAIL (error == NULL || *error == NULL, NULL);

  return _cogl_bitmap_from_file (ctx, filename, error);
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
  _COGL_RETURN_VAL_IF_FAIL (format != COGL_PIXEL_FORMAT_ANY, NULL);

  /* for now we fallback to cogl_pixel_buffer_new, later, we could ask
   * libdrm a tiled buffer for instance */
  rowstride = width * _cogl_pixel_format_get_bytes_per_pixel (format);

  pixel_buffer =
    cogl_pixel_buffer_new (context,
                           height * rowstride,
                           NULL); /* data */

  _COGL_RETURN_VAL_IF_FAIL (pixel_buffer != NULL, NULL);

  bitmap = cogl_bitmap_new_from_buffer (COGL_BUFFER (pixel_buffer),
                                        format,
                                        width, height,
                                        rowstride,
                                        0 /* offset */);

  cogl_object_unref (pixel_buffer);

  return bitmap;
}

#ifdef COGL_HAS_ANDROID_SUPPORT
CoglBitmap *
cogl_android_bitmap_new_from_asset (CoglContext *ctx,
                                    AAssetManager *manager,
                                    const char *filename,
                                    CoglError **error)
{
  _COGL_RETURN_VAL_IF_FAIL (ctx != NULL, NULL);
  _COGL_RETURN_VAL_IF_FAIL (manager != NULL, NULL);
  _COGL_RETURN_VAL_IF_FAIL (filename != NULL, NULL);
  _COGL_RETURN_VAL_IF_FAIL (error == NULL || *error == NULL, NULL);

  return _cogl_android_bitmap_new_from_asset (ctx, manager, filename, error);
}
#endif

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

uint32_t
cogl_bitmap_error_quark (void)
{
  return g_quark_from_static_string ("cogl-bitmap-error-quark");
}

uint8_t *
_cogl_bitmap_map (CoglBitmap *bitmap,
                  CoglBufferAccess access,
                  CoglBufferMapHint hints,
                  CoglError **error)
{
  /* Divert to another bitmap if this data is shared */
  if (bitmap->shared_bmp)
    return _cogl_bitmap_map (bitmap->shared_bmp, access, hints, error);

  g_assert (!bitmap->mapped);

  if (bitmap->buffer)
    {
      uint8_t *data = _cogl_buffer_map (bitmap->buffer,
                                        access,
                                        hints,
                                        error);

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

uint8_t *
_cogl_bitmap_gl_bind (CoglBitmap *bitmap,
                      CoglBufferAccess access,
                      CoglBufferMapHint hints,
                      CoglError **error)
{
  uint8_t *ptr;
  CoglError *internal_error = NULL;

  g_return_val_if_fail (access & (COGL_BUFFER_ACCESS_READ |
                                  COGL_BUFFER_ACCESS_WRITE),
                        NULL);

  /* Divert to another bitmap if this data is shared */
  if (bitmap->shared_bmp)
    return _cogl_bitmap_gl_bind (bitmap->shared_bmp, access, hints, error);

  _COGL_RETURN_VAL_IF_FAIL (!bitmap->bound, NULL);

  /* If the bitmap wasn't created from a buffer then the
     implementation of bind is the same as map */
  if (bitmap->buffer == NULL)
    {
      uint8_t *data = _cogl_bitmap_map (bitmap, access, hints, error);
      if (data)
        bitmap->bound = TRUE;
      return data;
    }

  if (access == COGL_BUFFER_ACCESS_READ)
    ptr = _cogl_buffer_gl_bind (bitmap->buffer,
                                COGL_BUFFER_BIND_TARGET_PIXEL_UNPACK,
                                &internal_error);
  else if (access == COGL_BUFFER_ACCESS_WRITE)
    ptr = _cogl_buffer_gl_bind (bitmap->buffer,
                                COGL_BUFFER_BIND_TARGET_PIXEL_PACK,
                                &internal_error);
  else
    {
      ptr = NULL;
      g_assert_not_reached ();
      return NULL;
    }

  /* NB: _cogl_buffer_gl_bind() may return NULL in non-error
   * conditions so we have to explicitly check internal_error to see
   * if an exception was thrown */
  if (internal_error)
    {
      _cogl_propagate_error (error, internal_error);
      return NULL;
    }

  bitmap->bound = TRUE;

  /* The data pointer actually stores the offset */
  return ptr + GPOINTER_TO_INT (bitmap->data);
}

void
_cogl_bitmap_gl_unbind (CoglBitmap *bitmap)
{
  /* Divert to another bitmap if this data is shared */
  if (bitmap->shared_bmp)
    {
      _cogl_bitmap_gl_unbind (bitmap->shared_bmp);
      return;
    }

  g_assert (bitmap->bound);
  bitmap->bound = FALSE;

  /* If the bitmap wasn't created from a pixel array then the
     implementation of unbind is the same as unmap */
  if (bitmap->buffer)
    _cogl_buffer_gl_unbind (bitmap->buffer);
  else
    _cogl_bitmap_unmap (bitmap);
}

CoglContext *
_cogl_bitmap_get_context (CoglBitmap *bitmap)
{
  return bitmap->context;
}
