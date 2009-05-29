/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009 Intel Corporation.
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

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_TYPES_H__
#define __COGL_TYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * CoglHandle:
 *
 * Type used for storing references to cogl objects, the CoglHandle is
 * a fully opaque type without any public data members.
 */
typedef gpointer CoglHandle;

/**
 * COGL_INVALID_HANDLE:
 *
 * A COGL handle that is not valid, used for unitialized handles as well as
 * error conditions.
 */
#define COGL_INVALID_HANDLE NULL

#define COGL_TYPE_HANDLE        (cogl_handle_get_type ())
GType cogl_handle_get_type (void) G_GNUC_CONST;

CoglHandle cogl_handle_ref   (CoglHandle handle);
void       cogl_handle_unref (CoglHandle Handle);

/**
 * CoglFuncPtr:
 *
 * The type used by cogl for function pointers, note that this type
 * is used as a generic catch-all cast for function pointers and the
 * actual arguments and return type may be different.
 */
typedef void (* CoglFuncPtr) (void);

/**
 * CoglFixed:
 *
 * Fixed point number using a (16.16) notation.
 */
typedef gint32 CoglFixed;

#define COGL_TYPE_FIXED         (cogl_fixed_get_type ())
GType cogl_fixed_get_type (void) G_GNUC_CONST;

/**
 * CoglAngle:
 *
 * Integer representation of an angle such that 1024 corresponds to
 * full circle (i.e., 2 * pi).
 *
 * Since: 1.0
 */
typedef gint32 CoglAngle;

typedef struct _CoglColor               CoglColor;
typedef struct _CoglTextureVertex       CoglTextureVertex;

/* Enum declarations */

#define COGL_PIXEL_FORMAT_24    2
#define COGL_PIXEL_FORMAT_32    3
#define COGL_A_BIT              (1 << 4)
#define COGL_BGR_BIT            (1 << 5)
#define COGL_AFIRST_BIT         (1 << 6)
#define COGL_PREMULT_BIT        (1 << 7)
#define COGL_UNORDERED_MASK     0x0F
#define COGL_UNPREMULT_MASK     0x7F

/**
 * CoglPixelFormat:
 * @COGL_PIXEL_FORMAT_ANY: Any format
 * @COGL_PIXEL_FORMAT_A_8: 8 bits alpha mask
 * @COGL_PIXEL_FORMAT_RGB_565: RGB, 16 bits
 * @COGL_PIXEL_FORMAT_RGBA_4444: RGBA, 16 bits
 * @COGL_PIXEL_FORMAT_RGBA_5551: RGBA, 16 bits
 * @COGL_PIXEL_FORMAT_YUV: FIXME
 * @COGL_PIXEL_FORMAT_G_8: FIXME
 * @COGL_PIXEL_FORMAT_RGB_888: RGB, 24 bits
 * @COGL_PIXEL_FORMAT_BGR_888: BGR, 24 bits
 * @COGL_PIXEL_FORMAT_RGBA_8888: RGBA, 32 bits
 * @COGL_PIXEL_FORMAT_BGRA_8888: BGRA, 32 bits
 * @COGL_PIXEL_FORMAT_ARGB_8888: ARGB, 32 bits
 * @COGL_PIXEL_FORMAT_ABGR_8888: ABGR, 32 bits
 * @COGL_PIXEL_FORMAT_RGBA_8888_PRE: Premultiplied RGBA, 32 bits
 * @COGL_PIXEL_FORMAT_BGRA_8888_PRE: Premultiplied BGRA, 32 bits
 * @COGL_PIXEL_FORMAT_ARGB_8888_PRE: Premultiplied ARGB, 32 bits
 * @COGL_PIXEL_FORMAT_ABGR_8888_PRE: Premultiplied ABGR, 32 bits
 * @COGL_PIXEL_FORMAT_RGBA_4444_PRE: Premultiplied RGBA, 16 bits
 * @COGL_PIXEL_FORMAT_RGBA_5551_PRE: Premultiplied RGBA, 16 bits
 *
 * Pixel formats used by COGL.
 *
 * Since: 0.8
 */
typedef enum { /*< prefix=COGL_PIXEL_FORMAT >*/
  COGL_PIXEL_FORMAT_ANY           = 0,
  COGL_PIXEL_FORMAT_A_8           = 1 | COGL_A_BIT,

  COGL_PIXEL_FORMAT_RGB_565       = 4,
  COGL_PIXEL_FORMAT_RGBA_4444     = 5 | COGL_A_BIT,
  COGL_PIXEL_FORMAT_RGBA_5551     = 6 | COGL_A_BIT,
  COGL_PIXEL_FORMAT_YUV           = 7,
  COGL_PIXEL_FORMAT_G_8           = 8,

  COGL_PIXEL_FORMAT_RGB_888       =  COGL_PIXEL_FORMAT_24,
  COGL_PIXEL_FORMAT_BGR_888       = (COGL_PIXEL_FORMAT_24 | COGL_BGR_BIT),

  COGL_PIXEL_FORMAT_RGBA_8888     = (COGL_PIXEL_FORMAT_32 | COGL_A_BIT),
  COGL_PIXEL_FORMAT_BGRA_8888     = (COGL_PIXEL_FORMAT_32 | COGL_A_BIT | COGL_BGR_BIT),
  COGL_PIXEL_FORMAT_ARGB_8888     = (COGL_PIXEL_FORMAT_32 | COGL_A_BIT | COGL_AFIRST_BIT),
  COGL_PIXEL_FORMAT_ABGR_8888     = (COGL_PIXEL_FORMAT_32 | COGL_A_BIT | COGL_BGR_BIT | COGL_AFIRST_BIT),

  COGL_PIXEL_FORMAT_RGBA_8888_PRE = (COGL_PIXEL_FORMAT_32 | COGL_A_BIT | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_BGRA_8888_PRE = (COGL_PIXEL_FORMAT_32 | COGL_A_BIT | COGL_PREMULT_BIT | COGL_BGR_BIT),
  COGL_PIXEL_FORMAT_ARGB_8888_PRE = (COGL_PIXEL_FORMAT_32 | COGL_A_BIT | COGL_PREMULT_BIT | COGL_AFIRST_BIT),
  COGL_PIXEL_FORMAT_ABGR_8888_PRE = (COGL_PIXEL_FORMAT_32 | COGL_A_BIT | COGL_PREMULT_BIT | COGL_BGR_BIT | COGL_AFIRST_BIT),
  COGL_PIXEL_FORMAT_RGBA_4444_PRE = (COGL_PIXEL_FORMAT_RGBA_4444 | COGL_A_BIT | COGL_PREMULT_BIT),
  COGL_PIXEL_FORMAT_RGBA_5551_PRE = (COGL_PIXEL_FORMAT_RGBA_5551 | COGL_A_BIT | COGL_PREMULT_BIT),
} CoglPixelFormat;

/**
 * CoglFeatureFlags:
 * @COGL_FEATURE_TEXTURE_RECTANGLE: ARB_texture_rectangle support
 * @COGL_FEATURE_TEXTURE_NPOT: ARB_texture_non_power_of_two support
 * @COGL_FEATURE_TEXTURE_YUV: ycbcr conversion support
 * @COGL_FEATURE_TEXTURE_READ_PIXELS: glReadPixels() support
 * @COGL_FEATURE_SHADERS_GLSL: GLSL support
 * @COGL_FEATURE_OFFSCREEN: FBO support
 * @COGL_FEATURE_OFFSCREEN_MULTISAMPLE: Multisample support on FBOs
 * @COGL_FEATURE_OFFSCREEN_BLIT: Blit support on FBOs
 * @COGL_FEATURE_FOUR_CLIP_PLANES: At least 4 clip planes available
 * @COGL_FEATURE_STENCIL_BUFFER: Stencil buffer support
 * @COGL_FEATURE_VBOS: VBO support
 *
 * Flags for the supported features.
 *
 * Since: 0.8
 */
typedef enum
{
  COGL_FEATURE_TEXTURE_RECTANGLE      = (1 << 1),
  COGL_FEATURE_TEXTURE_NPOT           = (1 << 2),
  COGL_FEATURE_TEXTURE_YUV            = (1 << 3),
  COGL_FEATURE_TEXTURE_READ_PIXELS    = (1 << 4),
  COGL_FEATURE_SHADERS_GLSL           = (1 << 5),
  COGL_FEATURE_OFFSCREEN              = (1 << 6),
  COGL_FEATURE_OFFSCREEN_MULTISAMPLE  = (1 << 7),
  COGL_FEATURE_OFFSCREEN_BLIT         = (1 << 8),
  COGL_FEATURE_FOUR_CLIP_PLANES       = (1 << 9),
  COGL_FEATURE_STENCIL_BUFFER         = (1 << 10),
  COGL_FEATURE_VBOS		      = (1 << 11)
} CoglFeatureFlags;

/**
 * CoglBufferTarget:
 * @COGL_WINDOW_BUFFER: FIXME
 * @COGL_OFFSCREEN_BUFFER: FIXME
 *
 * Target flags for FBOs.
 *
 * Since: 0.8
 */
typedef enum
{
  COGL_WINDOW_BUFFER      = (1 << 1),
  COGL_OFFSCREEN_BUFFER   = (1 << 2)
} CoglBufferTarget;

/**
 * CoglColor:
 *
 * A structure for holding a color definition. The contents of
 * the CoglColor structure are private and should never by accessed
 * directly.
 *
 * Since: 1.0
 */
struct _CoglColor
{
  /*< private >*/
  guint8 red;
  guint8 green;
  guint8 blue;

  guint8 alpha;

  /* padding in case we want to change to floats at
   * some point */
  guint32 padding0;
  guint32 padding1;
  guint32 padding2;
};

/**
 * CoglTextureVertex:
 * @x: Model x-coordinate
 * @y: Model y-coordinate
 * @z: Model z-coordinate
 * @tx: Texture x-coordinate
 * @ty: Texture y-coordinate
 * @color: The color to use at this vertex. This is ignored if
 * @use_color is %FALSE when calling cogl_texture_polygon().
 *
 * Used to specify vertex information when calling cogl_texture_polygon().
 */
struct _CoglTextureVertex
{
  float x, y, z;
  float tx, ty;
  CoglColor color;
};

/**
 * CoglTextureFlags:
 * @COGL_TEXTURE_NONE: No flags specified
 * @COGL_TEXTURE_AUTO_MIPMAP: Enables the automatic generation of the
 *   mipmap pyramid from the base level image whenever it is updated
 * @COGL_TEXTURE_NO_SLICING: Disables the slicing of the texture
 *
 * Flags to pass to the cogl_texture_new_* family of functions.
 *
 * Since: 1.0
 */
typedef enum {
  COGL_TEXTURE_NONE        = 0,
  COGL_TEXTURE_AUTO_MIPMAP = 1 << 0,
  COGL_TEXTURE_NO_SLICING  = 1 << 1
} CoglTextureFlags;

/**
 * CoglFogMode:
 * @COGL_FOG_MODE_LINEAR: Calculates the fog blend factor as:
 * |[
 *   f = end - eye_distance / end - start
 * ]|
 * @COGL_FOG_MODE_EXPONENTIAL: Calculates the fog blend factor as:
 * |[
 *   f = e ^ -(density * eye_distance)
 * ]|
 * @COGL_FOG_MODE_EXPONENTIAL_SQUARED: Calculates the fog blend factor as:
 * |[
 *   f = e ^ -(density * eye_distance)^2
 * ]|
 *
 * The fog mode determines the equation used to calculate the fogging blend
 * factor while fogging is enabled. The simplest %COGL_FOG_MODE_LINEAR mode
 * determines f as:
 *
 * |[
 *   f = end - eye_distance / end - start
 * ]|
 *
 * Where eye_distance is the distance of the current fragment in eye
 * coordinates from the origin.
 *
 * Since: 1.0
 */
typedef enum {
  COGL_FOG_MODE_LINEAR,
  COGL_FOG_MODE_EXPONENTIAL,
  COGL_FOG_MODE_EXPONENTIAL_SQUARED
} CoglFogMode;

G_END_DECLS

#endif /* __COGL_TYPES_H__ */
