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
 * #ClutterColor is a simple type for representing colors in Clutter.
 *
 * A #ClutterColor is expressed as a 4-tuple of values ranging from
 * zero to 255, one for each color channel plus one for the alpha.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pango/pango-attributes.h>
#include <gobject/gvaluecollector.h>

#include "clutter-main.h"
#include "clutter-color.h"
#include "clutter-private.h"
#include "clutter-debug.h"

/**
 * clutter_color_add:
 * @a: a #ClutterColor
 * @b: a #ClutterColor
 * @result: (out): return location for the result
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
 * @result: (out): return location for the result
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
 * @result: (out): return location for the lighter color
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
 * @result: (out): return location for the darker color
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

/*
 * clutter_color_to_hlsx:
 * @color: a #ClutterColor
 * @hue: return location for the hue value or %NULL
 * @luminance: return location for the luminance value or %NULL
 * @saturation: return location for the saturation value or %NULL
 *
 * Converts @color to the HLS format. Returned hue is in degrees (0 .. 360),
 * luminance and saturation from interval <0 .. 1>.
 *
 * The implementation is in fixed point because we don't particularly
 * care about precision. It can be moved to floating point at any later
 * date.
 */
static void
clutter_color_to_hlsx (const ClutterColor *color,
		       CoglFixed          *hue,
		       CoglFixed          *luminance,
		       CoglFixed          *saturation)
{
  CoglFixed red, green, blue;
  CoglFixed min, max, delta;
  CoglFixed h, l, s;
  
  g_return_if_fail (color != NULL);

  red   = COGL_FIXED_FAST_DIV (color->red,   COGL_FIXED_255);
  green = COGL_FIXED_FAST_DIV (color->green, COGL_FIXED_255);
  blue  = COGL_FIXED_FAST_DIV (color->blue,  COGL_FIXED_255);

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
      if (l <= COGL_FIXED_0_5)
	s = COGL_FIXED_DIV ((max - min), (max + min));
      else
	s = COGL_FIXED_DIV ((max - min),
                            (COGL_FIXED_FROM_INT (2) - max - min));

      delta = max - min;

      if (red == max)
	h = COGL_FIXED_DIV ((green - blue), delta);
      else if (green == max)
	h = COGL_FIXED_FROM_INT (2) + COGL_FIXED_DIV ((blue - red), delta);
      else if (blue == max)
	h = COGL_FIXED_FROM_INT (4) + COGL_FIXED_DIV ((red - green), delta);

      h *= 60;

      if (h < 0)
	h += COGL_FIXED_360;
    }

  if (hue)
    *hue = h;

  if (luminance)
    *luminance = l;

  if (saturation)
    *saturation = s;
}

/*
 * clutter_color_from_hlsx:
 * @dest: (out): return location for a #ClutterColor
 * @hue: hue value (0 .. 360)
 * @luminance: luminance value (0 .. 1)
 * @saturation: saturation value (0 .. 1)
 *
 * Converts a color expressed in HLS (hue, luminance and saturation)
 * values into a #ClutterColor.
 */
void
clutter_color_from_hlsx (ClutterColor *color,
			 CoglFixed     hue,
			 CoglFixed     luminance,
			 CoglFixed     saturation)
{
  CoglFixed h, l, s;
  CoglFixed m1, m2;
  
  g_return_if_fail (color != NULL);

  l = luminance;
  s = saturation;

  if (l <= COGL_FIXED_0_5)
    m2 = COGL_FIXED_MUL (l, (COGL_FIXED_1 + s));
  else
    m2 = l + s - COGL_FIXED_MUL (l, s);

  m1 = 2 * l - m2;

  if (s == 0)
    {
      color->red   = (guint8) (COGL_FIXED_TO_INT (l) * 255);
      color->green = (guint8) (COGL_FIXED_TO_INT (l) * 255);
      color->blue  = (guint8) (COGL_FIXED_TO_INT (l) * 255);
    }
  else
    {
      h = hue + COGL_FIXED_120;

      while (h > COGL_FIXED_360)
	h -= COGL_FIXED_360;

      while (h < 0)
	h += COGL_FIXED_360;

      if (h < COGL_FIXED_60)
        {
          CoglFixed tmp;

          tmp = (m1 + COGL_FIXED_MUL_DIV ((m2 - m1), h, COGL_FIXED_60));
          color->red = (guint8) (COGL_FIXED_TO_INT (tmp) * 255);
        }
      else if (h < COGL_FIXED_180)
	color->red = (guint8) (COGL_FIXED_TO_INT (m2) * 255);
      else if (h < COGL_FIXED_240)
        {
          CoglFixed tmp;

          tmp = (m1 + COGL_FIXED_MUL_DIV ((m2 - m1),
                                          (COGL_FIXED_240 - h),
                                          COGL_FIXED_60));
          color->red = (guint8) (COGL_FIXED_TO_INT (tmp) * 255);
        }
      else
	color->red = (guint8) (COGL_FIXED_TO_INT (m1) * 255);

      h = hue;
      while (h > COGL_FIXED_360)
	h -= COGL_FIXED_360;
      while (h < 0)
	h += COGL_FIXED_360;

      if (h < COGL_FIXED_60)
        {
          CoglFixed tmp;

          tmp = (m1 + COGL_FIXED_MUL_DIV ((m2 - m1), h, COGL_FIXED_60));
          color->green = (guint8) (COGL_FIXED_TO_INT (tmp) * 255);
        }
      else if (h < COGL_FIXED_180)
        color->green = (guint8) (COGL_FIXED_TO_INT (m2) * 255);
      else if (h < COGL_FIXED_240)
        {
          CoglFixed tmp;

          tmp = (m1 + COGL_FIXED_MUL_DIV ((m2 - m1),
                                          (COGL_FIXED_240 - h),
                                          COGL_FIXED_60));
          color->green = (guint8) (COGL_FIXED_TO_INT (tmp) * 255);
        }
      else
	color->green = (guint8) (COGL_FIXED_TO_INT (m1) * 255);

      h = hue - COGL_FIXED_120;

      while (h > COGL_FIXED_360)
	h -= COGL_FIXED_360;

      while (h < 0)
	h += COGL_FIXED_360;

      if (h < COGL_FIXED_60)
        {
          CoglFixed tmp;

          tmp = (m1 + COGL_FIXED_MUL_DIV ((m2 - m1), h, COGL_FIXED_60));
          color->blue = (guint8) (COGL_FIXED_TO_INT (tmp) * 255);
        }
      else if (h < COGL_FIXED_180)
	color->blue = (guint8) (COGL_FIXED_TO_INT (m2) * 255);
      else if (h < COGL_FIXED_240)
        {
          CoglFixed tmp;

          tmp = (m1 + COGL_FIXED_MUL_DIV ((m2 - m1),
                                          (COGL_FIXED_240 - h),
                                          COGL_FIXED_60));
          color->blue = (guint8) (COGL_FIXED_TO_INT (tmp) * 255);
        }
      else
	color->blue = (guint8) (COGL_FIXED_TO_INT (m1) * 255);
    }
}

/**
 * clutter_color_to_hls:
 * @color: a #ClutterColor
 * @hue: return location for the hue value or %NULL
 * @luminance: return location for the luminance value or %NULL
 * @saturation: return location for the saturation value or %NULL
 *
 * Converts @color to the HLS format.
 *
 * The @hue value is in the 0 .. 360 range. The @luminance and
 * @saturation values are in the 0 .. 1 range.
 */
void
clutter_color_to_hls (const ClutterColor *color,
		      gfloat             *hue,
		      gfloat             *luminance,
		      gfloat             *saturation)
{
  CoglFixed h, l, s;
  
  clutter_color_to_hlsx (color, &h, &l, &s);
  
  if (hue)
    *hue = COGL_FIXED_TO_FLOAT (h);

  if (luminance)
    *luminance = COGL_FIXED_TO_FLOAT (l);

  if (saturation)
    *saturation = COGL_FIXED_TO_FLOAT (s);
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
			gfloat        hue,
			gfloat        luminance,
			gfloat        saturation)
{
  CoglFixed h, l, s;

  h = COGL_FIXED_FROM_FLOAT (hue);
  l = COGL_FIXED_FROM_FLOAT (luminance);
  s = COGL_FIXED_FROM_FLOAT (saturation);

  clutter_color_from_hlsx (color, h, l, s);
}

/*
 * clutter_color_shadex:
 * @color: a #ClutterColor
 * @factor: the shade factor to apply, as a fixed point value
 * @result: (out): return location for the shaded color
 * 
 * Shades @color by @factor and saves the modified color into @result.
 */
static void
clutter_color_shadex (const ClutterColor *color,
                      CoglFixed           factor,
                      ClutterColor       *result)
{
  CoglFixed h, l, s;

  g_return_if_fail (color != NULL);
  g_return_if_fail (result != NULL);
  
  clutter_color_to_hlsx (color, &h, &l, &s);

  l = COGL_FIXED_MUL (l, factor);
  if (l > COGL_FIXED_1)
    l = COGL_FIXED_1;
  else if (l < 0)
    l = 0;

  s = COGL_FIXED_MUL (s, factor);
  if (s > COGL_FIXED_1)
    s = COGL_FIXED_1;
  else if (s < 0)
    s = 0;
  
  clutter_color_from_hlsx (result, h, l, s);

  result->alpha = color->alpha;
}

/**
 * clutter_color_shade:
 * @color: a #ClutterColor
 * @factor: the shade factor to apply
 * @result: (out): return location for the shaded color
 *
 * Shades @color by @factor and saves the modified color into @result.
 */
void
clutter_color_shade (const ClutterColor *color,
                     gdouble             factor,
		     ClutterColor       *result)
{
  clutter_color_shadex (color,
                        COGL_FIXED_FROM_FLOAT (factor),
                        result);
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
 * @color: (out): return location for a #ClutterColor
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

/**
 * clutter_color_from_string:
 * @color: (out): return location for a #ClutterColor
 * @str: a string specifiying a color (named color or #RRGGBBAA)
 *
 * Parses a string definition of a color, filling the
 * <structfield>red</structfield>, <structfield>green</structfield>, 
 * <structfield>blue</structfield> and <structfield>alpha</structfield> 
 * channels of @color. If alpha is not specified it will be set full opaque.
 *
 * The @color is not allocated.
 *
 * The color may be defined by any of the formats understood by
 * pango_color_from_string(); these include literal color names, like
 * <literal>Red</literal> or <literal>DarkSlateGray</literal>, or
 * hexadecimal specifications like <literal>&num;3050b2</literal> or
 * <literal>&num;333</literal>.
 *
 * Return value: %TRUE if parsing succeeded.
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

  /* if the string contains a color encoded using the hexadecimal
   * notations (#rrggbbaa or #rrggbb) we attempt a rough pass at
   * parsing the color ourselves, as we need the alpha channel that
   * Pango can't retrieve.
   */
  if (str[0] == '#')
    {
      gint32 result;

      if (sscanf (str + 1, "%x", &result))
	{
	  if (strlen (str) == 9)
	    {
              /* #rrggbbaa */
	      color->red   = (result >> 24) & 0xff;
	      color->green = (result >> 16) & 0xff;
	      color->blue  = (result >>  8) & 0xff;

	      color->alpha = result & 0xff;

	      return TRUE;
	    }
	  else if (strlen (str) == 7)
	    {
              /* #rrggbb */
	      color->red   = (result >> 16) & 0xff;
	      color->green = (result >>  8) & 0xff;
	      color->blue  = result & 0xff;

	      color->alpha = 0xff;

	      return TRUE;
	    }
	}

      /* XXX - should we return FALSE here? it's not like
       * Pango is endowed with mystical parsing powers and
       * will be able to do better than the code above.
       * still, it doesn't hurt
       */
    }
  
  /* Fall back to pango for named colors */
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
 * @v1: a #ClutterColor
 * @v2: a #ClutterColor
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
 * @v: a #ClutterColor
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
 * Return value: the newly allocated color. Use clutter_color_free()
 *   when done
 *
 * Since: 0.8.4
 */
ClutterColor *
clutter_color_new (guint8 red,
                   guint8 green,
                   guint8 blue,
                   guint8 alpha)
{
  ClutterColor *color;

  color = g_slice_new (ClutterColor);

  color->red   = red;
  color->green = green;
  color->blue  = blue;
  color->alpha = alpha;

  return color;
}

static void
clutter_value_transform_color_string (const GValue *src,
                                      GValue       *dest)
{
  gchar *string = clutter_color_to_string (src->data[0].v_pointer);

  g_value_take_string (dest, string);
}

static void
clutter_value_transform_string_color (const GValue *src,
                                      GValue       *dest)
{
  ClutterColor color = { 0, };

  clutter_color_from_string (&color, g_value_get_string (src));

  clutter_value_set_color (dest, &color);
}

GType
clutter_color_get_type (void)
{
  static GType _clutter_color_type = 0;
  
  if (G_UNLIKELY (_clutter_color_type == 0))
    {
       _clutter_color_type =
         g_boxed_type_register_static (I_("ClutterColor"),
                                       (GBoxedCopyFunc) clutter_color_copy,
                                       (GBoxedFreeFunc) clutter_color_free);

       g_value_register_transform_func (_clutter_color_type, G_TYPE_STRING,
                                        clutter_value_transform_color_string);
       g_value_register_transform_func (G_TYPE_STRING, _clutter_color_type,
                                        clutter_value_transform_string_color);
    }

  return _clutter_color_type;
}

static void
clutter_value_init_color (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
clutter_value_free_color (GValue *value)
{
  if (!(value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS))
    clutter_color_free (value->data[0].v_pointer);
}

static void
clutter_value_copy_color (const GValue *src,
                          GValue       *dest)
{
  dest->data[0].v_pointer = clutter_color_copy (src->data[0].v_pointer);
}

static gpointer
clutter_value_peek_color (const GValue *value)
{
  return value->data[0].v_pointer;
}

static gchar *
clutter_value_collect_color (GValue      *value,
                             guint        n_collect_values,
                             GTypeCValue *collect_values,
                             guint        collect_flags)
{
  if (!collect_values[0].v_pointer)
      value->data[0].v_pointer = NULL;
  else
    {
      if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
        {
          value->data[0].v_pointer = collect_values[0].v_pointer;
          value->data[1].v_uint = G_VALUE_NOCOPY_CONTENTS;
        }
      else
        {
          value->data[0].v_pointer =
            clutter_color_copy (collect_values[0].v_pointer);
        }
    }

  return NULL;
}

static gchar *
clutter_value_lcopy_color (const GValue *value,
                           guint         n_collect_values,
                           GTypeCValue  *collect_values,
                           guint         collect_flags)
{
  ClutterColor **color_p = collect_values[0].v_pointer;

  if (!color_p)
    return g_strdup_printf ("value location for `%s' passed as NULL",
                            G_VALUE_TYPE_NAME (value));

  if (!value->data[0].v_pointer)
    *color_p = NULL;
  else
    {
      if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
        *color_p = value->data[0].v_pointer;
      else
        *color_p = clutter_color_copy (value->data[0].v_pointer);
    }

  return NULL;
}

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

  value->data[0].v_pointer = clutter_color_copy (color);
}

/**
 * clutter_value_get_color:
 * @value: a #GValue initialized to #CLUTTER_TYPE_COLOR
 *
 * Gets the #ClutterColor contained in @value.
 *
 * Return value: the colors inside the passed #GValue
 *
 * Since: 0.8.4
 */
G_CONST_RETURN ClutterColor *
clutter_value_get_color (const GValue *value)
{
  g_return_val_if_fail (CLUTTER_VALUE_HOLDS_COLOR (value), NULL);

  return value->data[0].v_pointer;
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
  value->data[0].v_pointer = CLUTTER_PARAM_SPEC_COLOR (pspec)->default_value;
  value->data[1].v_uint = G_VALUE_NOCOPY_CONTENTS;
}

static gint
param_color_values_cmp (GParamSpec   *pspec,
                        const GValue *value1,
                        const GValue *value2)
{
  guint32 color1, color2;

  color1 = clutter_color_to_pixel (value1->data[0].v_pointer);
  color2 = clutter_color_to_pixel (value2->data[0].v_pointer);

  if (color1 < color2)
    return -1;
  else if (color1 == color2)
    return 0;
  else
    return 1;
}

static const GTypeValueTable _clutter_color_value_table = {
  clutter_value_init_color,
  clutter_value_free_color,
  clutter_value_copy_color,
  clutter_value_peek_color,
  "p",
  clutter_value_collect_color,
  "p",
  clutter_value_lcopy_color
};

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
 * clutter_param_spec_color:
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
