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

/* TO rgba */

inline static void
_cogl_g_to_rgba (const guint8 *src, guint8 *dst)
{
  dst[0] = src[0];
  dst[1] = src[0];
  dst[2] = src[0];
  dst[3] = 255;
}

inline static void
_cogl_rgb_to_rgba (const guint8 *src, guint8 *dst)
{
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = 255;
}

inline static void
_cogl_bgr_to_rgba (const guint8 *src, guint8 *dst)
{
  dst[0] = src[2];
  dst[1] = src[1];
  dst[2] = src[0];
  dst[3] = 255;
}

inline static void
_cogl_bgra_to_rgba (const guint8 *src, guint8 *dst)
{
  dst[0] = src[2];
  dst[1] = src[1];
  dst[2] = src[0];
  dst[3] = src[3];
}

inline static void
_cogl_argb_to_rgba (const guint8 *src, guint8 *dst)
{
  dst[0] = src[1];
  dst[1] = src[2];
  dst[2] = src[3];
  dst[3] = src[0];
}

inline static void
_cogl_abgr_to_rgba (const guint8 *src, guint8 *dst)
{
  dst[0] = src[3];
  dst[1] = src[2];
  dst[2] = src[1];
  dst[3] = src[0];
}

inline static void
_cogl_rgba_to_rgba (const guint8 *src, guint8 *dst)
{
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

/* FROM rgba */

inline static void
_cogl_rgba_to_g (const guint8 *src, guint8 *dst)
{
  dst[0] = (src[0] + src[1] + src[2]) / 3;
}

inline static void
_cogl_rgba_to_rgb (const guint8 *src, guint8 *dst)
{
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
}

inline static void
_cogl_rgba_to_bgr (const guint8 *src, guint8 *dst)
{
  dst[0] = src[2];
  dst[1] = src[1];
  dst[2] = src[0];
}

inline static void
_cogl_rgba_to_bgra (const guint8 *src, guint8 *dst)
{
  dst[0] = src[2];
  dst[1] = src[1];
  dst[2] = src[0];
  dst[3] = src[3];
}

inline static void
_cogl_rgba_to_argb (const guint8 *src, guint8 *dst)
{
  dst[0] = src[3];
  dst[1] = src[0];
  dst[2] = src[1];
  dst[3] = src[2];
}

inline static void
_cogl_rgba_to_abgr (const guint8 *src, guint8 *dst)
{
  dst[0] = src[3];
  dst[1] = src[2];
  dst[2] = src[1];
  dst[3] = src[0];
}

/* (Un)Premultiplication */

inline static void
_cogl_unpremult_alpha_0 (guint8 *dst)
{
  dst[0] = 0;
  dst[1] = 0;
  dst[2] = 0;
  dst[3] = 0;
}

inline static void
_cogl_unpremult_alpha_last (guint8 *dst)
{
  guint8 alpha = dst[3];

  dst[0] = (dst[0] * 255) / alpha;
  dst[1] = (dst[1] * 255) / alpha;
  dst[2] = (dst[2] * 255) / alpha;
}

inline static void
_cogl_unpremult_alpha_first (guint8 *dst)
{
  guint8 alpha = dst[0];

  dst[1] = (dst[1] * 255) / alpha;
  dst[2] = (dst[2] * 255) / alpha;
  dst[3] = (dst[3] * 255) / alpha;
}

/* No division form of floor((c*a + 128)/255) (I first encountered
 * this in the RENDER implementation in the X server.) Being exact
 * is important for a == 255 - we want to get exactly c.
 */
#define MULT(d,a,t)                             \
  G_STMT_START {                                \
    t = d * a + 128;                            \
    d = ((t >> 8) + t) >> 8;                    \
  } G_STMT_END

inline static void
_cogl_premult_alpha_last (guint8 *dst)
{
  guint8 alpha = dst[3];
  /* Using a separate temporary per component has given slightly better
   * code generation with GCC in the past; it shouldn't do any worse in
   * any case.
   */
  unsigned int t1, t2, t3;
  MULT(dst[0], alpha, t1);
  MULT(dst[1], alpha, t2);
  MULT(dst[2], alpha, t3);
}

inline static void
_cogl_premult_alpha_first (guint8 *dst)
{
  guint8 alpha = dst[0];
  unsigned int t1, t2, t3;

  MULT(dst[1], alpha, t1);
  MULT(dst[2], alpha, t2);
  MULT(dst[3], alpha, t3);
}

#undef MULT

/* Use the SSE optimized version to premult four pixels at once when
   it is available. The same assembler code works for x86 and x86-64
   because it doesn't refer to any non-SSE registers directly */
#if defined(__SSE2__) && defined(__GNUC__) \
  && (defined(__x86_64) || defined(__i386))
#define COGL_USE_PREMULT_SSE2
#endif

#ifdef COGL_USE_PREMULT_SSE2

inline static void
_cogl_premult_alpha_last_four_pixels_sse2 (guint8 *p)
{
  /* 8 copies of 128 used below */
  static const gint16 eight_halves[8] __attribute__ ((aligned (16))) =
    { 128, 128, 128, 128, 128, 128, 128, 128 };
  /* Mask of the rgb components of the four pixels */
  static const gint8 just_rgb[16] __attribute__ ((aligned (16))) =
    { 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
      0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00 };
  /* Each SSE register only holds two pixels because we need to work
     with 16-bit intermediate values. We still do four pixels by
     interleaving two registers in the hope that it will pipeline
     better */
  asm (/* Load eight_halves into xmm5 for later */
       "movdqa (%1), %%xmm5\n"
       /* Clear xmm3 */
       "pxor %%xmm3, %%xmm3\n"
       /* Load two pixels from p into the low half of xmm0 */
       "movlps (%0), %%xmm0\n"
       /* Load the next set of two pixels from p into the low half of xmm1 */
       "movlps 8(%0), %%xmm1\n"
       /* Unpack 8 bytes from the low quad-words in each register to 8
          16-bit values */
       "punpcklbw %%xmm3, %%xmm0\n"
       "punpcklbw %%xmm3, %%xmm1\n"
       /* Copy alpha values of the first pixel in xmm0 to all
          components of the first pixel in xmm2 */
       "pshuflw $255, %%xmm0, %%xmm2\n"
       /* same for xmm1 and xmm3 */
       "pshuflw $255, %%xmm1, %%xmm3\n"
       /* The above also copies the second pixel directly so we now
          want to replace the RGB components with copies of the alpha
          components */
       "pshufhw $255, %%xmm2, %%xmm2\n"
       "pshufhw $255, %%xmm3, %%xmm3\n"
       /* Multiply the rgb components by the alpha */
       "pmullw %%xmm2, %%xmm0\n"
       "pmullw %%xmm3, %%xmm1\n"
       /* Add 128 to each component */
       "paddw %%xmm5, %%xmm0\n"
       "paddw %%xmm5, %%xmm1\n"
       /* Copy the results to temporary registers xmm4 and xmm5 */
       "movdqa %%xmm0, %%xmm4\n"
       "movdqa %%xmm1, %%xmm5\n"
       /* Divide the results by 256 */
       "psrlw $8, %%xmm0\n"
       "psrlw $8, %%xmm1\n"
       /* Add the temporaries back in */
       "paddw %%xmm4, %%xmm0\n"
       "paddw %%xmm5, %%xmm1\n"
       /* Divide again */
       "psrlw $8, %%xmm0\n"
       "psrlw $8, %%xmm1\n"
       /* Pack the results back as bytes */
       "packuswb %%xmm1, %%xmm0\n"
       /* Load just_rgb into xmm3 for later */
       "movdqa (%2), %%xmm3\n"
       /* Reload all four pixels into xmm2 */
       "movups (%0), %%xmm2\n"
       /* Mask out the alpha from the results */
       "andps %%xmm3, %%xmm0\n"
       /* Mask out the RGB from the original four pixels */
       "andnps %%xmm2, %%xmm3\n"
       /* Combine the two to get the right alpha values */
       "orps %%xmm3, %%xmm0\n"
       /* Write to memory */
       "movdqu %%xmm0, (%0)\n"
       : /* no outputs */
       : "r" (p), "r" (eight_halves), "r" (just_rgb)
       : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5");
}

#endif /* COGL_USE_PREMULT_SSE2 */

gboolean
_cogl_bitmap_fallback_can_convert (CoglPixelFormat src, CoglPixelFormat dst)
{
  if (src == dst)
    return FALSE;

  switch (src & COGL_UNORDERED_MASK)
    {
    case COGL_PIXEL_FORMAT_G_8:
    case COGL_PIXEL_FORMAT_24:
    case COGL_PIXEL_FORMAT_32:

      if ((dst & COGL_UNORDERED_MASK) != COGL_PIXEL_FORMAT_24 &&
	  (dst & COGL_UNORDERED_MASK) != COGL_PIXEL_FORMAT_32 &&
	  (dst & COGL_UNORDERED_MASK) != COGL_PIXEL_FORMAT_G_8)
	return FALSE;
      break;

    default:
      return FALSE;
    }

  return TRUE;
}

gboolean
_cogl_bitmap_fallback_can_unpremult (CoglPixelFormat format)
{
  return ((format & COGL_UNORDERED_MASK) == COGL_PIXEL_FORMAT_32);
}

gboolean
_cogl_bitmap_fallback_can_premult (CoglPixelFormat format)
{
  return ((format & COGL_UNORDERED_MASK) == COGL_PIXEL_FORMAT_32);
}

gboolean
_cogl_bitmap_fallback_convert (const CoglBitmap *bmp,
			       CoglBitmap       *dst_bmp,
			       CoglPixelFormat   dst_format)
{
  guint8  *src;
  guint8  *dst;
  int      src_bpp;
  int      dst_bpp;
  int      x,y;
  guint8   temp_rgba[4] = {0,0,0,0};

  /* Make sure conversion supported */
  if (!_cogl_bitmap_fallback_can_convert (bmp->format, dst_format))
    return FALSE;

  src_bpp = _cogl_get_format_bpp (bmp->format);
  dst_bpp = _cogl_get_format_bpp (dst_format);

  /* Initialize destination bitmap */
  *dst_bmp = *bmp;
  dst_bmp->rowstride = sizeof(guint8) * dst_bpp * dst_bmp->width;
  dst_bmp->format = ((bmp->format & COGL_PREMULT_BIT) |
		     (dst_format & COGL_UNPREMULT_MASK));

  /* Allocate a new buffer to hold converted data */
  dst_bmp->data = g_malloc (sizeof(guint8)
			    * dst_bmp->height
			    * dst_bmp->rowstride);

  /* FIXME: Optimize */
  for (y = 0; y < bmp->height; y++)
    {
      src = (guint8*)bmp->data      + y * bmp->rowstride;
      dst = (guint8*)dst_bmp->data  + y * dst_bmp->rowstride;

      for (x = 0; x < bmp->width; x++)
	{
	  /* FIXME: Would be nice to at least remove this inner
           * branching, but not sure it can be done without
           * rewriting of the whole loop */
	  switch (bmp->format & COGL_UNPREMULT_MASK)
	    {
	    case COGL_PIXEL_FORMAT_G_8:
	      _cogl_g_to_rgba (src, temp_rgba); break;
	    case COGL_PIXEL_FORMAT_RGB_888:
	      _cogl_rgb_to_rgba (src, temp_rgba); break;
	    case COGL_PIXEL_FORMAT_BGR_888:
	      _cogl_bgr_to_rgba (src, temp_rgba); break;
	    case COGL_PIXEL_FORMAT_RGBA_8888:
	      _cogl_rgba_to_rgba (src, temp_rgba); break;
	    case COGL_PIXEL_FORMAT_BGRA_8888:
	      _cogl_bgra_to_rgba (src, temp_rgba); break;
	    case COGL_PIXEL_FORMAT_ARGB_8888:
	      _cogl_argb_to_rgba (src, temp_rgba); break;
	    case COGL_PIXEL_FORMAT_ABGR_8888:
	      _cogl_abgr_to_rgba (src, temp_rgba); break;
	    default:
	      break;
	    }

	  switch (dst_format & COGL_UNPREMULT_MASK)
	    {
	    case COGL_PIXEL_FORMAT_G_8:
	      _cogl_rgba_to_g (temp_rgba, dst); break;
	    case COGL_PIXEL_FORMAT_RGB_888:
	      _cogl_rgba_to_rgb (temp_rgba, dst); break;
	    case COGL_PIXEL_FORMAT_BGR_888:
	      _cogl_rgba_to_bgr (temp_rgba, dst); break;
	    case COGL_PIXEL_FORMAT_RGBA_8888:
	      _cogl_rgba_to_rgba (temp_rgba, dst); break;
	    case COGL_PIXEL_FORMAT_BGRA_8888:
	      _cogl_rgba_to_bgra (temp_rgba, dst); break;
	    case COGL_PIXEL_FORMAT_ARGB_8888:
	      _cogl_rgba_to_argb (temp_rgba, dst); break;
	    case COGL_PIXEL_FORMAT_ABGR_8888:
	      _cogl_rgba_to_abgr (temp_rgba, dst); break;
	    default:
	      break;
	    }

	  src += src_bpp;
	  dst += dst_bpp;
	}
    }

  return TRUE;
}

gboolean
_cogl_bitmap_fallback_unpremult (CoglBitmap *bmp)
{
  guint8  *p;
  int      x,y;

  /* Make sure format supported for un-premultiplication */
  if (!_cogl_bitmap_fallback_can_unpremult (bmp->format))
    return FALSE;

  for (y = 0; y < bmp->height; y++)
    {
      p = (guint8*) bmp->data + y * bmp->rowstride;

      if (bmp->format & COGL_AFIRST_BIT)
        {
          for (x = 0; x < bmp->width; x++)
            {
              if (p[0] == 0)
                _cogl_unpremult_alpha_0 (p);
              else
                _cogl_unpremult_alpha_first (p);
              p += 4;
            }
        }
      else
        {
          for (x = 0; x < bmp->width; x++)
            {
              if (p[3] == 0)
                _cogl_unpremult_alpha_0 (p);
              else
                _cogl_unpremult_alpha_last (p);
              p += 4;
            }
        }
    }

  bmp->format &= ~COGL_PREMULT_BIT;

  return TRUE;
}

gboolean
_cogl_bitmap_fallback_premult (CoglBitmap *bmp)
{
  guint8  *p;
  int      x,y;

  /* Make sure format supported for un-premultiplication */
  if (!_cogl_bitmap_fallback_can_premult (bmp->format))
    return FALSE;

  for (y = 0; y < bmp->height; y++)
    {
      p = (guint8*) bmp->data + y * bmp->rowstride;

      if (bmp->format & COGL_AFIRST_BIT)
        {
          for (x = 0; x < bmp->width; x++)
            {
              _cogl_premult_alpha_first (p);
              p += 4;
            }
        }
      else
        {
          x = bmp->width;

#ifdef COGL_USE_PREMULT_SSE2

          /* Process 4 pixels at a time */
          while (x >= 4)
            {
              _cogl_premult_alpha_last_four_pixels_sse2 (p);
              p += 4 * 4;
              x -= 4;
            }

          /* If there are any pixels left we will fall through and
             handle them below */

#endif /* COGL_USE_PREMULT_SSE2 */

          while (x-- > 0)
            {
              _cogl_premult_alpha_last (p);
              p += 4;
            }
        }
    }

  bmp->format |= COGL_PREMULT_BIT;

  return TRUE;
}

gboolean
_cogl_bitmap_fallback_from_file (CoglBitmap  *bmp,
				 const char  *filename)
{
  /* FIXME: use jpeglib, libpng, etc. manually maybe */
  return FALSE;
}
