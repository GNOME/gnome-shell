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

/**
 * SECTION:clutter-color
 * @short_description: Color management and manipulation.
 *
 * #ClutterColor is a simple type for representing colors.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-main.h"
#include "clutter-color.h"
#include "clutter-private.h"
#include "clutter-debug.h"

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
 * color inside @dest. This function assumes that the components
 * of @src1 are greater than the components of @src2; the result is,
 * otherwise, undefined.
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

  dest->red   = CLAMP (src1->red   - src2->red,   0, 255);
  dest->green = CLAMP (src1->green - src2->green, 0, 255);
  dest->blue  = CLAMP (src1->blue  - src2->blue,  0, 255);

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
  /* 0x14ccd is ClutterFixed for 1.3 */
  clutter_color_shadex (src, dest, 0x14ccd);
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
  /* 0xb333 is ClutterFixed for 0.7 */
  clutter_color_shadex (src, dest, 0xb333);
}

/**
 * clutter_color_to_hlsx:
 * @src: a #ClutterColor
 * @hue: return location for the hue value or %NULL
 * @luminance: return location for the luminance value or %NULL
 * @saturation: return location for the saturation value or %NULL
 *
 * Converts @src to the HLS format. Returned hue is in degrees (0 .. 360),
 * luminance and saturation from interval <0 .. 1>.
 */
void
clutter_color_to_hlsx (const ClutterColor *src,
		       ClutterFixed       *hue,
		       ClutterFixed       *luminance,
		       ClutterFixed       *saturation)
{
  ClutterFixed red, green, blue;
  ClutterFixed min, max, delta;
  ClutterFixed h, l, s;
  
  g_return_if_fail (src != NULL);

  red   = CLUTTER_INT_TO_FIXED (src->red)   / 255;
  green = CLUTTER_INT_TO_FIXED (src->green) / 255;
  blue  = CLUTTER_INT_TO_FIXED (src->blue)  / 255;

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
      if (l <= CFX_ONE/2)
	s = CFX_DIV ((max - min), (max + min));
      else
	s = CFX_DIV ((max - min), (CLUTTER_INT_TO_FIXED (2) - max - min));

      delta = max - min;
      if (red == max)
	h = CFX_DIV ((green - blue), delta);
      else if (green == max)
	h = CLUTTER_INT_TO_FIXED (2) + CFX_DIV ((blue - red), delta);
      else if (blue == max)
	h = CLUTTER_INT_TO_FIXED (4) + CFX_DIV ((red - green), delta);

      h *= 60;
      if (h < 0)
	h += CLUTTER_INT_TO_FIXED (360);
    }

  if (hue)
    *hue = h;

  if (luminance)
    *luminance = l;

  if (saturation)
    *saturation = s;
}

/**
 * clutter_color_from_hlsx:
 * @dest: return location for a #ClutterColor
 * @hue: hue value (0 .. 360)
 * @luminance: luminance value (0 .. 1)
 * @saturation: saturation value (0 .. 1)
 *
 * Converts a color expressed in HLS (hue, luminance and saturation)
 * values into a #ClutterColor.
 */

void
clutter_color_from_hlsx (ClutterColor *dest,
			 ClutterFixed   hue,
			 ClutterFixed   luminance,
			 ClutterFixed   saturation)
{
  ClutterFixed h, l, s;
  ClutterFixed m1, m2;
  
  g_return_if_fail (dest != NULL);

  l = luminance;
  s = saturation;

  if (l <= CFX_ONE/2)
    m2 = CFX_MUL (l, (CFX_ONE + s));
  else
    m2 = l + s - CFX_MUL (l,s);

  m1 = 2 * l - m2;

  if (s == 0)
    {
      dest->red   = (guint8) CFX_INT (l * 255);
      dest->green = (guint8) CFX_INT (l * 255);
      dest->blue  = (guint8) CFX_INT (l * 255);
    }
  else
    {
      h = hue + CFX_120;
      while (h > CFX_360)
	h -= CFX_360;
      while (h < 0)
	h += CFX_360;

      if (h < CFX_60)
	dest->red = (guint8) CFX_INT((m1 + CFX_MUL((m2-m1), h) / 60) * 255);
      else if (h < CFX_180)
	dest->red = (guint8) CFX_INT (m2 * 255);
      else if (h < CFX_240)
	dest->red = (guint8)CFX_INT((m1+CFX_MUL((m2-m1),(CFX_240-h))/60)*255);
      else
	dest->red = (guint8) CFX_INT (m1 * 255);

      h = hue;
      while (h > CFX_360)
	h -= CFX_360;
      while (h < 0)
	h += CFX_360;

      if (h < CFX_60)
	dest->green = (guint8)CFX_INT((m1 + CFX_MUL((m2 - m1), h) / 60) * 255);
      else if (h < CFX_180)
        dest->green = (guint8) CFX_INT (m2 * 255);
      else if (h < CFX_240)
	dest->green =
	    (guint8) CFX_INT((m1 + CFX_MUL ((m2-m1), (CFX_240-h)) / 60) * 255);
      else
	dest->green = (guint8) CFX_INT (m1 * 255);

      h = hue - CFX_120;
      while (h > CFX_360)
	h -= CFX_360;
      while (h < 0)
	h += CFX_360;

      if (h < CFX_60)
	dest->blue = (guint8) CFX_INT ((m1 + CFX_MUL ((m2-m1), h) / 60) * 255);
      else if (h < CFX_180)
	dest->blue = (guint8) CFX_INT (m2 * 255);
      else if (h < CFX_240)
	dest->blue = (guint8)CFX_INT((m1+CFX_MUL((m2-m1),(CFX_240-h))/60)*255);
      else
	dest->blue = (guint8) CFX_INT(m1 * 255);
    }
}

/**
 * clutter_color_to_hls:
 * @src: a #ClutterColor
 * @hue: return location for the hue value or %NULL
 * @luminance: return location for the luminance value or %NULL
 * @saturation: return location for the saturation value or %NULL
 *
 * Converts @src to the HLS format. Returned HLS values are from interval
 * 0 .. 255.
 */
void
clutter_color_to_hls (const ClutterColor *src,
		      guint8             *hue,
		      guint8             *luminance,
		      guint8             *saturation)
{
  ClutterFixed h, l, s;
  
  clutter_color_to_hlsx (src, &h, &l, &s);
  
  if (hue)
    *hue = (guint8) CFX_INT (h * 255) / 360;

  if (luminance)
    *luminance = (guint8) CFX_INT (l * 255);

  if (saturation)
    *saturation = (guint8) CFX_INT (s * 255);
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
  ClutterFixed h, l, s;

  h = CLUTTER_INT_TO_FIXED (hue * 360) / 255;
  l = CLUTTER_INT_TO_FIXED (luminance)  / 255;
  s = CLUTTER_INT_TO_FIXED (saturation) / 255;

  clutter_color_from_hlsx (dest, h, l, s);
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
  clutter_color_shadex (src, dest, CLUTTER_FLOAT_TO_FIXED (shade));
}

/**
 * clutter_color_shadex:
 * @src: a #ClutterColor
 * @dest: return location for the shaded color
 * @shade: #ClutterFixed the shade factor to apply
 * 
 * Fixed point version of clutter_color_shade().
 *
 * Shades @src by the factor of @shade and saves the modified
 * color into @dest.
 *
 * Since: 0.2
 */
void
clutter_color_shadex (const ClutterColor *src,
		      ClutterColor       *dest,
		      ClutterFixed        shade)
{
  ClutterFixed h, l, s;

  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);
  
  clutter_color_to_hlsx (src, &h, &l, &s);

  l = CFX_MUL (l, shade);
  if (l > CFX_ONE)
    l = CFX_ONE;
  else if (l < 0)
    l = 0;

  s = CFX_MUL (s, shade);
  if (s > CFX_ONE)
    s = CFX_ONE;
  else if (s < 0)
    s = 0;
  
  clutter_color_from_hlsx (dest, h, l, s);
  dest->alpha = src->alpha;
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
  dest->alpha = pixel & 0xff;
}

/**
 * clutter_color_parse:
 * @color: a string specifiying a color (named color or #RRGGBBAA)
 * @dest: return location for a #ClutterColor
 *
 * Parses a string definition of a color, filling the
 * <structfield>red</structfield>, <structfield>green</structfield>, 
 * <structfield>blue</structfield> and <structfield>alpha</structfield> 
 * channels of @dest. If alpha is not specified it will be set full opaque.
 * The color in @dest is not allocated.
 *
 * The color may be defined by any of the formats understood by
 * <function>pango_color_parse</function>; these include literal color
 * names, like <literal>Red</literal> or <literal>DarkSlateGray</literal>,
 * or hexadecimal specifications like <literal>&num;3050b2</literal> or
 * <literal>&num;333</literal>.
 *
 * Return value: %TRUE if parsing succeeded.
 *
 * Since: 0.2
 */
gboolean
clutter_color_parse (const gchar  *color,
                     ClutterColor *dest)
{
  PangoColor pango_color;

  /* parse ourselves to get alpha */
  if (color[0] == '#')
    {
      gint32 result;

      if (sscanf (color + 1, "%x", &result))
	{
	  if (strlen (color) == 9)
	    {
	      dest->red   = result >> 24 & 0xff;
	      dest->green = (result >> 16) & 0xff;
	      dest->blue  = (result >> 8) & 0xff;
	      dest->alpha = result & 0xff;

	      return TRUE;
	    }
	  else if (strlen (color) == 7)
	    {
	      dest->red   = (result >> 16) & 0xff;
	      dest->green = (result >> 8) & 0xff;
	      dest->blue  = result & 0xff;
	      dest->alpha = 0xff;

	      return TRUE;
	    }
	}
    }
  
  /* Fall back to pango for named colors - note pango does not handle alpha */
  if (pango_color_parse (&pango_color, color))
    {
      dest->red   = pango_color.red;
      dest->green = pango_color.green;
      dest->blue  = pango_color.blue;
      dest->alpha = 0xff;

      return TRUE;
    }

  return FALSE;
}

/**
 * clutter_color_to_string:
 * @color: a #ClutterColor
 *
 * Returns a textual specification of @color in the hexadecimal form
 * <literal>&num;rrggbbaa</literal>, where <literal>r</literal>,
 * <literal>g</literal>, <literal>b</literal> and <literal>a</literal> are
 * hex digits representing the red, green, blue and alpha components
 * respectively.
 *
 * Return value: a newly-allocated text string
 *
 * Since: 0.2
 */
gchar *
clutter_color_to_string (const ClutterColor *color)
{
  g_return_val_if_fail (color != NULL, NULL);

  return g_strdup_printf ("#%02x%02x%02x%02x",
                          color->red,
                          color->green,
                          color->blue,
                          color->alpha);
}

/**
 * clutter_color_equal:
 * @a: a #ClutterColor
 * @b: a #ClutterColor
 *
 * Compares two #ClutterColor<!-- -->s and checks if they are the same.
 *
 * Return value: %TRUE if the two colors are the same.
 *
 * Since: 0.2
 */
gboolean
clutter_color_equal (const ClutterColor *a,
                     const ClutterColor *b)
{
  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (b != NULL, FALSE);

  if (a == b)
    return TRUE;

  return (a->red == b->red &&
          a->green == b->green &&
          a->blue == b->blue &&
          a->alpha == b->alpha);
}

/**
 * clutter_color_copy:
 * @color: a #ClutterColor
 *
 * Makes a copy of the color structure.  The result must be
 * freed using clutter_color_free().
 *
 * Return value: an allocated copy of @color.
 *
 * Since: 0.2
 */
ClutterColor *
clutter_color_copy (const ClutterColor *color)
{
  ClutterColor *result;
  
  g_return_val_if_fail (color != NULL, NULL);

  result = g_slice_new (ClutterColor);
  *result = *color;

  return result;
}

/**
 * clutter_color_free:
 * @color: a #ClutterColor
 *
 * Frees a color structure created with clutter_color_copy().
 *
 * Since: 0.2
 */
void
clutter_color_free (ClutterColor *color)
{
  g_return_if_fail (color != NULL);

  g_slice_free (ClutterColor, color);
}

GType
clutter_color_get_type (void)
{
  static GType our_type = 0;
  
  if (!our_type)
    our_type = g_boxed_type_register_static ("ClutterColor",
		    			     (GBoxedCopyFunc) clutter_color_copy,
					     (GBoxedFreeFunc) clutter_color_free);
  return our_type;
}
