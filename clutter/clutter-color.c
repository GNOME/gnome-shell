/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

#include "clutter-color.h"

/**
 * clutter_color_add:
 * @src1: a #ClutterColor
 * @src2: a #ClutterColor
 * @dest: return location for the result
 *
 * Adds @src2 to @src1 and saves the resulting color
 * inside @dest.
 *
 * The alpha channel of @dest is as the maximum value
 * between the alpha channels of @src1 and @src2.
 */
void
clutter_color_add (const ClutterColor *src1,
		   const ClutterColor *src2,
		   ClutterColor       *dest)
{
  g_return_if_fail (src1 != NULL);
  g_return_if_fail (src2 != NULL);
  g_return_if_fail (dest != NULL);

  dest->red   = CLAMP (src1->red   + src2->red,   0, 255);
  dest->green = CLAMP (src1->green + src2->green, 0, 255);
  dest->blue  = CLAMP (src1->blue  + src2->blue,  0, 255);

  dest->alpha = MAX (src1->alpha, src2->alpha);
}

/**
 * clutter_color_subtract:
 * @src1: a #ClutterColor
 * @src2: a #ClutterColor
 * @dest: return location for the result
 *
 * Subtracts @src2 from @src1 and saves the resulting
 * color inside @dest.
 *
 * The alpha channel of @dest is set as the minimum value
 * between the alpha channels of @src1 and @src2.
 */
void
clutter_color_subtract (const ClutterColor *src1,
			const ClutterColor *src2,
			ClutterColor       *dest)
{
  g_return_if_fail (src1 != NULL);
  g_return_if_fail (src2 != NULL);
  g_return_if_fail (dest != NULL);

  dest->red   = CLAMP (src2->red   - src1->red,   0, 255);
  dest->green = CLAMP (src2->green - src1->green, 0, 255);
  dest->blue  = CLAMP (src2->blue  - src1->blue,  0, 255);

  dest->alpha = MIN (src1->alpha, src2->alpha);
}

/**
 * clutter_color_lighten:
 * @src: a #ClutterColor
 * @dest: return location for the lighter color
 *
 * Lightens @src by a fixed amount, and saves the changed
 * color in @dest.
 */
void
clutter_color_lighten (const ClutterColor *src,
		       ClutterColor       *dest)
{
  clutter_color_shade (src, dest, 1.3);
}

/**
 * clutter_color_darken:
 * @src: a #ClutterColor
 * @dest: return location for the darker color
 *
 * Darkens @src by a fixed amount, and saves the changed color
 * in @dest.
 */
void
clutter_color_darken (const ClutterColor *src,
		      ClutterColor       *dest)
{
  clutter_color_shade (src, dest, 0.7);
}

/**
 * clutter_color_to_hls:
 * @src: a #ClutterColor
 * @hue: return location for the hue value or %NULL
 * @luminance: return location for the luminance value or %NULL
 * @saturation: return location for the saturation value or %NULL
 *
 * Converts @src to the HLS format.
 */
void
clutter_color_to_hls (const ClutterColor *src,
		      guint8             *hue,
		      guint8             *luminance,
		      guint8             *saturation)
{
  gdouble red, green, blue;
  gdouble min, max, delta;
  gdouble h, l, s;
  
  g_return_if_fail (src != NULL);

  red = src->red / 255.0;
  green = src->green / 255.0;
  blue = src->blue / 255.0;

  if (red > green)
    {
      if (red > blue)
	max = red;
      else
	max = blue;

      if (green < blue)
	min = green;
      else
	min = blue;
    }
  else
    {
      if (green > blue)
	max = green;
      else
	max = blue;

      if (red < blue)
	min = red;
      else
	min = blue;
    }

  l = (max + min) / 2;
  s = 0;
  h = 0;

  if (max != min)
    {
      if (l <= 0.5)
	s = (max - min) / (max + min);
      else
	s = (max - min) / (2 - max - min);

      delta = max - min;
      if (red == max)
	h = (green - blue) / delta;
      else if (green == max)
	h = 2 + (blue - red) / delta;
      else if (blue == max)
	h = 4 + (red - green) / delta;

      h *= 60;
      if (h < 0.0)
	h += 360;
    }

  if (hue)
    *hue = (guint8) (h * 255);

  if (luminance)
    *luminance = (guint8) (l * 255);

  if (saturation)
    *saturation = (guint8) (s * 255);
}

/**
 * clutter_color_from_hls:
 * @dest: return location for a #ClutterColor
 * @hue: hue value (0 .. 255)
 * @luminance: luminance value (0 .. 255)
 * @saturation: saturation value (0 .. 255)
 *
 * Converts a color expressed in HLS (hue, luminance and saturation)
 * values into a #ClutterColor.
 */
void
clutter_color_from_hls (ClutterColor *dest,
			guint8        hue,
			guint8        luminance,
			guint8        saturation)
{
  gdouble h, l, s;
  gdouble m1, m2;
  
  g_return_if_fail (dest != NULL);

  l = (gdouble) luminance / 255.0;
  s = (gdouble) saturation / 255.0;

  if (l <= 0.5)
    m2 = l * (1 - s);
  else
    m2 = l + s - l * s;

  m1 = 2 * l - m2;

  if (s == 0)
    {
      dest->red = (guint8) l * 255;
      dest->green = (guint8) l * 255;
      dest->blue = (guint8) l * 255;
    }
  else
    {
      h = ((gdouble) hue / 255.0) + 120;
      while (h > 360)
	h -= 360;
      while (h < 0)
	h += 360;

      if (h < 60)
	dest->red = (guint8) (m1 + (m2 - m1) * h / 60) * 255;
      else if (h < 180)
	dest->red = (guint8) m2 * 255;
      else if (h < 240)
	dest->red = (guint8) (m1 + (m2 - m1) * (240 - h) / 60) * 255;
      else
	dest->red = (guint8) m1 * 255;

      h = (gdouble) hue / 255.0;
      while (h > 360)
	h -= 360;
      while (h < 0)
	h += 360;

      if (h < 60)
	dest->green = (guint8) (m1 + (m2 - m1) * h / 60) * 255;
      else if (h < 180)
        dest->green = (guint8) m2 * 255;
      else if (h < 240)
	dest->green = (guint8) (m1 + (m2 - m1) * (240 - h) / 60) * 255;
      else
	dest->green = (guint8) m1 * 255;

      h = ((gdouble) hue / 255.0) - 120;
      while (h > 360)
	h -= 360;
      while (h < 0)
	h += 360;

      if (h < 60)
	dest->blue = (guint8) (m1 + (m2 - m1) * h / 60) * 255;
      else if (h < 180)
	dest->blue = (guint8) m2 * 255;
      else if (h < 240)
	dest->blue = (guint8) (m1 + (m2 - m1) * (240 - h) / 60) * 255;
      else
	dest->blue = (guint8) m1 * 255;
    }
}

/**
 * clutter_color_shade:
 * @src: a #ClutterColor
 * @dest: return location for the shaded color
 * @shade: the shade factor to apply
 * 
 * Shades @src by the factor of @shade and saves the modified
 * color into @dest.
 */
void
clutter_color_shade (const ClutterColor *src,
		     ClutterColor       *dest,
		     gdouble             shade)
{
  guint8 h, l, s;
  gdouble h1, l1, s1;

  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);
  
  clutter_color_to_hls (src, &h, &l, &s);

  h1 = (gdouble) h / 255.0;
  l1 = (gdouble) l / 255.0;
  s1 = (gdouble) s / 255.0;
  
  l1 *= shade;
  if (l1 > 1.0)
    l1 = 1.0;
  else if (l1 < 0.0)
    l1 = 0.0;
  
  s1 *= shade;
  if (s1 > 1.0)
    s1 = 1.0;
  else if (s1 < 0.0)
    s1 = 0.0;

  h = (guint8) h1 * 255;
  l = (guint8) l1 * 255;
  s = (guint8) s1 * 255;

  clutter_color_from_hls (dest, h, l, s);
}

/**
 * clutter_color_to_pixel:
 * @src: a #ClutterColor
 *
 * Converts @src into a packed 32 bit integer, containing
 * all the four 8 bit channels used by #ClutterColor.
 *
 * Return value: a packed color
 */
guint32
clutter_color_to_pixel (const ClutterColor *src)
{
  g_return_val_if_fail (src != NULL, 0);
  
  return (src->alpha | src->blue << 8 | src->green << 16  | src->red << 24);
}

/**
 * clutter_color_from_pixel:
 * @dest: return location for a #ClutterColor
 * @pixel: a 32 bit packed integer containing a color
 *
 * Converts @pixel from the packed representation of a four 8 bit channel
 * color to a #ClutterColor.
 */
void
clutter_color_from_pixel (ClutterColor *dest,
			  guint32       pixel)
{
  g_return_if_fail (dest != NULL);

  dest->red = pixel >> 24;
  dest->green = (pixel >> 16) & 0xff;
  dest->blue = (pixel >> 8) & 0xff;
  dest->alpha = pixel % 0xff;
}

static ClutterColor *
clutter_color_copy (ClutterColor *color)
{
  ClutterColor *result = g_new0 (ClutterColor, 1);

  *result = *color;

  return result;
}

GType
clutter_color_get_type (void)
{
  static GType our_type = 0;
  
  if (!our_type)
    our_type = g_boxed_type_register_static ("ClutterColor",
		    			     (GBoxedCopyFunc) clutter_color_copy,
					     (GBoxedFreeFunc) g_free);
  return our_type;
}
