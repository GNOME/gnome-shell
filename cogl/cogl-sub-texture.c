/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009,2010 Intel Corporation.
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
#include "cogl-context-private.h"
#include "cogl-handle.h"
#include "cogl-texture-driver.h"

#include <string.h>
#include <math.h>

static void _cogl_sub_texture_free (CoglSubTexture *sub_tex);

COGL_TEXTURE_INTERNAL_DEFINE (SubTexture, sub_texture);

static const CoglTextureVtable cogl_sub_texture_vtable;

static void
_cogl_sub_texture_map_range (float *t1, float *t2,
                             int sub_offset,
                             int sub_size,
                             int full_size)
{
  float t1_frac, t1_int, t2_frac, t2_int;

  t1_frac = modff (*t1, &t1_int);
  t2_frac = modff (*t2, &t2_int);

  if (t1_frac < 0.0f)
    {
      t1_frac += 1.0f;
      t1_int -= 1.0f;
    }
  if (t2_frac < 0.0f)
    {
      t2_frac += 1.0f;
      t2_int -= 1.0f;
    }

  /* If one of the coordinates is zero we need to make sure it is
     still greater than the other coordinate if it was originally so
     we'll flip it to the other side  */
  if (*t1 < *t2)
    {
      if (t2_frac == 0.0f)
        {
          t2_frac = 1.0f;
          t2_int -= 1.0f;
        }
    }
  else
    {
      if (t1_frac == 0.0f)
        {
          t1_frac = 1.0f;
          t1_int -= 1.0f;
        }
    }

  /* Convert the fractional part leaving the integer part intact */
  t1_frac = (sub_offset + t1_frac * sub_size) / full_size;
  *t1 = t1_frac + t1_int;

  t2_frac = (sub_offset + t2_frac * sub_size) / full_size;
  *t2 = t2_frac + t2_int;
}

static void
_cogl_sub_texture_map_quad (CoglSubTexture *sub_tex,
                            float *coords)
{
  unsigned int full_width = cogl_texture_get_width (sub_tex->full_texture);
  unsigned int full_height = cogl_texture_get_height (sub_tex->full_texture);

  _cogl_sub_texture_map_range (coords + 0, coords + 2,
                               sub_tex->sub_x, sub_tex->sub_width,
                               full_width);
  _cogl_sub_texture_map_range (coords + 1, coords + 3,
                               sub_tex->sub_y, sub_tex->sub_height,
                               full_height);
}

/* Maps from the texture coordinates of the full texture to the
   texture coordinates of the sub texture */
static float
_cogl_sub_texture_unmap_coord (float t,
                               int sub_offset,
                               int sub_size,
                               int full_size)
{
  float frac_part, int_part;

  /* Convert the fractional part leaving the integer part in tact */
  frac_part = modff (t, &int_part);

  if (cogl_util_float_signbit (frac_part))
    frac_part = ((1.0f + frac_part) * full_size -
                 sub_offset - sub_size) / sub_size;
  else
    frac_part = (frac_part * full_size - sub_offset) / sub_size;

  return frac_part + int_part;
}

static void
_cogl_sub_texture_unmap_coords (CoglSubTexture *sub_tex,
                                float *s,
                                float *t)
{
  unsigned int full_width = cogl_texture_get_width (sub_tex->full_texture);
  unsigned int full_height = cogl_texture_get_height (sub_tex->full_texture);

  *s = _cogl_sub_texture_unmap_coord (*s, sub_tex->sub_x, sub_tex->sub_width,
                                      full_width);
  *t = _cogl_sub_texture_unmap_coord (*t, sub_tex->sub_y, sub_tex->sub_height,
                                      full_height);
}

typedef struct _CoglSubTextureForeachData
{
  CoglSubTexture *sub_tex;
  CoglTextureSliceCallback callback;
  void *user_data;
} CoglSubTextureForeachData;

static void
_cogl_sub_texture_foreach_cb (CoglHandle handle,
                              const float *slice_coords,
                              const float *full_virtual_coords,
                              void *user_data)
{
  CoglSubTextureForeachData *data = user_data;
  float virtual_coords[4];

  memcpy (virtual_coords, full_virtual_coords, sizeof (virtual_coords));
  /* Convert the virtual coords from the full-texture space to the sub
     texture space */
  _cogl_sub_texture_unmap_coords (data->sub_tex,
                                  &virtual_coords[0],
                                  &virtual_coords[1]);
  _cogl_sub_texture_unmap_coords (data->sub_tex,
                                  &virtual_coords[2],
                                  &virtual_coords[3]);

  data->callback (handle,
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

  _cogl_sub_texture_map_quad (data->sub_tex, mapped_coords);

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

  _cogl_texture_iterate_manual_repeats (_cogl_sub_texture_manual_repeat_cb,
                                        virtual_tx_1, virtual_ty_1,
                                        virtual_tx_2, virtual_ty_2,
                                        &data);
}

static void
_cogl_sub_texture_set_wrap_mode_parameters (CoglTexture *tex,
                                            GLenum wrap_mode_s,
                                            GLenum wrap_mode_t,
                                            GLenum wrap_mode_p)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  _cogl_texture_set_wrap_mode_parameters (sub_tex->full_texture,
                                          wrap_mode_s,
                                          wrap_mode_t,
                                          wrap_mode_p);
}

static void
_cogl_sub_texture_free (CoglSubTexture *sub_tex)
{
  cogl_handle_unref (sub_tex->next_texture);
  cogl_handle_unref (sub_tex->full_texture);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (sub_tex));
}

CoglHandle
_cogl_sub_texture_new (CoglHandle next_texture,
                       int sub_x, int sub_y,
                       int sub_width, int sub_height)
{
  CoglHandle      full_texture;
  CoglSubTexture *sub_tex;
  CoglTexture    *tex;
  unsigned int    next_width, next_height;

  next_width = cogl_texture_get_width (next_texture);
  next_height = cogl_texture_get_height (next_texture);

  /* The region must specify a non-zero subset of the full texture */
  g_return_val_if_fail (sub_x >= 0 && sub_y >= 0, COGL_INVALID_HANDLE);
  g_return_val_if_fail (sub_width > 0 && sub_height > 0, COGL_INVALID_HANDLE);
  g_return_val_if_fail (sub_x + sub_width <= next_width, COGL_INVALID_HANDLE);
  g_return_val_if_fail (sub_y + sub_height <= next_height, COGL_INVALID_HANDLE);

  sub_tex = g_new (CoglSubTexture, 1);

  tex = COGL_TEXTURE (sub_tex);

  _cogl_texture_init (tex, &cogl_sub_texture_vtable);

  /* If the next texture is also a sub texture we can avoid one level
     of indirection by referencing the full texture of that texture
     instead. */
  if (_cogl_is_sub_texture (next_texture))
    {
      CoglSubTexture *other_sub_tex =
        _cogl_sub_texture_pointer_from_handle (next_texture);
      full_texture = other_sub_tex->full_texture;
      sub_x += other_sub_tex->sub_x;
      sub_y += other_sub_tex->sub_y;
    }
  else
    full_texture = next_texture;

  sub_tex->next_texture = cogl_handle_ref (next_texture);
  sub_tex->full_texture = cogl_handle_ref (full_texture);

  sub_tex->sub_x = sub_x;
  sub_tex->sub_y = sub_y;
  sub_tex->sub_width = sub_width;
  sub_tex->sub_height = sub_height;

  return _cogl_sub_texture_handle_new (sub_tex);
}

static int
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

  /* We can hardware repeat if the subtexture actually represents all of the
     of the full texture */
  return (sub_tex->sub_width ==
          cogl_texture_get_width (sub_tex->full_texture) &&
          sub_tex->sub_height ==
          cogl_texture_get_height (sub_tex->full_texture) &&
          _cogl_texture_can_hardware_repeat (sub_tex->full_texture));
}

static void
_cogl_sub_texture_transform_coords_to_gl (CoglTexture *tex,
                                          float *s,
                                          float *t)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  /* This won't work if the sub texture is not the size of the full
     texture and the coordinates are outside the range [0,1] */
  *s = ((*s * sub_tex->sub_width + sub_tex->sub_x) /
        cogl_texture_get_width (sub_tex->full_texture));
  *t = ((*t * sub_tex->sub_height + sub_tex->sub_y) /
        cogl_texture_get_height (sub_tex->full_texture));

  _cogl_texture_transform_coords_to_gl (sub_tex->full_texture, s, t);
}

static CoglTransformResult
_cogl_sub_texture_transform_quad_coords_to_gl (CoglTexture *tex,
                                               float *coords)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);
  int i;

  /* We can't support repeating with this method. In this case
     cogl-primitives will resort to manual repeating */
  for (i = 0; i < 4; i++)
    if (coords[i] < 0.0f || coords[i] > 1.0f)
      return COGL_TRANSFORM_SOFTWARE_REPEAT;

  _cogl_sub_texture_map_quad (sub_tex, coords);

  return _cogl_texture_transform_quad_coords_to_gl (sub_tex->full_texture,
                                                    coords);
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
_cogl_sub_texture_pre_paint (CoglTexture *tex,
                             CoglTexturePrePaintFlags flags)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  _cogl_texture_pre_paint (sub_tex->full_texture, flags);
}

static void
_cogl_sub_texture_ensure_non_quad_rendering (CoglTexture *tex)
{
}

static gboolean
_cogl_sub_texture_set_region (CoglTexture    *tex,
                              int             src_x,
                              int             src_y,
                              int             dst_x,
                              int             dst_y,
                              unsigned int    dst_width,
                              unsigned int    dst_height,
                              CoglBitmap     *bmp)
{
  CoglSubTexture  *sub_tex = COGL_SUB_TEXTURE (tex);

  return _cogl_texture_set_region_from_bitmap (sub_tex->full_texture,
                                               src_x, src_y,
                                               dst_x + sub_tex->sub_x,
                                               dst_y + sub_tex->sub_y,
                                               dst_width, dst_height,
                                               bmp);
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

static int
_cogl_sub_texture_get_width (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return sub_tex->sub_width;
}

static int
_cogl_sub_texture_get_height (CoglTexture *tex)
{
  CoglSubTexture *sub_tex = COGL_SUB_TEXTURE (tex);

  return sub_tex->sub_height;
}

static const CoglTextureVtable
cogl_sub_texture_vtable =
  {
    _cogl_sub_texture_set_region,
    NULL, /* get_data */
    _cogl_sub_texture_foreach_sub_texture_in_region,
    _cogl_sub_texture_get_max_waste,
    _cogl_sub_texture_is_sliced,
    _cogl_sub_texture_can_hardware_repeat,
    _cogl_sub_texture_transform_coords_to_gl,
    _cogl_sub_texture_transform_quad_coords_to_gl,
    _cogl_sub_texture_get_gl_texture,
    _cogl_sub_texture_set_filters,
    _cogl_sub_texture_pre_paint,
    _cogl_sub_texture_ensure_non_quad_rendering,
    _cogl_sub_texture_set_wrap_mode_parameters,
    _cogl_sub_texture_get_format,
    _cogl_sub_texture_get_gl_format,
    _cogl_sub_texture_get_width,
    _cogl_sub_texture_get_height,
    NULL /* is_foreign */
  };
