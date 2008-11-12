/* cogl-color.h: Color type for COGL
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

#ifndef __COGL_COLOR_H__
#define __COGL_COLOR_H__

#include <cogl/cogl-types.h>

G_BEGIN_DECLS

CoglColor *cogl_color_new  (void);
CoglColor *cogl_color_copy (const CoglColor *color);
void       cogl_color_free (CoglColor       *color);

/**
 * cogl_color_set_from_4ub:
 * @dest: return location for a #CoglColor
 * @red: value of the red channel, between 0 and 255
 * @green: value of the green channel, between 0 and 255
 * @blue: value of the blue channel, between 0 and 255
 * @alpha: value of the alpha channel, between 0 and 255
 *
 * Sets the values of the passed channels into a #CoglColor.
 *
 * Since: 1.0
 */
void cogl_color_set_from_4ub (CoglColor *dest,
                              guint8 red,
                              guint8 green,
                              guint8 blue,
                              guint8 alpha);
/**
 * cogl_color_set_from_4d:
 * @dest: return location for a #CoglColor
 * @red: value of the red channel, between 0 and 1
 * @green: value of the green channel, between 0 and 1
 * @blue: value of the blue channel, between 0 and 1
 * @alpha: value of the alpha channel, between 0 and 1
 *
 * Sets the values of the passed channels into a #CoglColor.
 *
 * Since: 1.0
 */
void cogl_color_set_from_4d  (CoglColor *dest,
                              gdouble red,
                              gdouble green,
                              gdouble blue,
                              gdouble alpha);

/**
 * cogl_color_set_from_4x:
 * @dest: return location for a #CoglColor
 * @red: value of the red channel, between 0 and %COGL_FIXED_1
 * @green: value of the green channel, between 0 and %COGL_FIXED_1
 * @blue: value of the blue channel, between 0 and %COGL_FIXED_1
 * @alpha: value of the alpha channel, between 0 and %COGL_FIXED_1
 *
 * Sets the values of the passed channels into a #CoglColor
 *
 * Since: 1.0
 */
void cogl_color_set_from_4x (CoglColor *dest,
                             CoglFixed  red,
                             CoglFixed  green,
                             CoglFixed  blue,
                             CoglFixed  alpha);

/**
 * cogl_color_get_red_byte:
 * @color: a #CoglColor
 *
 * Retrieves the red channel of @color as a byte value
 * between 0 and 255
 *
 * Return value: the red channel of the passed color
 *
 * Since: 1.0
 */
unsigned char cogl_color_get_red_byte    (const CoglColor *color);

/**
 * cogl_color_get_green_byte:
 * @color: a #CoglColor
 *
 * Retrieves the green channel of @color as a byte value
 * between 0 and 255
 *
 * Return value: the green channel of the passed color
 *
 * Since: 1.0
 */
unsigned char cogl_color_get_green_byte   (const CoglColor *color);

/**
 * cogl_color_get_blue_byte:
 * @color: a #CoglColor
 *
 * Retrieves the blue channel of @color as a byte value
 * between 0 and 255
 *
 * Return value: the blue channel of the passed color
 *
 * Since: 1.0
 */
unsigned char cogl_color_get_blue_byte   (const CoglColor *color);

/**
 * cogl_color_get_alpha_byte:
 * @color: a #CoglColor
 *
 * Retrieves the alpha channel of @color as a byte value
 * between 0 and 255
 *
 * Return value: the alpha channel of the passed color
 *
 * Since: 1.0
 */
unsigned char cogl_color_get_alpha_byte  (const CoglColor *color);

/**
 * cogl_color_get_red_float:
 * @color: a #CoglColor
 *
 * Retrieves the red channel of @color as a floating point
 * value between 0.0 and 1.0
 *
 * Return value: the red channel of the passed color
 *
 * Since: 1.0
 */
float         cogl_color_get_red_float   (const CoglColor *color);

/**
 * cogl_color_get_green_float:
 * @color: a #CoglColor
 *
 * Retrieves the green channel of @color as a floating point
 * value between 0.0 and 1.0
 *
 * Return value: the green channel of the passed color
 *
 * Since: 1.0
 */
float         cogl_color_get_green_float (const CoglColor *color);

/**
 * cogl_color_get_blue_float:
 * @color: a #CoglColor
 *
 * Retrieves the blue channel of @color as a floating point
 * value between 0.0 and 1.0
 *
 * Return value: the blue channel of the passed color
 *
 * Since: 1.0
 */
float         cogl_color_get_blue_float  (const CoglColor *color);

/**
 * cogl_color_get_alpha_float:
 * @color: a #CoglColor
 *
 * Retrieves the alpha channel of @color as a floating point
 * value between 0.0 and 1.0
 *
 * Return value: the alpha channel of the passed color
 *
 * Since: 1.0
 */
float         cogl_color_get_alpha_float (const CoglColor *color);

/**
 * cogl_color_get_red:
 * @color: a #CoglColor
 *
 * Retrieves the red channel of @color as a fixed point
 * value between 0 and %COGL_FIXED_1.
 *
 * Return value: the red channel of the passed color
 *
 * Since: 1.0
 */
CoglFixed     cogl_color_get_red         (const CoglColor *color);

/**
 * cogl_color_get_green:
 * @color: a #CoglColor
 *
 * Retrieves the green channel of @color as a fixed point
 * value between 0 and %COGL_FIXED_1.
 *
 * Return value: the green channel of the passed color
 *
 * Since: 1.0
 */
CoglFixed     cogl_color_get_green       (const CoglColor *color);

/**
 * cogl_color_get_blue:
 * @color: a #CoglColor
 *
 * Retrieves the blue channel of @color as a fixed point
 * value between 0 and %COGL_FIXED_1.
 *
 * Return value: the blue channel of the passed color
 *
 * Since: 1.0
 */
CoglFixed     cogl_color_get_blue        (const CoglColor *color);

/**
 * cogl_color_get_alpha:
 * @color: a #CoglColor
 *
 * Retrieves the alpha channel of @color as a fixed point
 * value between 0 and %COGL_FIXED_1.
 *
 * Return value: the alpha channel of the passed color
 *
 * Since: 1.0
 */
CoglFixed     cogl_color_get_alpha       (const CoglColor *color);

/**
 * cogl_set_source_color:
 * @color: a #CoglColor
 *
 * Sets the source color using normalized values for each component.
 * This color will be used for any subsequent drawing operation.
 *
 * See also cogl_set_source_color4ub() and cogl_set_source_color4x()
 * if you already have the color components.
 *
 * Since: 1.0
 */
void            cogl_set_source_color         (const CoglColor *color);

/**
 * cogl_set_source_color4ub:
 * @red: value of the red channel, between 0 and 255
 * @green: value of the green channel, between 0 and 255
 * @blue: value of the blue channel, between 0 and 255
 * @alpha: value of the alpha channel, between 0 and 255
 *
 * Sets the source color using unsigned bytes for each component. This
 * color will be used for any subsequent drawing operation.
 *
 * The value for each component is an unsigned byte in the range
 * between 0 and 255.
 *
 * Since: 1.0
 */
void cogl_set_source_color4ub (guint8 red,
                               guint8 green,
                               guint8 blue,
                               guint8 alpha);

/**
 * cogl_set_source_color4x:
 * @red: value of the red channel, between 0 and %COGL_FIXED_1
 * @green: value of the green channel, between 0 and %COGL_FIXED_1
 * @blue: value of the blue channel, between 0 and %COGL_FIXED_1
 * @alpha: value of the alpha channel, between 0 and %COGL_FIXED_1
 *
 * Sets the source color using normalized values for each component.
 * This color will be used for any subsequent drawing operation.
 *
 * The value for each component is a fixed point number in the range
 * between 0 and %COGL_FIXED_1. If the values passed in are outside that
 * range, they will be clamped.
 *
 * Since: 1.0
 */
void cogl_set_source_color4x (CoglFixed red,
                              CoglFixed green,
                              CoglFixed blue,
                              CoglFixed alpha);

G_END_DECLS

#endif /* __COGL_COLOR_H__ */
