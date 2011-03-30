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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_BITMAP_H
#define __COGL_BITMAP_H

#include <glib.h>

#include "cogl-handle.h"
#include "cogl-buffer.h"
#include "cogl-bitmap.h"

/*
 * CoglBitmapDestroyNotify:
 * @data: The image data
 * @destroy_data: The callback closure data that was given to
 *   _cogl_bitmap_new_from_data().
 *
 * Function prototype that is used to destroy the bitmap data when
 * _cogl_bitmap_new_from_data() is called.
 */
typedef void (* CoglBitmapDestroyNotify) (guint8 *data, void *destroy_data);

/*
 * _cogl_bitmap_new_from_data:
 * @data: A pointer to the data. The bitmap will take ownership of this data.
 * @format: The format of the pixel data.
 * @width: The width of the bitmap.
 * @height: The height of the bitmap.
 * @rowstride: The rowstride of the bitmap (the number of bytes from
 *   the start of one row of the bitmap to the next).
 * @destroy_fn: A function to be called when the bitmap is
 *   destroyed. This should free @data. %NULL can be used instead if
 *   no free is needed.
 * @destroy_fn_data: This pointer will get passed to @destroy_fn.
 *
 * Creates a bitmap using some existing data. The data is not copied
 * so the bitmap will take ownership of the data pointer. When the
 * bitmap is freed @destroy_fn will be called to free the data.
 *
 * Return value: A new %CoglBitmap.
 */
CoglBitmap *
_cogl_bitmap_new_from_data (guint8                  *data,
                            CoglPixelFormat          format,
                            int                      width,
                            int                      height,
                            int                      rowstride,
                            CoglBitmapDestroyNotify  destroy_fn,
                            gpointer                 destroy_fn_data);

/* The idea of this function is that it will create a bitmap that
   shares the actual data with another bitmap. This is needed for the
   atlas texture backend because it needs upload a bitmap to a sub
   texture but override the format so that it ignores the premult
   flag. */
CoglBitmap *
_cogl_bitmap_new_shared (CoglBitmap      *shared_bmp,
                         CoglPixelFormat  format,
                         int              width,
                         int              height,
                         int              rowstride);

/* This creates a cogl bitmap that internally references a pixel
   array. The data is not copied. _cogl_bitmap_map will divert to
   mapping the pixel array */
CoglBitmap *
_cogl_bitmap_new_from_buffer (CoglBuffer      *buffer,
                              CoglPixelFormat  format,
                              int              width,
                              int              height,
                              int              rowstride,
                              int              offset);

gboolean
_cogl_bitmap_can_convert (CoglPixelFormat src, CoglPixelFormat dst);

gboolean
_cogl_bitmap_fallback_can_convert (CoglPixelFormat src, CoglPixelFormat dst);

gboolean
_cogl_bitmap_can_unpremult (CoglPixelFormat format);

gboolean
_cogl_bitmap_fallback_can_unpremult (CoglPixelFormat format);

gboolean
_cogl_bitmap_can_premult (CoglPixelFormat format);

gboolean
_cogl_bitmap_fallback_can_premult (CoglPixelFormat format);

CoglBitmap *
_cogl_bitmap_convert (CoglBitmap *bmp,
		      CoglPixelFormat   dst_format);
CoglBitmap *
_cogl_bitmap_fallback_convert (CoglBitmap *bmp,
			       CoglPixelFormat   dst_format);

gboolean
_cogl_bitmap_unpremult (CoglBitmap *dst_bmp);

gboolean
_cogl_bitmap_fallback_unpremult (CoglBitmap *dst_bmp);

gboolean
_cogl_bitmap_premult (CoglBitmap *dst_bmp);

gboolean
_cogl_bitmap_fallback_premult (CoglBitmap *dst_bmp);

CoglBitmap *
_cogl_bitmap_from_file (const char *filename,
			GError     **error);

CoglBitmap *
_cogl_bitmap_fallback_from_file (const char *filename);

gboolean
_cogl_bitmap_convert_premult_status (CoglBitmap      *bmp,
                                     CoglPixelFormat  dst_format);

CoglBitmap *
_cogl_bitmap_convert_format_and_premult (CoglBitmap *bmp,
                                         CoglPixelFormat   dst_format);

void
_cogl_bitmap_copy_subregion (CoglBitmap *src,
			     CoglBitmap *dst,
			     int         src_x,
			     int         src_y,
			     int         dst_x,
			     int         dst_y,
			     int         width,
			     int         height);

/* Creates a deep copy of the source bitmap */
CoglBitmap *
_cogl_bitmap_copy (CoglBitmap *src_bmp);

gboolean
_cogl_bitmap_get_size_from_file (const char *filename,
                                 int        *width,
                                 int        *height);

CoglPixelFormat
_cogl_bitmap_get_format (CoglBitmap *bitmap);

void
_cogl_bitmap_set_format (CoglBitmap *bitmap,
                         CoglPixelFormat format);

int
_cogl_bitmap_get_width (CoglBitmap *bitmap);

int
_cogl_bitmap_get_height (CoglBitmap *bitmap);

int
_cogl_bitmap_get_rowstride (CoglBitmap *bitmap);

/* Maps the bitmap so that the pixels can be accessed directly or if
   the bitmap is just a memory bitmap then it just returns the pointer
   to memory. Note that the bitmap isn't guaranteed to allocated to
   the full size of rowstride*height so it is not safe to read up to
   the rowstride of the last row. This will be the case if the user
   uploads data using gdk_pixbuf_new_subpixbuf with a sub region
   containing the last row of the pixbuf because in that case the
   rowstride can be much larger than the width of the image */
guint8 *
_cogl_bitmap_map (CoglBitmap *bitmap,
                  CoglBufferAccess access,
                  CoglBufferMapHint hints);

void
_cogl_bitmap_unmap (CoglBitmap *bitmap);

/* These two are replacements for map and unmap that should used when
   the pointer is going to be passed to GL for pixel packing or
   unpacking. The address might not be valid for reading if the bitmap
   was created with new_from_buffer but it will however be good to
   pass to glTexImage2D for example. The access should be READ for
   unpacking and WRITE for packing. It can not be both */
guint8 *
_cogl_bitmap_bind (CoglBitmap *bitmap,
                   CoglBufferAccess access,
                   CoglBufferMapHint hints);

void
_cogl_bitmap_unbind (CoglBitmap *bitmap);

#endif /* __COGL_BITMAP_H */
