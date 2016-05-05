/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012 Intel Corporation
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

/**
 * SECTION:clutter-cairo
 * @Title: Cairo integration
 * @Short_Description: Functions for interoperating with Cairo
 *
 * Clutter provides some utility functions for using Cairo.
 */

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include "clutter-cairo.h"
#include "clutter-color.h"

/**
 * clutter_cairo_set_source_color:
 * @cr: a Cairo context
 * @color: a #ClutterColor
 *
 * Utility function for setting the source color of @cr using
 * a #ClutterColor. This function is the equivalent of:
 *
 * |[
 *   cairo_set_source_rgba (cr,
 *                          color->red / 255.0,
 *                          color->green / 255.0,
 *                          color->blue / 255.0,
 *                          color->alpha / 255.0);
 * ]|
 *
 * Since: 1.0
 */
void
clutter_cairo_set_source_color (cairo_t            *cr,
                                const ClutterColor *color)
{
  g_return_if_fail (cr != NULL);
  g_return_if_fail (color != NULL);

  if (color->alpha == 0xff)
    cairo_set_source_rgb (cr,
                          color->red / 255.0,
                          color->green / 255.0,
                          color->blue / 255.0);
  else
    cairo_set_source_rgba (cr,
                           color->red / 255.0,
                           color->green / 255.0,
                           color->blue / 255.0,
                           color->alpha / 255.0);
}

/**
 * clutter_cairo_clear:
 * @cr: a Cairo context
 *
 * Utility function to clear a Cairo context.
 *
 * Since: 1.12
 */
void
clutter_cairo_clear (cairo_t *cr)
{
  cairo_save (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_restore (cr);
}
