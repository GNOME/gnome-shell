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

#ifndef __COGL_BITMAP_H
#define __COGL_BITMAP_H

#include <glib.h>

#include "cogl-handle.h"

typedef struct _CoglBitmap
{
  CoglHandleObject _parent;
  guchar          *data;
  CoglPixelFormat  format;
  gint             width;
  gint             height;
  gint             rowstride;
} CoglBitmap;

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

gboolean
_cogl_bitmap_convert (const CoglBitmap *bmp,
		      CoglBitmap       *dst_bmp,
		      CoglPixelFormat   dst_format);
gboolean
_cogl_bitmap_fallback_convert (const CoglBitmap *bmp,
			       CoglBitmap       *dst_bmp,
			       CoglPixelFormat   dst_format);

gboolean
_cogl_bitmap_unpremult (CoglBitmap *dst_bmp);

gboolean
_cogl_bitmap_fallback_unpremult (CoglBitmap *dst_bmp);

gboolean
_cogl_bitmap_premult (CoglBitmap *dst_bmp);

gboolean
_cogl_bitmap_fallback_premult (CoglBitmap *dst_bmp);

gboolean
_cogl_bitmap_from_file (CoglBitmap  *bmp,
			const gchar *filename,
			GError     **error);

gboolean
_cogl_bitmap_fallback_from_file (CoglBitmap  *bmp,
				 const gchar *filename);

gboolean
_cogl_bitmap_convert_premult_status (CoglBitmap      *bmp,
                                     CoglPixelFormat  dst_format);

gboolean
_cogl_bitmap_convert_format_and_premult (const CoglBitmap *bmp,
                                         CoglBitmap       *dst_bmp,
                                         CoglPixelFormat   dst_format);

void
_cogl_bitmap_copy_subregion (CoglBitmap *src,
			     CoglBitmap *dst,
			     gint        src_x,
			     gint        src_y,
			     gint        dst_x,
			     gint        dst_y,
			     gint        width,
			     gint        height);

gboolean
_cogl_bitmap_get_size_from_file (const gchar *filename,
                                 gint        *width,
                                 gint        *height);

#endif /* __COGL_BITMAP_H */
