#ifndef __COGL_COLOR_H__
#define __COGL_COLOR_H__

#include <glib.h>
#include <cogl/cogl-fixed.h>

G_BEGIN_DECLS

typedef struct _CoglColor       CoglColor;

/**
 * CoglColor:
 *
 * A structure for holding a color definition. The contents of
 * the CoglColor structure are private and should never by accessed
 * directly.
 *
 * Since: 1.0
 */
struct _CoglColor
{
  /*< private >*/
  CoglFixed red;
  CoglFixed green;
  CoglFixed blue;

  CoglFixed alpha;
};

/**
 * cogl_color_set_from_4ub:
 * @dest: return location for a #CoglColor
 * @red: value of the red channel, between 0 and 255
 * @green: value of the green channel, between 0 and 255
 * @blue: value of the blue channel, between 0 and 255
 * @alpha: value of the alpha channel, between 0 and 255
 *
 * Sets the values of the passed channel into a #CoglColor.
 *
 * Since: 1.0
 */
void cogl_color_set_from_4ub (CoglColor *dest,
                              guint8 red,
                              guint8 green,
                              guint8 blue,
                              guint8 alpha);
/**
 * cogl_color_set_from_4ub:
 * @dest: return location for a #CoglColor
 * @red: value of the red channel, between 0 and 1
 * @green: value of the green channel, between 0 and 1
 * @blue: value of the blue channel, between 0 and 1
 * @alpha: value of the alpha channel, between 0 and 1
 *
 * Sets the values of the passed channel into a #CoglColor.
 *
 * Since: 1.0
 */
void cogl_color_set_from_4d  (CoglColor *dest,
                              gdouble red,
                              gdouble green,
                              gdouble blue,
                              gdouble alpha);

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
 * value between 0 and %COGL_FIXED_1.
 *
 * Return value: the red channel of the passed color
 *
 * Since: 1.0
 */
CoglFixed     cogl_color_get_red         (const CoglColor *color);

/**
 * cogl_color_get_green:
 * @color: a #CoglColor
 *
 * Retrieves the green channel of @color as a fixed point
 * value between 0 and %COGL_FIXED_1.
 *
 * Return value: the green channel of the passed color
 *
 * Since: 1.0
 */
CoglFixed     cogl_color_get_green       (const CoglColor *color);

/**
 * cogl_color_get_blue:
 * @color: a #CoglColor
 *
 * Retrieves the blue channel of @color as a fixed point
 * value between 0 and %COGL_FIXED_1.
 *
 * Return value: the blue channel of the passed color
 *
 * Since: 1.0
 */
CoglFixed     cogl_color_get_blue        (const CoglColor *color);

/**
 * cogl_color_get_alpha:
 * @color: a #CoglColor
 *
 * Retrieves the alpha channel of @color as a fixed point
 * value between 0 and %COGL_FIXED_1.
 *
 * Return value: the alpha channel of the passed color
 *
 * Since: 1.0
 */
CoglFixed     cogl_color_get_alpha       (const CoglColor *color);

G_END_DECLS

#endif /* __COGL_COLOR_H__ */
