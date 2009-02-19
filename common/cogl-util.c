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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"

#include "cogl-internal.h"
#include "cogl-material.h"
#include "cogl-offscreen.h"
#include "cogl-shader.h"
#include "cogl-texture.h"
#include "cogl-types.h"
#include "cogl-util.h"

/**
 * cogl_util_next_p2:
 * @a: Value to get the next power
 *
 * Calculates the next power greater than @a.
 *
 * Return value: The next power after @a.
 */
int
cogl_util_next_p2 (int a)
{
  int rval=1;

  while(rval < a) 
    rval <<= 1;

  return rval;
}

/* gtypes */

CoglHandle
cogl_handle_ref (CoglHandle handle)
{
  g_return_val_if_fail (handle != COGL_INVALID_HANDLE, COGL_INVALID_HANDLE);

  if (cogl_is_texture (handle))
    return cogl_texture_ref (handle);

  if (cogl_is_offscreen (handle))
    return cogl_offscreen_ref (handle);

  if (cogl_is_material (handle))
    return cogl_material_ref (handle);

  if (cogl_is_program (handle))
    return cogl_program_ref (handle);

  if (cogl_is_shader (handle))
    return cogl_shader_ref (handle);

  return COGL_INVALID_HANDLE;
}

void
cogl_handle_unref (CoglHandle handle)
{
  g_return_if_fail (handle != COGL_INVALID_HANDLE);

  if (cogl_is_texture (handle))
    cogl_texture_unref (handle);

  if (cogl_is_offscreen (handle))
    cogl_offscreen_unref (handle);

  if (cogl_is_material (handle))
    cogl_material_unref (handle);

  if (cogl_is_program (handle))
    cogl_program_unref (handle);

  if (cogl_is_shader (handle))
    cogl_shader_unref (handle);
}

GType
cogl_handle_get_type (void)
{
  static GType our_type = 0;

  if (G_UNLIKELY (our_type == 0))
    our_type = g_boxed_type_register_static (g_intern_static_string ("CoglHandle"),
                                            (GBoxedCopyFunc) cogl_handle_ref,
                                            (GBoxedFreeFunc) cogl_handle_unref);

  return our_type;
}

GType
cogl_pixel_format_get_type (void)
{
  static GType gtype = 0;

  if (G_UNLIKELY (gtype == 0))
    {
      const GEnumValue values[] = {
        { COGL_PIXEL_FORMAT_ANY, "COGL_PIXEL_FORMAT_ANY", "any" },
        { COGL_PIXEL_FORMAT_A_8, "COGL_PIXEL_FORMAT_A_8", "a-8" },
        { COGL_PIXEL_FORMAT_RGB_565, "COGL_PIXEL_FORMAT_RGB_565", "rgb-565" },
        { COGL_PIXEL_FORMAT_RGBA_4444, "COGL_PIXEL_FORMAT_RGBA_4444", "rgba-4444" },
        { COGL_PIXEL_FORMAT_RGBA_5551, "COGL_PIXEL_FORMAT_RGBA_5551", "rgba-5551" },
        { COGL_PIXEL_FORMAT_YUV, "COGL_PIXEL_FORMAT_YUV", "yuv" },
        { COGL_PIXEL_FORMAT_G_8, "COGL_PIXEL_FORMAT_G_8", "g-8" },
        { COGL_PIXEL_FORMAT_RGB_888, "COGL_PIXEL_FORMAT_RGB_888", "rgb-888" },
        { COGL_PIXEL_FORMAT_BGR_888, "COGL_PIXEL_FORMAT_BGR_888", "bgr-888" },
        { COGL_PIXEL_FORMAT_RGBA_8888, "COGL_PIXEL_FORMAT_RGBA_8888", "rgba-8888" },
        { COGL_PIXEL_FORMAT_BGRA_8888, "COGL_PIXEL_FORMAT_BGRA_8888", "bgra-8888" },
        { COGL_PIXEL_FORMAT_ARGB_8888, "COGL_PIXEL_FORMAT_ARGB_8888", "argb-8888" },
        { COGL_PIXEL_FORMAT_ABGR_8888, "COGL_PIXEL_FORMAT_ABGR_8888", "abgr-8888" },
        { COGL_PIXEL_FORMAT_RGBA_8888_PRE, "COGL_PIXEL_FORMAT_RGBA_8888_PRE", "rgba-8888-pre" },
        { COGL_PIXEL_FORMAT_BGRA_8888_PRE, "COGL_PIXEL_FORMAT_BGRA_8888_PRE", "bgra-8888-pre" },
        { COGL_PIXEL_FORMAT_ARGB_8888_PRE, "COGL_PIXEL_FORMAT_ARGB_8888_PRE", "argb-8888-pre" },
        { COGL_PIXEL_FORMAT_ABGR_8888_PRE, "COGL_PIXEL_FORMAT_ABGR_8888_PRE", "abgr-8888-pre" },
        { COGL_PIXEL_FORMAT_RGBA_4444_PRE, "COGL_PIXEL_FORMAT_RGBA_4444_PRE", "rgba-4444-pre" },
        { COGL_PIXEL_FORMAT_RGBA_5551_PRE, "COGL_PIXEL_FORMAT_RGBA_5551_PRE", "rgba-5551-pre" },
        { 0, NULL, NULL }
      };

      gtype =
        g_enum_register_static (g_intern_static_string ("CoglPixelFormat"),
                                values);
    }

  return gtype;
}

GType
cogl_feature_flags_get_type (void)
{
  static GType gtype = 0;

  if (G_UNLIKELY (gtype == 0))
    {
      static const GFlagsValue values[] = {
        { COGL_FEATURE_TEXTURE_RECTANGLE, "COGL_FEATURE_TEXTURE_RECTANGLE", "texture-rectangle" },
        { COGL_FEATURE_TEXTURE_NPOT, "COGL_FEATURE_TEXTURE_NPOT", "texture-npot" },
        { COGL_FEATURE_TEXTURE_YUV, "COGL_FEATURE_TEXTURE_YUV", "yuv" },
        { COGL_FEATURE_TEXTURE_READ_PIXELS, "COGL_FEATURE_TEXTURE_READ_PIXELS", "read-pixels" },
        { COGL_FEATURE_SHADERS_GLSL, "COGL_FEATURE_SHADERS_GLSL", "shaders-glsl" },
        { COGL_FEATURE_OFFSCREEN, "COGL_FEATURE_OFFSCREEN", "offscreen" },
        { COGL_FEATURE_OFFSCREEN_MULTISAMPLE, "COGL_FEATURE_OFFSCREEN_MULTISAMPLE", "offscreen-multisample" },
        { COGL_FEATURE_OFFSCREEN_BLIT, "COGL_FEATURE_OFFSCREEN_BLIT", "offscreen-blit" },
        { COGL_FEATURE_FOUR_CLIP_PLANES, "COGL_FEATURE_FOUR_CLIP_PLANES", "four-clip-planes" },
        { COGL_FEATURE_STENCIL_BUFFER, "COGL_FEATURE_STENCIL_BUFFER", "stencil-buffer" },
        { 0, NULL, NULL }
      };

      gtype =
        g_flags_register_static (g_intern_static_string ("CoglFeatureFlags"),
                                 values);
    }

  return gtype;
}

GType
cogl_buffer_target_get_type (void)
{
  static GType gtype = 0;

  if (G_UNLIKELY (gtype == 0))
    {
      static const GFlagsValue values[] = {
        { COGL_WINDOW_BUFFER, "COGL_WINDOW_BUFFER", "window-buffer" },
        { COGL_MASK_BUFFER, "COGL_MASK_BUFFER", "mask-buffer" },
        { COGL_OFFSCREEN_BUFFER, "COGL_OFFSCREEN_BUFFER", "offscreen-buffer" },
        { 0, NULL, NULL }
      };

      gtype =
        g_flags_register_static (g_intern_static_string ("CoglBufferTarget"),
                                 values);
    }

  return gtype;
}

GType
cogl_texture_flags_get_type (void)
{
  static GType gtype = 0;

  if (G_UNLIKELY (gtype == 0))
    {
      static const GFlagsValue values[] = {
        { COGL_TEXTURE_NONE, "COGL_TEXTURE_NONE", "none" },
        { COGL_TEXTURE_AUTO_MIPMAP, "COGL_TEXTURE_AUTO_MIPMAP", "auto-mipmap" },
        { 0, NULL, NULL }
      };

      gtype =
        g_flags_register_static (g_intern_static_string ("CoglTextureFlags"),
                                 values);
    }

  return gtype;
}
