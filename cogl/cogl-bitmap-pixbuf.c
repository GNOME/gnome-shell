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
  g_assert (filename != NULL);
  g_assert (error == NULL || *error == NULL);

  CFURLRef url = CFURLCreateFromFileSystemRepresentation (NULL, (guchar*)filename, strlen(filename), false);
  CGImageSourceRef image_source = CGImageSourceCreateWithURL (url, NULL);
  int save_errno = errno;
  CFRelease (url);
  if (image_source == NULL)
    {
      /* doesn't exist, not readable, etc. */
      g_set_error (error, COGL_BITMAP_ERROR, COGL_BITMAP_ERROR_FAILED,
                   "%s", g_strerror (save_errno));
      return NULL;
    }

  /* Unknown images would be cleanly caught as zero width/height below, but try
   * to provide better error message
   */
  CFStringRef type = CGImageSourceGetType (image_source);
  if (type == NULL)
    {
      CFRelease (image_source);
      g_set_error (error, COGL_BITMAP_ERROR, COGL_BITMAP_ERROR_UNKNOWN_TYPE,
                   "Unknown image type");
      return NULL;
    }
  CFRelease (type);

  CGImageRef image = CGImageSourceCreateImageAtIndex (image_source, 0, NULL);
  CFRelease (image_source);

  gsize width = CGImageGetWidth (image);
  gsize height = CGImageGetHeight (image);
  if (width == 0 || height == 0)
    {
      /* incomplete or corrupt */
      CFRelease (image);
      g_set_error (error, COGL_BITMAP_ERROR, COGL_BITMAP_ERROR_CORRUPT_IMAGE,
                   "Image has zero width or height");
      return NULL;
    }

  /* allocate buffer big enough to hold pixel data */
  gsize rowstride;
  rowstride = 4 * width;
  guint8 *out_data = g_malloc0 (height * rowstride);

  /* render to buffer */
  CGColorSpaceRef color_space = CGColorSpaceCreateWithName (kCGColorSpaceGenericRGB);
  CGContextRef bitmap_context = CGBitmapContextCreate (out_data,
                                                       width, height, 8,
                                                       rowstride, color_space,
                                                       kCGImageAlphaPremultipliedFirst);
  CGColorSpaceRelease (color_space);

  const CGRect rect = {{0, 0}, {width, height}};
  CGContextDrawImage (bitmap_context, rect, image);

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
  g_return_val_if_fail (filename != NULL, FALSE);

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
  int               aligned_rowstride;
  int               bits_per_sample;
  int               n_channels;
  guint8           *pixels;
  guint8           *out_data;
  guint8           *out;
  int               r;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

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

  /* Work out what the rowstride would be if it was packing the data
     but aligned to 4 bytes */
  aligned_rowstride = (width * n_channels + 3) & ~3;

  /* The documentation for GdkPixbuf states that is not safe to read
     all of the data as height*rowstride because the buffer might not
     be allocated to include the full length of the rowstride for the
     last row so arguably we should always copy the buffer when
     rowstride != width*bpp because some places in Cogl assume that it
     can memcpy(height*rowstride). However that rule is probably only
     in place so that GdkPixbuf can implement gdk_pixbuf_new_subpixbuf
     by just sharing the data and setting a large rowstride. That does
     not apply in this case because we are just creating a new buffer
     for a file. It seems very unlikely that GdkPixbuf would not
     allocate the full rowstride in this case and it is highly
     desirable to avoid copying the buffer. This instead just assumes
     that whatever buffer pixbuf points into will always be allocated
     to a 4-byte aligned buffer so we can avoid copying unless the
     rowstride is unusually large */
  if (rowstride <= aligned_rowstride)
    return _cogl_bitmap_new_from_data (gdk_pixbuf_get_pixels (pixbuf),
                                       pixel_format,
                                       width,
                                       height,
                                       rowstride,
                                       _cogl_bitmap_unref_pixbuf,
                                       pixbuf);

  pixels   = gdk_pixbuf_get_pixels (pixbuf);
  out_data = g_malloc (aligned_rowstride * height);
  out      = out_data;

  for (r = 0; r < height; ++r)
    {
      memcpy (out, pixels, n_channels * width);
      pixels += rowstride;
      out += aligned_rowstride;
    }

  /* Destroy GdkPixbuf object */
  g_object_unref (pixbuf);

  return _cogl_bitmap_new_from_data (out_data,
                                     pixel_format,
                                     width,
                                     height,
                                     aligned_rowstride,
                                     (CoglBitmapDestroyNotify) g_free,
                                     NULL);
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

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

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
