/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-private.h"
#include "cogl-bitmap-private.h"
#include "cogl-context-private.h"
#include "cogl-texture-private.h"

#include <string.h>

#define component_type uint8_t
#define component_size 8
/* We want to specially optimise the packing when we are converting
   to/from an 8-bit type so that it won't do anything. That way for
   example if we are just doing a swizzle conversion then the inner
   loop for the conversion will be really simple */
#define UNPACK_BYTE(b) (b)
#define PACK_BYTE(b) (b)
#include "cogl-bitmap-packing.h"
#undef PACK_BYTE
#undef UNPACK_BYTE
#undef component_type
#undef component_size

#define component_type uint16_t
#define component_size 16
#define UNPACK_BYTE(b) (((b) * 65535 + 127) / 255)
#define PACK_BYTE(b) (((b) * 255 + 32767) / 65535)
#include "cogl-bitmap-packing.h"
#undef PACK_BYTE
#undef UNPACK_BYTE
#undef component_type
#undef component_size

/* (Un)Premultiplication */

inline static void
_cogl_unpremult_alpha_0 (uint8_t *dst)
{
  dst[0] = 0;
  dst[1] = 0;
  dst[2] = 0;
  dst[3] = 0;
}

inline static void
_cogl_unpremult_alpha_last (uint8_t *dst)
{
  uint8_t alpha = dst[3];

  dst[0] = (dst[0] * 255) / alpha;
  dst[1] = (dst[1] * 255) / alpha;
  dst[2] = (dst[2] * 255) / alpha;
}

inline static void
_cogl_unpremult_alpha_first (uint8_t *dst)
{
  uint8_t alpha = dst[0];

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
_cogl_premult_alpha_last (uint8_t *dst)
{
  uint8_t alpha = dst[3];
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
_cogl_premult_alpha_first (uint8_t *dst)
{
  uint8_t alpha = dst[0];
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
_cogl_premult_alpha_last_four_pixels_sse2 (uint8_t *p)
{
  /* 8 copies of 128 used below */
  static const int16_t eight_halves[8] __attribute__ ((aligned (16))) =
    { 128, 128, 128, 128, 128, 128, 128, 128 };
  /* Mask of the rgb components of the four pixels */
  static const int8_t just_rgb[16] __attribute__ ((aligned (16))) =
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

static void
_cogl_bitmap_premult_unpacked_span_8 (uint8_t *data,
                                      int width)
{
#ifdef COGL_USE_PREMULT_SSE2

  /* Process 4 pixels at a time */
  while (width >= 4)
    {
      _cogl_premult_alpha_last_four_pixels_sse2 (data);
      data += 4 * 4;
      width -= 4;
    }

  /* If there are any pixels left we will fall through and
     handle them below */

#endif /* COGL_USE_PREMULT_SSE2 */

  while (width-- > 0)
    {
      _cogl_premult_alpha_last (data);
      data += 4;
    }
}

static void
_cogl_bitmap_unpremult_unpacked_span_8 (uint8_t *data,
                                        int width)
{
  int x;

  for (x = 0; x < width; x++)
    {
      if (data[3] == 0)
        _cogl_unpremult_alpha_0 (data);
      else
        _cogl_unpremult_alpha_last (data);
      data += 4;
    }
}

static void
_cogl_bitmap_unpremult_unpacked_span_16 (uint16_t *data,
                                         int width)
{
  while (width-- > 0)
    {
      uint16_t alpha = data[3];

      if (alpha == 0)
        memset (data, 0, sizeof (uint16_t) * 3);
      else
        {
          data[0] = (data[0] * 65535) / alpha;
          data[1] = (data[1] * 65535) / alpha;
          data[2] = (data[2] * 65535) / alpha;
        }
    }
}

static void
_cogl_bitmap_premult_unpacked_span_16 (uint16_t *data,
                                       int width)
{
  while (width-- > 0)
    {
      uint16_t alpha = data[3];

      data[0] = (data[0] * alpha) / 65535;
      data[1] = (data[1] * alpha) / 65535;
      data[2] = (data[2] * alpha) / 65535;
    }
}

static CoglBool
_cogl_bitmap_can_fast_premult (CoglPixelFormat format)
{
  switch (format & ~COGL_PREMULT_BIT)
    {
    case COGL_PIXEL_FORMAT_RGBA_8888:
    case COGL_PIXEL_FORMAT_BGRA_8888:
    case COGL_PIXEL_FORMAT_ARGB_8888:
    case COGL_PIXEL_FORMAT_ABGR_8888:
      return TRUE;

    default:
      return FALSE;
    }
}

static CoglBool
_cogl_bitmap_needs_short_temp_buffer (CoglPixelFormat format)
{
  /* If the format is using more than 8 bits per component then we'll
     unpack into a 16-bit per component buffer instead of 8-bit so we
     won't lose as much precision. If we ever add support for formats
     with more than 16 bits for at least one of the components then we
     should probably do something else here, maybe convert to
     floats */
  switch (format)
    {
    case COGL_PIXEL_FORMAT_DEPTH_16:
    case COGL_PIXEL_FORMAT_DEPTH_32:
    case COGL_PIXEL_FORMAT_DEPTH_24_STENCIL_8:
    case COGL_PIXEL_FORMAT_ANY:
    case COGL_PIXEL_FORMAT_YUV:
      g_assert_not_reached ();

    case COGL_PIXEL_FORMAT_A_8:
    case COGL_PIXEL_FORMAT_RG_88:
    case COGL_PIXEL_FORMAT_RGB_565:
    case COGL_PIXEL_FORMAT_RGBA_4444:
    case COGL_PIXEL_FORMAT_RGBA_5551:
    case COGL_PIXEL_FORMAT_G_8:
    case COGL_PIXEL_FORMAT_RGB_888:
    case COGL_PIXEL_FORMAT_BGR_888:
    case COGL_PIXEL_FORMAT_RGBA_8888:
    case COGL_PIXEL_FORMAT_BGRA_8888:
    case COGL_PIXEL_FORMAT_ARGB_8888:
    case COGL_PIXEL_FORMAT_ABGR_8888:
    case COGL_PIXEL_FORMAT_RGBA_8888_PRE:
    case COGL_PIXEL_FORMAT_BGRA_8888_PRE:
    case COGL_PIXEL_FORMAT_ARGB_8888_PRE:
    case COGL_PIXEL_FORMAT_ABGR_8888_PRE:
    case COGL_PIXEL_FORMAT_RGBA_4444_PRE:
    case COGL_PIXEL_FORMAT_RGBA_5551_PRE:
      return FALSE;

    case COGL_PIXEL_FORMAT_RGBA_1010102:
    case COGL_PIXEL_FORMAT_BGRA_1010102:
    case COGL_PIXEL_FORMAT_ARGB_2101010:
    case COGL_PIXEL_FORMAT_ABGR_2101010:
    case COGL_PIXEL_FORMAT_RGBA_1010102_PRE:
    case COGL_PIXEL_FORMAT_BGRA_1010102_PRE:
    case COGL_PIXEL_FORMAT_ARGB_2101010_PRE:
    case COGL_PIXEL_FORMAT_ABGR_2101010_PRE:
      return TRUE;
    }

  g_assert_not_reached ();
}

CoglBool
_cogl_bitmap_convert_into_bitmap (CoglBitmap *src_bmp,
                                  CoglBitmap *dst_bmp,
                                  CoglError **error)
{
  uint8_t *src_data;
  uint8_t *dst_data;
  uint8_t *src;
  uint8_t *dst;
  void *tmp_row;
  int src_rowstride;
  int dst_rowstride;
  int y;
  int width, height;
  CoglPixelFormat src_format;
  CoglPixelFormat dst_format;
  CoglBool use_16;
  CoglBool need_premult;

  src_format = cogl_bitmap_get_format (src_bmp);
  src_rowstride = cogl_bitmap_get_rowstride (src_bmp);
  dst_format = cogl_bitmap_get_format (dst_bmp);
  dst_rowstride = cogl_bitmap_get_rowstride (dst_bmp);
  width = cogl_bitmap_get_width (src_bmp);
  height = cogl_bitmap_get_height (src_bmp);

  _COGL_RETURN_VAL_IF_FAIL (width == cogl_bitmap_get_width (dst_bmp), FALSE);
  _COGL_RETURN_VAL_IF_FAIL (height == cogl_bitmap_get_height (dst_bmp), FALSE);

  need_premult
    = ((src_format & COGL_PREMULT_BIT) != (dst_format & COGL_PREMULT_BIT) &&
       src_format != COGL_PIXEL_FORMAT_A_8 &&
       dst_format != COGL_PIXEL_FORMAT_A_8 &&
       (src_format & dst_format & COGL_A_BIT));

  /* If the base format is the same then we can just copy the bitmap
     instead */
  if ((src_format & ~COGL_PREMULT_BIT) == (dst_format & ~COGL_PREMULT_BIT) &&
      (!need_premult || _cogl_bitmap_can_fast_premult (dst_format)))
    {
      if (!_cogl_bitmap_copy_subregion (src_bmp, dst_bmp,
                                        0, 0, /* src_x / src_y */
                                        0, 0, /* dst_x / dst_y */
                                        width, height,
                                        error))
        return FALSE;

      if (need_premult)
        {
          if ((dst_format & COGL_PREMULT_BIT))
            {
              if (!_cogl_bitmap_premult (dst_bmp, error))
                return FALSE;
            }
          else
            {
              if (!_cogl_bitmap_unpremult (dst_bmp, error))
                return FALSE;
            }
        }

      return TRUE;
    }

  src_data = _cogl_bitmap_map (src_bmp, COGL_BUFFER_ACCESS_READ, 0, error);
  if (src_data == NULL)
    return FALSE;
  dst_data = _cogl_bitmap_map (dst_bmp,
                               COGL_BUFFER_ACCESS_WRITE,
                               COGL_BUFFER_MAP_HINT_DISCARD,
                               error);
  if (dst_data == NULL)
    {
      _cogl_bitmap_unmap (src_bmp);
      return FALSE;
    }

  use_16 = _cogl_bitmap_needs_short_temp_buffer (dst_format);

  /* Allocate a buffer to hold a temporary RGBA row */
  tmp_row = g_malloc (width *
                      (use_16 ? sizeof (uint16_t) : sizeof (uint8_t)) * 4);

  /* FIXME: Optimize */
  for (y = 0; y < height; y++)
    {
      src = src_data + y * src_rowstride;
      dst = dst_data + y * dst_rowstride;

      if (use_16)
        _cogl_unpack_16 (src_format, src, tmp_row, width);
      else
        _cogl_unpack_8 (src_format, src, tmp_row, width);

      /* Handle premultiplication */
      if (need_premult)
        {
          if (dst_format & COGL_PREMULT_BIT)
            {
              if (use_16)
                _cogl_bitmap_premult_unpacked_span_16 (tmp_row, width);
              else
                _cogl_bitmap_premult_unpacked_span_8 (tmp_row, width);
            }
          else
            {
              if (use_16)
                _cogl_bitmap_unpremult_unpacked_span_16 (tmp_row, width);
              else
                _cogl_bitmap_unpremult_unpacked_span_8 (tmp_row, width);
            }
        }

      if (use_16)
        _cogl_pack_16 (dst_format, tmp_row, dst, width);
      else
        _cogl_pack_8 (dst_format, tmp_row, dst, width);
    }

  _cogl_bitmap_unmap (src_bmp);
  _cogl_bitmap_unmap (dst_bmp);

  g_free (tmp_row);

  return TRUE;
}

CoglBitmap *
_cogl_bitmap_convert (CoglBitmap *src_bmp,
                      CoglPixelFormat dst_format,
                      CoglError **error)
{
  CoglBitmap *dst_bmp;
  int width, height;

  _COGL_GET_CONTEXT (ctx, NULL);

  width = cogl_bitmap_get_width (src_bmp);
  height = cogl_bitmap_get_height (src_bmp);

  dst_bmp = _cogl_bitmap_new_with_malloc_buffer (ctx,
                                                 width, height,
                                                 dst_format,
                                                 error);
  if (!dst_bmp)
    return NULL;

  if (!_cogl_bitmap_convert_into_bitmap (src_bmp, dst_bmp, error))
    {
      cogl_object_unref (dst_bmp);
      return NULL;
    }

  return dst_bmp;
}

static CoglBool
driver_can_convert (CoglContext *ctx,
                    CoglPixelFormat src_format,
                    CoglPixelFormat internal_format)
{
  if (!_cogl_has_private_feature (ctx, COGL_PRIVATE_FEATURE_FORMAT_CONVERSION))
    return FALSE;

  if (src_format == internal_format)
    return TRUE;

  /* If the driver doesn't natively support alpha textures then it
   * won't work correctly to convert to/from component-alpha
   * textures */
  if (!_cogl_has_private_feature (ctx, COGL_PRIVATE_FEATURE_ALPHA_TEXTURES) &&
      (src_format == COGL_PIXEL_FORMAT_A_8 ||
       internal_format == COGL_PIXEL_FORMAT_A_8))
    return FALSE;

  /* Same for red-green textures. If red-green textures aren't
   * supported then the internal format should never be RG_88 but we
   * should still be able to convert from an RG source image */
  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_RG) &&
      src_format == COGL_PIXEL_FORMAT_RG_88)
    return FALSE;

  return TRUE;
}

CoglBitmap *
_cogl_bitmap_convert_for_upload (CoglBitmap *src_bmp,
                                 CoglPixelFormat internal_format,
                                 CoglBool can_convert_in_place,
                                 CoglError **error)
{
  CoglContext *ctx = _cogl_bitmap_get_context (src_bmp);
  CoglPixelFormat src_format = cogl_bitmap_get_format (src_bmp);
  CoglBitmap *dst_bmp;

  _COGL_RETURN_VAL_IF_FAIL (internal_format != COGL_PIXEL_FORMAT_ANY, NULL);

  /* OpenGL supports specifying a different format for the internal
     format when uploading texture data. We should use this to convert
     formats because it is likely to be faster and support more types
     than the Cogl bitmap code. However under GLES the internal format
     must be the same as the bitmap format and it only supports a
     limited number of formats so we must convert using the Cogl
     bitmap code instead */

  if (driver_can_convert (ctx, src_format, internal_format))
    {
      /* If the source format does not have the same premult flag as the
         internal_format then we need to copy and convert it */
      if (_cogl_texture_needs_premult_conversion (src_format,
                                                  internal_format))
        {
          if (can_convert_in_place)
            {
              if (_cogl_bitmap_convert_premult_status (src_bmp,
                                                       (src_format ^
                                                        COGL_PREMULT_BIT),
                                                       error))
                {
                  dst_bmp = cogl_object_ref (src_bmp);
                }
              else
                return NULL;
            }
          else
            {
              dst_bmp = _cogl_bitmap_convert (src_bmp,
                                              src_format ^ COGL_PREMULT_BIT,
                                              error);
              if (dst_bmp == NULL)
                return NULL;
            }
        }
      else
        dst_bmp = cogl_object_ref (src_bmp);
    }
  else
    {
      CoglPixelFormat closest_format;

      closest_format =
        ctx->driver_vtable->pixel_format_to_gl (ctx,
                                                internal_format,
                                                NULL, /* ignore gl intformat */
                                                NULL, /* ignore gl format */
                                                NULL); /* ignore gl type */

      if (closest_format != src_format)
        dst_bmp = _cogl_bitmap_convert (src_bmp, closest_format, error);
      else
        dst_bmp = cogl_object_ref (src_bmp);
    }

  return dst_bmp;
}

CoglBool
_cogl_bitmap_unpremult (CoglBitmap *bmp,
                        CoglError **error)
{
  uint8_t *p, *data;
  uint16_t *tmp_row;
  int x,y;
  CoglPixelFormat format;
  int width, height;
  int rowstride;

  format = cogl_bitmap_get_format (bmp);
  width = cogl_bitmap_get_width (bmp);
  height = cogl_bitmap_get_height (bmp);
  rowstride = cogl_bitmap_get_rowstride (bmp);

  if ((data = _cogl_bitmap_map (bmp,
                                COGL_BUFFER_ACCESS_READ |
                                COGL_BUFFER_ACCESS_WRITE,
                                0,
                                error)) == NULL)
    return FALSE;

  /* If we can't directly unpremult the data inline then we'll
     allocate a temporary row and unpack the data. This assumes if we
      can fast premult then we can also fast unpremult */
  if (_cogl_bitmap_can_fast_premult (format))
    tmp_row = NULL;
  else
    tmp_row = g_malloc (sizeof (uint16_t) * 4 * width);

  for (y = 0; y < height; y++)
    {
      p = (uint8_t*) data + y * rowstride;

      if (tmp_row)
        {
          _cogl_unpack_16 (format, p, tmp_row, width);
          _cogl_bitmap_unpremult_unpacked_span_16 (tmp_row, width);
          _cogl_pack_16 (format, tmp_row, p, width);
        }
      else
        {
          if (format & COGL_AFIRST_BIT)
            {
              for (x = 0; x < width; x++)
                {
                  if (p[0] == 0)
                    _cogl_unpremult_alpha_0 (p);
                  else
                    _cogl_unpremult_alpha_first (p);
                  p += 4;
                }
            }
          else
            _cogl_bitmap_unpremult_unpacked_span_8 (p, width);
        }
    }

  g_free (tmp_row);

  _cogl_bitmap_unmap (bmp);

  _cogl_bitmap_set_format (bmp, format & ~COGL_PREMULT_BIT);

  return TRUE;
}

CoglBool
_cogl_bitmap_premult (CoglBitmap *bmp,
                      CoglError **error)
{
  uint8_t *p, *data;
  uint16_t *tmp_row;
  int x,y;
  CoglPixelFormat format;
  int width, height;
  int rowstride;

  format = cogl_bitmap_get_format (bmp);
  width = cogl_bitmap_get_width (bmp);
  height = cogl_bitmap_get_height (bmp);
  rowstride = cogl_bitmap_get_rowstride (bmp);

  if ((data = _cogl_bitmap_map (bmp,
                                COGL_BUFFER_ACCESS_READ |
                                COGL_BUFFER_ACCESS_WRITE,
                                0,
                                error)) == NULL)
    return FALSE;

  /* If we can't directly premult the data inline then we'll allocate
     a temporary row and unpack the data. */
  if (_cogl_bitmap_can_fast_premult (format))
    tmp_row = NULL;
  else
    tmp_row = g_malloc (sizeof (uint16_t) * 4 * width);

  for (y = 0; y < height; y++)
    {
      p = (uint8_t*) data + y * rowstride;

      if (tmp_row)
        {
          _cogl_unpack_16 (format, p, tmp_row, width);
          _cogl_bitmap_premult_unpacked_span_16 (tmp_row, width);
          _cogl_pack_16 (format, tmp_row, p, width);
        }
      else
        {
          if (format & COGL_AFIRST_BIT)
            {
              for (x = 0; x < width; x++)
                {
                  _cogl_premult_alpha_first (p);
                  p += 4;
                }
            }
          else
            _cogl_bitmap_premult_unpacked_span_8 (p, width);
        }
    }

  g_free (tmp_row);

  _cogl_bitmap_unmap (bmp);

  _cogl_bitmap_set_format (bmp, format | COGL_PREMULT_BIT);

  return TRUE;
}
