#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-color.h"
#include "cogl-fixed.h"

void
cogl_color_set_from_4ub (CoglColor *dest,
                         guint8     red,
                         guint8     green,
                         guint8     blue,
                         guint8     alpha)
{
  g_return_if_fail (dest != NULL);

  dest->red   = COGL_FIXED_FROM_FLOAT ((float) red   / 0xff * 1.0);
  dest->green = COGL_FIXED_FROM_FLOAT ((float) green / 0xff * 1.0);
  dest->blue  = COGL_FIXED_FROM_FLOAT ((float) blue  / 0xff * 1.0);
  dest->alpha = COGL_FIXED_FROM_FLOAT ((float) alpha / 0xff * 1.0);
}

void
cogl_color_set_from_4d (CoglColor *dest,
                        gdouble    red,
                        gdouble    green,
                        gdouble    blue,
                        gdouble    alpha)
{
  g_return_if_fail (dest != NULL);

  dest->red   = COGL_FIXED_FROM_FLOAT (CLAMP (red,   0.0, 1.0));
  dest->green = COGL_FIXED_FROM_FLOAT (CLAMP (green, 0.0, 1.0));
  dest->blue  = COGL_FIXED_FROM_FLOAT (CLAMP (blue,  0.0, 1.0));
  dest->alpha = COGL_FIXED_FROM_FLOAT (CLAMP (alpha, 0.0, 1.0));
}

unsigned char
cogl_color_get_red_byte (const CoglColor *color)
{
  return COGL_FIXED_TO_INT (color->red * 255);
}

float
cogl_color_get_red_float (const CoglColor *color)
{
  return COGL_FIXED_TO_FLOAT (color->red);
}

CoglFixed
cogl_color_get_red (const CoglColor *color)
{
  return color->red;
}

unsigned char
cogl_color_get_green_byte (const CoglColor *color)
{
  return COGL_FIXED_TO_INT (color->green * 255);
}

float
cogl_color_get_green_float (const CoglColor *color)
{
  return COGL_FIXED_TO_FLOAT (color->green);
}

CoglFixed
cogl_color_get_green (const CoglColor *color)
{
  return color->green;
}

unsigned char
cogl_color_get_blue_byte (const CoglColor *color)
{
  return COGL_FIXED_TO_INT (color->blue * 255);
}

float
cogl_color_get_blue_float (const CoglColor *color)
{
  return COGL_FIXED_TO_FLOAT (color->blue);
}

CoglFixed
cogl_color_get_blue (const CoglColor *color)
{
  return color->blue;
}

unsigned char
cogl_color_get_alpha_byte (const CoglColor *color)
{
  return COGL_FIXED_TO_INT (color->alpha * 255);
}

float
cogl_color_get_alpha_float (const CoglColor *color)
{
  return COGL_FIXED_TO_FLOAT (color->alpha);
}

CoglFixed
cogl_color_get_alpha (const CoglColor *color)
{
  return color->alpha;
}
