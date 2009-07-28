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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl.h"
#include "cogl-color.h"
#include "cogl-fixed.h"

CoglColor *
cogl_color_new (void)
{
  return g_slice_new (CoglColor);
}

CoglColor *
cogl_color_copy (const CoglColor *color)
{
  if (G_LIKELY (color))
    return g_slice_dup (CoglColor, color);

  return NULL;
}

void
cogl_color_free (CoglColor *color)
{
  if (G_LIKELY (color))
    g_slice_free (CoglColor, color);
}

void
cogl_color_set_from_4ub (CoglColor *dest,
                         guint8     red,
                         guint8     green,
                         guint8     blue,
                         guint8     alpha)
{
  g_return_if_fail (dest != NULL);

  dest->red   = red;
  dest->green = green;
  dest->blue  = blue;
  dest->alpha = alpha;
}

void
cogl_color_set_from_4f (CoglColor *dest,
                        float  red,
                        float  green,
                        float  blue,
                        float  alpha)
{
  g_return_if_fail (dest != NULL);

  dest->red   =  (red * 255);
  dest->green =  (green * 255);
  dest->blue  =  (blue * 255);
  dest->alpha =  (alpha * 255);
}

unsigned char
cogl_color_get_red_byte (const CoglColor *color)
{
  return color->red;
}

float
cogl_color_get_red_float (const CoglColor *color)
{
  return (float) color->red / 255.0;
}

float
cogl_color_get_red (const CoglColor *color)
{
  return  ((float) color->red / 255.0);
}

unsigned char
cogl_color_get_green_byte (const CoglColor *color)
{
  return color->green;
}

float
cogl_color_get_green_float (const CoglColor *color)
{
  return (float) color->green / 255.0;
}

float
cogl_color_get_green (const CoglColor *color)
{
  return  ((float) color->green / 255.0);
}

unsigned char
cogl_color_get_blue_byte (const CoglColor *color)
{
  return color->blue;
}

float
cogl_color_get_blue_float (const CoglColor *color)
{
  return (float) color->blue / 255.0;
}

float
cogl_color_get_blue (const CoglColor *color)
{
  return  ((float) color->blue / 255.0);
}

unsigned char
cogl_color_get_alpha_byte (const CoglColor *color)
{
  return color->alpha;
}

float
cogl_color_get_alpha_float (const CoglColor *color)
{
  return (float) color->alpha / 255.0;
}

float
cogl_color_get_alpha (const CoglColor *color)
{
  return  ((float) color->alpha / 255.0);
}

void
cogl_color_premultiply (CoglColor *color)
{
  color->red = (color->red * color->alpha + 128) / 255;
  color->green = (color->green * color->alpha + 128) / 255;
  color->blue = (color->blue * color->alpha + 128) / 255;
}

void
cogl_set_source_color4ub (guint8 red,
                          guint8 green,
                          guint8 blue,
                          guint8 alpha)
{
  CoglColor c = { 0, };

  cogl_color_set_from_4ub (&c, red, green, blue, alpha);
  cogl_set_source_color (&c);
}

void
cogl_set_source_color4f (float red,
                         float green,
                         float blue,
                         float alpha)
{
  CoglColor c = { 0, };

  cogl_color_set_from_4f (&c, red, green, blue, alpha);
  cogl_set_source_color (&c);
}

gboolean
cogl_color_equal (gconstpointer v1, gconstpointer v2)
{
  const guint32 *c1 = v1, *c2 = v2;

  g_return_val_if_fail (v1 != NULL, FALSE);
  g_return_val_if_fail (v2 != NULL, FALSE);

  /* XXX: We don't compare the padding */
  return *c1 == *c2 ? TRUE : FALSE;
}

