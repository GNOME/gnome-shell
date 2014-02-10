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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:clutter-color
 * @short_description: Color management and manipulation.
 *
 * #ClutterColor is a simple type for representing colors in Clutter.
 *
 * A #ClutterColor is expressed as a 4-tuple of values ranging from
 * zero to 255, one for each color channel plus one for the alpha.
 *
 * The alpha channel is fully opaque at 255 and fully transparent at 0.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include <pango/pango-attributes.h>

#include "clutter-interval.h"
#include "clutter-main.h"
#include "clutter-color.h"
#include "clutter-private.h"
#include "clutter-debug.h"

/* XXX - keep in sync with the ClutterStaticColor enumeration order */
static const ClutterColor const static_colors[] = {
  /* CGA/EGA color palette */
  { 0xff, 0xff, 0xff, 0xff },   /* white */
  { 0x00, 0x00, 0x00, 0xff },   /* black */
  { 0xff, 0x00, 0x00, 0xff },   /* red */
  { 0x80, 0x00, 0x00, 0xff },   /* dark red */
  { 0x00, 0xff, 0x00, 0xff },   /* green */
  { 0x00, 0x80, 0x00, 0xff },   /* dark green */
  { 0x00, 0x00, 0xff, 0xff },   /* blue */
  { 0x00, 0x00, 0x80, 0xff },   /* dark blue */
  { 0x00, 0xff, 0xff, 0xff },   /* cyan */
  { 0x00, 0x80, 0x80, 0xff },   /* dark cyan */
  { 0xff, 0x00, 0xff, 0xff },   /* magenta */
  { 0x80, 0x00, 0x80, 0xff },   /* dark magenta */
  { 0xff, 0xff, 0x00, 0xff },   /* yellow */
  { 0x80, 0x80, 0x00, 0xff },   /* dark yellow */
  { 0xa0, 0xa0, 0xa4, 0xff },   /* gray */
  { 0x80, 0x80, 0x80, 0xff },   /* dark gray */
  { 0xc0, 0xc0, 0xc0, 0xff },   /* light gray */

  /* Tango Icon color palette */
  { 0xed, 0xd4, 0x00, 0xff },   /* butter */
  { 0xfc, 0xe9, 0x4f, 0xff },   /* butter light */
  { 0xc4, 0xa0, 0x00, 0xff },   /* butter dark */
  { 0xf5, 0x79, 0x00, 0xff },   /* orange */
  { 0xfc, 0xaf, 0x3e, 0xff },   /* orange light */
  { 0xce, 0x5c, 0x00, 0xff },   /* orange dark */
  { 0xc1, 0x7d, 0x11, 0xff },   /* chocolate */
  { 0xe9, 0xb9, 0x6e, 0xff },   /* chocolate light */
  { 0x8f, 0x59, 0x02, 0xff },   /* chocolate dark */
  { 0x73, 0xd2, 0x16, 0xff },   /* chameleon */
  { 0x8a, 0xe2, 0x34, 0xff },   /* chameleon light */
  { 0x4e, 0x9a, 0x06, 0xff },   /* chameleon dark */
  { 0x34, 0x65, 0xa4, 0xff },   /* sky blue */
  { 0x72, 0x9f, 0xcf, 0xff },   /* sky blue light */
  { 0x20, 0x4a, 0x87, 0xff },   /* sky blue dark */
  { 0x75, 0x50, 0x7b, 0xff },   /* plum */
  { 0xad, 0x7f, 0xa8, 0xff },   /* plum light */
  { 0x5c, 0x35, 0x66, 0xff },   /* plum dark */
  { 0xcc, 0x00, 0x00, 0xff },   /* scarlet red */
  { 0xef, 0x29, 0x29, 0xff },   /* scarlet red light */
  { 0xa4, 0x00, 0x00, 0xff },   /* scarlet red dark */
  { 0xee, 0xee, 0xec, 0xff },   /* aluminium 1 */
  { 0xd3, 0xd7, 0xcf, 0xff },   /* aluminium 2 */
  { 0xba, 0xbd, 0xb6, 0xff },   /* aluminium 3 */
  { 0x88, 0x8a, 0x85, 0xff },   /* aluminium 4 */
  { 0x55, 0x57, 0x53, 0xff },   /* aluminium 5 */
  { 0x2e, 0x34, 0x36, 0xff },   /* aluminium 6 */

  /* last color */
  { 0x00, 0x00, 0x00, 0x00 }    /* transparent */
};

/**
 * clutter_color_get_static:
 * @color: the named global color
 *
 * Retrieves a static color for the given @color name
 *
 * Static colors are created by Clutter and are guaranteed to always be
 * available and valid
 *
 * Return value: a pointer to a static color; the returned pointer
 *   is owned by Clutter and it should never be modified or freed
 *
 * Since: 1.6
 */
const ClutterColor *
clutter_color_get_static (ClutterStaticColor color)
{
  g_return_val_if_fail (color >= CLUTTER_COLOR_WHITE &&
                        color <= CLUTTER_COLOR_TRANSPARENT, NULL);

  return &static_colors[color];
}

/**
 * clutter_color_add:
 * @a: a #ClutterColor
 * @b: a #ClutterColor
 * @result: (out caller-allocates): return location for the result
 *
 * Adds @a to @b and saves the resulting color inside @result.
 *
 * The alpha channel of @result is set as as the maximum value
 * between the alpha channels of @a and @b.
 */
void
clutter_color_add (const ClutterColor *a,
		   const ClutterColor *b,
		   ClutterColor       *result)
{
  g_return_if_fail (a != NULL);
  g_return_if_fail (b != NULL);
  g_return_if_fail (result != NULL);

  result->red   = CLAMP (a->red   + b->red,   0, 255);
  result->green = CLAMP (a->green + b->green, 0, 255);
  result->blue  = CLAMP (a->blue  + b->blue,  0, 255);

  result->alpha = MAX (a->alpha, b->alpha);
}

/**
 * clutter_color_subtract:
 * @a: a #ClutterColor
 * @b: a #ClutterColor
 * @result: (out caller-allocates): return location for the result
 *
 * Subtracts @b from @a and saves the resulting color inside @result.
 *
 * This function assumes that the components of @a are greater than the
 * components of @b; the result is, otherwise, undefined.
 *
 * The alpha channel of @result is set as the minimum value
 * between the alpha channels of @a and @b.
 */
void
clutter_color_subtract (const ClutterColor *a,
			const ClutterColor *b,
			ClutterColor       *result)
{
  g_return_if_fail (a != NULL);
  g_return_if_fail (b != NULL);
  g_return_if_fail (result != NULL);

  result->red   = CLAMP (a->red   - b->red,   0, 255);
  result->green = CLAMP (a->green - b->green, 0, 255);
  result->blue  = CLAMP (a->blue  - b->blue,  0, 255);

  result->alpha = MIN (a->alpha, b->alpha);
}

/**
 * clutter_color_lighten:
 * @color: a #ClutterColor
 * @result: (out caller-allocates): return location for the lighter color
 *
 * Lightens @color by a fixed amount, and saves the changed color
 * in @result.
 */
void
clutter_color_lighten (const ClutterColor *color,
		       ClutterColor       *result)
{
  clutter_color_shade (color, 1.3, result);
}

/**
 * clutter_color_darken:
 * @color: a #ClutterColor
 * @result: (out caller-allocates): return location for the darker color
 *
 * Darkens @color by a fixed amount, and saves the changed color
 * in @result.
 */
void
clutter_color_darken (const ClutterColor *color,
		      ClutterColor       *result)
{
  clutter_color_shade (color, 0.7, result);
}

/**
 * clutter_color_to_hls:
 * @color: a #ClutterColor
 * @hue: (out): return location for the hue value or %NULL
 * @luminance: (out): return location for the luminance value or %NULL
 * @saturation: (out): return location for the saturation value or %NULL
 *
 * Converts @color to the HLS format.
 *
 * The @hue value is in the 0 .. 360 range. The @luminance and
 * @saturation values are in the 0 .. 1 range.
 */
void
clutter_color_to_hls (const ClutterColor *color,
		      float              *hue,
		      float              *luminance,
		      float              *saturation)
{
  float red, green, blue;
  float min, max, delta;
  float h, l, s;
  
  g_return_if_fail (color != NULL);

  red   = color->red / 255.0;
  green = color->green / 255.0;
  blue  = color->blue / 255.0;

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
	s = (max - min) / (2.0 - max - min);

      delta = max - min;

      if (red == max)
	h = (green - blue) / delta;
      else if (green == max)
	h = 2.0 + (blue - red) / delta;
      else if (blue == max)
	h = 4.0 + (red - green) / delta;

      h *= 60;

      if (h < 0)
	h += 360.0;
    }

  if (hue)
    *hue = h;

  if (luminance)
    *luminance = l;

  if (saturation)
    *saturation = s;
}

/**
 * clutter_color_from_hls:
 * @color: (out): return location for a #ClutterColor
 * @hue: hue value, in the 0 .. 360 range
 * @luminance: luminance value, in the 0 .. 1 range
 * @saturation: saturation value, in the 0 .. 1 range
 *
 * Converts a color expressed in HLS (hue, luminance and saturation)
 * values into a #ClutterColor.
 */
void
clutter_color_from_hls (ClutterColor *color,
			float         hue,
			float         luminance,
			float         saturation)
{
  float tmp1, tmp2;
  float tmp3[3];
  float clr[3];
  int   i;

  hue /= 360.0;

  if (saturation == 0)
    {
      color->red = color->green = color->blue = (luminance * 255);

      return;
    }

  if (luminance <= 0.5)
    tmp2 = luminance * (1.0 + saturation);
  else
    tmp2 = luminance + saturation - (luminance * saturation);

  tmp1 = 2.0 * luminance - tmp2;

  tmp3[0] = hue + 1.0 / 3.0;
  tmp3[1] = hue;
  tmp3[2] = hue - 1.0 / 3.0;

  for (i = 0; i < 3; i++)
    {
      if (tmp3[i] < 0)
        tmp3[i] += 1.0;

      if (tmp3[i] > 1)
        tmp3[i] -= 1.0;

      if (6.0 * tmp3[i] < 1.0)
        clr[i] = tmp1 + (tmp2 - tmp1) * tmp3[i] * 6.0;
      else if (2.0 * tmp3[i] < 1.0)
        clr[i] = tmp2;
      else if (3.0 * tmp3[i] < 2.0)
        clr[i] = (tmp1 + (tmp2 - tmp1) * ((2.0 / 3.0) - tmp3[i]) * 6.0);
      else
        clr[i] = tmp1;
    }

  color->red   = floorf (clr[0] * 255.0 + 0.5);
  color->green = floorf (clr[1] * 255.0 + 0.5);
  color->blue  = floorf (clr[2] * 255.0 + 0.5);
}

/**
 * clutter_color_shade:
 * @color: a #ClutterColor
 * @factor: the shade factor to apply
 * @result: (out caller-allocates): return location for the shaded color
 *
 * Shades @color by @factor and saves the modified color into @result.
 */
void
clutter_color_shade (const ClutterColor *color,
                     gdouble             factor,
                     ClutterColor       *result)
{
  float h, l, s;

  g_return_if_fail (color != NULL);
  g_return_if_fail (result != NULL);
  
  clutter_color_to_hls (color, &h, &l, &s);

  l = CLAMP (l * factor, 0.0, 1.0);
  s = CLAMP (s * factor, 0.0, 1.0);
  
  clutter_color_from_hls (result, h, l, s);

  result->alpha = color->alpha;
}

/**
 * clutter_color_to_pixel:
 * @color: a #ClutterColor
 *
 * Converts @color into a packed 32 bit integer, containing
 * all the four 8 bit channels used by #ClutterColor.
 *
 * Return value: a packed color
 */
guint32
clutter_color_to_pixel (const ClutterColor *color)
{
  g_return_val_if_fail (color != NULL, 0);
  
  return (color->alpha       |
          color->blue  << 8  |
          color->green << 16 |
          color->red   << 24);
}

/**
 * clutter_color_from_pixel:
 * @color: (out caller-allocates): return location for a #ClutterColor
 * @pixel: a 32 bit packed integer containing a color
 *
 * Converts @pixel from the packed representation of a four 8 bit channel
 * color to a #ClutterColor.
 */
void
clutter_color_from_pixel (ClutterColor *color,
			  guint32       pixel)
{
  g_return_if_fail (color != NULL);

  color->red   =  pixel >> 24;
  color->green = (pixel >> 16) & 0xff;
  color->blue  = (pixel >> 8)  & 0xff;
  color->alpha =  pixel        & 0xff;
}

static inline void
skip_whitespace (gchar **str)
{
  while (g_ascii_isspace (**str))
    *str += 1;
}

static inline void
parse_rgb_value (gchar   *str,
                 guint8  *color,
                 gchar  **endp)
{
  gdouble number;
  gchar *p;

  skip_whitespace (&str);

  number = g_ascii_strtod (str, endp);

  p = *endp;

  skip_whitespace (&p);

  if (*p == '%')
    {
      *endp = (gchar *) (p + 1);

      *color = CLAMP (number / 100.0, 0.0, 1.0) * 255;
    }
  else
    *color = CLAMP (number, 0, 255);
}

static gboolean
parse_rgba (ClutterColor *color,
            gchar        *str,
            gboolean      has_alpha)
{
  skip_whitespace (&str);

  if (*str != '(')
    return FALSE;

  str += 1;

  /* red */
  parse_rgb_value (str, &color->red, &str);
  skip_whitespace (&str);
  if (*str != ',')
    return FALSE;

  str += 1;

  /* green */
  parse_rgb_value (str, &color->green, &str);
  skip_whitespace (&str);
  if (*str != ',')
    return FALSE;

  str += 1;

  /* blue */
  parse_rgb_value (str, &color->blue, &str);
  skip_whitespace (&str);

  /* alpha (optional); since the alpha channel value can only
   * be between 0 and 1 we don't use the parse_rgb_value()
   * function
   */
  if (has_alpha)
    {
      gdouble number;

      if (*str != ',')
        return FALSE;

      str += 1;

      skip_whitespace (&str);
      number = g_ascii_strtod (str, &str);

      color->alpha = CLAMP (number * 255.0, 0, 255);
    }
  else
    color->alpha = 255;

  skip_whitespace (&str);
  if (*str != ')')
    return FALSE;

  return TRUE;
}

static gboolean
parse_hsla (ClutterColor *color,
            gchar        *str,
            gboolean      has_alpha)
{
  gdouble number;
  gdouble h, l, s;

  skip_whitespace (&str);

  if (*str != '(')
    return FALSE;

  str += 1;

  /* hue */
  skip_whitespace (&str);
  /* we don't do any angle normalization here because
   * clutter_color_from_hls() will do it for us
   */
  number = g_ascii_strtod (str, &str);
  skip_whitespace (&str);
  if (*str != ',')
    return FALSE;

  h = number;

  str += 1;

  /* saturation */
  skip_whitespace (&str);
  number = g_ascii_strtod (str, &str);
  skip_whitespace (&str);
  if (*str != '%')
    return FALSE;

  str += 1;

  s = CLAMP (number / 100.0, 0.0, 1.0);
  skip_whitespace (&str);
  if (*str != ',')
    return FALSE;

  str += 1;

  /* luminance */
  skip_whitespace (&str);
  number = g_ascii_strtod (str, &str);
  skip_whitespace (&str);
  if (*str != '%')
    return FALSE;

  str += 1;

  l = CLAMP (number / 100.0, 0.0, 1.0);
  skip_whitespace (&str);

  /* alpha (optional); since the alpha channel value can only
   * be between 0 and 1 we don't use the parse_rgb_value()
   * function
   */
  if (has_alpha)
    {
      if (*str != ',')
        return FALSE;

      str += 1;

      skip_whitespace (&str);
      number = g_ascii_strtod (str, &str);

      color->alpha = CLAMP (number * 255.0, 0, 255);
    }
  else
    color->alpha = 255;

  skip_whitespace (&str);
  if (*str != ')')
    return FALSE;

  clutter_color_from_hls (color, h, l, s);

  return TRUE;
}

/**
 * clutter_color_from_string:
 * @color: (out caller-allocates): return location for a #ClutterColor
 * @str: a string specifiying a color
 *
 * Parses a string definition of a color, filling the
 * <structfield>red</structfield>, <structfield>green</structfield>, 
 * <structfield>blue</structfield> and <structfield>alpha</structfield> 
 * channels of @color.
 *
 * The @color is not allocated.
 *
 * The format of @str can be either one of:
 *
 * <itemizedlist>
 * <listitem>
 *   <para>a standard name (as taken from the X11 rgb.txt file)</para>
 * </listitem>
 * <listitem>
 *   <para>an hexadecimal value in the form: <literal>&num;rgb</literal>,
 *   <literal>&num;rrggbb</literal>, <literal>&num;rgba</literal> or
 *   <literal>&num;rrggbbaa</literal></para>
 * </listitem>
 * <listitem>
 *   <para>a RGB color in the form: <literal>rgb(r, g, b)</literal></para>
 * </listitem>
 * <listitem>
 *   <para>a RGB color in the form: <literal>rgba(r, g, b, a)</literal></para>
 * </listitem>
 * <listitem>
 *   <para>a HSL color in the form: <literal>hsl(h, s, l)</literal></para>
 * </listitem>
 * <listitem>
 *   <para>a HSL color in the form: <literal>hsla(h, s, l, a)</literal></para>
 * </listitem>
 * </itemizedlist>
 *
 * where 'r', 'g', 'b' and 'a' are (respectively) the red, green, blue color
 * intensities and the opacity. The 'h', 's' and 'l' are (respectively) the
 * hue, saturation and luminance values.
 *
 * In the rgb() and rgba() formats, the 'r', 'g', and 'b' values are either
 * integers between 0 and 255, or percentage values in the range between 0%
 * and 100%; the percentages require the '%' character. The 'a' value, if
 * specified, can only be a floating point value between 0.0 and 1.0.
 *
 * In the hls() and hlsa() formats, the 'h' value (hue) is an angle between
 * 0 and 360.0 degrees; the 'l' and 's' values (luminance and saturation) are
 * percentage values in the range between 0% and 100%. The 'a' value, if specified,
 * can only be a floating point value between 0.0 and 1.0.
 *
 * Whitespace inside the definitions is ignored; no leading whitespace
 * is allowed.
 *
 * If the alpha component is not specified then it is assumed to be set to
 * be fully opaque.
 *
 * Return value: %TRUE if parsing succeeded, and %FALSE otherwise
 *
 * Since: 1.0
 */
gboolean
clutter_color_from_string (ClutterColor *color,
                           const gchar  *str)
{
  PangoColor pango_color = { 0, };

  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (str != NULL, FALSE);

  if (strncmp (str, "rgb", 3) == 0)
    {
      gchar *s = (gchar *) str;
      gboolean res;

      if (strncmp (str, "rgba", 4) == 0)
        res = parse_rgba (color, s + 4, TRUE);
      else
        res = parse_rgba (color, s + 3, FALSE);

      return res;
    }

  if (strncmp (str, "hsl", 3) == 0)
    {
      gchar *s = (gchar *) str;
      gboolean res;

      if (strncmp (str, "hsla", 4) == 0)
        res = parse_hsla (color, s + 4, TRUE);
      else
        res = parse_hsla (color, s + 3, FALSE);

      return res;
    }

  /* if the string contains a color encoded using the hexadecimal
   * notations (#rrggbbaa or #rgba) we attempt a rough pass at
   * parsing the color ourselves, as we need the alpha channel that
   * Pango can't retrieve.
   */
  if (str[0] == '#' && str[1] != '\0')
    {
      gsize length = strlen (str + 1);
      gint32 result;

      if (sscanf (str + 1, "%x", &result) == 1)
        {
          switch (length)
            {
            case 8: /* rrggbbaa */
              color->red   = (result >> 24) & 0xff;
              color->green = (result >> 16) & 0xff;
              color->blue  = (result >>  8) & 0xff;

              color->alpha = result & 0xff;

              return TRUE;

            case 6: /* #rrggbb */
              color->red   = (result >> 16) & 0xff;
              color->green = (result >>  8) & 0xff;
              color->blue  = result & 0xff;

              color->alpha = 0xff;

              return TRUE;

            case 4: /* #rgba */
              color->red   = ((result >> 12) & 0xf);
              color->green = ((result >>  8) & 0xf);
              color->blue  = ((result >>  4) & 0xf);
              color->alpha = result & 0xf;

              color->red   = (color->red   << 4) | color->red;
              color->green = (color->green << 4) | color->green;
              color->blue  = (color->blue  << 4) | color->blue;
              color->alpha = (color->alpha << 4) | color->alpha;

              return TRUE;

            case 3: /* #rgb */
              color->red   = ((result >>  8) & 0xf);
              color->green = ((result >>  4) & 0xf);
              color->blue  = result & 0xf;

              color->red   = (color->red   << 4) | color->red;
              color->green = (color->green << 4) | color->green;
              color->blue  = (color->blue  << 4) | color->blue;

              color->alpha = 0xff;

              return TRUE;

            default:
              return FALSE;
            }
        }
    }

  /* fall back to pango for X11-style named colors; see:
   *
   *   http://en.wikipedia.org/wiki/X11_color_names
   *
   * for a list. at some point we might even ship with our own list generated
   * from X11/rgb.txt, like we generate the key symbols.
   */
  if (pango_color_parse (&pango_color, str))
    {
      color->red   = pango_color.red;
      color->green = pango_color.green;
      color->blue  = pango_color.blue;

      color->alpha = 0xff;

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
 * hexadecimal digits representing the red, green, blue and alpha components
 * respectively.
 *
 * Return value: (transfer full): a newly-allocated text string
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
 * @v1: (type Clutter.Color): a #ClutterColor
 * @v2: (type Clutter.Color): a #ClutterColor
 *
 * Compares two #ClutterColor<!-- -->s and checks if they are the same.
 *
 * This function can be passed to g_hash_table_new() as the @key_equal_func
 * parameter, when using #ClutterColor<!-- -->s as keys in a #GHashTable.
 *
 * Return value: %TRUE if the two colors are the same.
 *
 * Since: 0.2
 */
gboolean
clutter_color_equal (gconstpointer v1,
                     gconstpointer v2)
{
  const ClutterColor *a, *b;

  g_return_val_if_fail (v1 != NULL, FALSE);
  g_return_val_if_fail (v2 != NULL, FALSE);

  if (v1 == v2)
    return TRUE;

  a = v1;
  b = v2;

  return (a->red   == b->red   &&
          a->green == b->green &&
          a->blue  == b->blue  &&
          a->alpha == b->alpha);
}

/**
 * clutter_color_hash:
 * @v: (type Clutter.Color): a #ClutterColor
 *
 * Converts a #ClutterColor to a hash value.
 *
 * This function can be passed to g_hash_table_new() as the @hash_func
 * parameter, when using #ClutterColor<!-- -->s as keys in a #GHashTable.
 *
 * Return value: a hash value corresponding to the color
 *
 * Since: 1.0
 */
guint
clutter_color_hash (gconstpointer v)
{
  return clutter_color_to_pixel ((const ClutterColor *) v);
}

/**
 * clutter_color_interpolate:
 * @initial: the initial #ClutterColor
 * @final: the final #ClutterColor
 * @progress: the interpolation progress
 * @result: (out): return location for the interpolation
 *
 * Interpolates between @initial and @final #ClutterColor<!-- -->s
 * using @progress
 *
 * Since: 1.6
 */
void
clutter_color_interpolate (const ClutterColor *initial,
                           const ClutterColor *final,
                           gdouble             progress,
                           ClutterColor       *result)
{
  g_return_if_fail (initial != NULL);
  g_return_if_fail (final != NULL);
  g_return_if_fail (result != NULL);

  result->red   = initial->red   + (final->red   - initial->red)   * progress;
  result->green = initial->green + (final->green - initial->green) * progress;
  result->blue  = initial->blue  + (final->blue  - initial->blue)  * progress;
  result->alpha = initial->alpha + (final->alpha - initial->alpha) * progress;
}

static gboolean
clutter_color_progress (const GValue *a,
                        const GValue *b,
                        gdouble       progress,
                        GValue       *retval)
{
  const ClutterColor *a_color = clutter_value_get_color (a);
  const ClutterColor *b_color = clutter_value_get_color (b);
  ClutterColor res = { 0, };

  clutter_color_interpolate (a_color, b_color, progress, &res);
  clutter_value_set_color (retval, &res);

  return TRUE;
}

/**
 * clutter_color_copy:
 * @color: a #ClutterColor
 *
 * Makes a copy of the color structure.  The result must be
 * freed using clutter_color_free().
 *
 * Return value: (transfer full): an allocated copy of @color.
 *
 * Since: 0.2
 */
ClutterColor *
clutter_color_copy (const ClutterColor *color)
{
  if (G_LIKELY (color != NULL))
    return g_slice_dup (ClutterColor, color);

  return NULL;
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
  if (G_LIKELY (color != NULL))
    g_slice_free (ClutterColor, color);
}

/**
 * clutter_color_new:
 * @red: red component of the color, between 0 and 255
 * @green: green component of the color, between 0 and 255
 * @blue: blue component of the color, between 0 and 255
 * @alpha: alpha component of the color, between 0 and 255
 *
 * Creates a new #ClutterColor with the given values.
 *
 * This function is the equivalent of:
 *
 * |[
 *   clutter_color_init (clutter_color_alloc (), red, green, blue, alpha);
 * ]|
 *
 * Return value: (transfer full): the newly allocated color.
 *   Use clutter_color_free() when done
 *
 * Since: 0.8.4
 */
ClutterColor *
clutter_color_new (guint8 red,
                   guint8 green,
                   guint8 blue,
                   guint8 alpha)
{
  return clutter_color_init (clutter_color_alloc (),
                             red,
                             green,
                             blue,
                             alpha);
}

/**
 * clutter_color_alloc: (constructor)
 *
 * Allocates a new, transparent black #ClutterColor.
 *
 * Return value: (transfer full): the newly allocated #ClutterColor; use
 *   clutter_color_free() to free its resources
 *
 * Since: 1.12
 */
ClutterColor *
clutter_color_alloc (void)
{
  return g_slice_new0 (ClutterColor);
}

/**
 * clutter_color_init:
 * @color: a #ClutterColor
 * @red: red component of the color, between 0 and 255
 * @green: green component of the color, between 0 and 255
 * @blue: blue component of the color, between 0 and 255
 * @alpha: alpha component of the color, between 0 and 255
 *
 * Initializes @color with the given values.
 *
 * Return value: (transfer none): the initialized #ClutterColor
 *
 * Since: 1.12
 */
ClutterColor *
clutter_color_init (ClutterColor *color,
                    guint8        red,
                    guint8        green,
                    guint8        blue,
                    guint8        alpha)
{
  g_return_val_if_fail (color != NULL, NULL);

  color->red = red;
  color->green = green;
  color->blue = blue;
  color->alpha = alpha;

  return color;
}

static void
clutter_value_transform_color_string (const GValue *src,
                                      GValue       *dest)
{
  const ClutterColor *color = g_value_get_boxed (src);

  if (color)
    {
      gchar *string = clutter_color_to_string (color);

      g_value_take_string (dest, string);
    }
  else
    g_value_set_string (dest, NULL);
}

static void
clutter_value_transform_string_color (const GValue *src,
                                      GValue       *dest)
{
  const char *str = g_value_get_string (src);

  if (str)
    {
      ClutterColor color = { 0, };

      clutter_color_from_string (&color, str);

      clutter_value_set_color (dest, &color);
    }
  else
    clutter_value_set_color (dest, NULL);
}

G_DEFINE_BOXED_TYPE_WITH_CODE (ClutterColor, clutter_color,
                               clutter_color_copy,
                               clutter_color_free,
                               CLUTTER_REGISTER_VALUE_TRANSFORM_TO (G_TYPE_STRING, clutter_value_transform_color_string)
                               CLUTTER_REGISTER_VALUE_TRANSFORM_FROM (G_TYPE_STRING, clutter_value_transform_string_color)
                               CLUTTER_REGISTER_INTERVAL_PROGRESS (clutter_color_progress));

/**
 * clutter_value_set_color:
 * @value: a #GValue initialized to #CLUTTER_TYPE_COLOR
 * @color: the color to set
 *
 * Sets @value to @color.
 *
 * Since: 0.8.4
 */
void
clutter_value_set_color (GValue             *value,
                         const ClutterColor *color)
{
  g_return_if_fail (CLUTTER_VALUE_HOLDS_COLOR (value));

  g_value_set_boxed (value, color);
}

/**
 * clutter_value_get_color:
 * @value: a #GValue initialized to #CLUTTER_TYPE_COLOR
 *
 * Gets the #ClutterColor contained in @value.
 *
 * Return value: (transfer none): the color inside the passed #GValue
 *
 * Since: 0.8.4
 */
const ClutterColor *
clutter_value_get_color (const GValue *value)
{
  g_return_val_if_fail (CLUTTER_VALUE_HOLDS_COLOR (value), NULL);

  return g_value_get_boxed (value);
}

static void
param_color_init (GParamSpec *pspec)
{
  ClutterParamSpecColor *cspec = CLUTTER_PARAM_SPEC_COLOR (pspec);

  cspec->default_value = NULL;
}

static void
param_color_finalize (GParamSpec *pspec)
{
  ClutterParamSpecColor *cspec = CLUTTER_PARAM_SPEC_COLOR (pspec);

  clutter_color_free (cspec->default_value);
}

static void
param_color_set_default (GParamSpec *pspec,
                         GValue     *value)
{
  const ClutterColor *default_value =
    CLUTTER_PARAM_SPEC_COLOR (pspec)->default_value;
  clutter_value_set_color (value, default_value);
}

static gint
param_color_values_cmp (GParamSpec   *pspec,
                        const GValue *value1,
                        const GValue *value2)
{
  const ClutterColor *color1 = g_value_get_boxed (value1);
  const ClutterColor *color2 = g_value_get_boxed (value2);
  int pixel1, pixel2;

  if (color1 == NULL)
    return color2 == NULL ? 0 : -1;

  pixel1 = clutter_color_to_pixel (color1);
  pixel2 = clutter_color_to_pixel (color2);

  if (pixel1 < pixel2)
    return -1;
  else if (pixel1 == pixel2)
    return 0;
  else
    return 1;
}

GType
clutter_param_color_get_type (void)
{
  static GType pspec_type = 0;

  if (G_UNLIKELY (pspec_type == 0))
    {
      const GParamSpecTypeInfo pspec_info = {
        sizeof (ClutterParamSpecColor),
        16,
        param_color_init,
        CLUTTER_TYPE_COLOR,
        param_color_finalize,
        param_color_set_default,
        NULL,
        param_color_values_cmp,
      };

      pspec_type = g_param_type_register_static (I_("ClutterParamSpecColor"),
                                                 &pspec_info);
    }

  return pspec_type;
}

/**
 * clutter_param_spec_color: (skip)
 * @name: name of the property
 * @nick: short name
 * @blurb: description (can be translatable)
 * @default_value: default value
 * @flags: flags for the param spec
 *
 * Creates a #GParamSpec for properties using #ClutterColor.
 *
 * Return value: the newly created #GParamSpec
 *
 * Since: 0.8.4
 */
GParamSpec *
clutter_param_spec_color (const gchar        *name,
                          const gchar        *nick,
                          const gchar        *blurb,
                          const ClutterColor *default_value,
                          GParamFlags         flags)
{
  ClutterParamSpecColor *cspec;

  cspec = g_param_spec_internal (CLUTTER_TYPE_PARAM_COLOR,
                                 name, nick, blurb, flags);

  cspec->default_value = clutter_color_copy (default_value);

  return G_PARAM_SPEC (cspec);
}
