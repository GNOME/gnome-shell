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

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_BITMAP_H__
#define __COGL_BITMAP_H__

#include <cogl/cogl-types.h>

G_BEGIN_DECLS

typedef struct _CoglBitmap CoglBitmap;

/**
 * SECTION:cogl-bitmap
 * @short_description: Fuctions for loading images
 *
 * Cogl allows loading image data into memory as CoglBitmaps without
 * loading them immediately into GPU textures.
 *
 * #CoglBitmap is available since Cogl 1.0
 */


/**
 * cogl_bitmap_new_from_file:
 * @filename: the file to load.
 * @error: a #GError or %NULL.
 *
 * Loads an image file from disk. This function can be safely called from
 * within a thread.
 *
 * Return value: a #CoglBitmap to the new loaded image data, or
 *   %NULL if loading the image failed.
 *
 * Since: 1.0
 */
CoglBitmap *
cogl_bitmap_new_from_file (const char *filename,
                           GError **error);

/**
 * cogl_bitmap_get_size_from_file:
 * @filename: the file to check
 * @width: (out): return location for the bitmap width, or %NULL
 * @height: (out): return location for the bitmap height, or %NULL
 *
 * Parses an image file enough to extract the width and height
 * of the bitmap.
 *
 * Return value: %TRUE if the image was successfully parsed
 *
 * Since: 1.0
 */
gboolean
cogl_bitmap_get_size_from_file (const char *filename,
                                int *width,
                                int *height);

/**
 * cogl_is_bitmap:
 * @handle: a #CoglHandle for a bitmap
 *
 * Checks whether @handle is a #CoglHandle for a bitmap
 *
 * Return value: %TRUE if the passed handle represents a bitmap,
 *   and %FALSE otherwise
 *
 * Since: 1.0
 */
gboolean
cogl_is_bitmap (CoglHandle handle);

/**
 * COGL_BITMAP_ERROR:
 *
 * #GError domain for bitmap errors.
 *
 * Since: 1.4
 */
#define COGL_BITMAP_ERROR (cogl_bitmap_error_quark ())

/**
 * CoglBitmapError:
 * @COGL_BITMAP_ERROR_FAILED: Generic failure code, something went
 *   wrong.
 * @COGL_ERROR_UNKNOWN_TYPE: Unknown image type.
 * @COGL_ERROR_CORRUPT_IMAGE: An image file was broken somehow.
 *
 * Error codes that can be thrown when performing bitmap
 * operations. Note that gdk_pixbuf_new_from_file() can also throw
 * errors directly from the underlying image loading library. For
 * example, if GdkPixbuf is used then errors #GdkPixbufError<!-- -->s
 * will be used directly.
 *
 * Since: 1.4
 */
typedef enum {
  COGL_BITMAP_ERROR_FAILED,
  COGL_BITMAP_ERROR_UNKNOWN_TYPE,
  COGL_BITMAP_ERROR_CORRUPT_IMAGE
} CoglBitmapError;

GQuark cogl_bitmap_error_quark (void);

G_END_DECLS

#endif /* __COGL_BITMAP_H__ */
