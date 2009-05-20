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

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "cogl.h"

#include "cogl-fixed.h"
#include "cogl-internal.h"
#include "cogl-material.h"
#include "cogl-current-matrix.h"
#include "cogl-offscreen.h"
#include "cogl-shader.h"
#include "cogl-texture.h"
#include "cogl-types.h"
#include "cogl-handle.h"
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
  CoglHandleObject *obj = (CoglHandleObject *)handle;

  g_return_val_if_fail (handle != COGL_INVALID_HANDLE, COGL_INVALID_HANDLE);

  obj->ref_count++;
  return handle;
}

void
cogl_handle_unref (CoglHandle handle)
{
  CoglHandleObject *obj = (CoglHandleObject *)handle;

  g_return_if_fail (handle != COGL_INVALID_HANDLE);

  if (--obj->ref_count < 1)
    {
      void (*free_func)(void *obj);

      COGL_HANDLE_DEBUG_FREE (obj);
      free_func = obj->klass->virt_free;
      free_func (obj);
    }
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
      static const GEnumValue values[] = {
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
cogl_matrix_mode_get_type (void)
{
  static GType gtype = 0;

  if (G_UNLIKELY (gtype == 0))
    {
      static const GEnumValue values[] = {
        { COGL_MATRIX_MODELVIEW, "COGL_MATRIX_MODELVIEW", "modelview" },
        { COGL_MATRIX_PROJECTION, "COGL_MATRIX_PROJECTION", "projection" },
        { COGL_MATRIX_TEXTURE, "COGL_MATRIX_TEXTURE", "texture" },
        { 0, NULL, NULL }
      };

      gtype =
        g_enum_register_static (g_intern_static_string ("CoglMatrixMode"),
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

GType
cogl_fog_mode_get_type (void)
{
  static GType gtype = 0;

  if (G_UNLIKELY (gtype == 0))
    {
      static const GEnumValue values[] = {
        { COGL_FOG_MODE_LINEAR, "COGL_FOG_MODE_LINEAR", "linear" },
        { COGL_FOG_MODE_EXPONENTIAL, "COGL_FOG_MODE_EXPONENTIAL", "exponential" },
        { COGL_FOG_MODE_EXPONENTIAL_SQUARED, "COGL_FOG_MODE_EXPONENTIAL_SQUARED", "exponential-squared" },
        { 0, NULL, NULL }
      };

      gtype =
        g_enum_register_static (g_intern_static_string ("CoglFogMode"),
                                values);
    }

  return gtype;
}

/*
 * CoglFixed
 */

static GTypeInfo _info = {
  0,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  0,
  NULL,
  NULL,
};

static GTypeFundamentalInfo _finfo = { 0, };

static void
cogl_value_init_fixed (GValue *value)
{
  value->data[0].v_int = 0;
}

static void
cogl_value_copy_fixed (const GValue *src,
                          GValue       *dest)
{
  dest->data[0].v_int = src->data[0].v_int;
}

static gchar *
cogl_value_collect_fixed (GValue      *value,
                             guint        n_collect_values,
                             GTypeCValue *collect_values,
                             guint        collect_flags)
{
  value->data[0].v_int = collect_values[0].v_int;

  return NULL;
}

static gchar *
cogl_value_lcopy_fixed (const GValue *value,
                           guint         n_collect_values,
                           GTypeCValue  *collect_values,
                           guint         collect_flags)
{
  gint32 *fixed_p = collect_values[0].v_pointer;

  if (!fixed_p)
    return g_strdup_printf ("value location for '%s' passed as NULL",
                            G_VALUE_TYPE_NAME (value));

  *fixed_p = value->data[0].v_int;

  return NULL;
}

static void
cogl_value_transform_fixed_int (const GValue *src,
                                GValue       *dest)
{
  dest->data[0].v_int = COGL_FIXED_TO_INT (src->data[0].v_int);
}

static void
cogl_value_transform_fixed_double (const GValue *src,
                                      GValue       *dest)
{
  dest->data[0].v_double = COGL_FIXED_TO_DOUBLE (src->data[0].v_int);
}

static void
cogl_value_transform_fixed_float (const GValue *src,
                                     GValue       *dest)
{
  dest->data[0].v_float = COGL_FIXED_TO_FLOAT (src->data[0].v_int);
}

static void
cogl_value_transform_int_fixed (const GValue *src,
                                   GValue       *dest)
{
  dest->data[0].v_int = COGL_FIXED_FROM_INT (src->data[0].v_int);
}

static void
cogl_value_transform_double_fixed (const GValue *src,
                                      GValue       *dest)
{
  dest->data[0].v_int = COGL_FIXED_FROM_DOUBLE (src->data[0].v_double);
}

static void
cogl_value_transform_float_fixed (const GValue *src,
                                     GValue       *dest)
{
  dest->data[0].v_int = COGL_FIXED_FROM_FLOAT (src->data[0].v_float);
}


static const GTypeValueTable _cogl_fixed_value_table = {
  cogl_value_init_fixed,
  NULL,
  cogl_value_copy_fixed,
  NULL,
  "i",
  cogl_value_collect_fixed,
  "p",
  cogl_value_lcopy_fixed
};

GType
cogl_fixed_get_type (void)
{
  static GType _cogl_fixed_type = 0;

  if (G_UNLIKELY (_cogl_fixed_type == 0))
    {
      _info.value_table = & _cogl_fixed_value_table;
      _cogl_fixed_type =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     g_intern_static_string ("CoglFixed"),
                                     &_info, &_finfo, 0);

      g_value_register_transform_func (_cogl_fixed_type, G_TYPE_INT,
                                       cogl_value_transform_fixed_int);
      g_value_register_transform_func (G_TYPE_INT, _cogl_fixed_type,
                                       cogl_value_transform_int_fixed);

      g_value_register_transform_func (_cogl_fixed_type, G_TYPE_FLOAT,
                                       cogl_value_transform_fixed_float);
      g_value_register_transform_func (G_TYPE_FLOAT, _cogl_fixed_type,

                                       cogl_value_transform_float_fixed);
      g_value_register_transform_func (_cogl_fixed_type, G_TYPE_DOUBLE,
                                       cogl_value_transform_fixed_double);
      g_value_register_transform_func (G_TYPE_DOUBLE, _cogl_fixed_type,
                                       cogl_value_transform_double_fixed);
    }

  return _cogl_fixed_type;
}


