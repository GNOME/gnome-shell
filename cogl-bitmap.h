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

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_BITMAP_H__
#define __COGL_BITMAP_H__

#include <cogl/cogl-types.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-bitmap
 * @short_description: Fuctions for loading images but not directly
 *                     into textures
 *
 * Cogl allows loading image data into memory as CoglBitmaps without
 * loading them immediately into GPU textures.
 */


/**
 * cogl_bitmap_new_from_file:
 * @filename: the file to load.
 * @error: a #GError or %NULL.
 *
 * Load an image file from disk. This function can be safely called from
 * within a thread.
 *
 * Returns: A CoglBitmap to the new loaded image data, or %NULL if loading
 * the image failed.
 *
 * Since: 1.0
 */
CoglHandle      cogl_bitmap_new_from_file     (const gchar    *filename,
                                               GError        **error);

/**
 * cogl_bitmap_get_size_from_file:
 * @filename: the file to check
 * @width: return location for the bitmap width
 * @height: return location for the bitmap height
 *
 * Parses an image file enough to extract the width and height
 * of the bitmap.
 *
 * Since: 1.0
 */
gboolean        cogl_bitmap_get_size_from_file (const gchar   *filename,
                                                gint          *width,
                                                gint          *height);

G_END_DECLS

#endif /* __COGL_BITMAP_H__ */
