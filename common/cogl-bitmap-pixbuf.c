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
#include "cogl-bitmap.h"

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
_cogl_bitmap_convert (const CoglBitmap *bmp,
		      CoglBitmap       *dst_bmp,
		      CoglPixelFormat   dst_format)
{
  return FALSE;
}

gboolean
_cogl_bitmap_unpremult (const CoglBitmap *bmp,
			CoglBitmap       *dst_bmp)
{
  return FALSE;
}

#ifdef USE_QUARTZ

/* lacking GdkPixbuf and other useful GError domains, define one of our own */

#define COGL_BITMAP_ERROR cogl_bitmap_error_quark ()

typedef enum {
  COGL_BITMAP_ERROR_FAILED,
  COGL_BITMAP_ERROR_UNKNOWN_TYPE,
  COGL_BITMAP_ERROR_CORRUPT_IMAGE
} CoglBitmapError;

GQuark
cogl_bitmap_error_quark (void)
{
  return g_quark_from_static_string ("cogl-bitmap-error-quark");
}

gboolean
_cogl_bitmap_get_size_from_file (const gchar *filename,
                                 gint        *width,
                                 gint        *height)
{
  if (width)
    *width = 0;

  if (height)
    *height = 0;

  return TRUE;
}

/* the error does not contain the filename as the caller already has it */
gboolean
_cogl_bitmap_from_file (CoglBitmap  *bmp,
			const gchar *filename,
			GError     **error)
{
  g_assert (bmp != NULL);
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
      return FALSE;
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
      return FALSE;
    }
  CFRelease (type);

  CGImageRef image = CGImageSourceCreateImageAtIndex (image_source, 0, NULL);
  CFRelease (image_source);

  size_t width = CGImageGetWidth (image);
  size_t height = CGImageGetHeight (image);
  if (width == 0 || height == 0)
    {
      /* incomplete or corrupt */
      CFRelease (image);
      g_set_error (error, COGL_BITMAP_ERROR, COGL_BITMAP_ERROR_CORRUPT_IMAGE,
                   "Image has zero width or height");
      return FALSE;
    }

  /* allocate buffer big enough to hold pixel data */
  size_t rowstride;
  CGBitmapInfo bitmap_info = CGImageGetBitmapInfo (image);
  if ((bitmap_info & kCGBitmapAlphaInfoMask) == kCGImageAlphaNone)
    {
      bitmap_info = kCGImageAlphaNone;
      rowstride = 3 * width;
    }
  else
    {
      bitmap_info = kCGImageAlphaPremultipliedFirst;
      rowstride = 4 * width;
    }
  guint8 *out_data = g_malloc0 (height * rowstride);

  /* render to buffer */
  CGColorSpaceRef color_space = CGColorSpaceCreateWithName (kCGColorSpaceGenericRGB);
  CGContextRef bitmap_context = CGBitmapContextCreate (out_data,
                                                       width, height, 8,
                                                       rowstride, color_space,
                                                       bitmap_info);
  CGColorSpaceRelease (color_space);

  const CGRect rect = {{0, 0}, {width, height}};
  CGContextDrawImage (bitmap_context, rect, image);

  CGImageRelease (image);
  CGContextRelease (bitmap_context);

  /* store bitmap info */
  bmp->data = out_data;
  bmp->format = bitmap_info == kCGImageAlphaPremultipliedFirst
                ? COGL_PIXEL_FORMAT_ARGB_8888
                : COGL_PIXEL_FORMAT_RGB_888;
  bmp->width = width;
  bmp->height = height;
  bmp->rowstride = rowstride;

  return TRUE;
}

#elif defined(USE_GDKPIXBUF)

gboolean
_cogl_bitmap_get_size_from_file (const gchar *filename,
                                 gint        *width,
                                 gint        *height)
{
  g_return_val_if_fail (filename != NULL, FALSE);

  if (gdk_pixbuf_get_file_info (filename, width, height) != NULL)
    return TRUE;

  return FALSE;
}

gboolean
_cogl_bitmap_from_file (CoglBitmap   *bmp,
			const gchar  *filename,
			GError      **error)
{
  GdkPixbuf        *pixbuf;
  gboolean          has_alpha;
  GdkColorspace     color_space;
  CoglPixelFormat   pixel_format;
  gint              width;
  gint              height;
  gint              rowstride;
  gint              bits_per_sample;
  gint              n_channels;
  gint              last_row_size;
  guchar           *pixels;
  guchar           *out_data;
  guchar           *out;
  gint              r;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (bmp == NULL)
    return FALSE;

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

  /* The docs say this is the right way */
  last_row_size = width * ((n_channels * bits_per_sample + 7) / 8);

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

  /* FIXME: Any way to destroy pixbuf but retain pixel data? */

  pixels   = gdk_pixbuf_get_pixels (pixbuf);
  out_data = (guchar*) g_malloc (height * rowstride);
  out      = out_data;

  /* Copy up to last row */
  for (r = 0; r < height-1; ++r)
    {
      memcpy (out, pixels, rowstride);
      pixels += rowstride;
      out += rowstride;
    }

  /* Copy last row */
  memcpy (out, pixels, last_row_size);

  /* Destroy GdkPixbuf object */
  g_object_unref (pixbuf);

  /* Store bitmap info */
  bmp->data = out_data; /* The stored data the same alignment constraints as a
                         * gdkpixbuf but stores a full rowstride in the last
                         * scanline
                         */
  bmp->format = pixel_format;
  bmp->width = width;
  bmp->height = height;
  bmp->rowstride = rowstride;

  return TRUE;
}

#else

#include "stb_image.c"

gboolean
_cogl_bitmap_get_size_from_file (const gchar *filename,
                                 gint        *width,
                                 gint        *height)
{
  if (width)
    *width = 0;

  if (height)
    *height = 0;

  return TRUE;
}

gboolean
_cogl_bitmap_from_file (CoglBitmap  *bmp,
			const gchar *filename,
			GError     **error)
{
  gint              stb_pixel_format;
  gint              width;
  gint              height;
  guchar           *pixels;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (bmp == NULL)
    return FALSE;

  /* Load from file using stb */
  pixels = stbi_load (filename,
                      &width, &height, &stb_pixel_format,
                      STBI_rgb_alpha);
  if (pixels == NULL)
    return FALSE;

  /* Store bitmap info */
  bmp->data = g_memdup (pixels, height * width * 4);
  bmp->format = COGL_PIXEL_FORMAT_RGBA_8888;
  bmp->width = width;
  bmp->height = height;
  bmp->rowstride = width * 4;

  free (pixels);

  return TRUE;
}
#endif
