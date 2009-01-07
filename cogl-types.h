/* cogl-types.h: Shared COGL types
 * This file is part of Clutter
 *
 * Copyright (C) 2008  Intel Corporation.
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
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_TYPES_H__
#define __COGL_TYPES_H__

#include <glib.h>

G_BEGIN_DECLS

/**
 * CoglBitmap:
 *
 * Type used for storing image data.
 */
typedef struct _CoglBitmap CoglBitmap;

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
 * @COGL_PIXEL_FORMAT_ANY:
 * @COGL_PIXEL_FORMAT_A_8:
 * @COGL_PIXEL_FORMAT_RGB_888:
 * @COGL_PIXEL_FORMAT_BGR_888:
 * @COGL_PIXEL_FORMAT_RGBA_8888:
 * @COGL_PIXEL_FORMAT_BGRA_8888:
 * @COGL_PIXEL_FORMAT_ARGB_8888:
 * @COGL_PIXEL_FORMAT_ABGR_8888:
 * @COGL_PIXEL_FORMAT_RGBA_8888_PRE:
 * @COGL_PIXEL_FORMAT_BGRA_8888_PRE:
 * @COGL_PIXEL_FORMAT_ARGB_8888_PRE:
 * @COGL_PIXEL_FORMAT_ABGR_8888_PRE:
 * @COGL_PIXEL_FORMAT_RGB_565:
 * @COGL_PIXEL_FORMAT_RGBA_4444:
 * @COGL_PIXEL_FORMAT_RGBA_5551:
 * @COGL_PIXEL_FORMAT_RGBA_4444_PRE:
 * @COGL_PIXEL_FORMAT_RGBA_5551_PRE:
 * @COGL_PIXEL_FORMAT_YUV:
 * @COGL_PIXEL_FORMAT_G_8:
 *
 * Pixel formats used by COGL.
 */
typedef enum
{
  COGL_PIXEL_FORMAT_ANY           = 0,
  COGL_PIXEL_FORMAT_A_8           = 1 | COGL_A_BIT,

  COGL_PIXEL_FORMAT_RGB_565       = 4,
  COGL_PIXEL_FORMAT_RGBA_4444     = 5 | COGL_A_BIT,
  COGL_PIXEL_FORMAT_RGBA_5551     = 6 | COGL_A_BIT,
  COGL_PIXEL_FORMAT_YUV           = 7,
  COGL_PIXEL_FORMAT_G_8           = 8,
  
  COGL_PIXEL_FORMAT_RGB_888       =  COGL_PIXEL_FORMAT_24,

  COGL_PIXEL_FORMAT_BGR_888       = (COGL_PIXEL_FORMAT_24 |
                                     COGL_BGR_BIT),

  COGL_PIXEL_FORMAT_RGBA_8888     =  COGL_PIXEL_FORMAT_32 |
                                     COGL_A_BIT,

  COGL_PIXEL_FORMAT_BGRA_8888     = (COGL_PIXEL_FORMAT_32 |
                                     COGL_A_BIT           |
                                     COGL_BGR_BIT),

  COGL_PIXEL_FORMAT_ARGB_8888     = (COGL_PIXEL_FORMAT_32 |
                                     COGL_A_BIT           |
                                     COGL_AFIRST_BIT),

  COGL_PIXEL_FORMAT_ABGR_8888     = (COGL_PIXEL_FORMAT_32 |
                                     COGL_A_BIT           |
                                     COGL_BGR_BIT         |
                                     COGL_AFIRST_BIT),

  COGL_PIXEL_FORMAT_RGBA_8888_PRE = (COGL_PIXEL_FORMAT_32 |
                                     COGL_A_BIT           |
                                     COGL_PREMULT_BIT),

  COGL_PIXEL_FORMAT_BGRA_8888_PRE = (COGL_PIXEL_FORMAT_32 |
                                     COGL_A_BIT           |
                                     COGL_PREMULT_BIT     |
                                     COGL_BGR_BIT),

  COGL_PIXEL_FORMAT_ARGB_8888_PRE = (COGL_PIXEL_FORMAT_32 |
                                     COGL_A_BIT           |
                                     COGL_PREMULT_BIT     |
                                     COGL_AFIRST_BIT),

  COGL_PIXEL_FORMAT_ABGR_8888_PRE = (COGL_PIXEL_FORMAT_32 |
                                     COGL_A_BIT           |
                                     COGL_PREMULT_BIT     |
                                     COGL_BGR_BIT         |
                                     COGL_AFIRST_BIT),
  
  COGL_PIXEL_FORMAT_RGBA_4444_PRE = (COGL_PIXEL_FORMAT_RGBA_4444 |
                                     COGL_A_BIT                  |
                                     COGL_PREMULT_BIT),

  COGL_PIXEL_FORMAT_RGBA_5551_PRE = (COGL_PIXEL_FORMAT_RGBA_5551 |
                                     COGL_A_BIT                  |
                                     COGL_PREMULT_BIT),
  
  
} CoglPixelFormat;

/**
 * CoglFeatureFlags:
 * @COGL_FEATURE_TEXTURE_RECTANGLE:
 * @COGL_FEATURE_TEXTURE_NPOT:
 * @COGL_FEATURE_TEXTURE_YUV:
 * @COGL_FEATURE_TEXTURE_READ_PIXELS:
 * @COGL_FEATURE_SHADERS_GLSL:
 * @COGL_FEATURE_OFFSCREEN:
 * @COGL_FEATURE_OFFSCREEN_MULTISAMPLE:
 * @COGL_FEATURE_OFFSCREEN_BLIT:
 * @COGL_FEATURE_FOUR_CLIP_PLANES:
 * @COGL_FEATURE_STENCIL_BUFFER:
 * @COGL_FEATURE_VBOS:
 *
 * Flags for the supported features.
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
 * @COGL_WINDOW_BUFFER:
 * @COGL_MASK_BUFFER:
 * @COGL_OFFSCREEN_BUFFER:
 *
 *
 */
typedef enum
{
  COGL_WINDOW_BUFFER      = (1 << 1),
  COGL_MASK_BUFFER        = (1 << 2),
  COGL_OFFSCREEN_BUFFER   = (1 << 3)
  
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
  CoglFixed x, y, z;
  CoglFixed tx, ty;
  CoglColor color;
};

G_END_DECLS

#endif /* __COGL_TYPES_H__ */
