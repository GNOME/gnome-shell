/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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
 *
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-sub-texture-private.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-texture-driver.h"

#include <string.h>
#include <math.h>

static void _cogl_sub_texture_free (CoglSubTexture *sub_tex);

COGL_HANDLE_DEFINE (SubTexture, sub_texture);

static const CoglTextureVtable cogl_sub_texture_vtable;

/* Maps from the texture coordinates of this texture to the texture
   coordinates of the full texture */

static void
_cogl_sub_texture_map_coordinate_pair (CoglSubTexture *sub_tex,
                                       gfloat *tx, gfloat *ty)
{
  *tx = *tx * (sub_tex->tx2 - sub_tex->tx1) + sub_tex->tx1;
  *ty = *ty * (sub_tex->ty2 - sub_tex->ty1) + sub_tex->ty1;
}

static void
_cogl_sub_texture_map_coordinate_set (CoglSubTexture *sub_tex,
                                      gfloat *tx1, gfloat *ty1,
                                      gfloat *tx2, gfloat *ty2)
{
  _cogl_sub_texture_map_coordinate_pair (sub_tex, tx1, ty1);
  _cogl_sub_texture_map_coordinate_pair (sub_tex, tx2, ty2);
}

/* Maps from the texture coordinates of the full texture to the
   texture coordinates of the sub texture */
static void
_cogl_sub_texture_unmap_coordinate_pair (CoglSubTexture *sub_tex,
                                         gfloat *coords)
{
  if (sub_tex->tx1 == sub_tex->tx2)
    coords[0] = sub_tex->tx1;
  else
    coords[0] = (coords[0] - sub_tex->tx1) / (sub_tex->tx2 - sub_tex->tx1);

  if (sub_tex->ty1 == sub_tex->ty2)
    coords[0] = sub_tex->ty1;
  else
    coords[1] = (coords[1] - sub_tex->ty1) / (sub_tex->ty2 - sub_tex->ty1);
}

static void
_cogl_sub_texture_unmap_coordinate_set (CoglSubTexture *sub_tex,
                                        gfloat *coords)
{
  _cogl_sub_texture_unmap_coordinate_pair (sub_tex, coords);
  _cogl_sub_texture_unmap_coordinate_pair (sub_tex, coords + 2);
}

static gboolean
_cogl_sub_texture_same_int_part (float t1, float t2)
{
  float int_part1, int_part2;
  float frac_part1, frac_part2;

  frac_part1 = modff (t1, &int_part1);
  frac_part2 = modff (t2, &int_part2);

  return (int_part1 == int_part2 ||
          ((frac_part1 == 0.0f || frac_part2 == 0.0f) &&
           ABS (int_part1 - int_part2) == 1.0f));
}

typedef struct _CoglSubTextureForeachData
{
  CoglSubTexture *sub_tex;
  CoglTextureSliceCallback callback;
  void *user_data;
} CoglSubTextureForeachData;

static void
_cogl_sub_texture_foreach_cb (CoglHandle handle,
                              GLuint gl_handle,
                              GLenum gl_target,
                              const float *slice_coords,
                              const float *full_virtual_coords,
                              void *user_data)
{
  CoglSubTextureForeachData *data = user_data;
  float virtual_coords[4];

  memcpy (virtual_coords, full_virtual_coords, sizeof (virtual_coords));
  /* Convert the virtual coords from the full-texture space to the sub
     texture space */
  _cogl_sub_texture_unmap_coordinate_set (data->sub_tex, virtual_coords);

  data->callback (handle, gl_handle, gl_target,
                  slice_coords, virtual_coords,
                  data->user_data);
}

static void
_cogl_sub_texture_manual_repeat_cb (const float *coords,
                                    void *user_data)
{
  CoglSubTextureForeachData *data = user_data;
  float mapped_coords[4];

  memcpy (mapped_coords, coords, sizeof (mapped_coords));

  _cogl_sub_texture_map_coordinate_set (data->sub_tex,
                                        &mapped_coords[0],
                                        &mapped_coords[1],
                                        &mapped_coords[2],
                                        &mapped_coords[3]);

  _cogl_texture_foreach_sub_texture_in_region (data->sub_tex->full_texture,
                                               mapped_coords[0],
                                               mapped_coords[1],
                                               mapped_coords[2],
                                               mapped_coords[3],
                                               _cogl_sub_texture_foreach_cb,
                                               user_data);
}

static void
_cogl_sub_texture_foreach_sub_texture_in_region (
                                       CoglTexture *tex,
                                       float virtual_tx_1,
                                       float virtual_ty_1,
                                       float virtual_tx_2,
                                       float virtual_ty_2,
                                       CoglTextureSliceCallback callback,
                                       void *user_data)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);
  CoglSubTextureForeachData data;

  data.sub_tex = sub_tex;
  data.callback = callback;
  data.user_data = user_data;

  /* If there is no repeating or the sub texture coordinates are a
     multiple of the whole texture then we can just directly map the
     texture coordinates */
  if (sub_tex->tex_coords_are_a_multiple ||
      (_cogl_sub_texture_same_int_part (virtual_tx_1, virtual_tx_2) &&
       _cogl_sub_texture_same_int_part (virtual_ty_1, virtual_ty_2)))
    {
      _cogl_sub_texture_map_coordinate_set (sub_tex,
                                            &virtual_tx_1,
                                            &virtual_ty_1,
                                            &virtual_tx_2,
                                            &virtual_ty_2);

      _cogl_texture_foreach_sub_texture_in_region
        (sub_tex->full_texture,
         virtual_tx_1, virtual_ty_1,
         virtual_tx_2, virtual_ty_2,
         _cogl_sub_texture_foreach_cb, &data);
    }
  else
    _cogl_texture_iterate_manual_repeats (_cogl_sub_texture_manual_repeat_cb,
                                          virtual_tx_1, virtual_ty_1,
                                          virtual_tx_2, virtual_ty_2,
                                          &data);
}

static void
_cogl_sub_texture_set_wrap_mode_parameter (CoglTexture *tex,
                                           GLenum wrap_mode)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  _cogl_texture_set_wrap_mode_parameter (sub_tex->full_texture, wrap_mode);
}

static void
_cogl_sub_texture_free (CoglSubTexture *sub_tex)
{
  cogl_handle_unref (sub_tex->full_texture);

  g_free (sub_tex);
}

CoglHandle
_cogl_sub_texture_new (CoglHandle full_texture,
                       gfloat tx1, gfloat ty1,
                       gfloat tx2, gfloat ty2)
{
  CoglSubTexture *sub_tex;
  CoglTexture    *tex;
  gfloat          integer_part;

  sub_tex = g_new (CoglSubTexture, 1);

  tex = COGL_TEXTURE (sub_tex);
  tex->vtable = &cogl_sub_texture_vtable;

  sub_tex->full_texture = cogl_handle_ref (full_texture);

  sub_tex->tx1 = tx1;
  sub_tex->ty1 = ty1;
  sub_tex->tx2 = tx2;
  sub_tex->ty2 = ty2;

  /* Track whether the texture coords are a multiple of one because in
     that case we can use hardware repeating */
  sub_tex->tex_coords_are_a_multiple
    = (modff (tx1, &integer_part) == 0.0f &&
       modff (ty1, &integer_part) == 0.0f &&
       modff (tx2, &integer_part) == 0.0f &&
       modff (ty2, &integer_part) == 0.0f);

  return _cogl_sub_texture_handle_new (sub_tex);
}

static gint
_cogl_sub_texture_get_max_waste (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return cogl_texture_get_max_waste (sub_tex->full_texture);
}

static gboolean
_cogl_sub_texture_is_sliced (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return cogl_texture_is_sliced (sub_tex->full_texture);
}

static gboolean
_cogl_sub_texture_can_hardware_repeat (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  /* We can hardware repeat if the full texture can hardware repeat
     and the coordinates for the subregion are all a multiple of the
     full size of the texture (ie, they have no fractional part) */

  return (sub_tex->tex_coords_are_a_multiple &&
          _cogl_texture_can_hardware_repeat (sub_tex->full_texture));
}

static void
_cogl_sub_texture_transform_coords_to_gl (CoglTexture *tex,
                                          float *s,
                                          float *t)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  _cogl_sub_texture_map_coordinate_pair (sub_tex, s, t);
  _cogl_texture_transform_coords_to_gl (sub_tex->full_texture, s, t);
}

static gboolean
_cogl_sub_texture_get_gl_texture (CoglTexture *tex,
                                  GLuint *out_gl_handle,
                                  GLenum *out_gl_target)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return cogl_texture_get_gl_texture (sub_tex->full_texture,
                                      out_gl_handle,
                                      out_gl_target);
}

static void
_cogl_sub_texture_set_filters (CoglTexture *tex,
                               GLenum min_filter,
                               GLenum mag_filter)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  _cogl_texture_set_filters (sub_tex->full_texture, min_filter, mag_filter);
}

static void
_cogl_sub_texture_ensure_mipmaps (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  _cogl_texture_ensure_mipmaps (sub_tex->full_texture);
}

static void
_cogl_sub_texture_get_next_chunk (int pos, int end,
                                  int tex_size,
                                  int *chunk_start, int *chunk_end)
{
  /* pos and end may be negative or greater than the size of the
     texture. We want to calculate the next largest range we can copy
     in one chunk */

  if (pos < 0)
    /* The behaviour of % for negative numbers is implementation
       dependant in C89 so we have to do this */
    *chunk_start = (tex_size - pos) % tex_size;
  else
    *chunk_start = pos % tex_size;

  /* If the region is larger than the remaining size of the texture
     then we need to crop it */
  if (end - pos > tex_size - *chunk_start)
    end = pos + tex_size - *chunk_start;

  if (end < 0)
    *chunk_end = (tex_size - end) % tex_size;
  else
    *chunk_end = end % tex_size;

  if (*chunk_end == 0)
    *chunk_end = tex_size;
}

static void
_cogl_sub_texture_get_x_pixel_pos (CoglSubTexture *sub_tex,
                                   gint *px1, gint *px2)
{
  gint full_width = cogl_texture_get_width (sub_tex->full_texture);

  *px1 = full_width * sub_tex->tx1;
  *px2 = full_width * sub_tex->tx2;

  if (*px1 > *px2)
    {
      gint temp = *px1;
      *px1 = *px2;
      *px2 = temp;
    }
}

static void
_cogl_sub_texture_get_y_pixel_pos (CoglSubTexture *sub_tex,
                                   gint *py1, gint *py2)
{
  gint full_width = cogl_texture_get_width (sub_tex->full_texture);

  *py1 = full_width * sub_tex->ty1;
  *py2 = full_width * sub_tex->ty2;

  if (*py1 > *py2)
    {
      gint temp = *py1;
      *py1 = *py2;
      *py2 = temp;
    }
}

static gboolean
_cogl_sub_texture_set_region (CoglTexture    *tex,
                              int             src_x,
                              int             src_y,
                              int             dst_x,
                              int             dst_y,
                              unsigned int    dst_width,
                              unsigned int    dst_height,
                              int             width,
                              int             height,
                              CoglPixelFormat format,
                              unsigned int    rowstride,
                              const guint8   *data)
{
  CoglSubTexture  *sub_tex          = COGL_SUB_TEXTURE (tex);
  gint             full_tex_width, full_tex_height;
  gint             bpp;
  gint             px1, py1, px2, py2;
  gint             it_x, it_y;
  gint             src_x1, src_y1, src_x2, src_y2;
  CoglBitmap       source_bmp;
  CoglBitmap       temp_bmp;
  gboolean         source_bmp_owner = FALSE;
  CoglPixelFormat  closest_format;
  GLenum           closest_gl_format;
  GLenum           closest_gl_type;
  gboolean         success;
  CoglPixelFormat  tex_format;

  /* Check for valid format */
  if (format == COGL_PIXEL_FORMAT_ANY)
    return FALSE;

  /* Shortcut out early if the image is empty */
  if (width == 0 || height == 0)
    return TRUE;

  /* FIXME: If the sub texture coordinates are swapped around then we
     should flip the bitmap */

  _cogl_sub_texture_get_x_pixel_pos (sub_tex, &px1, &px2);
  _cogl_sub_texture_get_y_pixel_pos (sub_tex, &py1, &py2);

  full_tex_width = cogl_texture_get_width (sub_tex->full_texture);
  full_tex_height = cogl_texture_get_height (sub_tex->full_texture);

  /* Init source bitmap */
  source_bmp.width = width;
  source_bmp.height = height;
  source_bmp.format = format;
  source_bmp.data = (guchar*) data;

  /* Rowstride from texture width if none specified */
  bpp = _cogl_get_format_bpp (format);
  source_bmp.rowstride = (rowstride == 0) ? width * bpp : rowstride;

  /* Find closest format to internal that's supported by GL */
  tex_format = cogl_texture_get_format (sub_tex->full_texture);
  closest_format = _cogl_pixel_format_to_gl (tex_format,
                                             NULL, /* don't need */
                                             &closest_gl_format,
                                             &closest_gl_type);

  /* If no direct match, convert */
  if (closest_format != format)
    {
      /* Convert to required format */
      success = _cogl_bitmap_convert_and_premult (&source_bmp,
                                                  &temp_bmp,
                                                  closest_format);

      /* Swap bitmaps if succeeded */
      if (!success) return FALSE;
      source_bmp = temp_bmp;
      source_bmp_owner = TRUE;
    }

  for (it_y = py1; it_y < py2; it_y += src_y2 - src_y1)
    {
      _cogl_sub_texture_get_next_chunk (it_y, py2, full_tex_width,
                                        &src_y1, &src_y2);

      for (it_x = px1; it_x < px2; it_x += src_x2 - src_x1)
        {
          gint virt_x_1, virt_y_1, virt_width, virt_height;
          gint copy_dst_x, copy_dst_y, copy_dst_width, copy_dst_height;

          _cogl_sub_texture_get_next_chunk (it_x, px2, full_tex_height,
                                            &src_x1, &src_x2);

          /* Offset of the chunk from the left edge in the virtual sub
             texture coordinates */
          virt_x_1 = it_x - px1;
          /* Pixel width covered by this chunk */
          virt_width = src_x2 - src_x1;
          /* Offset of the chunk from the top edge in the virtual sub
             texture coordinates */
          virt_y_1 = it_y - py1;
          /* Pixel height covered by this chunk */
          virt_height = src_y2 - src_y1;

          /* Check if this chunk intersects with the update region */
          if (dst_x + dst_width <= virt_x_1 ||
              dst_x >= virt_x_1 + virt_width ||
              dst_y + dst_height <= it_y - py1 ||
              dst_y >= virt_y_1 + virt_height)
            continue;

          /* Calculate the intersection in virtual coordinates */
          copy_dst_width = dst_width;
          if (dst_x < virt_x_1)
            {
              copy_dst_width -= virt_x_1 - dst_x;
              copy_dst_x = virt_x_1;
            }
          else
            copy_dst_x = dst_x;
          if (copy_dst_width + copy_dst_x > virt_x_1 + virt_width)
            copy_dst_width = virt_x_1 + virt_width - copy_dst_x;

          copy_dst_height = dst_height;
          if (dst_y < virt_y_1)
            {
              copy_dst_height -= virt_y_1 - dst_y;
              copy_dst_y = virt_y_1;
            }
          else
            copy_dst_y = dst_y;
          if (copy_dst_height + copy_dst_y > virt_y_1 + virt_height)
            copy_dst_height = virt_y_1 + virt_height - copy_dst_y;

          /* Update the region in the full texture */
          cogl_texture_set_region (sub_tex->full_texture,
                                   src_x + copy_dst_x - dst_x,
                                   src_y + copy_dst_y - dst_y,
                                   src_x1 + copy_dst_x - virt_x_1,
                                   src_y1 + copy_dst_y - virt_y_1,
                                   copy_dst_width,
                                   copy_dst_height,
                                   width,
                                   height,
                                   format,
                                   rowstride,
                                   data);
        }
    }

  /* Free data if owner */
  if (source_bmp_owner)
    g_free (source_bmp.data);

  return TRUE;
}

static void
_cogl_sub_texture_copy_region (guchar *dst,
                               const guchar *src,
                               gint dst_x, gint dst_y,
                               gint src_x, gint src_y,
                               gint width, gint height,
                               gint dst_rowstride,
                               gint src_rowstride,
                               gint bpp)
{
  int y;

  dst += dst_x * bpp + dst_y * dst_rowstride;
  src += src_x * bpp + src_y * src_rowstride;

  for (y = 0; y < height; y++)
    {
      memcpy (dst, src, bpp * width);
      dst += dst_rowstride;
      src += src_rowstride;
    }
}

static int
_cogl_sub_texture_get_data (CoglTexture     *tex,
                            CoglPixelFormat  format,
                            unsigned int     rowstride,
                            guint8          *data)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);
  unsigned int full_rowstride;
  guint8 *full_data;
  int byte_size, full_size;
  gint bpp;
  gint px1, py1, px2, py2;
  gint full_tex_width, full_tex_height;

  /* FIXME: This gets the full data from the full texture and then
     copies a subregion of that. It would be better if there was a
     texture_get_sub_data virtual and it can just munge the texture
     coordinates */

  /* Default to internal format if none specified */
  if (format == COGL_PIXEL_FORMAT_ANY)
    format = cogl_texture_get_format (sub_tex->full_texture);

  _cogl_sub_texture_get_x_pixel_pos (sub_tex, &px1, &px2);
  _cogl_sub_texture_get_y_pixel_pos (sub_tex, &py1, &py2);

  full_tex_width = cogl_texture_get_width (sub_tex->full_texture);
  full_tex_height = cogl_texture_get_height (sub_tex->full_texture);

  /* Rowstride from texture width if none specified */
  bpp = _cogl_get_format_bpp (format);
  if (rowstride == 0)
    rowstride = px2 - px1;

  /* Return byte size if only that requested */
  byte_size = (py2 - py1) * rowstride;
  if (data == NULL)
    return byte_size;

  full_rowstride = _cogl_get_format_bpp (format) * full_tex_width;
  full_data = g_malloc (full_rowstride * full_tex_height);

  full_size = cogl_texture_get_data (sub_tex->full_texture, format,
                                     full_rowstride, full_data);

  if (full_size)
    {
      int dst_x, dst_y;
      int src_x1, src_y1;
      int src_x2, src_y2;

      for (dst_y = py1; dst_y < py2; dst_y += src_y2 - src_y1)
        {
          _cogl_sub_texture_get_next_chunk (dst_y, py2, full_tex_width,
                                            &src_y1, &src_y2);

          for (dst_x = px1; dst_x < px2; dst_x += src_x2 - src_x1)
            {
              _cogl_sub_texture_get_next_chunk (dst_x, px2, full_tex_height,
                                                &src_x1, &src_x2);

              _cogl_sub_texture_copy_region (data, full_data,
                                             dst_x - px1, dst_y - py1,
                                             src_x1, src_y1,
                                             src_x2 - src_x1,
                                             src_y2 - src_y1,
                                             rowstride,
                                             full_rowstride,
                                             bpp);
            }
        }
    }
  else
    byte_size = 0;

  g_free (full_data);

  return byte_size;
}

static CoglPixelFormat
_cogl_sub_texture_get_format (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return cogl_texture_get_format (sub_tex->full_texture);
}

static GLenum
_cogl_sub_texture_get_gl_format (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return _cogl_texture_get_gl_format (sub_tex->full_texture);
}

static gint
_cogl_sub_texture_get_width (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);
  gint px1, px2;

  _cogl_sub_texture_get_x_pixel_pos (sub_tex, &px1, &px2);

  return px2 - px1;
}

static gint
_cogl_sub_texture_get_height (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);
  gint py1, py2;

  _cogl_sub_texture_get_y_pixel_pos (sub_tex, &py1, &py2);

  return py2 - py1;
}

static const CoglTextureVtable
cogl_sub_texture_vtable =
  {
    _cogl_sub_texture_set_region,
    _cogl_sub_texture_get_data,
    _cogl_sub_texture_foreach_sub_texture_in_region,
    _cogl_sub_texture_get_max_waste,
    _cogl_sub_texture_is_sliced,
    _cogl_sub_texture_can_hardware_repeat,
    _cogl_sub_texture_transform_coords_to_gl,
    _cogl_sub_texture_get_gl_texture,
    _cogl_sub_texture_set_filters,
    _cogl_sub_texture_ensure_mipmaps,
    _cogl_sub_texture_set_wrap_mode_parameter,
    _cogl_sub_texture_get_format,
    _cogl_sub_texture_get_gl_format,
    _cogl_sub_texture_get_width,
    _cogl_sub_texture_get_height
  };
