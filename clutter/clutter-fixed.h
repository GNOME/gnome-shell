/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Tomas Frydrych <tf@openedhand.com>
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

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_FIXED_H__
#define __CLUTTER_FIXED_H__

#include <glib-object.h>
#include <cogl/cogl.h>

G_BEGIN_DECLS

/**
 * ClutterFixed:
 *
 * Fixed point number (16.16)
 */
typedef float ClutterFixed;

/**
 * ClutterAngle:
 *
 * An abstract representation of an angle.
 */
typedef float ClutterAngle;

#define CLUTTER_ANGLE_FROM_DEG(x)  ((float)(x))
#define CLUTTER_ANGLE_FROM_DEGX(x) (CLUTTER_FIXED_TO_FLOAT (x))
#define CLUTTER_ANGLE_TO_DEG(x)    ((float)(x))
#define CLUTTER_ANGLE_TO_DEGX(x)   (CLUTTER_FLOAT_TO_FIXED (x))

/*
 * some commonly used constants
 */

/**
 * CFX_ONE:
 *
 * 1.0 represented as a fixed point value.
 */
#define CFX_ONE         1.0

/**
 * CFX_HALF:
 *
 * 0.5 represented as a fixed point value.
 */
#define CFX_HALF        0.5

/**
 * CFX_MAX:
 *
 * Maximum fixed point value.
 */
#define CFX_MAX         G_MAXFLOAT

/**
 * CFX_MIN:
 *
 * Minimum fixed point value.
 */
#define CFX_MIN         (-G_MAXFLOAT)

/**
 * CFX_PI:
 *
 * Fixed point representation of Pi
 */
#define CFX_PI          G_PI
/**
 * CFX_2PI:
 *
 * Fixed point representation of Pi*2
 */
#define CFX_2PI         (G_PI * 2)
/**
 * CFX_PI_2:
 *
 * Fixed point representation of Pi/2
 */
#define CFX_PI_2        (G_PI / 2)
/**
 * CFX_PI_4:
 *
 * Fixed point representation of Pi/4
 */
#define CFX_PI_4        (G_PI / 4)
/**
 * CFX_360:
 *
 * Fixed point representation of the number 360
 */
#define CFX_360         360.0
/**
 * CFX_240:
 *
 * Fixed point representation of the number 240
 */
#define CFX_240         240.0
/**
 * CFX_180:
 *
 * Fixed point representation of the number 180
 */
#define CFX_180         180.0
/**
 * CFX_120:
 *
 * Fixed point representation of the number 120
 */
#define CFX_120         120.0
/**
 * CFX_60:
 *
 * Fixed point representation of the number 60
 */
#define CFX_60          60.0
/**
 * CFX_RADIANS_TO_DEGREES:
 *
 * Fixed point representation of the number 180 / pi
 */
#define CFX_RADIANS_TO_DEGREES  (180.0 / G_PI)
/**
 * CFX_255:
 *
 * Fixed point representation of the number 255
 */
#define CFX_255         255.0

/**
 * CLUTTER_FIXED_TO_FLOAT:
 * @x: a fixed point value
 *
 * Convert a fixed point value to float.
 */
#define CLUTTER_FIXED_TO_FLOAT(x)       (x)

/**
 * CLUTTER_FIXED_TO_DOUBLE:
 * @x: a fixed point value
 *
 * Convert a fixed point value to double.
 */
#define CLUTTER_FIXED_TO_DOUBLE(x)      ((double)(x))

/**
 * CLUTTER_FLOAT_TO_FIXED:
 * @x: a floating point value
 *
 * Convert a float value to fixed.
 */
#define CLUTTER_FLOAT_TO_FIXED(x)       ((x))

/**
 * CLUTTER_FLOAT_TO_INT:
 * @x: a floating point value
 *
 * Convert a float value to int.
 */
#define CLUTTER_FLOAT_TO_INT(x)         ((int)(x))

/**
 * CLUTTER_FLOAT_TO_UINT:
 * @x: a floating point value
 *
 * Convert a float value to unsigned int.
 */
#define CLUTTER_FLOAT_TO_UINT(x)        ((unsigned int)(x))

/**
 * CLUTTER_INT_TO_FIXED:
 * @x: an integer value
 *
 * Convert an integer value to fixed point.
 */
#define CLUTTER_INT_TO_FIXED(x)         ((float)(x))

/**
 * CLUTTER_FIXED_TO_INT:
 * @x: a fixed point value
 *
 * Converts a fixed point value to integer (removing the decimal part).
 *
 * Since: 0.6
 */
#define CLUTTER_FIXED_TO_INT(x)         ((int)(x))

/**
 * CLUTTER_FIXED_FRACTION:
 * @x: a fixed point value
 *
 * Retrieves the fractionary part of a fixed point value
 */
#define CLUTTER_FIXED_FRACTION(x)       ((x)-floorf (x))

/**
 * CLUTTER_FIXED_FLOOR:
 * @x: a fixed point value
 *
 * Round down a fixed point value to an integer.
 */
#define CLUTTER_FIXED_FLOOR(x)          (floorf (x))

/**
 * CLUTTER_FIXED_CEIL:
 * @x: a fixed point value
 *
 * Round up a fixed point value to an integer.
 */
#define CLUTTER_FIXED_CEIL(x)           (ceilf (x))

/**
 * CLUTTER_FIXED_MUL:
 * @x: a fixed point value
 * @y: a fixed point value
 *
 * Multiply two fixed point values
 */
#define CLUTTER_FIXED_MUL(x,y)          ((x) * (y))

/**
 * CLUTTER_FIXED_DIV:
 * @x: a fixed point value
 * @y: a fixed point value
 *
 * Divide two fixed point values
 */
#define CLUTTER_FIXED_DIV(x,y)          ((x) / (y))

#define clutter_qmulx(x,y)              ((x) * (y))
#define clutter_qdivx(x,y)              ((x) / (y))

#define clutter_sinx(a)                 sinf (a * (G_PI/180.0))
#define clutter_tanx(a)                 tanf (a * (G_PI/180.0))
#define clutter_atanx(a)                atanf (a * (G_PI/180.0))
#define clutter_atan2x(x,y)             atan2f (x, y)
#define clutter_cosx(a)                 cosf (a * (G_PI/180.0))

#define CLUTTER_TYPE_FIXED                 (clutter_fixed_get_type ())
#define CLUTTER_TYPE_PARAM_FIXED           (clutter_param_fixed_get_type ())
#define CLUTTER_PARAM_SPEC_FIXED(pspec)    (G_TYPE_CHECK_INSTANCE_CAST ((pspec), CLUTTER_TYPE_PARAM_FIXED, ClutterParamSpecFixed))
#define CLUTTER_IS_PARAM_SPEC_FIXED(pspec) (G_TYPE_CHECK_INSTANCE_TYPE ((pspec), CLUTTER_TYPE_PARAM_FIXED))

/**
 * CLUTTER_VALUE_HOLDS_FIXED:
 * @x: a #GValue
 *
 * Evaluates to %TRUE if @x holds a #ClutterFixed.
 *
 * Since: 0.8
 */
#define CLUTTER_VALUE_HOLDS_FIXED(x)    (G_VALUE_HOLDS ((x), CLUTTER_TYPE_FIXED))

typedef struct _ClutterParamSpecFixed   ClutterParamSpecFixed;

/**
 * CLUTTER_MAXFIXED:
 *
 * Higher boundary for #ClutterFixed
 *
 * Since: 0.8
 */
#define CLUTTER_MAXFIXED        G_MAXFLOAT

/**
 * CLUTTER_MINFIXED:
 *
 * Lower boundary for #ClutterFixed
 *
 * Since: 0.8
 */
#define CLUTTER_MINFIXED        (-G_MAXFLOAT)

/**
 * ClutterParamSpecFixed
 * @minimum: lower boundary
 * @maximum: higher boundary
 * @default_value: default value
 *
 * #GParamSpec subclass for fixed point based properties
 *
 * Since: 0.8
 */
struct _ClutterParamSpecFixed
{
  /*< private >*/
  GParamSpec    parent_instance;

  /*< public >*/
  ClutterFixed  minimum;
  ClutterFixed  maximum;
  ClutterFixed  default_value;
};

GType        clutter_fixed_get_type       (void) G_GNUC_CONST;
GType        clutter_param_fixed_get_type (void) G_GNUC_CONST;

void         clutter_value_set_fixed      (GValue       *value,
                                           ClutterFixed  fixed_);
ClutterFixed clutter_value_get_fixed      (const GValue *value);

GParamSpec * clutter_param_spec_fixed     (const gchar  *name,
                                           const gchar  *nick,
                                           const gchar  *blurb,
                                           ClutterFixed  minimum,
                                           ClutterFixed  maximum,
                                           ClutterFixed  default_value,
                                           GParamFlags   flags);


G_END_DECLS

#endif /* __CLUTTER_FIXED_H__ */
