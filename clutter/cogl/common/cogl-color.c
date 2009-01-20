#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
cogl_color_set_from_4d (CoglColor *dest,
                        gdouble    red,
                        gdouble    green,
                        gdouble    blue,
                        gdouble    alpha)
{
  g_return_if_fail (dest != NULL);

  dest->red   = 255 * red;
  dest->green = 255 * green;
  dest->blue  = 255 * blue;
  dest->alpha = 255 * alpha;
}

void
cogl_color_set_from_4x (CoglColor *dest,
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
cogl_set_source_color4x (float red,
                         float green,
                         float blue,
                         float alpha)
{
  CoglColor c = { 0, };

  cogl_color_set_from_4x (&c, red, green, blue, alpha);
  cogl_set_source_color (&c);
}
