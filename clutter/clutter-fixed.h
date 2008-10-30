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
typedef CoglFixed ClutterFixed;

/**
 * ClutterAngle:
 *
 * Integer representation of an angle such that 1024 corresponds to
 * full circle (i.e., 2*Pi).
 */
typedef CoglAngle ClutterAngle;    /* angle such that 1024 == 2*PI */

#define CLUTTER_ANGLE_FROM_DEG(x)  (COGL_ANGLE_FROM_DEG (x))
#define CLUTTER_ANGLE_FROM_DEGX(x) (COGL_ANGLE_FROM_DEGX (x))
#define CLUTTER_ANGLE_TO_DEG(x)    (COGL_ANGLE_TO_DEG (x))
#define CLUTTER_ANGLE_TO_DEGX(x)   (COGL_ANGLE_TO_DEGX (x))

/*
 * some commonly used constants
 */

/**
 * CFX_Q:
 *
 * Size in bits of decimal part of floating point value.
 */
#define CFX_Q           COGL_FIXED_Q

/**
 * CFX_ONE:
 *
 * 1.0 represented as a fixed point value.
 */
#define CFX_ONE         COGL_FIXED_1

/**
 * CFX_HALF:
 *
 * 0.5 represented as a fixed point value.
 */
#define CFX_HALF        COGL_FIXED_0_5

/**
 * CFX_MAX:
 *
 * Maximum fixed point value.
 */
#define CFX_MAX         COGL_FIXED_MAX

/**
 * CFX_MIN:
 *
 * Minimum fixed point value.
 */
#define CFX_MIN         COGL_FIXED_MIN

/**
 * CFX_PI:
 *
 * Fixed point representation of Pi
 */
#define CFX_PI          COGL_FIXED_PI
/**
 * CFX_2PI:
 *
 * Fixed point representation of Pi*2
 */
#define CFX_2PI         COGL_FIXED_2_PI
/**
 * CFX_PI_2:
 *
 * Fixed point representation of Pi/2
 */
#define CFX_PI_2        COGL_FIXED_PI_2
/**
 * CFX_PI_4:
 *
 * Fixed point representation of Pi/4
 */
#define CFX_PI_4        COGL_FIXED_PI_4
/**
 * CFX_360:
 *
 * Fixed point representation of the number 360
 */
#define CFX_360         COGL_FIXED_360
/**
 * CFX_240:
 *
 * Fixed point representation of the number 240
 */
#define CFX_240         COGL_FIXED_240
/**
 * CFX_180:
 *
 * Fixed point representation of the number 180
 */
#define CFX_180         COGL_FIXED_180
/**
 * CFX_120:
 *
 * Fixed point representation of the number 120
 */
#define CFX_120         COGL_FIXED_120
/**
 * CFX_60:
 *
 * Fixed point representation of the number 60
 */
#define CFX_60          COGL_FIXED_60
/**
 * CFX_RADIANS_TO_DEGREES:
 *
 * Fixed point representation of the number 180 / pi
 */
#define CFX_RADIANS_TO_DEGREES  COGL_RADIANS_TO_DEGREES
/**
 * CFX_255:
 *
 * Fixed point representation of the number 255
 */
#define CFX_255         COGL_FIXED_255

/**
 * CLUTTER_FIXED_TO_FLOAT:
 * @x: a fixed point value
 *
 * Convert a fixed point value to float.
 */
#define CLUTTER_FIXED_TO_FLOAT(x)       COGL_FIXED_TO_FLOAT ((x))

/**
 * CLUTTER_FIXED_TO_DOUBLE:
 * @x: a fixed point value
 *
 * Convert a fixed point value to double.
 */
#define CLUTTER_FIXED_TO_DOUBLE(x)      COGL_FIXED_TO_DOUBLE ((x))

/**
 * CLUTTER_FLOAT_TO_FIXED:
 * @x: a floating point value
 *
 * Convert a float value to fixed.
 */
#define CLUTTER_FLOAT_TO_FIXED(x)       COGL_FIXED_FROM_FLOAT ((x))

/**
 * CLUTTER_FLOAT_TO_INT:
 * @x: a floating point value
 *
 * Convert a float value to int.
 */
#define CLUTTER_FLOAT_TO_INT(x)         COGL_FLOAT_TO_INT ((x))

/**
 * CLUTTER_FLOAT_TO_UINT:
 * @x: a floating point value
 *
 * Convert a float value to unsigned int.
 */
#define CLUTTER_FLOAT_TO_UINT(x)        COGL_FLOAT_TO_UINT ((x))

/**
 * CLUTTER_INT_TO_FIXED:
 * @x: an integer value
 *
 * Convert an integer value to fixed point.
 */
#define CLUTTER_INT_TO_FIXED(x)         COGL_FIXED_FROM_INT ((x))

/**
 * CLUTTER_FIXED_TO_INT:
 * @x: a fixed point value
 *
 * Converts a fixed point value to integer (removing the decimal part).
 *
 * Since: 0.6
 */
#define CLUTTER_FIXED_TO_INT(x)         COGL_FIXED_TO_INT ((x))

/**
 * CLUTTER_FIXED_FRACTION:
 * @x: a fixed point value
 *
 * Retrieves the fractionary part of a fixed point value
 */
#define CLUTTER_FIXED_FRACTION(x)       COGL_FIXED_FRACTION ((x))

/**
 * CLUTTER_FIXED_FLOOR:
 * @x: a fixed point value
 *
 * Round down a fixed point value to an integer.
 */
#define CLUTTER_FIXED_FLOOR(x)          COGL_FIXED_FLOOR ((x))

/**
 * CLUTTER_FIXED_CEIL:
 * @x: a fixed point value
 *
 * Round up a fixed point value to an integer.
 */
#define CLUTTER_FIXED_CEIL(x)           COGL_FIXED_CEIL ((x))

/**
 * CLUTTER_FIXED_MUL:
 * @x: a fixed point value
 * @y: a fixed point value
 *
 * Multiply two fixed point values
 */
#define CLUTTER_FIXED_MUL(x,y)          COGL_FIXED_MUL ((x), (y))

/**
 * CLUTTER_FIXED_DIV:
 * @x: a fixed point value
 * @y: a fixed point value
 *
 * Divide two fixed point values
 */
#define CLUTTER_FIXED_DIV(x,y)          COGL_FIXED_DIV ((x), (y))

#define clutter_qmulx(x,y)              cogl_fixed_mul ((x), (y))
#define clutter_qdivx(x,y)              cogl_fixed_div ((x), (y))

#define clutter_sinx(a)                 cogl_fixed_sin ((a))
#define clutter_sini(a)                 cogl_angle_sin ((a))
#define clutter_tani(a)                 cogl_angle_tan ((a))
#define clutter_atani(a)                cogl_fixed_atan ((a))
#define clutter_atan2i(x,y)             cogl_fixed_atan2 ((x), (y))
#define clutter_cosx(a)                 cogl_fixed_cos ((a))
#define clutter_cosi(a)                 cogl_angle_cos ((a))

/**
 * CLUTTER_SQRTI_ARG_MAX
 *
 * Maximum argument that can be passed to #clutter_sqrti function.
 *
 * Since: 0.6
 */
#define CLUTTER_SQRTI_ARG_MAX           COGL_SQRTI_ARG_MAX

/**
 * CLUTTER_SQRTI_ARG_5_PERCENT
 *
 * Maximum argument that can be passed to #clutter_sqrti for which the
 * resulting error is < 5%
 *
 * Since: 0.6
 */
#define CLUTTER_SQRTI_ARG_5_PERCENT     COGL_SQRTI_ARG_5_PERCENT

/**
 * CLUTTER_SQRTI_ARG_10_PERCENT
 *
 * Maximum argument that can be passed to #clutter_sqrti for which the
 * resulting error is < 10%
 *
 * Since: 0.6
 */
#define CLUTTER_SQRTI_ARG_10_PERCENT    COGL_SQRTI_ARG_10_PERCENT

#define clutter_sqrtx(x)                cogl_fixed_sqrt ((x))
#define clutter_sqrti(x)                cogl_sqrti ((x))

#define clutter_log2x(x)                cogl_fixed_log2 ((x))
#define clutter_pow2x(x)                cogl_fixed_pow2 ((x))
#define clutter_powx(x,y)               cogl_fixed_pow ((x), (y))

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
#define CLUTTER_MAXFIXED        COGL_FIXED_MAX

/**
 * CLUTTER_MINFIXED:
 *
 * Lower boundary for #ClutterFixed
 *
 * Since: 0.8
 */
#define CLUTTER_MINFIXED        COGL_FIXED_MAX

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
