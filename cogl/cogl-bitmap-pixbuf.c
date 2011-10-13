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
#include "cogl-util.h"
#include "cogl-internal.h"
#include "cogl-bitmap-private.h"

#include <string.h>

#ifdef USE_QUARTZ
#include <ApplicationServices/ApplicationServices.h>
#elif defined(USE_GDKPIXBUF)
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

gboolean
_cogl_bitmap_can_convert (CoglPixelFormat src, CoglPixelFormat dst)
{
  return FALSE;
}

gboolean
_cogl_bitmap_can_unpremult (CoglPixelFormat format)
{
  return FALSE;
}

gboolean
_cogl_bitmap_can_premult (CoglPixelFormat format)
{
  return FALSE;
}

CoglBitmap *
_cogl_bitmap_convert (CoglBitmap *bmp,
		      CoglPixelFormat   dst_format)
{
  return NULL;
}

gboolean
_cogl_bitmap_unpremult (CoglBitmap *dst_bmp)
{
  return FALSE;
}

gboolean
_cogl_bitmap_premult (CoglBitmap *dst_bmp)
{
  return FALSE;
}

#ifdef USE_QUARTZ

gboolean
_cogl_bitmap_get_size_from_file (const char *filename,
                                 int        *width,
                                 int        *height)
{
  if (width)
    *width = 0;

  if (height)
    *height = 0;

  return TRUE;
}

/* the error does not contain the filename as the caller already has it */
CoglBitmap *
_cogl_bitmap_from_file (const char  *filename,
			GError     **error)
{
  CFURLRef url;
  CGImageSourceRef image_source;
  CGImageRef image;
  int save_errno;
  CFStringRef type;
  gsize width, height, rowstride;
  guint8 *out_data;
  CGColorSpaceRef color_space;
  CGContextRef bitmap_context;

  g_assert (filename != NULL);
  g_assert (error == NULL || *error == NULL);

  url = CFURLCreateFromFileSystemRepresentation (NULL,
                                                 (guchar *) filename,
                                                 strlen (filename),
                                                 false);
  image_source = CGImageSourceCreateWithURL (url, NULL);
  save_errno = errno;
  CFRelease (url);

  if (image_source == NULL)
    {
      /* doesn't exist, not readable, etc. */
      g_set_error_literal (error,
                           COGL_BITMAP_ERROR,
                           COGL_BITMAP_ERROR_FAILED,
                           g_strerror (save_errno));
      return NULL;
    }

  /* Unknown images would be cleanly caught as zero width/height below, but try
   * to provide better error message
   */
  type = CGImageSourceGetType (image_source);
  if (type == NULL)
    {
      CFRelease (image_source);
      g_set_error_literal (error,
                           COGL_BITMAP_ERROR,
                           COGL_BITMAP_ERROR_UNKNOWN_TYPE,
                           "Unknown image type");
      return NULL;
    }

  CFRelease (type);

  image = CGImageSourceCreateImageAtIndex (image_source, 0, NULL);
  CFRelease (image_source);

  width = CGImageGetWidth (image);
  height = CGImageGetHeight (image);
  if (width == 0 || height == 0)
    {
      /* incomplete or corrupt */
      CFRelease (image);
      g_set_error_literal (error,
                           COGL_BITMAP_ERROR,
                           COGL_BITMAP_ERROR_CORRUPT_IMAGE,
                           "Image has zero width or height");
      return NULL;
    }

  /* allocate buffer big enough to hold pixel data */
  rowstride = 4 * width;
  out_data = g_malloc0 (height * rowstride);

  /* render to buffer */
  color_space = CGColorSpaceCreateWithName (kCGColorSpaceGenericRGB);
  bitmap_context = CGBitmapContextCreate (out_data,
                                          width, height, 8,
                                          rowstride, color_space,
                                          kCGImageAlphaPremultipliedFirst);
  CGColorSpaceRelease (color_space);

  {
    const CGRect rect = {{0, 0}, {width, height}};

    CGContextDrawImage (bitmap_context, rect, image);
  }

  CGImageRelease (image);
  CGContextRelease (bitmap_context);

  /* store bitmap info */
  return _cogl_bitmap_new_from_data (out_data,
                                     COGL_PIXEL_FORMAT_ARGB_8888,
                                     width, height,
                                     rowstride,
                                     (CoglBitmapDestroyNotify) g_free,
                                     NULL);
}

#elif defined(USE_GDKPIXBUF)

gboolean
_cogl_bitmap_get_size_from_file (const char *filename,
                                 int        *width,
                                 int        *height)
{
  _COGL_RETURN_VAL_IF_FAIL (filename != NULL, FALSE);

  if (gdk_pixbuf_get_file_info (filename, width, height) != NULL)
    return TRUE;

  return FALSE;
}

static void
_cogl_bitmap_unref_pixbuf (guint8 *pixels,
                           void *pixbuf)
{
  g_object_unref (pixbuf);
}

CoglBitmap *
_cogl_bitmap_from_file (const char   *filename,
			GError      **error)
{
  GdkPixbuf        *pixbuf;
  gboolean          has_alpha;
  GdkColorspace     color_space;
  CoglPixelFormat   pixel_format;
  int               width;
  int               height;
  int               rowstride;
  int               bits_per_sample;
  int               n_channels;

  _COGL_RETURN_VAL_IF_FAIL (error == NULL || *error == NULL, FALSE);

  /* Load from file using GdkPixbuf */
  pixbuf = gdk_pixbuf_new_from_file (filename, error);
  if (pixbuf == NULL)
    return FALSE;

  /* Get pixbuf properties */
  has_alpha       = gdk_pixbuf_get_has_alpha (pixbuf);
  color_space     = gdk_pixbuf_get_colorspace (pixbuf);
  width           = gdk_pixbuf_get_width (pixbuf);
  height          = gdk_pixbuf_get_height (pixbuf);
  rowstride       = gdk_pixbuf_get_rowstride (pixbuf);
  bits_per_sample = gdk_pixbuf_get_bits_per_sample (pixbuf);
  n_channels      = gdk_pixbuf_get_n_channels (pixbuf);

  /* According to current docs this should be true and so
   * the translation to cogl pixel format below valid */
  g_assert (bits_per_sample == 8);

  if (has_alpha)
    g_assert (n_channels == 4);
  else
    g_assert (n_channels == 3);

  /* Translate to cogl pixel format */
  switch (color_space)
    {
    case GDK_COLORSPACE_RGB:
      /* The only format supported by GdkPixbuf so far */
      pixel_format = has_alpha ?
	COGL_PIXEL_FORMAT_RGBA_8888 :
	COGL_PIXEL_FORMAT_RGB_888;
      break;

    default:
      /* Ouch, spec changed! */
      g_object_unref (pixbuf);
      return FALSE;
    }

  /* We just use the data directly from the pixbuf so that we don't
     have to copy to a seperate buffer. Note that Cogl is expected not
     to read past the end of bpp*width on the last row even if the
     rowstride is much larger so we don't need to worry about
     GdkPixbuf's semantics that it may under-allocate the buffer. */
  return _cogl_bitmap_new_from_data (gdk_pixbuf_get_pixels (pixbuf),
                                     pixel_format,
                                     width,
                                     height,
                                     rowstride,
                                     _cogl_bitmap_unref_pixbuf,
                                     pixbuf);
}

#else

#include "stb_image.c"

gboolean
_cogl_bitmap_get_size_from_file (const char *filename,
                                 int        *width,
                                 int        *height)
{
  if (width)
    *width = 0;

  if (height)
    *height = 0;

  return TRUE;
}

CoglBitmap *
_cogl_bitmap_from_file (const char  *filename,
			GError     **error)
{
  CoglBitmap *bmp;
  int      stb_pixel_format;
  int      width;
  int      height;
  guint8  *pixels;

  _COGL_RETURN_VAL_IF_FAIL (error == NULL || *error == NULL, FALSE);

  /* Load from file using stb */
  pixels = stbi_load (filename,
                      &width, &height, &stb_pixel_format,
                      STBI_rgb_alpha);
  if (pixels == NULL)
    return FALSE;

  /* Store bitmap info */
  bmp = _cogl_bitmap_new_from_data (g_memdup (pixels, height * width * 4),
                                    COGL_PIXEL_FORMAT_RGBA_8888,
                                    width, height,
                                    width * 4,
                                    (CoglBitmapDestroyNotify) g_free,
                                    NULL);

  free (pixels);

  return bmp;
}
#endif
