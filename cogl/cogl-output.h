/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Owen Taylor <otaylor@redhat.com>
 */
#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_OUTPUT_H
#define __COGL_OUTPUT_H

#include <cogl/cogl-types.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-output
 * @short_description: information about an output device
 *
 * The #CoglOutput object holds information about an output device
 * such as a monitor or laptop display. It can be queried to find
 * out the position of the output with respect to the screen
 * coordinate system and other information such as the resolution
 * and refresh rate of the device.
 *
 * There can be any number of outputs which may overlap: the
 * same area of the screen may be displayed by multiple output
 * devices.
 *
 * XXX: though it's possible to query the position of the output
 * with respect to screen coordinates, there is currently no way
 * of finding out the position of a #CoglOnscreen in screen
 * coordinates, at least without using windowing-system specific
 * API's, so it's not easy to get the output positions relative
 * to the #CoglOnscreen.
 */

typedef struct _CoglOutput CoglOutput;
#define COGL_OUTPUT(X) ((CoglOutput *)(X))

/**
 * CoglSubpixelOrder:
 * @COGL_SUBPIXEL_ORDER_UNKNOWN: the layout of subpixel
 *   components for the device is unknown.
 * @COGL_SUBPIXEL_ORDER_NONE: the device displays colors
 *   without geometrically-separated subpixel components,
 *   or the positioning or colors of the components do not
 *   match any of the values in the enumeration.
 * @COGL_SUBPIXEL_ORDER_HORIZONTAL_RGB: the device has
 *   horizontally arranged components in the order
 *   red-green-blue from left to right.
 * @COGL_SUBPIXEL_ORDER_HORIZONTAL_BGR: the device has
 *   horizontally arranged  components in the order
 *   blue-green-red from left to right.
 * @COGL_SUBPIXEL_ORDER_VERTICAL_RGB: the device has
 *   vertically arranged components in the order
 *   red-green-blue from top to bottom.
 * @COGL_SUBPIXEL_ORDER_VERTICAL_BGR: the device has
 *   vertically arranged components in the order
 *   blue-green-red from top to bottom.
 *
 * Some output devices (such as LCD panels) display colors
 * by making each pixel consist of smaller "subpixels"
 * that each have a particular color. By using knowledge
 * of the layout of this subpixel components, it is possible
 * to create image content with higher resolution than the
 * pixel grid.
 *
 * Since: 1.14
 * Stability: unstable
 */
typedef enum {
  COGL_SUBPIXEL_ORDER_UNKNOWN,
  COGL_SUBPIXEL_ORDER_NONE,
  COGL_SUBPIXEL_ORDER_HORIZONTAL_RGB,
  COGL_SUBPIXEL_ORDER_HORIZONTAL_BGR,
  COGL_SUBPIXEL_ORDER_VERTICAL_RGB,
  COGL_SUBPIXEL_ORDER_VERTICAL_BGR
} CoglSubpixelOrder;

/**
 * cogl_is_output:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a #CoglOutput.
 *
 * Return value: %TRUE if the object references a #CoglOutput
 *   and %FALSE otherwise.
 * Since: 1.14
 * Stability: unstable
 */
CoglBool
cogl_is_output (void *object);

/**
 * cogl_output_get_x:
 * @output: a #CoglOutput
 *
 * Gets the X position of the output with respect to the coordinate
 * system of the screen.
 *
 * Return value: the X position of the output as a pixel offset
 *  from the left side of the screen coordinate space
 * Since: 1.14
 * Stability: unstable
 */
int
cogl_output_get_x (CoglOutput *output);

/**
 * cogl_output_get_y:
 * @output: a #CoglOutput
 *
 * Gets the Y position of the output with respect to the coordinate
 * system of the screen.
 *
 * Return value: the Y position of the output as a pixel offset
 *  from the top side of the screen coordinate space
 * Since: 1.14
 * Stability: unstable
 */
int
cogl_output_get_y (CoglOutput *output);

/**
 * cogl_output_get_width:
 * @output: a #CoglOutput
 *
 * Gets the width of the output in pixels.
 *
 * Return value: the width of the output in pixels
 * Since: 1.14
 * Stability: unstable
 */
int
cogl_output_get_width (CoglOutput *output);

/**
 * cogl_output_get_height:
 * @output: a #CoglOutput
 *
 * Gets the height of the output in pixels.
 *
 * Return value: the height of the output in pixels
 * Since: 1.14
 * Stability: unstable
 */
int
cogl_output_get_height (CoglOutput *output);

/**
 * cogl_output_get_mm_width:
 * @output: a #CoglOutput
 *
 * Gets the physical width of the output. In some cases (such as
 * as a projector), the value returned here might correspond to
 * nominal resolution rather than the actual physical size of the
 * output device.
 *
 * Return value: the height of the output in millimeters. A value
 *  of 0 indicates the width is unknown
 * Since: 1.14
 * Stability: unstable
 */
int
cogl_output_get_mm_width (CoglOutput *output);

/**
 * cogl_output_get_mm_height:
 * @output: a #CoglOutput
 *
 * Gets the physical height of the output. In some cases (such as
 * as a projector), the value returned here might correspond to
 * nominal resolution rather than the actual physical size of the
 * output device.
 *
 * Return value: the height of the output in millimeters. A value
 *  of 0 indicates that the height is unknown
 * Since: 1.14
 * Stability: unstable
 */
int
cogl_output_get_mm_height (CoglOutput *output);

/**
 * cogl_output_get_subpixel_order:
 * @output: a #CoglOutput
 *
 * For an output device where each pixel is made up of smaller components
 * with different colors, returns the layout of the subpixel
 * components.
 *
 * Return value: the order of subpixel components for the output device
 * Since: 1.14
 * Stability: unstable
 */
CoglSubpixelOrder
cogl_output_get_subpixel_order (CoglOutput *output);

/**
 * cogl_output_get_refresh_rate:
 * @output: a #CoglOutput
 *
 * Gets the number of times per second that the output device refreshes
 * the display contents.
 *
 * Return value: the refresh rate of the output device. A value of zero
 *  indicates that the refresh rate is unknown.
 * Since: 1.14
 * Stability: unstable
 */
float
cogl_output_get_refresh_rate (CoglOutput *output);

COGL_END_DECLS

#endif /* __COGL_OUTPUT_H */



