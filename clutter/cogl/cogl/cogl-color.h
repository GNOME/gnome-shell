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

/**
 * SECTION:cogl-color
 * @short_description: A generic color definition
 *
 * #CoglColor is a simple structure holding the definition of a color such
 * that it can be efficiently used by GL
 *
 * Since: 1.0
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_COLOR_H__
#define __COGL_COLOR_H__

#include <cogl/cogl-types.h>

G_BEGIN_DECLS

/**
 * cogl_color_new:
 *
 * Creates a new (empty) color
 *
 * Return value: a newly-allocated #CoglColor. Use cogl_color_free()
 *   to free the allocated resources
 *
 * Since: 1.0
 */
CoglColor *cogl_color_new  (void);

/**
 * cogl_color_copy:
 * @color: the color to copy
 *
 * Creates a copy of @color
 *
 * Return value: a newly-allocated #CoglColor. Use cogl_color_free()
 *   to free the allocate resources
 *
 * Since: 1.0
 */
CoglColor *cogl_color_copy (const CoglColor *color);

/**
 * cogl_color_free:
 * @color: the color to free
 *
 * Frees the resources allocated by cogl_color_new() and cogl_color_copy()
 *
 * Since: 1.0
 */
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
 * cogl_color_set_from_4f:
 * @dest: return location for a #CoglColor
 * @red: value of the red channel, between 0 and %1.0
 * @green: value of the green channel, between 0 and %1.0
 * @blue: value of the blue channel, between 0 and %1.0
 * @alpha: value of the alpha channel, between 0 and %1.0
 *
 * Sets the values of the passed channels into a #CoglColor
 *
 * Since: 1.0
 */
void cogl_color_set_from_4f (CoglColor *dest,
                             float  red,
                             float  green,
                             float  blue,
                             float  alpha);

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
 * value between 0 and %1.0.
 *
 * Return value: the red channel of the passed color
 *
 * Since: 1.0
 */
float     cogl_color_get_red         (const CoglColor *color);

/**
 * cogl_color_get_green:
 * @color: a #CoglColor
 *
 * Retrieves the green channel of @color as a fixed point
 * value between 0 and %1.0.
 *
 * Return value: the green channel of the passed color
 *
 * Since: 1.0
 */
float     cogl_color_get_green       (const CoglColor *color);

/**
 * cogl_color_get_blue:
 * @color: a #CoglColor
 *
 * Retrieves the blue channel of @color as a fixed point
 * value between 0 and %1.0.
 *
 * Return value: the blue channel of the passed color
 *
 * Since: 1.0
 */
float     cogl_color_get_blue        (const CoglColor *color);

/**
 * cogl_color_get_alpha:
 * @color: a #CoglColor
 *
 * Retrieves the alpha channel of @color as a fixed point
 * value between 0 and %1.0.
 *
 * Return value: the alpha channel of the passed color
 *
 * Since: 1.0
 */
float     cogl_color_get_alpha       (const CoglColor *color);

/**
 * cogl_color_premultiply:
 * @color: the color to premultiply
 *
 * Converts a non-premultiplied color to a pre-multiplied color. For
 * example, semi-transparent red is (1.0, 0, 0, 0.5) when non-premultiplied
 * and (0.5, 0, 0, 0.5) when premultiplied.
 *
 * Since: 1.0
 */
void cogl_color_premultiply (CoglColor *color);

/**
 * cogl_color_equal:
 * @v1: a #CoglColor
 * @v2: a #CoglColor
 *
 * Compares two #CoglColor<!-- -->s and checks if they are the same.
 *
 * This function can be passed to g_hash_table_new() as the @key_equal_func
 * parameter, when using #CoglColor<!-- -->s as keys in a #GHashTable.
 *
 * Return value: %TRUE if the two colors are the same.
 *
 * Since: 1.0
 */
gboolean cogl_color_equal (gconstpointer v1,
                           gconstpointer v2);

G_END_DECLS

#endif /* __COGL_COLOR_H__ */
